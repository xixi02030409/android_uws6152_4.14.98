// SPDX-License-Identifier: GPL-2.0
/*
 * sprd_debuglog.c — UNISOC AP sleep/wakeup audit probe (r57-rebuild)
 *
 * Semantic rewrite of factory "debug_driver" (divo-63, gcc4.9), recovered
 * from factory_kernel.bin 0x84437b4-0x84446xx via capstone + kallsyms.
 * Format strings are 1:1 with factory so audit output is comparable.
 *
 * Role: audit probe, NOT a sleep controller. Each audit group walks a DT
 * supplied register table (syscon regmap), prints "##--<sys>.<reg>: 0x%08x"
 * so we can see which register domain blocks deep sleep.
 *
 * DT binding (custom table, W527-owned):
 *   debug-log {
 *       compatible = "sprd,debuglog-sharkl5";
 *       audit-0 {
 *           sprd,audit-name = "AP DEEP SLEEP CONDITION CHECK";
 *           sprd,syscon = <&xx_syscon>;
 *           sprd,reg-table = <addr expect mask shift addr expect ...>;
 *       };
 *       audit-1 { sprd,audit-name = "AP SUBSYS STATE MONITOR"; ... };
 *   };
 *
 * Table entry layout (4 x u32): {reg_addr, expect_val, mask, shift}
 *   PASS condition: ((val >> shift) & mask) == expect_val
 *   expect_val == -1U means "log only, no pass/fail judgement".
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>

#define DRV_NAME	"sprd_debuglog"
#define KTAG		DRV_NAME

/* factory format strings (1:1, keep for output parity) */
static const char fstr_init[]	= "\x016%s: Init debug log driver\n";
static const char fstr_nodev[]	= "\x013%s: Get debug device error\n";
static const char fstr_nosip[]	= "Get wakeup sip error.\n";
static const char fstr_nomem[]	= "%s: Debug memory alloc error\n";
static const char fstr_nomd[]	= "No matched private driver data found\n";
static const char fstr_cevent[]	= "Sleep condition check event memory alloc error\n";
static const char fstr_wevent[]	= "Wakeup source get event memory alloc error\n";
static const char fstr_pevent[]	= "Wakeup source print event memory alloc error\n";
static const char fstr_sevent[]	= "Subsys monitor event memory alloc error\n";
static const char fstr_regerr[]	= "Register debug log error\n";

/* ---- wakeup_info registry (factory: wakeup_info_register/unregister) ---- */

struct wakeup_info {
	struct list_head	list;
	const char		*owner;		/* registering driver name */
	void			*data;		/* opaque payload */
};

static LIST_HEAD(wakeup_info_list);
static DEFINE_MUTEX(wakeup_info_lock);
static int wakeup_info_count;

/**
 * wakeup_info_register - register a wakeup info entry (factory EXPORT_SYMBOL)
 * @owner: caller name (kept, not copied)
 * @data:  opaque payload
 * Returns 0 on success.
 */
int wakeup_info_register(const char *owner, void *data)
{
	struct wakeup_info *w;

	w = kzalloc(sizeof(*w), GFP_KERNEL);
	if (!w) {
		pr_err(KTAG ": %s: wakeup info memory alloc error\n", owner ?: "?");
		return -ENOMEM;
	}
	w->owner = owner;
	w->data = data;
	mutex_lock(&wakeup_info_lock);
	list_add_tail(&w->list, &wakeup_info_list);
	wakeup_info_count++;
	mutex_unlock(&wakeup_info_lock);
	return 0;
}
EXPORT_SYMBOL(wakeup_info_register);

/**
 * wakeup_info_unregister - remove a wakeup info entry (factory EXPORT_SYMBOL)
 * @data: the payload registered with
 */
void wakeup_info_unregister(void *data)
{
	struct wakeup_info *w, *tmp;

	mutex_lock(&wakeup_info_lock);
	list_for_each_entry_safe(w, tmp, &wakeup_info_list, list) {
		if (w->data == data) {
			list_del(&w->list);
			wakeup_info_count--;
			kfree(w);
			break;
		}
	}
	mutex_unlock(&wakeup_info_lock);
}
EXPORT_SYMBOL(wakeup_info_unregister);

/* ---- audit table ---- */

struct audit_entry {
	const char		*name;		/* audit group name (DT) */
	struct regmap		*map;		/* syscon regmap */
	struct device_node	*np;		/* audit sub-node */
	u32			*regs;		/* reg-table array */
	int			nregs;		/* u32 count */
};

struct dbg_priv {
	struct device		*dev;
	int			naudits;
	struct audit_entry	*audits;
};

/* one reg-table entry */
struct reg_ent {
	u32 addr;
	u32 expect;
	u32 mask;
	u32 shift;
};

