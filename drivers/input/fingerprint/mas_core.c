/*
 * mas_core.c - W527 Microarray SPI fingerprint driver (factory-spec port, r38)
 *
 * Sources:
 *   - ZTE Blade A3 tree drivers/input/fingerprint/microarray/madev_tee.c
 *     (public Microarray driver, same family as W527 factory - function names
 *      mas_probe/mas_ioctl/mas_interrupt/mas_get_interrupt_gpio match factory
 *      kallsyms one-to-one)
 *   - W527 factory binary capstone reverse of mas_read/mas_write/mas_sync/
 *     mas_mmap (the TEE reference lacks these fops; factory HAL uses them):
 *       mas_write: memset(tx,0,0x8000) + copy_from_user, tail zero-fill
 *       mas_read : mas_sync(tx,rx,count) + copy_to_user, -EFAULT on error
 *       mas_sync : spi_transfer{tx,rx,len,bits=8,speed=spi->max_speed_hz}+spi_sync
 *       mas_mmap : 128KB buffer, pgprot_noncached remap
 *
 * DTB binding (W527, untouched dtb partition):
 *   soc/ap-apb/spi@70a00000/fingerprint@2
 *     compatible = "microarray,mafp"; reg = 0;
 *     spi-max-frequency = <9600000>; fpint-gpios = <...>;
 *
 * Userspace ABI (must match vendor fingerprintd/mas HAL):
 *   /dev/madev0, ioctl magic 'M' (MA_IOC_*), mmap 128KB, read/write SPI channel
 *   input events: KEY_FN_F1..F7 mapped to FINGERPRINT_SWIPE_x, TAP, LONGPRESS
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/compat.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/uaccess.h>
#include <linux/pm_wakeup.h>
#include <linux/delay.h>

#define MA_DRV_VERSION		0x00004006
#define MA_CHR_DEV_NAME		"madev"
#define MA_CHR_FILE_NAME	"madev0"
#define MA_EINT_NAME		"sprd,finger_print-eint"
#define MA_DTS_COMPAT		"microarray,mafp"
#define MA_BUF_SIZE		0x8000		/* factory: count > 0x8000 rejected */
#define MA_MMAP_SIZE		(128 * 1024)	/* factory mas_mmap: 0x20000 */

/* key mapping (Microarray SDK convention, same as ZTE reference) */
#define FINGERPRINT_SWIPE_UP	KEY_FN_F1
#define FINGERPRINT_SWIPE_DOWN	KEY_FN_F2
#define FINGERPRINT_SWIPE_LEFT	KEY_FN_F3
#define FINGERPRINT_SWIPE_RIGHT	KEY_FN_F4
#define FINGERPRINT_TAP		KEY_FN_F5
#define FINGERPRINT_DTAP	KEY_FN_F6
#define FINGERPRINT_LONGPRESS	KEY_FN_F7

