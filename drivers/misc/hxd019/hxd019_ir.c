// SPDX-License-Identifier: GPL-2.0
/*
 * hxd019_ir.c - HXD019 IR transceiver controller driver for W527 (SPRD UWS6152)
 *
 * Faithful 4.14 reimplementation of the factory "infra"/"ctrl" driver,
 * reverse-engineered from factory kernel Image (capstone, 2026-10-31):
 *   - platform driver name "infra", DT compatible "hxdkj,hxd019"
 *   - /dev/ctrl (misc) with write=TX frame queue, read=RX/learn pull,
 *     ioctl 0x8002-0x8008 = manual GPIO primitives
 *   - /sys/class/ctrl/ctrl/hxdenable (power), /sys/.../hxd019/infra_pin (debug)
 *   - 3-wire bitbang: SDA(gpio-sda)=data, SCL(gpio-scl)=clock, busy-gpios=status
 *   - timing: ~27us slot unit; wake = SCL low 7x1ms + high 20x1ms
 *
 * Fixes vs factory (documented divergences):
 *   - power gpio prop: DTB has "pwr-gpios", factory queried "pwr-gpio" (broken)
 *   - pinctrl states: factory names "gpio_128_f4"/"gpio_127_f4" (and _f1 on
 *     close) do not exist in the W527 hxd019 DTB node; fall back to the
 *     node's own "sda_gpio_func"/"scl_gpio_func" / "sda_sda_func"/"scl_sda_func"
 *   - ctrl_read 1s wait: factory busy-spins udelay(1ms)x1000; we msleep-poll
 *     BUSY with 1s cap (early exit when chip asserts BUSY)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/string.h>

#define DRV_NAME		"infra"
#define MISC_NAME		"ctrl"

#define CTRL_IOC_BUSY_H2L	0x8002
#define CTRL_IOC_BUSY_LOW	0x8003
#define CTRL_IOC_SDA_HIGH	0x8005
#define CTRL_IOC_SDA_LOW	0x8006
#define CTRL_IOC_SCL_HIGH	0x8007
#define CTRL_IOC_SCL_LOW	0x8008

#define TX_FRAME_MAX		0x100
#define RX_BUF			0x100

#define SLOT_US			27
#define SLOT_1MS		1000

#define FM_401030		0x401030
#define FM_502030		0x502030
#define FM_316230		0x316230
#define FM_330			0x000330
#define FM_AA99880330		0xAA99880330ULL

struct tx_pkt {
	u8 data[TX_FRAME_MAX];
	size_t len;
	struct list_head list;
};

struct hxd_entry {
	int gpio;
	bool requested;
};

struct hxd_ctrl {
	struct hxd_entry e[5];
	int pwr_gpio;
	bool pwr_requested;
	bool gpio_inited;
	bool pwr_on;
	bool chrdev_ready;

	struct pinctrl *pinctrl;
	struct pinctrl_state *st_sda_gpio, *st_scl_gpio;
	struct pinctrl_state *st_sda_func, *st_scl_func;

	struct list_head queue;
	spinlock_t qlock;
	struct work_struct work;
	struct class *cls;
	struct device *cdev;

	int mode_flag;
	u8 rxb[RX_BUF];
};

static struct hxd_ctrl *hxd;
static struct platform_device *hxd_pdev;

static void g_out(int gpio, int v)
{
	if (gpio_is_valid(gpio))
		gpio_set_value(gpio, v);
}

static void slot_us(void)
{
	udelay(SLOT_US);
}

static void slot_1ms(void)
{
	usleep_range(SLOT_1MS, SLOT_1MS + 20);
}

/* gpio_init (factory 0x86068a0): idle - SDA=1, SCL=1, BUSY=input */
static void hxd_gpio_init(void)
{
	g_out(hxd->e[1].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	if (gpio_is_valid(hxd->e[0].gpio))
		gpio_direction_input(hxd->e[0].gpio);
	g_out(hxd->e[1].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
}

/* gpio_open (factory 0x8606b30): one-shot SDA/SCL bring-up + pinctrl gpio func */
static int hxd_gpio_open(void)
{
	int num, i;

	if (hxd->gpio_inited) {
		g_out(hxd->e[1].gpio, 1);
		g_out(hxd->e[2].gpio, 1);
		g_out(hxd->e[2].gpio, 1);
		g_out(hxd->e[1].gpio, 1);
		return 0;
	}

	for (i = 1; i <= 2; i++) {
		if (i == 1 && hxd->st_sda_gpio)
			pinctrl_select_state(hxd->pinctrl, hxd->st_sda_gpio);
		if (i == 2 && hxd->st_scl_gpio)
			pinctrl_select_state(hxd->pinctrl, hxd->st_scl_gpio);
		num = of_get_named_gpio(hxd_pdev->dev.of_node,
					i == 1 ? "gpio-sda" : "gpio-scl", 0);
		if (num < 0)
			return num;
		hxd->e[i].gpio = num;
		if (!hxd->e[i].requested) {
			if (gpio_request(num, i == 1 ? "gpio-sda" : "gpio-scl"))
				return -EBUSY;
			hxd->e[i].requested = true;
		}
		gpio_direction_output(num, 0);
	}

	hxd->gpio_inited = true;
	g_out(hxd->e[1].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	g_out(hxd->e[1].gpio, 1);
	return 0;
}

/* gpio_close (factory 0x8606c10): free SDA/SCL, restore func states, idle */
static void hxd_gpio_close(void)
{
	if (hxd->gpio_inited) {
		hxd->gpio_inited = false;
		if (hxd->e[1].requested) {
			gpio_free(hxd->e[1].gpio);
			hxd->e[1].requested = false;
		}
		if (hxd->e[2].requested) {
			gpio_free(hxd->e[2].gpio);
			hxd->e[2].requested = false;
		}
		if (hxd->st_sda_func)
			pinctrl_select_state(hxd->pinctrl, hxd->st_sda_func);
		if (hxd->st_scl_func)
			pinctrl_select_state(hxd->pinctrl, hxd->st_scl_func);
	}
	g_out(hxd->e[1].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	g_out(hxd->e[1].gpio, 1);
}

/* gpio_start (factory 0x8606cb4) */
static void hxd_start(void)
{
	g_out(hxd->e[1].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	g_out(hxd->e[1].gpio, 1);
	slot_us();
	slot_us();
	g_out(hxd->e[1].gpio, 0);
	slot_us(); slot_us(); slot_us(); slot_us();
	slot_us(); slot_us(); slot_us(); slot_us();
	slot_us();
	g_out(hxd->e[0].gpio, 0);
	slot_us();
	slot_us();
}

/* gpio_stop (factory 0x8606ddc) */
static void hxd_stop(void)
{
	g_out(hxd->e[1].gpio, 1);
	g_out(hxd->e[2].gpio, 1);
	g_out(hxd->e[2].gpio, 0);
	g_out(hxd->e[1].gpio, 0);
	slot_us();
	g_out(hxd->e[2].gpio, 1);
	slot_us();
	g_out(hxd->e[1].gpio, 1);
	slot_us();
}

/* gpio_check_ack (factory 0x8606e80): sample data line during clock pulse */
static int hxd_check_ack(void)
{
	int ack;

	if (gpio_is_valid(hxd->e[1].gpio))
		gpio_direction_input(hxd->e[1].gpio);
	slot_us(); slot_us(); slot_us(); slot_us();
	g_out(hxd->e[2].gpio, 1);
	slot_us(); slot_us(); slot_us(); slot_us();
	ack = gpio_get_value(hxd->e[1].gpio);
	slot_us(); slot_us(); slot_us(); slot_us();
	g_out(hxd->e[2].gpio, 0);
	slot_us(); slot_us(); slot_us(); slot_us();
	return ack;
}

/* gpio_send_ack (factory 0x8606fa0) */
static void hxd_send_ack(void)
{
	g_out(hxd->e[1].gpio, 1);
	slot_us(); slot_us();
	g_out(hxd->e[2].gpio, 1);
	slot_us();
	g_out(hxd->e[2].gpio, 0);
}

/* gpio_write_data (factory 0x86070f4): 8 bits MSB first, SDA=data SCL=clk */
static void hxd_write_byte(u8 byte)
{
	int i;

	g_out(hxd->e[1].gpio, 1);
	g_out(hxd->e[2].gpio, 0);
	slot_us();
	for (i = 7; i >= 0; i--) {
		slot_us();
		g_out(hxd->e[1].gpio, (byte >> i) & 1);
		slot_us();
		g_out(hxd->e[2].gpio, 1);
		slot_us();
		g_out(hxd->e[2].gpio, 0);
	}
	hxd_check_ack();
}

/* gpio_read_data (factory 0x8607014): read 8 bits MSB first, then send_ack */
static u8 hxd_read_byte(void)
{
	u8 val = 0;
	int i;

	if (gpio_is_valid(hxd->e[1].gpio))
		gpio_direction_input(hxd->e[1].gpio);
	for (i = 0; i < 8; i++) {
		g_out(hxd->e[2].gpio, 1);
		slot_us();
		val <<= 1;
		val |= gpio_get_value(hxd->e[1].gpio) & 1;
		g_out(hxd->e[2].gpio, 0);
		slot_us(); slot_us();
	}
	g_out(hxd->e[2].gpio, 0);
	slot_us();
	hxd_send_ack();
	return val;
}

/* wake sequence (factory process_commands / gpio_learn / ctrl_read) */
static void hxd_wake(void)
{
	int i;

	hxd_gpio_open();
	slot_us();
	for (i = 0; i < 7; i++) {
		g_out(hxd->e[2].gpio, 0);
		if (i < 6)
			slot_1ms();
	}
	for (i = 0; i < 20; i++) {
		g_out(hxd->e[2].gpio, 1);
		if (i < 19)
			slot_1ms();
	}
}

/* TX worker (factory process_commands 0x860743c) */
static void process_commands(struct work_struct *work)
{
	struct hxd_ctrl *c = container_of(work, struct hxd_ctrl, work);
	struct tx_pkt *pkt;
	u32 cmd24;
	u64 magic5;
	int i;

	for (;;) {
		spin_lock(&c->qlock);
		if (list_empty(&c->queue)) {
			spin_unlock(&c->qlock);
			return;
		}
		pkt = list_first_entry(&c->queue, struct tx_pkt, list);
		list_del(&pkt->list);
		spin_unlock(&c->qlock);

		cmd24 = pkt->data[0] | (pkt->data[1] << 8) |
			(pkt->data[2] << 16);
		if (cmd24 == FM_401030)
			c->mode_flag = 0;
		else if (cmd24 == FM_502030)
			c->mode_flag = 1;

		if (pkt->data[0] != 0x30)
			goto free_pkt;
		if (cmd24 == FM_316230 || cmd24 == FM_330)
			goto free_pkt;
		magic5 = (u64)pkt->data[0] | ((u64)pkt->data[1] << 8) |
			 ((u64)pkt->data[2] << 16) | ((u64)pkt->data[3] << 24) |
			 ((u64)pkt->data[4] << 32);
		if (magic5 == FM_AA99880330)
			goto free_pkt;
		if (cmd24 == FM_330) {
			if (pkt->data[7] == 0xff)
				pkt->data[7] = 0;
			if (pkt->data[TX_FRAME_MAX - 1] == 0xff)
				pkt->data[TX_FRAME_MAX - 1] = 0;
		}
		if ((u8)(pkt->data[5] - 0x10) > 0x70)
			pkt->data[5] = 0x34;

		hxd_wake();
		hxd_start();
		slot_us();
		for (i = 0; i < pkt->len; i++) {
			hxd_write_byte(pkt->data[i]);
			slot_us();
		}
		slot_us();
		hxd_stop();
		slot_us();
		hxd_gpio_close();
		slot_us();
		if (gpio_is_valid(hxd->e[2].gpio))
			gpio_direction_input(hxd->e[2].gpio);

free_pkt:
		kfree(pkt);
	}
}

static int ctrl_open(struct inode *ino, struct file *f)
{
	return 0;
}

static int ctrl_release(struct inode *ino, struct file *f)
{
	return 0;
}

/* factory ctrl_write 0x8606570: enqueue frame (<=0x100B), kick worker */
static ssize_t ctrl_write(struct file *f, const char __user *ubuf,
			  size_t count, loff_t *ppos)
{
	struct tx_pkt *pkt;

	if (count > TX_FRAME_MAX)
		return -EINVAL;
	if (!hxd || !hxd->chrdev_ready)
		return -ENODEV;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt)
		return -ENOMEM;

	if (copy_from_user(pkt->data, ubuf, count)) {
		kfree(pkt);
		return -EFAULT;
	}
	pkt->len = count;

	spin_lock(&hxd->qlock);
	list_add_tail(&pkt->list, &hxd->queue);
	spin_unlock(&hxd->qlock);
	schedule_work(&hxd->work);
	return count;
}

/*
 * factory ctrl_read 0x86071d4: wait (up to 1s) for BUSY assert, then
 * 30 52/62 ... start 31 ... read n(+1) bytes ... stop. Factory busy-spins
 * 1s always; we msleep-poll with early exit, same 1s cap.
 */
static ssize_t ctrl_read(struct file *f, char __user *ubuf,
			 size_t count, loff_t *ppos)
{
	u8 tmp;
	int n, i, waited;

	if (!count)
		return 0;
	if (!hxd || !hxd->chrdev_ready)
		return -ENODEV;

	memset(hxd->rxb, 0, RX_BUF);
	for (waited = 0; waited < 1000; waited++) {
		if (gpio_is_valid(hxd->e[0].gpio) &&
		    gpio_get_value(hxd->e[0].gpio))
			break;
		msleep(1);
	}
	if (waited >= 1000)
		return 0;

	n = (count < 0xff) ? count : 0xff;

	hxd_wake();
	hxd_start();
	slot_us();
	hxd_write_byte(0x30);
	slot_us();
	hxd_write_byte(hxd->mode_flag ? 0x62 : 0x52);
	slot_us();
	hxd_start();
	slot_us();
	hxd_write_byte(0x31);
	slot_us();

	tmp = hxd_read_byte();
	hxd->rxb[0] = tmp;
	for (i = 1; i < n; i++) {
		tmp = hxd_read_byte();
		hxd->rxb[i] = tmp;
		slot_us();
	}
	hxd_read_byte();

	slot_us();
	hxd_stop();
	slot_us();
	hxd_gpio_close();
	slot_us();
	if (gpio_is_valid(hxd->e[2].gpio))
		gpio_direction_input(hxd->e[2].gpio);

	if (copy_to_user(ubuf, hxd->rxb, RX_BUF))
		return -EFAULT;
	return n;
}

/* factory ctrl_ioctl 0x8606444: manual pin primitives (0x8004=ENOTTY) */
static long ctrl_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	if (!hxd || !hxd->chrdev_ready)
		return -ENODEV;

	switch (cmd) {
	case CTRL_IOC_BUSY_H2L:
		g_out(hxd->e[0].gpio, 0);
		g_out(hxd->e[0].gpio, 1);
		break;
	case CTRL_IOC_BUSY_LOW:
		g_out(hxd->e[0].gpio, 0);
		g_out(hxd->e[0].gpio, 0);
		break;
	case CTRL_IOC_SDA_HIGH:
		g_out(hxd->e[1].gpio, 1);
		g_out(hxd->e[1].gpio, 1);
		break;
	case CTRL_IOC_SDA_LOW:
		g_out(hxd->e[1].gpio, 1);
		g_out(hxd->e[1].gpio, 0);
		break;
	case CTRL_IOC_SCL_HIGH:
		g_out(hxd->e[2].gpio, 1);
		g_out(hxd->e[2].gpio, 1);
		break;
	case CTRL_IOC_SCL_LOW:
		g_out(hxd->e[2].gpio, 1);
		g_out(hxd->e[2].gpio, 0);
		break;
	default:
		return -ENOTTY;
	}
	if (gpio_is_valid(hxd->e[0].gpio))
		gpio_direction_input(hxd->e[0].gpio);
	return 0;
}

static const struct file_operations ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= ctrl_open,
	.release	= ctrl_release,
	.write		= ctrl_write,
	.read		= ctrl_read,
	.unlocked_ioctl	= ctrl_ioctl,
};

/*
 * Manual cdev (mas/madev0 style) - misc_register returned -EBUSY on this
 * kernel even with a clean /proc/misc, root cause untracked; the mas driver
 * proved manual chrdev+class works here, so use that.
 */
static dev_t ctrl_devt;
static struct cdev ctrl_cdev;
static struct class *ctrl_cls;

/* sysfs: hxdenable (power) */
static ssize_t hxd_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", hxd->pwr_on ? 1 : 0);
}

/*
 * factory hxd_enable_store 0x86066a0; prop fixed: DTB has "pwr-gpios"
 * (factory queried "pwr-gpio" - dead path on this board).
 */
static ssize_t hxd_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int num, ret;
	u8 val;

	ret = kstrtou8(buf, 0, &val);
	if (ret)
		return ret;

	if (val == 1) {
		if (hxd->pwr_on)
			return count;
		num = of_get_named_gpio(hxd_pdev->dev.of_node, "pwr-gpios", 0);
		if (num < 0)
			num = of_get_named_gpio(hxd_pdev->dev.of_node,
						"pwr1-gpios", 0);
		if (num < 0) {
			dev_err(dev, "hxd019: no pwr gpio (%d)\n", num);
			return num;
		}
		hxd->pwr_gpio = num;
		if (!hxd->pwr_requested) {
			if (gpio_request(num, "hxd_power_en"))
				return -EBUSY;
			hxd->pwr_requested = true;
		}
		gpio_direction_output(num, 1);
		hxd->pwr_on = true;
	} else if (val == 0) {
		if (!hxd->pwr_on)
			return count;
		if (hxd->pwr_requested && gpio_is_valid(hxd->pwr_gpio)) {
			gpio_direction_output(hxd->pwr_gpio, 0);
			gpio_free(hxd->pwr_gpio);
			hxd->pwr_requested = false;
		}
		hxd->pwr_on = false;
	}
	return count;
}

