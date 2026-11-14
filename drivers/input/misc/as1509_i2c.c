// SPDX-License-Identifier: GPL-2.0
/*
 * as1509_i2c.c - AS1509 magnetic rotary encoder driver for W527 crown
 *
 * 来源依据：factory_kernel.elf 符号级反汇编（as1509_reverse_spec.md，2026-10-25）
 * 全部寄存器地址/流程/键值为 factory ROM 实证，非猜测。
 *
 * 硬件连接（实机 DTB 实证 2026-10-31）：
 *   - i2c2 @ 0x19, compatible "as,as1509_i2c", status okay
 *   - as,irq-gpio = bank23 port15（中断脚，唯一 GPIO，无 regulator supply 属性）
 *   - key-code-up=0x41 down=0x42 front=0x3d back=0x3e（DTB 可覆盖）
 *
 * 与 factory 驱动的差异（我们的改进，非对齐项）：
 *   1. 免 regulator/电源时序（DTB 无 supply 属性，芯片常供假设，probe 用
 *      chip id 验证兜底）
 *   2. 未校准 fallback：factory 依赖用户态 ioctl 写入 4 个校准参考点
 *      （ref 全 0 时 get_sensor_position 恒返 0，表冠只报 up 键一次）；
 *      本驱动 ref 全 0 时改用角度寄存器 0x19/0x1A 象限法直接得 pos，
 *      无需出厂校准即有四方向。真机首刷用 dmesg dump 校验角度语义。
 *
 * 数据通路（factory 实证）：
 *   IRQ(表冠转动) -> GetData(0x11 起 10 字节) -> 清中断(0x20 脉冲)
 *   -> XYZ 曼哈顿最近参考点得 pos(0-3) -> EV_KEY 上报(状态去抖)
 *   -> 以最新读数为窗口中心重设三轴阈值(±100) -> 重新使能 IRQ
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/pm_wakeup.h>
#include <linux/mutex.h>

/* ---- 寄存器地图（factory 实证，勿改） ---- */
#define AS1509_REG_CHIPID	0x00	/* 读，== 0x89 */
#define AS1509_REG_CONFIG	0x04	/* Init 写 0x08 */
#define AS1509_REG_CTRL5	0x05	/* bit7 = DRDY 使能 */
#define AS1509_REG_SOFT_RESET	0x07	/* 写 0x01（真机实证值） */
#define AS1509_REG_ODR		0x08	/* [7:4]=ODR；SetODR(0x70) -> ODR=7 */
#define AS1509_REG_STATUS	0x10	/* bit0 = DRDY */
#define AS1509_REG_X_LO		0x11	/* X lo（0x11..0x16 = X/Y/Z s16 LE） */
#define AS1509_REG_ANGLE_LO	0x19	/* 角度 u16 LE（0x19 lo, 0x1A hi） */
#define AS1509_REG_INT_CTRL	0x20	/* bit0 写1再写0 = 清中断脉冲 */
#define AS1509_REG_INT_AXIS	0x21	/* X=0x01 Y=0x02 Z=0x80；关全轴 &0x7c */
#define AS1509_REG_TH_BASE	0x28	/* X 阈值 16 位对 @0x28(lo)/0x2A(hi) */
#define AS1509_REG_CFG48	0x48	/* Init 写 0x00 */
#define AS1509_REG_AVERXY	0x59	/* [2:0] 平均采样，SetAverXY(2) */

#define AS1509_CHIP_ID		0x89
#define AS1509_GETDATA_LEN	10	/* factory GetData(buf,10) */
#define AS1509_GETDATA_REG	0x11	/* 起点：XYZ(6)+未知(2)+角度(2) */
#define AS1509_TH_WINDOW	100	/* 阈值窗口半宽（value±100） */

/* keycode 索引与 factory 参考点语义一致：0=up 1=down 2=front 3=back */
enum as1509_pos { AS_POS_UP = 0, AS_POS_DOWN, AS_POS_FRONT, AS_POS_BACK, AS_POS_N };