/* ioctl table (magic 'M', Microarray standard) */
#define MA_IOC_MAGIC		'M'
#define MA_IOC_DELK		_IO(MA_IOC_MAGIC, 1)
#define MA_IOC_SLEP		_IO(MA_IOC_MAGIC, 2)
#define MA_IOC_WKUP		_IO(MA_IOC_MAGIC, 3)
#define MA_IOC_ENCK		_IO(MA_IOC_MAGIC, 4)
#define MA_IOC_DICK		_IO(MA_IOC_MAGIC, 5)
#define MA_IOC_EINT		_IO(MA_IOC_MAGIC, 6)
#define MA_IOC_DINT		_IO(MA_IOC_MAGIC, 7)
#define MA_IOC_TPDW		_IO(MA_IOC_MAGIC, 8)
#define MA_IOC_TPUP		_IO(MA_IOC_MAGIC, 9)
#define MA_IOC_SGTP		_IO(MA_IOC_MAGIC, 11)
#define MA_IOC_DBTP		_IO(MA_IOC_MAGIC, 12)
#define MA_IOC_LGTP		_IO(MA_IOC_MAGIC, 13)
#define MA_IOC_VTIM		_IOR(MA_IOC_MAGIC, 14, unsigned char)
#define MA_IOC_CNUM		_IOR(MA_IOC_MAGIC, 15, unsigned char)
#define MA_IOC_SNUM		_IOR(MA_IOC_MAGIC, 16, unsigned char)
#define MA_IOC_UKRP		_IOW(MA_IOC_MAGIC, 17, unsigned char)
#define MA_IOC_NAVW		_IO(MA_IOC_MAGIC, 18)
#define MA_IOC_NAVA		_IO(MA_IOC_MAGIC, 19)
#define MA_IOC_NAVS		_IO(MA_IOC_MAGIC, 20)
#define MA_IOC_NAVD		_IO(MA_IOC_MAGIC, 21)
#define MA_IOC_SPAR		_IOW(MA_IOC_MAGIC, 22, unsigned int)
#define MA_IOC_GPAR		_IOR(MA_IOC_MAGIC, 23, unsigned int)
#define MA_IOC_PWOF		_IO(MA_IOC_MAGIC, 24)
#define MA_IOC_PWON		_IO(MA_IOC_MAGIC, 25)
#define MA_IOC_GVER		_IOR(MA_IOC_MAGIC, 26, unsigned int)
#define MA_IOC_STSP		_IOW(MA_IOC_MAGIC, 27, unsigned int)
#define MA_IOC_EIRQ		_IO(MA_IOC_MAGIC, 28)
#define MA_IOC_DIRQ		_IO(MA_IOC_MAGIC, 29)
#define MA_IOC_FD_WAIT_CMD	_IO(MA_IOC_MAGIC, 30)
#define MA_IOC_TEST_WAKE_FD	_IO(MA_IOC_MAGIC, 31)
#define MA_IOC_TEST_WAIT_FD_RET	_IO(MA_IOC_MAGIC, 32)
#define MA_IOC_FD_WAKE_TEST_RET	_IO(MA_IOC_MAGIC, 33)
#define MA_IOC_GET_SCREEN	_IOR(MA_IOC_MAGIC, 34, unsigned int)
#define MA_IOC_GET_INT_STATE	_IOR(MA_IOC_MAGIC, 35, unsigned int)
#define MA_IOC_GET_FACTORY_FLAG	_IOR(MA_IOC_MAGIC, 36, unsigned int)
#define MA_IOC_SET_FACTORY_FLAG	_IOW(MA_IOC_MAGIC, 37, unsigned int)
/* ---- factory-only 命令（capstone 提取自 factory mas_ioctl 0xffffff8008608118，
 *      26 cmd 全集: 1,2,3,5,7,9,11,12,13,18,20,21,31,33-44,47；ZTE 表缺 38-44,47）---- */
#define MA_IOC_SPWR		_IO(MA_IOC_MAGIC, 38)  /* case@8714: 从用户取 4B 存全局 */
#define MA_IOC_SCST		_IO(MA_IOC_MAGIC, 39)  /* case@8880(与41共用): 清标志[x1+1]=0，HAL 心跳循环发此命令 */
#define MA_IOC_SENR		_IO(MA_IOC_MAGIC, 40)  /* case@8850: 置标志[x1+2]=1 + 附带调用 */
#define MA_IOC_CCST		_IO(MA_IOC_MAGIC, 41)  /* case@8880: 同 39 */
#define MA_IOC_X42		_IO(MA_IOC_MAGIC, 42)  /* 二级比较链未还原: stub */
#define MA_IOC_X43		_IO(MA_IOC_MAGIC, 43)  /* 二级比较链未还原: stub */
#define MA_IOC_GSNR		_IO(MA_IOC_MAGIC, 44)  /* case@8ab8: 内部函数取值存全局 */
#define MA_IOC_SPID		_IO(MA_IOC_MAGIC, 47)  /* case@8b38: 外部调用+用户 arg 校验 */
static u8 ma_flag[4];   /* factory [ma_spi_t+1..2] 标志字节的对应物 */

#define MALOG_TAG "MAFP_"
#define MALOGE(x)	pr_err(MALOG_TAG "%s: error! %s, ret = %d\n", __func__, x, ret)
#define MALOGW(x)	pr_warn(MALOG_TAG "%s: warn! %s\n", __func__, x)

/* driver state */
static unsigned int irq_num, ret;
static unsigned int is_enable_irq;
static unsigned int irq_flag;
static unsigned int ma_drv_reg;
static unsigned int is_screen_on = 1;
static unsigned int ma_speed;
static unsigned int int_pin_state;
static bool irq_enabled;
static unsigned int g_factory_flag;
static unsigned int u1_flag, u2_flag;

static struct spi_device *ma_spi_t;
static struct input_dev *ma_input_t;
static struct wakeup_source *ma_ws;
static struct notifier_block mas_fb_nb;

static int int_gpio = -1;

static dev_t mas_devt;
static struct cdev *mas_cdev;
static struct class *mas_class;

