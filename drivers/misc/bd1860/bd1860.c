// bd1860.c - W527 r43: BD1860 heart-rate sensor driver (factory behavior 1:1)
//
// Everything here is transcribed from the factory kernel binary
// (bd1860_survey/REVERSE_NOTES.md sections 7.1 / 9.5 / 10). The transaction
// sequence and control flow are replicated verbatim, so behavior does not
// depend on our interpretation of register semantics.
//
// Structure (factory-verified):
//   probe            : i2c bind (DTB compatible "bd,bd1860"), input dev "bd1860",
//                      class/bd1860/device/{hrenable,hrtest} + debug {dump,id,reg}
//   hrenable 1       : BD_Sensor_Init(mode 2) + polling kthread pump
//   hrenable 0       : BD_Sensor_Close (07=00, E8=01)
//   polling kthread  : wait_for_completion -> dataread -> re-arm -> usleep 46ms
//                      (factory cycle 63ms = 46ms sleep + i2c time, 15.9Hz)
//   dataread mode2   : chA direct reads 0x27/28/29 (PPG24) + 0x2e/2f (aux16)
//                      chB: if AGC adjusting -> wr 0x17/0x18=curA + EB latch,
//                           then reads 0x32/33/34 (PPG24) + 0x39/3a (aux16)
//                      chC: if AGC adjusting -> wr 0x19/0x1a=curB + EB latch,
//                           then reads 0x3d/3e/3f (PPG24) + 0x44/45 (aux16)
//                      per-channel AGC: settle window (0x2903,0x34bc],
//                      step +-5 (aux<=0x1b57 / aux>0x3c8c) else +-1,
//                      cur caps (0x77, 6], activation window !(0x2133,0x3a98]
//                      19-frame history gate before hp mirrors are refreshed
//   frame print      : factory line=257 25-value format (result[] slots) +
//                      r43 raw-pipeline line for A/B verification vs factory_ab
//                      result[0..4] stay 0 until health algorithm is ported (r44)
//   input            : EV_ABS codes 0..4 from result[0..4] + EV_SYN (factory)

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include "bd1860_alg.h"

#define BD_DRV_NAME		"bd1860"
#define BD_CLASS_NAME		"bd1860"
#define BD_DEV_NAME		"device"

#define BD_RING_LEN		20	/* per-channel ring buffers (factory 0x13+1) */
#define BD_POLL_US		46000	/* factory usleep_range(0xb3b0, 0xb3b0)	*/

/* r51: per-frame debug prints default OFF.
 * Measured 2026-11-03: 20Hz printk (UART console sync write) ate 4 cores
 * down to 227% idle with SystemUI at 63% -- user-visible UI stutter while
 * a sensor session is open. Factory ships no such prints; keep them
 * opt-in via /sys/module/bd1860/parameters/dbg. */
static bool bd_dbg_en;
module_param_named(dbg, bd_dbg_en, bool, 0644);
MODULE_PARM_DESC(dbg, "per-frame debug prints: line=257 / r44-raw / r45-regs / r51-arrays");


struct bd_chan {
	u16	cur;			/* LED current / mux value			*/
	u8	retry;			/* AGC adjusting (factory 0x662/0x663)		*/
	u8	active;			/* AGC ran in window (factory 0x665/0x666)	*/
	s32	ppg[BD_RING_LEN];	/* PPG24 ring (factory 0x668/0x6d0/0x738)	*/
	u16	aux[BD_RING_LEN];	/* aux16 ring (factory 0x5c0/0x5f8/0x630)	*/
	/* health-pro input mirrors (factory 0x7a0/0x808/0x870) */
	s32	hp_ppg[BD_RING_LEN];
	s32	hp_auxsum;		/* +0x50: last-4 aux average>>2 sum		*/
	s32	hp_cur;			/* +0x54: current value snapshot		*/
	s32	hp_flag;		/* +0x58: active flag snapshot			*/
};

struct bd_state {
	struct i2c_client	*cli;
	struct input_dev	*input;
	struct class		*cls;
	struct device		*clsdev;
	struct completion	comp;
	struct task_struct	*poll_task;
	struct mutex		lock;

	int			enabled;	/* factory +4 hr_is_enable	*/
	int			mode;		/* factory +8 test_kinds	*/
	int			pwr_gpio;	/* factory +0 pwr_gpio_number	*/