struct as1509_data {
	struct i2c_client	*client;
	struct input_dev	*input;
	struct wakeup_source	*ws;
	struct work_struct	irq_work;
	struct mutex		lock;		/* i2c 访问与阈值重设串行化 */
	int			irq;
	bool			irq_armed;	/* 防重入（factory data+0xac） */
	u32			keycode[AS_POS_N];
	/* 最新读数（factory data+0x30/0x32/0x34） */
	s16			x, y, z;
	/* 校准参考点（factory data+0x5a..0x70，ioctl 校准路径 v2 再开） */
	s16			ref[AS_POS_N][3];
	bool			use_angle;	/* ref 全 0 时走角度象限 fallback */
	int			last_pos;	/* 状态去抖（factory g_pos） */
	atomic_t		irq_cnt;	/* 看门狗重配计数 */
	/* ---- r55: factory sysfs 对齐（och1970 class 群，probe 反汇编实证）---- */
	bool			enabled;	/* factory data+0xe4，enable_store 写 */
	bool			poll_on;	/* factory data+0xdc，och1970_store 写 */
	struct class		*och1970_class;	/* factory __class_create("och1970") */
	struct device		*och1970_dev;	/* factory device_create(..,"device") */
	struct workqueue_struct	*wq;		/* factory "as1509-work-queue" */
	struct work_struct	poll_work;	/* factory data+0xb0 polling_work */
};

/* ---------------- 底层 i2c 原语 ---------------- */

static int as1509_rd(struct as1509_data *d, u8 reg, u8 *val)
{
	int r = i2c_smbus_read_byte_data(d->client, reg);
	if (r < 0)
		return r;
	*val = (u8)r;
	return 0;
}

static int as1509_wr(struct as1509_data *d, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(d->client, reg, val);
}

/* 16 位小端寄存器对写（阈值用，SMBus word 天然 LE） */
static int as1509_wr_word(struct as1509_data *d, u8 reg, u16 val)
{
	return i2c_smbus_write_word_data(d->client, reg, val);
}

/* 配对写 + 回读校验（factory Init 语义，最多 5 次重试） */
static int as1509_wr_verify(struct as1509_data *d, u8 reg, u8 val, const char *tag)
{
	int retry;
	u8 rb = 0;

	for (retry = 0; retry < 5; retry++) {
		if (!as1509_wr(d, reg, val) && !as1509_rd(d, reg, &rb) && rb == val) {
			dev_dbg(&d->client->dev, "%s: reg 0x%02x = 0x%02x ok\n",
				tag, reg, val);
			return 0;
		}
		usleep_range(1000, 2000);
	}
	dev_err(&d->client->dev, "%s: reg 0x%02x verify fail (read 0x%02x want 0x%02x)\n",
		tag, reg, rb, val);
	return -EIO;
}

/* ---------------- factory 寄存器操作还原 ---------------- */

/* SetODR(0x70)：reg8 = (val & 0xf0) | (old & 0x0f) —— spec 逐字还原 */
static int as1509_set_odr(struct as1509_data *d, u8 val)
{
	u8 old = 0;
	int err = as1509_rd(d, AS1509_REG_ODR, &old);

	if (err)
		return err;
	return as1509_wr(d, AS1509_REG_ODR, (val & 0xf0) | (old & 0x0f));
}

/* SetAverXY(2)：reg 0x59 [2:0] = val（spec：范围 0-6） */
static int as1509_set_aver_xy(struct as1509_data *d, u8 val)
{
	u8 old = 0;
	int err = as1509_rd(d, AS1509_REG_AVERXY, &old);

	if (err)
		return err;
	return as1509_wr(d, AS1509_REG_AVERXY, (old & ~0x07) | (val & 0x07));
}