static void audit_one_group(struct device *dev, struct audit_entry *e)
{
	struct reg_ent *re;
	int i;

	pr_info("###--%s\n", e->name);

	if (!e->map) {
		pr_err("  ##--%s: no regmap\n", e->name);
		return;
	}
	if (e->nregs < 4) {
		pr_err("   #--Bit list is empty\n");
		return;
	}

	re = (struct reg_ent *)e->regs;
	for (i = 0; i < e->nregs / 4; i++, re++) {
		unsigned int val = 0;
		int ret = regmap_read(e->map, re->addr, &val);

		if (ret) {
			pr_err("  ##--Read %s.reg%d(0x%08x) error\n",
			       e->name, i, re->addr);
			continue;
		}
		pr_info("  ##--%s.reg%d: 0x%08x\n", e->name, i, val);
		if (re->expect != 0xffffffffU) {
			u32 got = (val >> re->shift) & re->mask;
			u32 want = (re->expect >> re->shift) & re->mask;

			if (got != want)
				pr_err("  ##--Register table is error (got 0x%x want 0x%x)\n",
				       got, want);
		}
	}
	pr_info("###--END\n");
}

/* ---- sysfs trigger ---- */

static ssize_t audit_run_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct dbg_priv *p = dev_get_drvdata(dev);
	int i;

	if (!p)
		return -ENODEV;
	for (i = 0; i < p->naudits; i++)
		audit_one_group(dev, &p->audits[i]);
	return count;
}
static DEVICE_ATTR_WO(audit_run);

/* probe: exactly mirrors factory order of checks */
static int sprd_debuglog_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child;
	struct dbg_priv *p;
	struct audit_entry *e;
	int na, i = 0;
	int ret;

	pr_info(fstr_init, DRV_NAME);

	if (!np) {
		pr_err(fstr_nodev, DRV_NAME);
		return -EINVAL;
	}

	/* factory parity: SIP handle check (ATF channel alive) */
	{
		struct regmap *sipmap = syscon_regmap_lookup_by_phandle(np,
						"sprd,sip-syscon");

		if (IS_ERR(sipmap))
			pr_warn(fstr_nosip);
		/* warn only: probe continues, SIP is optional for audit */
	}

	na = of_get_child_count(np);
	if (na <= 0) {
		pr_err(fstr_nomd);
		return -EINVAL;
	}

	p = devm_kmalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p) {
		dev_err(dev, fstr_nomem, DRV_NAME);
		return -ENOMEM;
	}
	p->dev = dev;
	p->naudits = na;
	p->audits = devm_kcalloc(dev, na, sizeof(*p->audits), GFP_KERNEL);
	if (!p->audits) {
		dev_err(dev, fstr_nomem, DRV_NAME);
		return -ENOMEM;
	}

	for_each_child_of_node(np, child) {
		const char *aname = NULL;
		struct device_node *sys_np;

		if (i >= na)
			break;
		e = &p->audits[i];

		if (of_property_read_string(child, "sprd,audit-name", &aname))
			aname = child->name;
		e->name = devm_kstrdup(dev, aname ?: "?", GFP_KERNEL);

		sys_np = of_parse_phandle(child, "sprd,syscon", 0);
		if (sys_np)
			e->map = syscon_node_to_regmap(sys_np);
		of_node_put(sys_np);
		if (IS_ERR(e->map))
			e->map = NULL;

		e->nregs = of_property_count_u32_elems(child, "sprd,reg-table");
		if (e->nregs > 0 && e->nregs % 4 == 0) {
			e->regs = devm_kmalloc(dev, e->nregs * sizeof(u32),
					       GFP_KERNEL);
			if (!e->regs) {
				dev_err(dev, fstr_cevent);
				of_node_put(child);
				return -ENOMEM;
			}
			of_property_read_u32_array(child, "sprd,reg-table",
						   e->regs, e->nregs);
		} else {
			e->nregs = 0;
		}
		i++;
	}

	platform_set_drvdata(pdev, p);

	ret = device_create_file(dev, &dev_attr_audit_run);
	if (ret) {
		dev_err(dev, fstr_regerr);
		return ret;
	}

	/* boot-time audit run once (factory prints at probe era) */
	for (i = 0; i < p->naudits; i++)
		audit_one_group(dev, &p->audits[i]);

	return 0;
}

static int sprd_debuglog_remove(struct platform_device *pdev)
{
	struct dbg_priv *p = platform_get_drvdata(pdev);

	if (p && p->dev)
		device_remove_file(p->dev, &dev_attr_audit_run);
	return 0;
}

static const struct of_device_id sprd_debuglog_of_match[] = {
	{ .compatible = "sprd,debuglog-sharkl5" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sprd_debuglog_of_match);

static struct platform_driver sprd_debuglog_driver = {
	.probe	= sprd_debuglog_probe,
	.remove	= sprd_debuglog_remove,
	.driver	= {
		.name		= DRV_NAME,
		.of_match_table	= sprd_debuglog_of_match,
	},
};
module_platform_driver(sprd_debuglog_driver);

MODULE_DESCRIPTION("UNISOC AP sleep/wakeup audit probe (r57-rebuild rewrite)");
MODULE_LICENSE("GPL v2");