	int			frame_cnt;	/* factory [0x5bc]		*/
	struct bd_chan		ch[3];		/* A/B/C			*/
	u16			curC;		/* factory [0x69e] (static r43)	*/

	int			result[25];	/* factory g+0x70		*/

	s32			raw_ppg[3];	/* last-frame raw snapshot	*/
	u16			raw_aux[3];
};

static struct bd_state *bds;

static int bd_wr(struct i2c_client *cli, u8 reg, u8 val)
{
	u8 buf[2] = { reg, val };
	int ret = i2c_master_send(cli, buf, 2);

	return (ret == 2) ? 0 : (ret < 0 ? ret : -EIO);
}

static int bd_rd(struct i2c_client *cli, u8 reg)
{
	return i2c_smbus_read_byte_data(cli, reg);
}

/* ---------- factory I2C primitive adapters (alg side) ----------
 * factory semantics: BD_I2C_Read returns the data byte and sets *flag;
 * BD_I2C_Write returns success flag. First arg is the ignored client
 * pointer residue from decompilation. */
u8 BD_I2C_Read(u64 cli, u8 reg, u8 *flag)
{
	int v;

	(void)cli;
	if (!bds || !bds->cli) { *flag = 0; return 0; }
	v = bd_rd(bds->cli, reg);
	if (v < 0) { *flag = 0; return 0; }
	*flag = 1;
	return (u8)v;
}

u8 BD_I2C_Write(u64 cli, u8 reg, u8 val)
{
	int ret;

	(void)cli;
	if (!bds || !bds->cli) return 0;
	ret = bd_wr(bds->cli, reg, val);
	/* r45 P0: NACK forensics - print only on failure so normal operation
	 * stays silent; a failed init write would show reg/val/err directly */
	if (ret != 0)
		pr_info("[gxy_bd1860] i2c write FAIL reg=0x%02x val=0x%02x err=%d\n",
			reg, val, ret);
	return (ret == 0) ? 1 : 0;
}

/* ---------- factory init sequence (BD_16XX_init_start semantics,
 * ref values recovered from factory disasm 0x85ed140-0x85ed1c4) ---------- */
static int bd44_init(int mode)
{
	u8 init_ok, start_ok;

	test_kinds = (u8)mode;
	switch (mode) {
	case 2:		/* spo2 */
		sensor_refset[0] = 0; sensor_refset[1] = 8;  sensor_refset[2] = 8;
		wear_refdata[0] = 10000; wear_refdata[1] = 30000; wear_refdata[2] = 6000;
		break;
	case 0xd:	/* wear */
		sensor_refset[0] = 0; sensor_refset[1] = 0;  sensor_refset[2] = 2;
		wear_refdata[0] = 1500;  wear_refdata[1] = 500;   wear_refdata[2] = 10;
		break;
	default:	/* 6: hrm (factory echo '1'/'3') */
		sensor_refset[0] = 5; sensor_refset[1] = 0;  sensor_refset[2] = 2;
		wear_refdata[0] = 1500;  wear_refdata[1] = 100;   wear_refdata[2] = 5;
		break;
	}

	init_ok = BD_Sensor_Init((s32)mode, sensor_refset);
	start_ok = BD_Sensor_Start((u64)mode, wear_refdata);
	pr_info("[gxy_bd1860] init_start: temp1=%d,temp2=%d (mode=%d)\n",
		init_ok, start_ok, mode);
	return (init_ok && start_ok) ? 0 : -EIO;
}