/* reg5 bit7 置 1（factory Init 第 7 步 + work 看门狗重配复用） */
static int as1509_en_drdy(struct as1509_data *d)
{
	u8 v = 0;
	int err = as1509_rd(d, AS1509_REG_CTRL5, &v);

	if (err)
		return err;
	if (v & 0x80)
		return 0;
	return as1509_wr(d, AS1509_REG_CTRL5, v | 0x80);
}

/* SetInterruptClear：reg 0x20 bit0 写 1 再写 0（脉冲） */
static int as1509_int_clear(struct as1509_data *d)
{
	int err = as1509_wr(d, AS1509_REG_INT_CTRL, 0x01);

	if (err)
		return err;
	return as1509_wr(d, AS1509_REG_INT_CTRL, 0x00);
}

/* SetInterruptAxis：使能单轴中断；en=0 为 factory 特例全关（&0x7c） */
static int as1509_int_axis(struct as1509_data *d, int axis, int en)
{
	u8 v = 0;
	u8 bit;
	int err;

	switch (axis) {
	case 0: bit = 0x01; break;	/* X */
	case 1: bit = 0x02; break;	/* Y */
	case 2: bit = 0x80; break;	/* Z */
	default: return -EINVAL;
	}

	err = as1509_rd(d, AS1509_REG_INT_AXIS, &v);
	if (err)
		return err;
	if (en)
		v |= bit;
	else
		v &= 0x7c;		/* factory disable-all 语义 */
	return as1509_wr(d, AS1509_REG_INT_AXIS, v);
}

/* GetData：从 0x11 连续读 10 字节（buf[0..5]=XYZ s16 LE，buf[8..9]=角度 u16 LE） */
static int as1509_get_data(struct as1509_data *d, u8 *buf)
{
	return i2c_smbus_read_i2c_block_data(d->client, AS1509_GETDATA_REG,
					     AS1509_GETDATA_LEN, buf);
}

/* Set_change_Threshold：写 (v-100)@R、(v+100)@R+2，回读校验，再使能该轴中断
 * 阈值基址表（factory rodata 实证）：axis0->0x28, 1->0x2E, 2->0x34 */
static int as1509_set_threshold(struct as1509_data *d, int axis, s16 value)
{
	static const u8 base[3] = { 0x28, 0x2E, 0x34 };
	u8 reg = base[axis];
	s16 lo = (s16)(value - AS1509_TH_WINDOW);
	s16 hi = (s16)(value + AS1509_TH_WINDOW);
	int err;

	err = as1509_wr_word(d, reg, (u16)lo);
	if (err)
		return err;
	err = as1509_wr_word(d, reg + 2, (u16)hi);
	if (err)
		return err;
	return as1509_int_axis(d, axis, 1);
}

/* ---------------- 位置判定 ---------------- */

/*
 * ref 全 0（未校准）时走角度象限：angle/16384 -> pos 0-3
 * （角度寄存器实证存在；象限与 up/down/front/back 的对应关系
 *   待真机 getevent 校正，v2 可改）
 */
static int as1509_pos_by_angle(struct as1509_data *d, u16 angle)
{
	return (angle / 16384) & 0x03;
}

/* factory get_sensor_position 还原：曼哈顿 3D 最近参考点，同分偏向低索引 */
static int as1509_pos_by_ref(struct as1509_data *d, s16 x, s16 y, s16 z)
{
	int i, best = 0;
	int best_d = INT_MAX;

	for (i = 0; i < AS_POS_N; i++) {
		int dist = abs(d->ref[i][0] - x) +
			   abs(d->ref[i][1] - y) +
			   abs(d->ref[i][2] - z);

		if (dist < best_d) {	/* 严格小于 => 同分偏向低索引 */
			best_d = dist;
			best = i;
		}
	}
	return best;
}

/* report_key：状态变化去抖（factory report_key 语义） */
static void as1509_report(struct as1509_data *d, int pos)
{
	if (pos < 0 || pos >= AS_POS_N)
		return;
	if (d->last_pos == pos)
		return;

	input_report_key(d->input, d->keycode[pos], 1);
	input_report_key(d->input, d->keycode[pos], 0);
	input_sync(d->input);
	d->last_pos = pos;
	dev_dbg(&d->client->dev, "crown pos=%d key=0x%02x\n", pos, d->keycode[pos]);
}