static DEVICE_ATTR(hxdenable, 0664, hxd_enable_show, hxd_enable_store);

/* factory infra_pin_store 0x86061a4: "%d %d" idx(0-4) value, 3x+5ms */
static ssize_t infra_pin_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int idx, val, i;

	if (sscanf(buf, "%d %d", &idx, &val) != 2)
		return -EINVAL;
	if (idx > 4)
		return -EINVAL;

	for (i = 0; i < 3; i++) {
		if (hxd->e[idx].requested && gpio_is_valid(hxd->e[idx].gpio))
			gpio_set_value(hxd->e[idx].gpio, val);
		msleep(5);
	}
	return count;
}

static ssize_t infra_pin_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return -EINVAL;
}

static DEVICE_ATTR(infra_pin, 0644, infra_pin_show, infra_pin_store);

static struct attribute *infra_pin_attrs[] = {
	&dev_attr_infra_pin.attr,
	NULL,
};

static const struct attribute_group infra_pin_group = {
	.attrs = infra_pin_attrs,
};

static int infra_probe(struct platform_device *pdev)
{
	struct pinctrl_state *st;
	int num, ret;

	if (hxd)
		return -EBUSY;

	hxd = kzalloc(sizeof(*hxd), GFP_KERNEL);
	if (!hxd)
		return -ENOMEM;

	hxd->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(hxd->pinctrl)) {
		dev_err(&pdev->dev, "Failed to get pinctrl\n");
		kfree(hxd);
		hxd = NULL;
		return PTR_ERR(hxd->pinctrl);
	}

	/* resolve pinctrl states: factory names first, DTB-node names fallback */
	st = pinctrl_lookup_state(hxd->pinctrl, "gpio_128_f4");
	if (IS_ERR(st))
		st = pinctrl_lookup_state(hxd->pinctrl, "sda_gpio_func");
	hxd->st_sda_gpio = IS_ERR(st) ? NULL : st;
	st = pinctrl_lookup_state(hxd->pinctrl, "gpio_127_f4");
	if (IS_ERR(st))
		st = pinctrl_lookup_state(hxd->pinctrl, "scl_gpio_func");
	hxd->st_scl_gpio = IS_ERR(st) ? NULL : st;
	st = pinctrl_lookup_state(hxd->pinctrl, "gpio_128_f1");
	if (IS_ERR(st))
		st = pinctrl_lookup_state(hxd->pinctrl, "sda_sda_func");
	hxd->st_sda_func = IS_ERR(st) ? NULL : st;
	st = pinctrl_lookup_state(hxd->pinctrl, "gpio_127_f1");
	if (IS_ERR(st))
		st = pinctrl_lookup_state(hxd->pinctrl, "scl_scl_func");
	hxd->st_scl_func = IS_ERR(st) ? NULL : st;

	hxd_pdev = pdev;
	platform_set_drvdata(pdev, hxd);
	INIT_LIST_HEAD(&hxd->queue);
	spin_lock_init(&hxd->qlock);
	INIT_WORK(&hxd->work, process_commands);

	/* entry0: BUSY - request + output low (factory probe) */
	num = of_get_named_gpio(pdev->dev.of_node, "busy-gpios", 0);
	if (num >= 0) {
		hxd->e[0].gpio = num;
		if (!gpio_request(num, "busy-gpios")) {
			hxd->e[0].requested = true;
			gpio_direction_output(num, 0);
		}
	}

	/* entry3/4: pwr1/pwr2 optional */
	num = of_get_named_gpio(pdev->dev.of_node, "pwr1-gpios", 0);
	if (num >= 0)
		hxd->e[3].gpio = num;
	num = of_get_named_gpio(pdev->dev.of_node, "pwr2-gpios", 0);
	if (num >= 0)
		hxd->e[4].gpio = num;

	ret = sysfs_create_group(&pdev->dev.kobj, &infra_pin_group);
	if (ret)
		dev_warn(&pdev->dev, "infra_pin group failed: %d\n", ret);

	ret = alloc_chrdev_region(&ctrl_devt, 0, 1, MISC_NAME);
	if (ret) {
		dev_err(&pdev->dev, "alloc_chrdev_region failed: %d\n", ret);
		goto err_misc;
	}
	dev_info(&pdev->dev, "ctrl: devt %d:%d\n",
		 MAJOR(ctrl_devt), MINOR(ctrl_devt));

	cdev_init(&ctrl_cdev, &ctrl_fops);
	ctrl_cdev.owner = THIS_MODULE;
	ret = cdev_add(&ctrl_cdev, ctrl_devt, 1);
	if (ret) {
		dev_err(&pdev->dev, "cdev_add failed: %d\n", ret);
		goto err_cdev;
	}

	ctrl_cls = class_create(THIS_MODULE, "ctrl");
	if (IS_ERR(ctrl_cls)) {
		ret = PTR_ERR(ctrl_cls);
		dev_err(&pdev->dev, "class_create ctrl failed: %d\n", ret);
		goto err_cls;
	}
	hxd->cls = ctrl_cls;

	{
		struct device *cdev;

		cdev = device_create(ctrl_cls, NULL, ctrl_devt, NULL, "ctrl");
		if (IS_ERR(cdev)) {
			ret = PTR_ERR(cdev);
			dev_err(&pdev->dev, "device_create ctrl failed: %d\n",
				ret);
			goto err_dev;
		}
		hxd->cdev = cdev;
		ret = device_create_file(cdev, &dev_attr_hxdenable);
		if (ret)
			dev_warn(&pdev->dev, "hxdenable file failed: %d\n",
				 ret);
	}

	hxd->chrdev_ready = true;
	hxd_gpio_init();

	dev_info(&pdev->dev,
		 "hxd019 ctrl ready: /dev/ctrl devt %d:%d, busy=%d sda=%d scl=%d\n",
		 MAJOR(ctrl_devt), MINOR(ctrl_devt), hxd->e[0].gpio,
		 hxd->e[1].gpio, hxd->e[2].gpio);
	return 0;