/* ---------- per-frame collect + algorithm (factory BD_16XX_dataread) ---------- */
static void bd_16xx_dataread(struct bd_state *s)
{
	u8 ok;
	int i;

	memset(bd_data_result, 0, sizeof(bd_data_result));
	ok = BD_Sensor_Data_Read((int)test_kinds);
	if (ok == 1) {
		BD_health_pro((u32)test_kinds, 0, 0, 0, bd_data_result);
		/* r51: all per-frame prints gated by bd_dbg_en (default off).
		 * NOTE: pr_cont chains can be truncated by concurrent printk
		 * from other cores (observed as "bd_data_result = " with no
		 * values during WCN log storms) -- cosmetic, data unaffected. */
		if (bd_dbg_en) {
			printk("[gxy_bd1860] line=257 iRet=%u mode=%u bd_data_result = ",
			       ok, test_kinds);
			for (i = 0; i < 25; i++)
				pr_cont("%u ", bd_data_result[i]);
			pr_cont("\n");
			/* r43-raw debug line: raw pipeline visibility.
			 * r47: chA = live per-sample ch_A_buf (fixed slots 0/10/19);
			 * dcA = RAW_A_data[20] per-frame DC accumulator.
			 * (r46 lesson: RAW_A_data[0..19] is copied from RAW_A_buf,
			 * which is never written -- zombie chain, ppgA always 0;
			 * fc%26 indexing also lands on odd slots only.) */
			printk("[gxy_bd1860] r44-raw fc=%d chA=[%u %u %u] dcA=%u cur=[%u %u %u]\n",
			       s->frame_cnt,
			       bd44_dbg_raw_a(0), bd44_dbg_raw_a(10),
			       bd44_dbg_raw_a(19), bd44_dbg_dc_a(),
			       bd44_dbg_cur(0), bd44_dbg_cur(1), bd44_dbg_cur(2));
		}
		/* r47: ok-frame counter -- dump on 1st + every 10th ok frame
		 * (~12 s cadence). r45/r46 "%26==0" was mathematically dead:
		 * ok frames have fc == 19 (mod 20), always odd; %26==0
		 * requires even fc. The two sets never intersect. */
		{
			static u32 ok_cnt;

			ok_cnt++;
			/* r52: full RAW data-side AND buf-side dump every 20 ok
			 * frames, plus live phdata_count -- single scnprintf
			 * printk each (pr_cont-proof).
			 * r51 evidence: RAW_A_data stride-4 fill (slots 0/4/8/
			 * 12/16 only) and RAW_B_data impossible 0x2Bxxxxxx
			 * values. Comparing both sides + the index catches the
			 * polluter in one shot. */
			if (bd_dbg_en && (ok_cnt % 20) == 1) {
				char b[480];
				int n;
				int i2;

				n = scnprintf(b, sizeof(b),
					"[gxy_bd1860] r52 idx=%u arrA:",
					bd44_dbg_phcount());
				for (i2 = 0; i2 < 20; i2++)
					n += scnprintf(b + n, sizeof(b) - n,
						       " %u",
						       bd44_dbg_raw_arr(0, i2));
				printk("%s\n", b);
				n = scnprintf(b, sizeof(b), "[gxy_bd1860] r52 bufA:");
				for (i2 = 0; i2 < 20; i2++)
					n += scnprintf(b + n, sizeof(b) - n,
						       " %u",
						       bd44_dbg_buf_arr(0, i2));
				printk("%s\n", b);
				n = scnprintf(b, sizeof(b), "[gxy_bd1860] r52 arrB:");
				for (i2 = 0; i2 < 20; i2++)
					n += scnprintf(b + n, sizeof(b) - n,
						       " %u",
						       bd44_dbg_raw_arr(1, i2));
				printk("%s\n", b);
				n = scnprintf(b, sizeof(b), "[gxy_bd1860] r52 bufB:");
				for (i2 = 0; i2 < 20; i2++)
					n += scnprintf(b + n, sizeof(b) - n,
						       " %u",
						       bd44_dbg_buf_arr(1, i2));
				printk("%s\n", b);
				printk("[gxy_bd1860] r52 dcvolt: DC=%u DC1=%u\n",
				       bd44_dbg_dcvolt(0),
				       bd44_dbg_dcvolt(1));
			}
			if (bd_dbg_en && (ok_cnt == 1 || (ok_cnt % 10) == 0)
			    && s->cli) {
				int r[9];
				int k;
				static const u8 regs[9] =
					{ 0x27, 0x28, 0x29, 0x2e, 0x2f,
					  0x14, 0x15, 0x17, 0x18 };
				for (k = 0; k < 9; k++)
					r[k] = i2c_smbus_read_byte_data(
							bds->cli, regs[k]);
				printk("[gxy_bd1860] r45-regs ok%d: "
				       "27=%02x 28=%02x 29=%02x 2e=%02x "
				       "2f=%02x 14=%02x 15=%02x 17=%02x "
				       "18=%02x\n",
				       ok_cnt,
				       r[0], r[1], r[2], r[3], r[4],
				       r[5], r[6], r[7], r[8]);
			}
		}
		/* factory input report: EV_ABS codes 0..4 */
		if (s->input) {
			for (i = 0; i <= 4; i++)
				input_report_abs(s->input, i, bd_data_result[i]);
			input_sync(s->input);
		}
	}
	s->frame_cnt++;
}