/* ---------------- IRQ 工作体（factory as1509_irq_work 逐条还原） */

static void as1509_irq_work_fn(struct work_struct *work)
{
	struct as1509_data *d = container_of(work, struct as1509_data, irq_work);
	u8 buf[AS1509_GETDATA_LEN];
	int try, err, cnt, pos;
	s16 x, y, z;
	u16 angle;

	__pm_stay_awake(d->ws);

	/* GetData 最多 3 次重试（factory 语义，失败间隔 1-2ms） */
	for (try = 0; try < 3; try++) {
		err = as1509_get_data(d, buf);
		if (err == AS1509_GETDATA_LEN)
			break;
		usleep_range(1000, 2000);
	}
	if (err != AS1509_GETDATA_LEN) {
		dev_err(&d->client->dev, "get_data fail %d after 3 tries\n", err);
		goto out_rearm;
	}

	/* 首帧限频 dump：真机核对 XYZ/角度字节语义 */
	cnt = atomic_read(&d->irq_cnt);
	if (cnt < 5)
		dev_info(&d->client->dev,
			 "raw: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			 buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
			 buf[6], buf[7], buf[8], buf[9]);

	as1509_int_clear(d);

	x = (s16)(buf[0] | (buf[1] << 8));
	y = (s16)(buf[2] | (buf[3] << 8));
	z = (s16)(buf[4] | (buf[5] << 8));
	angle = (u16)(buf[8] | (buf[9] << 8));
	d->x = x; d->y = y; d->z = z;

	if (d->use_angle)
		pos = as1509_pos_by_angle(d, angle);
	else
		pos = as1509_pos_by_ref(d, x, y, z);
	as1509_report(d, pos);

	/* 以最新读数为窗口中心重设三轴阈值（factory 顺序） */
	mutex_lock(&d->lock);
	as1509_set_threshold(d, 0, x);
	as1509_set_threshold(d, 1, y);
	as1509_set_threshold(d, 2, z);
	mutex_unlock(&d->lock);

	/* 看门狗：>10 次重配 DRDY/AverXY/ODR（factory 语义） */
	if (cnt > 10) {
		as1509_en_drdy(d);
		as1509_set_aver_xy(d, 2);
		as1509_set_odr(d, 0x70);
		atomic_set(&d->irq_cnt, 0);
	} else {
		atomic_inc(&d->irq_cnt);
	}

out_rearm:
	enable_irq(d->irq);
	d->irq_armed = true;
	__pm_relax(d->ws);
}

static irqreturn_t as1509_irq_handler(int irq, void *dev_id)
{
	struct as1509_data *d = dev_id;

	if (!d->irq_armed)
		return IRQ_HANDLED;	/* 未重新武装，忽略（factory 铁律） */
	d->irq_armed = false;
	disable_irq_nosync(irq);
	schedule_work(&d->irq_work);
	return IRQ_HANDLED;
}

/* ---------------- r55: factory och1970 sysfs 对齐（probe/attr 反汇编逐条还原） ----------------
 * factory 在同一 probe 里建 och1970 class + device("device")，挂两个 attr：
 *   och1970(show=as1509_show: "%d %d %d\n" XYZ；store=as1509_store: '1' 开轮询)
 *   enable (show="%d"；store=kstrtobool，置 1 时重跑 AS1509_Init)
 * 数据源 = data+0x30/0x32/0x34 三个 s16 = 我们的 d->x/y/z（irq_work 已保存）
 */

static int as1509_init(struct as1509_data *d);	/* enable_store 重初始化用（定义在后） */

static ssize_t as1509_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct as1509_data *d = dev_get_drvdata(dev);

	dev_info(&d->client->dev, "as1509 dataxyz=%d,%d,%d\n",
		 d->x, d->y, d->z);
	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n", d->x, d->y, d->z);
}

