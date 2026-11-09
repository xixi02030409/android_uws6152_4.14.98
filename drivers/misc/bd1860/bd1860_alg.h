/* bd1860_alg.h - r44 decompiled health algorithm interface
 * 55 functions decompiled from factory kernel via Ghidra 12.2 headless.
 * All-integer code, factory-transaction 1:1. Do not hand-tune constants.
 */
#ifndef _BD1860_ALG_H
#define _BD1860_ALG_H

#include <linux/types.h>

/* ---- algorithm entries (signatures match decompiled definitions 1:1) ---- */
u32 BD_health_pro(u32 mode, u8 p2, u64 p3, u64 p4, u32 *result);
u8  BD_Sensor_Data_Read(int mode);
u8  BD_Sensor_Init(s32 mode, u8 *led_ref);
u8  BD_Sensor_Start(u64 mode, u32 *wear_ref);
u8  BD_Sensor_Close(void);
u8  BD_heartbp_init(void);	/* r45 P0: factory probe-tail baseline init */

/* ---- i2c primitives provided by the driver shell (bd1860.c) ----
 * factory semantics: return value = data byte / success flag,
 * first arg is the (ignored) client pointer residue */
u8 BD_I2C_Read(u64 cli, u8 reg, u8 *flag);
u8 BD_I2C_Write(u64 cli, u8 reg, u8 val);

/* ---- shared state (defined in bd1860_alg.c, driven by shell) ---- */
extern u8  test_kinds;
extern u8  sensor_refset[8];
extern u32 wear_refdata[3];
extern u8  hardware_flag1;
extern u32 bd_data_result[26];

/* debug accessors for the shell r43-raw line */
u32 bd44_dbg_raw_a(int i);
u32 bd44_dbg_dc_a(void);
u16 bd44_dbg_cur(int ch);
u32 bd44_dbg_raw_arr(int ch, int i);	/* r51: RAW_A/B/C_data full dump */
u32 bd44_dbg_buf_arr(int ch, int i);	/* r52: RAW_A/B/C_buf write-side dump */
u8  bd44_dbg_phcount(void);		/* r52: phdata_count live value */
u32 bd44_dbg_dcvolt(int which);		/* r52: DC_voltage(0) / DC_voltage1(1) */

#endif
