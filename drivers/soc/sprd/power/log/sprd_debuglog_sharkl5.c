/*
 * SharkL5 (T310 / UWS6152) platform data for the sprd debug power log
 * framework.
 *
 * The per-platform register-check tables and INTC bit-name maps of the
 * upstream variants (sprd_debuglog_sharkl5pro.c etc.) are not available
 * for SharkL5 in any public source tree.  To stay honest, this variant
 * registers only the generic wakeup-source reporting from
 * sprd_debuglog_drv.c and prints the RAW intc major/second/third values
 * instead of fabricated IRQ names.  The sleep-condition and power-domain
 * register tables stay empty until the factory register maps are
 * reverse-engineered.
 *
 * The DTB of this device carries the "sprd,debuglog-sharkl5" node, so the
 * framework binds here and the wakeup-source readout works through the
 * SIP firmware handle (CONFIG_SPRD_SIP_FW).
 *
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 * W527 minimal variant, 2026.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 */
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "sprd_debuglog_drv.h"

/* AP INTC irq N maps onto GIC irq N+32 (family-standard offset). */
#define INTC_TO_GIC(i)		((i) + 32)

/* Scratch slots consumed by ap_wakeup_source_print (see drv.c). */
static char *wakeup_source[2];
static char wakeup_name[96];

static int sharkl5_wakeup_source_match(u32 m, u32 s, u32 t, void *data, int num)
{
	char **str_list = (char **)data;
	int inum, ibit, gnum;

	if (!str_list || num != 2) {
		pr_err("%s: Data is error\n", __func__);
		return -EINVAL;
	}

	inum = (m >> 16) & 0xFFFF;
	ibit = m & 0xFFFF;
	gnum = INTC_TO_GIC((inum << 5) + ibit);

	snprintf(wakeup_name, sizeof(wakeup_name),
		 "RAW AP_INTC%u bit%u (gic irq~%d) m=0x%08x s=0x%08x t=0x%08x",
		 inum, ibit, gnum, m, s, t);

	str_list[0] = wakeup_name;
	str_list[1] = NULL;

	return 0;
}

/*
 * Sleep-condition and power-domain tables intentionally empty: the
 * framework skips them (see debug_driver_probe) and only the wakeup
 * source path is registered.  Fill them in once the SharkL5 factory
 * register maps are available.
 */
struct plat_data sprd_sharkl5_debug_data = {
	.sleep_condition_table = NULL,
	.sleep_condition_table_num = 0,
	.subsys_state_table = NULL,
	.subsys_state_table_num = 0,
	.wakeup_source_info = wakeup_source,
	.wakeup_source_info_num = 2,
	.wakeup_source_match = sharkl5_wakeup_source_match,
};