/* factory store：buf[0]=='1'(或 1) -> data+0xdc=1 + queue_work_on(4,wq,poll_work)
 * 否则 data+0xdc=0。polling_work 在 factory 是轮询备选路径，本驱动以 IRQ 为主，
 * 使能时触发一次 irq_work 读取一帧 XYZ（功能等价的最小实现） */
static ssize_t as1509_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct as1509_data *d = dev_get_drvdata(dev);

	dev_info(&d->client->dev, "as1509 as1509_store: start, buf[0] = 0x%x\n",
		 buf[0]);
	if (buf[0] == '1' || buf[0] == 1) {
		d->poll_on = true;
		d->enabled = true;
		if (d->wq)
			queue_work(d->wq, &d->poll_work);
		else
			schedule_work(&d->irq_work);
	} else {
		d->poll_on = false;
	}
	return count;
}

static ssize_t as1509_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct as1509_data *d = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", d->enabled ? 1 : 0);
}

/* factory enable_store：kstrtobool -> data+0xe4；置 1 时重跑 AS1509_Init，
 * 置 0 时清 polling 定时字段。本驱动对齐：1=重 init+开轮询标志，0=关 */
static ssize_t as1509_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct as1509_data *d = dev_get_drvdata(dev);
	bool v;
	int err;

	err = kstrtobool(buf, &v);
	if (err) {
		dev_err(&d->client->dev, "enable_store kstrtobool fail %d\n",
			err);
		return err;
	}
	dev_info(&d->client->dev, "as1509 enable_store: %d\n", v);
	d->poll_on = false;
	if (v) {
		d->enabled = true;
		err = as1509_init(d);
		if (err)
			dev_err(&d->client->dev,
				"enable_store re-init fail %d\n", err);
	} else {
		d->enabled = false;
	}
	return count;
}

static DEVICE_ATTR(och1970, 0664, as1509_show, as1509_store);
static DEVICE_ATTR(enable, 0664, as1509_enable_show, as1509_enable_store);

/* factory as1509_polling_work 的等价触发点：读一帧 + 位置判定（复用 irq_work） */
static void as1509_poll_work_fn(struct work_struct *work)
{
	struct as1509_data *d = container_of(work, struct as1509_data,
					     poll_work);

	schedule_work(&d->irq_work);
}

/* ---------------- Init（factory AS1509_Init 适配版） ---------------- */

static int as1509_init(struct as1509_data *d)
{
	struct device *dev = &d->client->dev;
	u8 id = 0;
	int err;

	/* factory 上电时序依赖 regulator+第二 GPIO，本机 DTB 均无 -> 跳过，
	 * 直接软复位后验 chip id（0x89 实证），读不到即退出 */
	err = as1509_wr(d, AS1509_REG_SOFT_RESET, 0x01);
	if (err) {
		dev_err(dev, "soft reset write fail: %d\n", err);
		return err;
	}
	msleep(50);

	err = as1509_rd(d, AS1509_REG_CHIPID, &id);
	if (err) {
		dev_err(dev, "chip id read fail: %d\n", err);
		return err;
	}
	if (id != AS1509_CHIP_ID) {
		dev_err(dev, "chip id mismatch: 0x%02x != 0x%02x\n",
			id, AS1509_CHIP_ID);
		return -ENODEV;
	}
	dev_info(dev, "AS1509 detected (id 0x%02x)\n", id);

	/* 配对写 + 回读校验（factory 顺序：(0x48,0)(0x04,0x08)(0x21,0)） */
	err = as1509_wr_verify(d, AS1509_REG_CFG48, 0x00, "cfg48");
	if (err)
		return err;
	err = as1509_wr_verify(d, AS1509_REG_CONFIG, 0x08, "config");
	if (err)
		return err;
	err = as1509_wr_verify(d, AS1509_REG_INT_AXIS, 0x00, "int_axis_off");
	if (err)
		return err;

	/* reg5|=0x80 -> AverXY(2) -> ODR(0x70)（factory Init 尾部顺序） */
	err = as1509_en_drdy(d);
	if (err)
		return err;
	err = as1509_set_aver_xy(d, 2);
	if (err)
		return err;
	return as1509_set_odr(d, 0x70);
}