static int bd_poll_task(void *data)
{
	struct bd_state *s = data;

	while (!kthread_should_stop()) {
		wait_for_completion_interruptible(&s->comp);
		bd_16xx_dataread(s);
		if (s->enabled)
			complete(&s->comp);	/* factory re-arm	*/
		usleep_range(BD_POLL_US, BD_POLL_US);
	}
	return 0;
}

/* ---------- sysfs: class/bd1860/device/{hrenable,hrtest} ---------- */

static ssize_t hrenable_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", bds ? bds->enabled : 0);
}

static ssize_t hrenable_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;

	if (!bds)
		return -ENODEV;

	mutex_lock(&bds->lock);
	pr_info("[gxy_bd1860] hrenable_store: buf[0] = 0x%x, enabled = %d\n",
		(u8)buf[0], bds->enabled);

	ret = 0;
	if (buf[0] == '0' || buf[0] == '\0') {
		if (!bds->enabled)
			goto out_unlock;
		BD_Sensor_Close();
		hardware_flag1 = 0;
		bds->enabled = 0;
	} else if (buf[0] == '1' || buf[0] == '3') {
		if (bds->enabled)
			goto out_unlock;
		msleep(100);
		ret = bd44_init(6);		/* factory: echo 1/3 -> mode 6 */
		if (!ret) {
			bds->mode = 6;
			bds->enabled = 1;
			complete(&bds->comp);
		}
	} else if (buf[0] == '2') {
		if (bds->enabled)
			goto out_unlock;
		msleep(100);
		ret = bd44_init(2);
		if (!ret) {
			bds->mode = 2;
			bds->enabled = 1;
			complete(&bds->comp);
		}
	} else if (buf[0] == '4') {
		if (bds->enabled)
			goto out_unlock;
		msleep(100);
		ret = bd44_init(0xd);
		if (!ret) {
			bds->mode = 0xd;
			bds->enabled = 1;
			complete(&bds->comp);
		}
	} else {
		pr_info("[gxy_bd1860] hrenable_store: invalid input\n");
	}
out_unlock:
	mutex_unlock(&bds->lock);
	return count;
}static DEVICE_ATTR_RW(hrenable);

static ssize_t hrtest_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	/* factory hrtest_show @ 0x85ed044: scnprintf(buf, 0x1000, "%s\n", "bd1860") */
	return scnprintf(buf, PAGE_SIZE, "%s\n", BD_DRV_NAME);
}

/* factory bd1860_test_read_and_write @ 0x85ed81c (capstone-verified):
 * wr reg5=0x08 -> printk(483) -> msleep(100) -> rd reg5 -> printk(486)
 * -> return (readback == 0x08). res = raw i2c_transfer count
 * (write 1 msg = 1, read 2 msgs = 2, negative = err); read buffer
 * starts 0xff so a failed read prints 0xff like factory. */
static int bd1860_test_read_and_write(void)
{
	int res, val;
	u8 rbuf = 0xff;

	res = bd_wr(bds->cli, 0x05, 0x08);
	res = (res == 0) ? 1 : res;
	pr_info("[gxy_bd1860] %s %d : write test, res = %d, reg = 0x%x, data = 0x%x\n",
		__func__, 483, res, 0x05, 0x08);
	msleep(100);
	val = bd_rd(bds->cli, 0x05);
	if (val >= 0)
		rbuf = (u8)val;
	res = (val >= 0) ? 2 : val;
	pr_info("[gxy_bd1860] %s %d : read test, res = %d, regvalue = 0x%x\n",
		__func__, 486, res, rbuf);
	return (rbuf == 0x08) ? 1 : 0;
}

static ssize_t hrtest_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	/* factory hrtest_store @ 0x85ed8f0: '0' or NUL triggers self test
	 * (verdict return value discarded, same as factory) */
	if (bds && (buf[0] == '0' || buf[0] == '\0'))
		bd1860_test_read_and_write();
	return count;
}
static DEVICE_ATTR_RW(hrtest);

/* ---------- sysfs: debug attrs on i2c dev (kept from r42 stub) ---------- */