err_dev:
	class_destroy(ctrl_cls);
	ctrl_cls = NULL;
err_cls:
	cdev_del(&ctrl_cdev);
err_cdev:
	unregister_chrdev_region(ctrl_devt, 1);
err_misc:
	sysfs_remove_group(&pdev->dev.kobj, &infra_pin_group);
	if (hxd->e[0].requested) {
		gpio_free(hxd->e[0].gpio);
		hxd->e[0].requested = false;
	}
	cancel_work_sync(&hxd->work);
	kfree(hxd);
	hxd = NULL;
	hxd_pdev = NULL;
	return ret;
}

static int infra_remove(struct platform_device *pdev)
{
	if (!hxd)
		return 0;
	hxd->chrdev_ready = false;
	cancel_work_sync(&hxd->work);
	if (hxd->cdev)
		device_destroy(ctrl_cls, ctrl_devt);
	if (ctrl_cls) {
		class_destroy(ctrl_cls);
		ctrl_cls = NULL;
	}
	cdev_del(&ctrl_cdev);
	unregister_chrdev_region(ctrl_devt, 1);
	sysfs_remove_group(&pdev->dev.kobj, &infra_pin_group);
	if (hxd->e[0].requested)
		gpio_free(hxd->e[0].gpio);
	if (hxd->pwr_requested && gpio_is_valid(hxd->pwr_gpio))
		gpio_free(hxd->pwr_gpio);
	kfree(hxd);
	hxd = NULL;
	hxd_pdev = NULL;
	return 0;
}

static const struct of_device_id infra_of_match[] = {
	{ .compatible = "hxdkj,hxd019" },
	{ },
};
MODULE_DEVICE_TABLE(of, infra_of_match);

static struct platform_driver infra_driver = {
	.probe	= infra_probe,
	.remove	= infra_remove,
	.driver	= {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(infra_of_match),
	},
};

static int __init infra_init(void)
{
	return platform_driver_register(&infra_driver);
}

static void __exit infra_exit(void)
{
	platform_driver_unregister(&infra_driver);
}

module_init(infra_init);
module_exit(infra_exit);
MODULE_DESCRIPTION("HXD019 IR transceiver ctrl driver (W527 factory port)");
MODULE_LICENSE("GPL");