/* ---------------- probe / remove ---------------- */

static int as1509_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct as1509_data *d;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	int irq_gpio, err;
	u8 buf[AS1509_GETDATA_LEN];

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA |
				     I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(dev, "adapter smbus capability insufficient\n");
		return -EOPNOTSUPP;
	}

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->client = client;
	mutex_init(&d->lock);
	atomic_set(&d->irq_cnt, 0);
	d->last_pos = -1;
	d->irq_armed = true;

	/* keycode：DTB 优先，缺省 = factory DTB 实证值 */
	d->keycode[AS_POS_UP]    = 0x41;
	d->keycode[AS_POS_DOWN]  = 0x42;
	d->keycode[AS_POS_FRONT] = 0x3d;
	d->keycode[AS_POS_BACK]  = 0x3e;
	if (np) {
		of_property_read_u32(np, "key-code-up",    &d->keycode[AS_POS_UP]);
		of_property_read_u32(np, "key-code-down",  &d->keycode[AS_POS_DOWN]);
		of_property_read_u32(np, "key-code-front", &d->keycode[AS_POS_FRONT]);
		of_property_read_u32(np, "key-code-back",  &d->keycode[AS_POS_BACK]);
	}

	/* IRQ GPIO（DTB as,irq-gpio，唯一 GPIO） */
	irq_gpio = of_get_named_gpio(np, "as,irq-gpio", 0);
	if (irq_gpio < 0) {
		dev_err(dev, "as,irq-gpio missing: %d\n", irq_gpio);
		return irq_gpio;
	}
	err = devm_gpio_request_one(dev, irq_gpio, GPIOF_IN, "as1509_irq");
	if (err) {
		dev_err(dev, "gpio %d request fail: %d\n", irq_gpio, err);
		return err;
	}
	d->irq = gpio_to_irq(irq_gpio);

	err = as1509_init(d);
	if (err)
		return err;

	/* 校准参考点：v1 无用户态校准通道 -> ref 全 0 + 角度象限 fallback。
	 * 先读一帧确认数据流可用（失败只警告不退出，IRQ 路径会重试） */
	if (!as1509_get_data(d, buf))
		dev_info(dev, "probe read ok: x=%d y=%d z=%d angle=%u\n",
			 (s16)(buf[0] | buf[1] << 8), (s16)(buf[2] | buf[3] << 8),
			 (s16)(buf[4] | buf[5] << 8),
			 (u16)(buf[8] | buf[9] << 8));
	memset(d->ref, 0, sizeof(d->ref));
	d->use_angle = true;

	/* wakeup source（factory 名 as1509_wakelock）；4.14 无 dev 参数，4.15 起才加 */
	d->ws = wakeup_source_register("as1509_wakelock");
	if (!d->ws)
		return -ENOMEM;

	/* input 设备：4 方向键 */
	d->input = devm_input_allocate_device(dev);
	if (!d->input) {
		wakeup_source_unregister(d->ws);
		return -ENOMEM;
	}
	d->input->name = "as1509_crown";
	d->input->id.bustype = BUS_I2C;
	input_set_capability(d->input, EV_KEY, d->keycode[AS_POS_UP]);
	input_set_capability(d->input, EV_KEY, d->keycode[AS_POS_DOWN]);
	input_set_capability(d->input, EV_KEY, d->keycode[AS_POS_FRONT]);
	input_set_capability(d->input, EV_KEY, d->keycode[AS_POS_BACK]);
	err = input_register_device(d->input);
	if (err) {
		wakeup_source_unregister(d->ws);
		return err;
	}

	/* ---- r55: factory och1970 class 群（probe 反汇编顺序还原）----
	 * factory: misc_register -> __class_create("och1970") ->
	 * device_create(.., "device") -> alloc_workqueue("as1509-work-queue")
	 * -> ... -> device_create_file(och1970) + device_create_file(enable)
	 * -> wakeup_source_register("as1509_wakelock") */
	d->wq = alloc_workqueue("as1509-work-queue", WQ_UNBOUND, 1);
	INIT_WORK(&d->poll_work, as1509_poll_work_fn);

	d->och1970_class = class_create(THIS_MODULE, "och1970");
	if (IS_ERR(d->och1970_class)) {
		dev_err(dev, "Failed to create class(och1970)\n");
		d->och1970_class = NULL;
	} else {
		d->och1970_dev = device_create(d->och1970_class, NULL, 0,
					       NULL, "device");
		if (IS_ERR(d->och1970_dev)) {
			dev_err(dev, "Failed to create device(och1970)\n");
			d->och1970_dev = NULL;
		} else {
			dev_set_drvdata(d->och1970_dev, d);
			device_create_file(d->och1970_dev, &dev_attr_och1970);
			device_create_file(d->och1970_dev, &dev_attr_enable);
			dev_info(dev,
				 "as1509 och1970 sysfs ready (och1970/enable)\n");
		}
	}

	INIT_WORK(&d->irq_work, as1509_irq_work_fn);
	/* 触发沿 DTB 未定义，v1 用上升沿 + 真机实测校正 */
	err = request_irq(d->irq, as1509_irq_handler,
			  IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			  "as1509_irq", d);
	if (err) {
		dev_err(dev, "irq %d request fail: %d\n", d->irq, err);
		wakeup_source_unregister(d->ws);
		return err;
	}
	enable_irq_wake(d->irq);
	device_init_wakeup(dev, true);

	i2c_set_clientdata(client, d);
	dev_info(dev, "as1509 probe ok: irq=%d keys up/down/front/back=0x%02x/0x%02x/0x%02x/0x%02x\n",
		 d->irq, d->keycode[0], d->keycode[1],
		 d->keycode[2], d->keycode[3]);
	return 0;
}