static DECLARE_WAIT_QUEUE_HEAD(gWaitq);
static DECLARE_WAIT_QUEUE_HEAD(U1_Waitq);
static DECLARE_WAIT_QUEUE_HEAD(U2_Waitq);
static DEFINE_MUTEX(ioctl_lock);

/* factory buffers (capstone: static tx/rx 0x8000 each + 128KB mmap region) */
static u8 mas_tx_buf[MA_BUF_SIZE];
static u8 mas_rx_buf[MA_BUF_SIZE];
static void *mas_mm_addr;

static void mas_work(struct work_struct *pws)
{
	irq_flag = 1;
	__pm_wakeup_event(ma_ws, 2000);	/* wake_lock_timeout 2*HZ */
	wake_up(&gWaitq);
}

static DECLARE_WORK(gWork, mas_work);
static struct workqueue_struct *gWorkq;

static irqreturn_t mas_interrupt(int irq, void *dev_id)
{
	queue_work(gWorkq, &gWork);
	return IRQ_HANDLED;
}

static void mas_set_input(void)
{
	struct input_dev *input;

	input = input_allocate_device();
	if (!input) {
		pr_warn(MALOG_TAG "%s: input_allocate_device failed.\n", __func__);
		return;
	}
	set_bit(EV_KEY, input->evbit);
	set_bit(FINGERPRINT_SWIPE_UP, input->keybit);
	set_bit(FINGERPRINT_SWIPE_DOWN, input->keybit);
	set_bit(FINGERPRINT_SWIPE_LEFT, input->keybit);
	set_bit(FINGERPRINT_SWIPE_RIGHT, input->keybit);
	set_bit(FINGERPRINT_TAP, input->keybit);
	set_bit(FINGERPRINT_DTAP, input->keybit);
	set_bit(FINGERPRINT_LONGPRESS, input->keybit);
	set_bit(KEY_POWER, input->keybit);
	input->name = MA_CHR_DEV_NAME;
	input->id.bustype = BUS_SPI;
	ret = input_register_device(input);
	if (ret) {
		input_free_device(input);
		pr_warn(MALOG_TAG "%s: input_register_device failed.\n", __func__);
		return;
	}
	ma_input_t = input;
}

/* ---- factory mas_sync: full-duplex spi transfer, bits=8, max_speed_hz ---- */
int mas_sync(void *tx, void *rx, int len)
{
	struct spi_message m;
	struct spi_transfer t;
	int err;

	if (!tx || !rx || len <= 0 || !ma_spi_t) {
		pr_err("MAFP mas_sync: param error\n");
		return -EINVAL;
	}
	spi_message_init(&m);
	memset(&t, 0, sizeof(t));
	t.tx_buf = tx;
	t.rx_buf = rx;
	t.len = len;
	t.bits_per_word = 8;
	t.speed_hz = ma_spi_t->max_speed_hz;
	spi_message_add_tail(&t, &m);
	err = spi_sync(ma_spi_t, &m);
	if (err)
		pr_err("MAFP mas_sync: spi_sync error %d\n", err);
	return err;
}

/* ---- factory mas_write: fill static tx buffer ---- */
static ssize_t mas_write(struct file *filp, const char __user *ubuf,
			 size_t count, loff_t *ppos)
{
	size_t not_copied;

	if (count > MA_BUF_SIZE) {
		pr_err("MAFP mas_write: count %zu too big (max %d)\n",
		       count, MA_BUF_SIZE);
		return -EINVAL;
	}
	memset(mas_tx_buf, 0, MA_BUF_SIZE);
	not_copied = copy_from_user(mas_tx_buf, ubuf, count);
	if (not_copied)
		memset(mas_tx_buf + count - not_copied, 0, not_copied);
	return count;
}

/* ---- factory mas_read: mas_sync(tx,rx,count) + copy_to_user ---- */
static ssize_t mas_read(struct file *filp, char __user *ubuf,
			size_t count, loff_t *ppos)
{
	int err;

	if (count > MA_BUF_SIZE) {
		pr_err("MAFP mas_read: count %zu too big (max %d)\n",
		       count, MA_BUF_SIZE);
		return -EINVAL;
	}
	err = mas_sync(mas_tx_buf, mas_rx_buf, count);
	if (err) {
		pr_err("MAFP mas_read: mas_sync error\n");
		return -EFAULT;
	}
	err = copy_to_user(ubuf, mas_rx_buf, count);
	if (err) {
		pr_err("MAFP mas_read: copy_to_user error\n");
		return -EFAULT;
	}
	return count;
}