static ssize_t bd_dump_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int i, n = 0;

	mutex_lock(&bds->lock);
	for (i = 0; i < 0x80; i++) {
		int v = bd_rd(bds->cli, (u8)i);

		if (v < 0)
			n += scnprintf(buf + n, PAGE_SIZE - n, "%02x: ERR%d\n",
				       i, v);
		else
			n += scnprintf(buf + n, PAGE_SIZE - n, "%02x: %02x\n",
				       i, v);
		if (n >= PAGE_SIZE - 32)
			break;
	}
	mutex_unlock(&bds->lock);
	return n;
}
static struct device_attribute dev_attr_dump =
		__ATTR(dump, 0444, bd_dump_show, NULL);

static ssize_t bd_id_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int v[8], i, n = 0;

	mutex_lock(&bds->lock);
	for (i = 0; i < 8; i++) {
		v[i] = bd_rd(bds->cli, (u8)i);
		if (v[i] < 0)
			v[i] = 0xEE;
	}
	mutex_unlock(&bds->lock);
	n = scnprintf(buf, PAGE_SIZE,
		      "reg00-07: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		      v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
	return n;
}
static struct device_attribute dev_attr_id =
		__ATTR(id, 0444, bd_id_show, NULL);

static u8 bd_reg_addr;
static ssize_t bd_reg_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int v;

	mutex_lock(&bds->lock);
	v = bd_rd(bds->cli, bd_reg_addr);
	mutex_unlock(&bds->lock);
	if (v < 0)
		return v;
	return scnprintf(buf, PAGE_SIZE, "reg 0x%02x = 0x%02x\n",
			 bd_reg_addr, v);
}

static ssize_t bd_reg_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned int addr = 0, val = 0;
	int items;

	items = sscanf(buf, "%x %x", &addr, &val);
	if (items < 1 || addr > 0xFF)
		return -EINVAL;
	mutex_lock(&bds->lock);
	bd_reg_addr = (u8)addr;
	if (items == 2)
		bd_wr(bds->cli, bd_reg_addr, (u8)val);
	mutex_unlock(&bds->lock);
	return count;
}
static struct device_attribute dev_attr_reg =
		__ATTR(reg, 0644, bd_reg_show, bd_reg_store);

static struct attribute *bd_i2c_attrs[] = {
	&dev_attr_dump.attr,
	&dev_attr_id.attr,
	&dev_attr_reg.attr,
	NULL,
};
static const struct attribute_group bd_i2c_group = { .attrs = bd_i2c_attrs };

/* ---------- probe / remove ---------- */

/* factory bd1860_power_init @ 0x85edb7c (1:1, incl. prints 555/557/563).
 * Real DTB has no "bd1860,pwr-gpio": of_get_named_gpio returns -2,
 * both prints show, no request - exactly factory behavior on this device. */
static void bd1860_power_init(struct bd_state *s)
{
	int gpio, ret;

	pr_info("[gxy_bd1860] %s %d : start\n", __func__, 555);
	gpio = of_get_named_gpio(s->cli->dev.of_node, "bd1860,pwr-gpio", 0);
	s->pwr_gpio = gpio;
	pr_info("[gxy_bd1860] %s %d : pBd1860->pwr_gpio_number===%d\n",
		__func__, 557, gpio);
	/* factory: if ((u32)gpio < 0x200) */
	if ((u32)gpio < 0x200) {
		ret = gpio_request(gpio, "hr_pwr_pin");
		if (ret < 0)
			pr_err("[gxy_bd1860] %s %d : gpio_request fail, pwr_gpio_number = %d\n",
			       __func__, 563, gpio);
		gpiod_direction_output_raw(gpio_to_desc(gpio), 0);
	}
}