static int as1509_remove(struct i2c_client *client)
{
	struct as1509_data *d = i2c_get_clientdata(client);

	disable_irq_wake(d->irq);
	free_irq(d->irq, d);
	cancel_work_sync(&d->irq_work);
	cancel_work_sync(&d->poll_work);
	if (d->wq)
		destroy_workqueue(d->wq);
	/* r55: factory och1970 sysfs 对应销毁（factory as1509_i2c_remove 语义） */
	if (d->och1970_dev) {
		device_remove_file(d->och1970_dev, &dev_attr_och1970);
		device_remove_file(d->och1970_dev, &dev_attr_enable);
		device_destroy(d->och1970_class, 0);
	}
	if (d->och1970_class)
		class_destroy(d->och1970_class);
	wakeup_source_unregister(d->ws);
	/* 关全轴中断（factory disable-all 语义） */
	as1509_int_axis(d, 0, 0);
	dev_info(&client->dev, "as1509 removed\n");
	return 0;
}

static const struct of_device_id as1509_of_match[] = {
	{ .compatible = "as,as1509_i2c" },
	{ }
};
MODULE_DEVICE_TABLE(of, as1509_of_match);

static const struct i2c_device_id as1509_i2c_id[] = {
	{ "as1509_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, as1509_i2c_id);

static struct i2c_driver as1509_i2c_driver = {
	.driver = {
		.name		= "as1509_i2c",
		.of_match_table	= of_match_ptr(as1509_of_match),
	},
	.probe		= as1509_probe,
	.remove		= as1509_remove,
	.id_table	= as1509_i2c_id,
};
module_i2c_driver(as1509_i2c_driver);

MODULE_AUTHOR("W527 project");
MODULE_DESCRIPTION("AS1509 magnetic rotary encoder (W527 crown) driver, factory-spec rewrite");
MODULE_LICENSE("GPL v2");