static long mas_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ioctl_ret = 0;
	int cp_ret = 0;

	switch (cmd) {
	case MA_IOC_DELK:
		__pm_wakeup_event(ma_ws, 5000);
		break;
	case MA_IOC_SLEP:
		irq_flag = 0;
		ioctl_ret = wait_event_freezekillable_unsafe(gWaitq, irq_flag != 0);
		return ioctl_ret;
	case MA_IOC_WKUP:
		irq_flag = 1;
		wake_up(&gWaitq);
		break;
	case MA_IOC_ENCK:
	case MA_IOC_DICK:
		/* spi clock always on on SPRD (factory stubs) */
		break;
	case MA_IOC_EINT:
		if (!irq_enabled) {
			irq_enabled = true;
			enable_irq(irq_num);
		}
		break;
	case MA_IOC_DINT:
		if (irq_enabled) {
			disable_irq_nosync(irq_num);
			irq_enabled = false;
		}
		break;
	case MA_IOC_TPDW:
		input_report_key(ma_input_t, FINGERPRINT_TAP, 1);
		input_sync(ma_input_t);
		break;
	case MA_IOC_TPUP:
		input_report_key(ma_input_t, FINGERPRINT_TAP, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_SGTP:
		input_report_key(ma_input_t, FINGERPRINT_TAP, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_TAP, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_DBTP:
		input_report_key(ma_input_t, FINGERPRINT_DTAP, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_DTAP, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_LGTP:
		input_report_key(ma_input_t, FINGERPRINT_LONGPRESS, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_LONGPRESS, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_NAVW:
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_UP, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_UP, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_NAVA:
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_LEFT, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_LEFT, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_NAVS:
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_DOWN, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_DOWN, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_NAVD:
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_RIGHT, 1);
		input_sync(ma_input_t);
		input_report_key(ma_input_t, FINGERPRINT_SWIPE_RIGHT, 0);
		input_sync(ma_input_t);
		break;
	case MA_IOC_SPAR:
		mutex_lock(&ioctl_lock);
		ret = copy_from_user(&ma_drv_reg, (unsigned int __user *)arg,
				     sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_GPAR:
		mutex_lock(&ioctl_lock);
		ret = copy_to_user((unsigned int __user *)arg, &ma_drv_reg,
				   sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_PWOF:
	case MA_IOC_PWON:
		/* power always on on W527 (no fpen-gpios in DTB) */
		break;
	case MA_IOC_GVER:
		mutex_lock(&ioctl_lock);
		cp_ret = put_user(MA_DRV_VERSION, (unsigned int __user *)arg);
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_STSP:
		mutex_lock(&ioctl_lock);
		cp_ret = copy_from_user(&ma_speed, (unsigned int __user *)arg,
					sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_EIRQ:
		if (is_enable_irq > 0)
			break;
		irq_enabled = true;
		ioctl_ret = request_irq(irq_num, mas_interrupt,
					IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND,
					MA_EINT_NAME, NULL);
		if (ioctl_ret < 0) {
			pr_err(MALOG_TAG "%s: request irq error\n", __func__);
			break;
		}
		enable_irq_wake(irq_num);
		is_enable_irq = ioctl_ret;
		break;
	case MA_IOC_DIRQ:
		if (is_enable_irq > 0) {
			free_irq(irq_num, NULL);
			is_enable_irq = 0;
		}
		break;
	case MA_IOC_FD_WAIT_CMD:
		u2_flag = 0;
		ioctl_ret = wait_event_interruptible(U2_Waitq, u2_flag != 0);
		break;
	case MA_IOC_TEST_WAKE_FD:
		u2_flag = 1;
		wake_up_interruptible(&U2_Waitq);
		break;
	case MA_IOC_TEST_WAIT_FD_RET:
		u1_flag = 0;
		ioctl_ret = wait_event_interruptible(U1_Waitq, u1_flag != 0);
		mutex_lock(&ioctl_lock);
		cp_ret = copy_to_user((unsigned int __user *)arg, &ma_drv_reg,
				      sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_FD_WAKE_TEST_RET:
		mutex_lock(&ioctl_lock);
		cp_ret = copy_from_user(&ma_drv_reg, (unsigned int __user *)arg,
					sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		msleep(20);
		u1_flag = 1;
		wake_up_interruptible(&U1_Waitq);
		break;
	case MA_IOC_GET_SCREEN:
		mutex_lock(&ioctl_lock);
		cp_ret = copy_to_user((unsigned int __user *)arg, &is_screen_on,
				      sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_GET_INT_STATE:
		int_pin_state = gpio_get_value(int_gpio);
		if (int_pin_state == 0 || int_pin_state == 1) {
			mutex_lock(&ioctl_lock);
			cp_ret = copy_to_user((unsigned int __user *)arg,
					      &int_pin_state,
					      sizeof(unsigned int));
			mutex_unlock(&ioctl_lock);
		}
		break;
	case MA_IOC_GET_FACTORY_FLAG:
		mutex_lock(&ioctl_lock);
		cp_ret = copy_to_user((unsigned int __user *)arg, &g_factory_flag,
				      sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_SET_FACTORY_FLAG:
		mutex_lock(&ioctl_lock);
		cp_ret = copy_from_user(&g_factory_flag, (unsigned int __user *)arg,
					sizeof(unsigned int));
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_SPWR:
		mutex_lock(&ioctl_lock);
		cp_ret = copy_from_user(ma_flag, (void __user *)arg, 4);
		mutex_unlock(&ioctl_lock);
		break;
	case MA_IOC_SCST:
	case MA_IOC_CCST:
		/* factory 语义: 清标志字节并返回 0（HAL dc_send_cmd 0x4d27 心跳） */
		ma_flag[1] = 0;
		break;
	case MA_IOC_SENR:
		ma_flag[2] = 1;
		break;
	case MA_IOC_X42:
	case MA_IOC_X43:
	case MA_IOC_GSNR:
	case MA_IOC_SPID:
		/* 可识别即返回 0，语义待 HAL 流程推进后按 logcat 迭代 */
		break;
	default:
		ioctl_ret = -EINVAL;
		pr_warn("MAFP mas_ioctl: no such cmd %u\n", cmd);
	}
	return ioctl_ret;
}

#ifdef CONFIG_COMPAT
static long mas_compat_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	return mas_ioctl(filp, cmd, arg);
}
#endif

/* ---- factory mas_mmap: 128KB buffer, noncached remap ---- */
static int mas_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long page;

	if (!mas_mm_addr) {
		mas_mm_addr = kzalloc(MA_MMAP_SIZE, GFP_KERNEL);
		if (!mas_mm_addr)
			return -ENOMEM;
	}
	page = virt_to_phys(mas_mm_addr) >> PAGE_SHIFT;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, page,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	return 0;
}

static const struct file_operations mas_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= mas_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= mas_compat_ioctl,
#endif
	.mmap		= mas_mmap,
	.read		= mas_read,
	.write		= mas_write,
};

static int mas_fb_notifier_callback(struct notifier_block *self,
				    unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (event != FB_EVENT_BLANK)
		return 0;
	blank = *(int *)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
		is_screen_on = 1;
		break;
	case FB_BLANK_POWERDOWN:
		is_screen_on = 0;
		break;
	default:
		break;
	}
	return 0;
}

static int mas_probe(struct spi_device *spi)
{
	int err;
	struct device_node *np = spi->dev.of_node;

	pr_info("MAFP mas_probe: start, compatible %s\n", MA_DTS_COMPAT);

	if (!np) {
		pr_err("MAFP mas_probe: no of_node\n");
		return -ENODEV;
	}

	ma_ws = wakeup_source_register("microarray_process_wakelock");
	if (!ma_ws)
		return -ENOMEM;
	gWorkq = create_singlethread_workqueue("mas_workqueue");
	if (!gWorkq) {
		err = -ENOMEM;
		goto err_ws;
	}
	INIT_WORK(&gWork, mas_work);

	/* irq from DTB "fpint-gpios" (factory mas_get_irq semantics) */
	int_gpio = of_get_named_gpio(np, "fpint-gpios", 0);
	pr_info("MAFP mas_probe: fpint-gpios = %d\n", int_gpio);
	if (int_gpio < 0) {
		pr_err("MAFP mas_probe: get fpint-gpios failed\n");
		err = int_gpio;
		goto err_wq;
	}
	err = gpio_request(int_gpio, "microarray_eint");
	if (err < 0) {
		pr_err("MAFP mas_probe: gpio request error\n");
		goto err_wq;
	}
	gpio_direction_input(int_gpio);
	irq_num = gpio_to_irq(int_gpio);
	if (irq_num < 0) {
		pr_err("MAFP mas_probe: gpio_to_irq failed\n");
		err = irq_num;
		goto err_gpio;
	}
	err = request_irq(irq_num, mas_interrupt,
			  IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND,
			  MA_EINT_NAME, NULL);
	if (err < 0) {
		pr_err("MAFP mas_probe: request_irq failed\n");
		goto err_gpio;
	}

	ma_spi_t = spi;

	/* chardev /dev/madev0 */
	err = alloc_chrdev_region(&mas_devt, 0, 1, MA_CHR_DEV_NAME);
	if (err < 0) {
		pr_err("MAFP mas_probe: alloc_chrdev_region error\n");
		goto err_irq;
	}
	mas_cdev = cdev_alloc();
	if (!mas_cdev) {
		err = -ENOMEM;
		goto err_chr;
	}
	mas_cdev->owner = THIS_MODULE;
	mas_cdev->ops = &mas_fops;
	err = cdev_add(mas_cdev, mas_devt, 1);
	if (err)
		goto err_chr;
	mas_class = class_create(THIS_MODULE, MA_CHR_DEV_NAME);
	if (IS_ERR(mas_class)) {
		err = PTR_ERR(mas_class);
		goto err_cdev;
	}
	if (IS_ERR(device_create(mas_class, NULL, mas_devt, NULL,
				 MA_CHR_FILE_NAME))) {
		err = -ENOMEM;
		goto err_class;
	}

	mas_set_input();

	mas_fb_nb.notifier_call = mas_fb_notifier_callback;
	err = fb_register_client(&mas_fb_nb);
	if (err)
		goto err_input;

	pr_info("MAFP mas_probe: done, irq=%u\n", irq_num);
	return 0;

err_input:
	ma_input_t = NULL;
err_class:
	class_destroy(mas_class);
	mas_class = NULL;
err_cdev:
	cdev_del(mas_cdev);
	mas_cdev = NULL;
err_chr:
	unregister_chrdev_region(mas_devt, 1);
err_irq:
	if (is_enable_irq > 0) {
		free_irq(irq_num, NULL);
		is_enable_irq = 0;
	}
err_gpio:
	gpio_free(int_gpio);
	int_gpio = -1;
err_wq:
	destroy_workqueue(gWorkq);
	gWorkq = NULL;
err_ws:
	wakeup_source_unregister(ma_ws);
	ma_ws = NULL;
	return err;
}

static int mas_remove(struct spi_device *spi)
{
	fb_unregister_client(&mas_fb_nb);
	if (ma_input_t) {
		input_unregister_device(ma_input_t);
		ma_input_t = NULL;
	}
	if (mas_class) {
		device_destroy(mas_class, mas_devt);
		class_destroy(mas_class);
		mas_class = NULL;
	}
	if (mas_cdev) {
		cdev_del(mas_cdev);
		mas_cdev = NULL;
	}
	unregister_chrdev_region(mas_devt, 1);
	if (is_enable_irq > 0) {
		free_irq(irq_num, NULL);
		is_enable_irq = 0;
	}
	if (int_gpio >= 0) {
		gpio_free(int_gpio);
		int_gpio = -1;
	}
	if (gWorkq) {
		destroy_workqueue(gWorkq);
		gWorkq = NULL;
	}
	if (ma_ws) {
		wakeup_source_unregister(ma_ws);
		ma_ws = NULL;
	}
	kfree(mas_mm_addr);
	mas_mm_addr = NULL;
	return 0;
}

static const struct of_device_id mas_of_match[] = {
	{ .compatible = MA_DTS_COMPAT },
	{ }
};
MODULE_DEVICE_TABLE(of, mas_of_match);

static struct spi_driver mas_spi_driver = {
	.driver = {
		.name		= "fingerprint",
		.owner		= THIS_MODULE,
		.of_match_table	= mas_of_match,
	},
	.probe	= mas_probe,
	.remove	= mas_remove,
};

static int __init mas_init(void)
{
	int err;

	err = spi_register_driver(&mas_spi_driver);
	if (err)
		pr_err("MAFP mas_init: spi_register_driver ret = %d\n", err);
	return err;
}

static void __exit mas_exit(void)
{
	spi_unregister_driver(&mas_spi_driver);
}

module_init(mas_init);
module_exit(mas_exit);

MODULE_AUTHOR("Microarray / W527 r38 port");
MODULE_DESCRIPTION("Microarray SPI fingerprint sensor driver (factory-spec)");
MODULE_LICENSE("GPL");