static int bd_probe(struct i2c_client *cli, const struct i2c_device_id *id)
{
	int ret;

	/* factory bd1860_i2c_probe @ 0x85edc38, prints 579..627 */
	pr_info("[gxy_bd1860] %s %d : bd1860start\n", __func__, 579);

	/* S1: factory gate (adapter must support byte data) */
	if (!i2c_check_functionality(cli->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("[gxy_bd1860] %s %d : i2c_check_functionality fail\n",
		       __func__, 582);
		return -ENODEV;
	}

	bds = devm_kzalloc(&cli->dev, sizeof(*bds), GFP_KERNEL);
	if (!bds) {
		pr_err("[gxy_bd1860] %s %d : kzalloc fail\n", __func__, 591);
		return -ENOMEM;
	}

	bds->cli = cli;
	i2c_set_clientdata(cli, bds);		/* S3: factory +0xb8 */
	mutex_init(&bds->lock);
	init_completion(&bds->comp);

	/* r44 extra (kept, proven on device): early liveness read */
	ret = i2c_smbus_read_byte_data(cli, 0x00);
	if (ret < 0) {
		pr_err("bd1860: i2c read failed at probe: %d\n", ret);
		return ret;
	}
	pr_info("bd1860: probe OK, i2c addr 0x%02x, reg00=0x%02x\n",
		cli->addr, ret);

	/* input device "bd1860" (factory bd1860_input_dev_init) */
	bds->input = devm_input_allocate_device(&cli->dev);
	if (!bds->input)
		return -ENOMEM;
	bds->input->name = "bd1860";
	for (ret = 0; ret <= 5; ret++) {
		input_set_capability(bds->input, EV_ABS, ret);
		input_set_abs_params(bds->input, ret, 0, 0xffff, 0, 0);
	}
	ret = input_register_device(bds->input);
	if (ret)
		return ret;

	/* class/bd1860/device + hrenable/hrtest (factory layout) */
	bds->cls = class_create(THIS_MODULE, BD_CLASS_NAME);
	if (IS_ERR(bds->cls)) {
		pr_err("[gxy_bd1860] %s %d : class_create fail\n",
		       __func__, 610);
		return PTR_ERR(bds->cls);
	}
	bds->clsdev = device_create(bds->cls, NULL, 0, NULL, BD_DEV_NAME);
	if (IS_ERR(bds->clsdev)) {
		pr_err("[gxy_bd1860] %s %d : device_create fail\n",
		       __func__, 616);
		ret = PTR_ERR(bds->clsdev);
		goto err_class;
	}
	ret = device_create_file(bds->clsdev, &dev_attr_hrenable);
	if (ret)
		goto err_clsdev;
	ret = device_create_file(bds->clsdev, &dev_attr_hrtest);
	if (ret)
		goto err_clsdev;

	/* debug attrs on i2c device (r42 stub legacy) */
	ret = sysfs_create_group(&cli->dev.kobj, &bd_i2c_group);
	if (ret)
		goto err_clsdev;

	bds->poll_task = kthread_run(bd_poll_task, bds, "polling-mode-task");
	if (IS_ERR(bds->poll_task)) {
		ret = PTR_ERR(bds->poll_task);
		bds->poll_task = NULL;
		goto err_sysfs;
	}

	/* factory order: thread_init -> BD_heartbp_init -> power_init
	 * (r45 P0: heartbp 15 baseline assigns, was missing) */
	BD_heartbp_init();
	bd1860_power_init(bds);

	/* S8: factory data->test_kinds = 0x32 after power_init */
	bds->mode = 0x32;
	test_kinds = 0x32;

	pr_info("[gxy_bd1860] %s %d : end\n", __func__, 627);
	return 0;

err_sysfs:
	sysfs_remove_group(&cli->dev.kobj, &bd_i2c_group);
err_clsdev:
	device_destroy(bds->cls, 0);
err_class:
	class_destroy(bds->cls);
	return ret;
}

static int bd_remove(struct i2c_client *cli)
{
	if (bds) {
		if (bds->poll_task)
			kthread_stop(bds->poll_task);
		if (bds->enabled) {
			BD_Sensor_Close();
			hardware_flag1 = 0;
		}
		sysfs_remove_group(&cli->dev.kobj, &bd_i2c_group);
		if (bds->clsdev) {
			device_remove_file(bds->clsdev, &dev_attr_hrenable);
			device_remove_file(bds->clsdev, &dev_attr_hrtest);
			device_destroy(bds->cls, 0);
		}
		if (bds->cls)
			class_destroy(bds->cls);
		bds = NULL;
	}
	return 0;
}

static const struct i2c_device_id bd_id_table[] = {
	{ BD_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bd_id_table);

static const struct of_device_id bd_of_match[] = {
	{ .compatible = "bd,bd1860" },
	{ }
};
MODULE_DEVICE_TABLE(of, bd_of_match);

static struct i2c_driver bd_driver = {
	.driver = {
		.name = BD_DRV_NAME,
		.of_match_table = bd_of_match,
	},
	.probe = bd_probe,
	.remove = bd_remove,
	.id_table = bd_id_table,
};
module_i2c_driver(bd_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BD1860 heart-rate driver, factory behavior 1:1 (W527 r43)");
