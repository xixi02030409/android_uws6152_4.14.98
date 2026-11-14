/* bd1860_alg.c - W527 r44 decompiled health algorithm cluster
 * Source: factory kernel 4.9.x BD1860 driver (binary, no source), decompiled
 * via Ghidra 12.2 headless + kallsyms symbol mapping. All-integer code.
 * DO NOT hand-tune constants: these implement the factory algorithm 1:1.
 * Coverage: 46 functions (collect/AGC/preproc/FFT/IIR/HR/HRV/SPO2/BP/wear)
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/delay.h>
#include "bd1860_alg.h"

static u32 bd44_guard; /* target for __key.* static-init guard writes */

/* ---------- initialized data (from factory .data) ---------- */
static __maybe_unused u8 gensor_state = 2;
static __maybe_unused u8 state_judge_count = 10;
static __maybe_unused u8 no_touch_count[6] = {6,0,0,0,0,0};
static __maybe_unused u8 to[512] = {7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,35,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,44,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,82,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* ---------- rodata tables (from factory .rodata) ---------- */
static const __maybe_unused u16 a_hrvlpfref[8] = {192,64456,2370,63110,1024,0,0,0};
static const __maybe_unused u16 b_hrvlpfref[8] = {5,20,30,20,5,0,0,0};
static const __maybe_unused u16 a_bpmlpfref[8] = {31,65349,696,64735,1024,0,0,0};
static const __maybe_unused u16 b_bpmlpfref[8] = {48,191,286,191,48,0,0,0};
static const __maybe_unused u16 a_lpfref[8] = {31,65349,696,64735,1024,0,0,0};
static const __maybe_unused u16 b_lpfref[8] = {48,191,286,191,48,0,0,0};
static const __maybe_unused u16 a_hpfref[8] = {196,64986,1220,63743,1024,0,0,0};
static const __maybe_unused u16 b_hpfref[8] = {220,0,65096,0,220,0,0,0};
static const __maybe_unused u16 sin_tab[256] = {0,1,3,4,6,7,9,10,12,14,15,17,18,20,21,23,24,25,27,28,30,31,32,34,35,36,38,39,40,41,42,44,45,46,47,48,49,50,51,52,53,54,54,55,56,57,57,58,59,59,60,60,61,61,62,62,62,63,63,63,63,63,63,63,64,63,63,63,63,63,63,63,62,62,62,61,61,60,60,59,59,58,57,57,56,55,54,54,53,52,51,50,49,48,47,46,45,44,42,41,40,39,38,36,35,34,32,31,30,28,27,25,24,23,21,20,18,17,15,14,12,10,9,7,6,4,3,1,0,65535,65533,65532,65530,65529,65527,65526,65524,65522,65521,65519,65518,65516,65515,65513,65512,65511,65509,65508,65506,65505,65504,65502,65501,65500,65498,65497,65496,65495,65494,65492,65491,65490,65489,65488,65487,65486,65485,65484,65483,65482,65482,65481,65480,65479,65479,65478,65477,65477,65476,65476,65475,65475,65474,65474,65474,65473,65473,65473,65473,65473,65473,65473,65472,65473,65473,65473,65473,65473,65473,65473,65474,65474,65474,65475,65475,65476,65476,65477,65477,65478,65479,65479,65480,65481,65482,65482,65483,65484,65485,65486,65487,65488,65489,65490,65491,65492,65494,65495,65496,65497,65498,65500,65501,65502,65504,65505,65506,65508,65509,65511,65512,65513,65515,65516,65518,65519,65521,65522,65524,65526,65527,65529,65530,65532,65533,65535};
static const __maybe_unused u16 gen_sin_tab[132] = {0,3,6,9,12,15,18,21,24,27,30,32,35,38,40,42,45,47,49,51,53,54,56,57,59,60,61,62,62,63,63,63,64,63,63,63,62,62,61,60,59,57,56,54,53,51,49,47,45,42,40,38,35,32,30,27,24,21,18,15,12,9,6,3,0,65533,65530,65527,65524,65521,65518,65515,65512,65509,65506,65504,65501,65498,65496,65494,65491,65489,65487,65485,65483,65482,65480,65479,65477,65476,65475,65474,65474,65473,65473,65473,65472,65473,65473,65473,65474,65474,65475,65476,65477,65479,65480,65482,65483,65485,65487,65489,65491,65494,65496,65498,65501,65504,65506,65509,65512,65515,65518,65521,65524,65527,65530,65533,7936,21055,0,0};

/* ---------- bss state ---------- */
static __maybe_unused u32 DC_refless_data1;
static __maybe_unused u32 DC_refless_data2;
static __maybe_unused u32 power_kal;
static __maybe_unused u16 user_count;
static __maybe_unused u16 debug_count;
static __maybe_unused u16 spo2_power_count;
static __maybe_unused u8 gensor_data_count;
static __maybe_unused u8 gensor_new_datalength;
static __maybe_unused u8 power_unormal_count;
static __maybe_unused u8 green_dc_value;
static __maybe_unused u8 red_dc_value;
static __maybe_unused u8 ir_dc_value;
static __maybe_unused u8 red_setflag;
static __maybe_unused u8 ir_setflag;
u8 hardware_flag1;
static __maybe_unused u8 lcd_flag[6];
static __maybe_unused s32 iir_data[10];
static __maybe_unused s32 iir_data1[10];
static __maybe_unused s32 iir_data2[10];
static __maybe_unused s32 iir_gen_data[10];
static __maybe_unused u8 spo2_data[20];
static __maybe_unused u32 red_ir_ac_old;
static __maybe_unused u32 red_ir_ac_new[2];
static __maybe_unused u64 spo2_fifo_result[1];
static __maybe_unused u64 spo2_test_buf[1];
static __maybe_unused u8 wave_data2[800];
static __maybe_unused u8 gesnor_fifo_databuf[128];
static __maybe_unused u8 bd_gensor_buf[256];
static __maybe_unused s32 cmplex_out[512];
static __maybe_unused s32 cmplex_gensor_out[256];
static __maybe_unused s32 green_iir_out[256];
static __maybe_unused s32 red_iir_out[256];
static __maybe_unused s32 ir_iir_out[256];
static __maybe_unused u16 lcd_data;
static __maybe_unused u16 blood_lcd_data;
static __maybe_unused u16 blood_data;
static __maybe_unused u16 Sta_data;
static __maybe_unused u16 Run_data;
static __maybe_unused u8 time_count;
static __maybe_unused u8 new_data_flag;
static __maybe_unused u8 sta_data_flag;
static __maybe_unused u8 sta_data_count;
static __maybe_unused u32 Sta_real_value;
static __maybe_unused u64 sta_data_buf[1];
static __maybe_unused u64 state_judge_power[1];
static __maybe_unused u8 math_work_count0;
static __maybe_unused u8 math_work_count1;
static __maybe_unused u8 math_work_count2;
static __maybe_unused u8 run_to_sta_count;
static __maybe_unused u32 heart_sbp_ave;
static __maybe_unused u32 sta_temp_data_buf;
static __maybe_unused u16 lcd_temp_data;
static __maybe_unused u8 sport_to_sta_flag;
static __maybe_unused u8 no_touch_flag;
static __maybe_unused u8 bpm_high_flag;
static __maybe_unused u8 bpm_sum_flag;
static __maybe_unused u32 gensor_freq_buf[16];
static __maybe_unused u32 gensor_power_buf[2];
static __maybe_unused u16 speed_state_buf[8];
static __maybe_unused u32 gensor_power_new[2];
static __maybe_unused u32 bp_data_buf[2];
static __maybe_unused u32 bp_gensor_data[2];
static __maybe_unused u32 bp_gensor_power;
static __maybe_unused u8 gensor_stateflag;
static __maybe_unused u8 speed_state_count;
static __maybe_unused u8 weak_sport_flag;
static __maybe_unused u8 sport_stable_flag;
static __maybe_unused u16 run_static_data;
static __maybe_unused u16 bp_varrange[3];
static __maybe_unused u32 gensor_freq_data[4];
static __maybe_unused u16 heart_olddata_fifo[3];
static __maybe_unused u8 heart_stable_count;
static __maybe_unused u8 state_stable_count;
static __maybe_unused u8 weak_sport_count;
static __maybe_unused u8 bpm_unormal_count;
static __maybe_unused u16 gensor_freq_ave;
static __maybe_unused u16 gensor_power_freq_ave;
static __maybe_unused u8 test_pro_state;
static __maybe_unused u8 test_pro_flag;
static __maybe_unused u8 test_pro_count;
static __maybe_unused u8 test_pro_count1;
static __maybe_unused u8 bp_gensor_flag;
static __maybe_unused u8 speed_state_flag;
static __maybe_unused u8 gensor_state_count;
static __maybe_unused u8 test_pro_state1;
static __maybe_unused u16 run_init_data;
static __maybe_unused u16 run_init_data1;
static __maybe_unused u16 bp_lim_data[3];
static __maybe_unused u16 run_data_buf[2];	/* r49: factory = u16 x2 (strh/ldrh only, symbol 8B) */
static __maybe_unused u32 speed_data_buf;
static __maybe_unused u16 bpm_start_data;
static __maybe_unused u16 bpm_now_data0;
static __maybe_unused u16 bpm_now_data1;
static __maybe_unused u16 bpm_stable_data;
static __maybe_unused u8 bp_gensor_oldflag;
static __maybe_unused u8 bpm_run_count;
static __maybe_unused u8 bpm_static_count;
static __maybe_unused u8 walk_state_count;
static __maybe_unused u16 all_code_count;
static __maybe_unused u16 all_code_count1;
static __maybe_unused s32 green_flt_buf[26];
static __maybe_unused s32 red_flt_buf[26];
static __maybe_unused s32 ir_flt_buf[26];
static __maybe_unused s32 green_flt2_buf[26];
static __maybe_unused s32 red_flt2_buf[26];
static __maybe_unused s32 ir_flt2_buf[26];
static __maybe_unused s32 green_flt_input[6];
static __maybe_unused s32 green_flt_output[6];
static __maybe_unused s32 red_flt_input[6];
static __maybe_unused s32 red_flt_output[6];
static __maybe_unused s32 ir_flt_input[6];
static __maybe_unused s32 ir_flt_output[6];
static __maybe_unused s32 green_hflt_input[6];
static __maybe_unused s32 green_hflt_output[6];
static __maybe_unused s32 red_hflt_input[6];
static __maybe_unused s32 red_hflt_output[6];
static __maybe_unused s32 ir_hflt_input[6];
static __maybe_unused s32 ir_hflt_output[5];
static __maybe_unused u32 fifo_iir_data;
static __maybe_unused u32 jump_delta;
static __maybe_unused u32 fifo_iir_data1;
static __maybe_unused u32 jump_delta1;
static __maybe_unused u32 fifo_iir_data2;
static __maybe_unused u32 jump_delta2;
static __maybe_unused u32 ch00_middata;
static __maybe_unused u32 ch01_middata;
static __maybe_unused u32 ch02_middata;
static __maybe_unused u8 power_unormal_flag;
static __maybe_unused u8 non_zero_count;
static __maybe_unused u8 unormal_flag;
static __maybe_unused u8 unormal_count;
static __maybe_unused u8 unormal_count1;
static __maybe_unused u8 status_flag;
static __maybe_unused u16 bpm_wavedata;
static __maybe_unused u16 bpm_disdata;
static __maybe_unused u16 lcd_disdata;
static __maybe_unused u16 hrv_avedata_ms;
static __maybe_unused u16 hrv_data_ms;
static __maybe_unused u16 hrv_data1_ms;
static __maybe_unused u16 hrv_rmssd_data;
static __maybe_unused u16 hrv_press_data;
static __maybe_unused u32 hrv_disp_buf[2];
static __maybe_unused u16 bpm_wavedatabuf[8];
static __maybe_unused u16 max_ms;
static __maybe_unused u16 min_ms;
static __maybe_unused u16 max_heart_ms;
static __maybe_unused u16 min_heart_ms;
static __maybe_unused u32 first_data;
static __maybe_unused u32 second_data;
static __maybe_unused u32 now_data;
static __maybe_unused u32 malloc_flag;
static __maybe_unused s32 hrv_data_buf[25];
static __maybe_unused u8 phdata_count;
static __maybe_unused u8 wear_count;
static __maybe_unused u16 ch_A_buf[28];
static __maybe_unused u16 ch_B_buf[28];
static __maybe_unused u16 ch_C_buf[25];
static __maybe_unused u8 ch_A_setflag;
static __maybe_unused u8 ch_B_setflag;
static __maybe_unused u8 ch_C_setflag;
static __maybe_unused u8 cha_offflag;
static __maybe_unused u8 chb_offflag;
static __maybe_unused u8 chc_offflag;
static __maybe_unused u32 RAW_A_buf[26];
static __maybe_unused u32 RAW_B_buf[26];
static __maybe_unused u32 RAW_C_buf[26];
static __maybe_unused u32 RAW_A_data[26];
static __maybe_unused u32 RAW_B_data[26];
static __maybe_unused u32 RAW_C_data[25];
static __maybe_unused u32 green_dc_power;
static __maybe_unused u32 red_dc_power;
static __maybe_unused u32 ir_dc_power;
static __maybe_unused u32 green_old_data;
static __maybe_unused u32 red_old_data;
static __maybe_unused u32 ir_old_data;
static __maybe_unused u32 green_ac_power;
static __maybe_unused u32 red_ac_power;
static __maybe_unused u32 ir_ac_power;
static __maybe_unused u32 green_result[10];
static __maybe_unused u32 red_result[10];
static __maybe_unused u32 ir_result[10];
static __maybe_unused u32 in_testbuf2[26];
static __maybe_unused u32 in_testbuf1[26];
static __maybe_unused u8 spo2_data_buf[16];
static __maybe_unused u32 DC_voltage1;
static __maybe_unused u16 sta_basic_value2;
static __maybe_unused u16 ch_B_expot;
static __maybe_unused u16 sta_basic_value;
static __maybe_unused u32 freq_powerave;
static __maybe_unused u32 wear_ref2;
static __maybe_unused u16 run_basic_value;
static __maybe_unused u32 ir_data_ave;
static __maybe_unused u64 DC_power[1];
static __maybe_unused u32 mid_heart_sbp[3];
static __maybe_unused u16 status_count1;
static __maybe_unused u16 real_freq_data;
static __maybe_unused u16 reflect_abssum;
static __maybe_unused u32 wear_ref3;
static __maybe_unused u8 led2_current;
static __maybe_unused u8 Sta_down_count;
static __maybe_unused u16 Sta_temp_data;
static __maybe_unused u32 DC_voltage;
static __maybe_unused u16 status_count0;
static __maybe_unused u32 Product_now_status;
static __maybe_unused u32 Sta_temp_data1;
static __maybe_unused u32 gensor_static_count;
static __maybe_unused u32 wear_ref1;
static __maybe_unused u32 gensor_sport_count;
static __maybe_unused u32 freq_maxpower[5];
static __maybe_unused u32 state_judge_flag;
static __maybe_unused u32 red_power_s;
static __maybe_unused u16 ch_C_expot;
static __maybe_unused u32 gensor_power;
static __maybe_unused u32 Product_old_status;
static __maybe_unused u32 sta_static_data;
static __maybe_unused u32 Product_test_status;
static __maybe_unused u16 led1_current;
static __maybe_unused u16 led0_current;
static __maybe_unused u32 red_data_ave;
static __maybe_unused u32 ir_temp_chage;
static __maybe_unused u16 ch_A_expot;
static __maybe_unused u8 ir_power_s;

/* ---------- shell-shared state (alg.h externs) ---------- */
u8 test_kinds;

/* ---------- shell-shared state (alg.h externs) ---------- */
u8 sensor_refset[8];

/* ---------- shell-shared state (alg.h externs) ---------- */
u32 wear_refdata[3];

/* ---------- shell-shared state (alg.h externs) ---------- */
u32 bd_data_result[26];

/* ---------- forward prototypes ---------- */
void Delay_ms(u16 param_1);
u8 BD_Sensor_Init(s32 param_1,u8 *param_2);
u8 BD_Sensor_Start(u64 param_1,u32 *param_2);
u8 BD_Sensor_Close(void);
u8 BD_Sensor_Data_Read(s32 param_1);
void Add_Complex(s32 *param_1,s32 *param_2,s32 *param_3);
void Sub_Complex(s32 *param_1,s32 *param_2,s32 *param_3);
void Multy_Complex(s32 *param_1,s32 *param_2,s32 *param_3);
s64 sin_look(u16 param_1,s64 param_2);
s64 cos_look(u16 param_1,s64 param_2,u16 param_3);
void getWN(u16 param_1,u64 param_2,s32 *param_3,u16 param_4);
void fft_test_bitreverse(void *param_1,void *param_2,u16 param_3);
void fft_bitreverse(void *param_1,void *param_2,u16 param_3);
void FFT(u64 param_1,void *param_2,u16 param_3);
void fft_mtest1(u16 param_1,s64 param_2,s64 param_3,u16 param_4);
void fft_mtest2(u16 param_1,s64 param_2,s64 param_3,u16 param_4);
s64 gen_data_iir_pro(u16 param_1);
u64 in_gensor_adcdata(u8 param_1,s64 param_2,void *param_3);
s32 data_bpmiir_pro(s32 param_1,s32 param_2);
s32 data_iir_pro(s32 param_1);
s32 data1_iir_pro(s32 param_1);
s32 data2_iir_pro(s32 param_1);
s32 data_hrviir_pro(s32 param_1);
s32 data_bpmlpf_pro(u32 param_1);
s32 data_hrvlpf_pro(u32 param_1);
s32 datared_lpf_pro(u32 param_1);
s32 datared_hpf_pro(u32 param_1);
s32 datair_lpf_pro(u32 param_1);
s32 datair_hpf_pro(u32 param_1);
s32 datagreen_lpf_pro(u32 param_1);
s32 datagreen_hpf_pro(u32 param_1);
u64 BD18XX_Wear_pro(u16 param_1,void *param_2);
u64 in_BPM_adcdata(u32 param_1,u16 param_2,s32 *param_3,void *param_4);
u64 in_spo2test_adcdata(u16 param_1,u32 *param_2,s64 param_3,u32 *param_4,s64 param_5 ,u32 *param_6,s64 param_7);
u64 in_spo2test1_adcdata(u16 param_1,void *param_2,void *param_3,void *param_4,void *param_5,void *param_6,void *param_7);
u64 in_HRV_adcdata(u16 param_1,u32 *param_2,void *param_3);
void Wear_SPO2_judge(void);
void Wear_Hrv_judge(void);
void Wear_judge(void);
void Product_state_judge2(void *param_1);
void Data_sequence(void *param_1,void *param_2);
u16 Gensor_sport_freq(u32 *param_1);
void Heart_real_math(u32 *param_1,u32 *param_2);
u16 BP_blood_user(u32 param_1);
void Gensor_init(void);
u64 Gensor_Date_Pro(void);
void Gensor_State_Read(s8 param_1);
void BD_Spo2test_math(u32 *param_1,u32 *param_2,u32 *param_3,u32 *param_4);
void BD_Spo2_math(u64 param_1,u64 param_2,u32 *param_3);
void BD_HRV_math(u16 param_1,void *param_2,void *param_3);
void BD1860_Wear_Check(void);
u32 BD_health_pro(u32 param_1,u8 param_2,u64 param_3,u64 param_4, u32 *param_5);

/* ==== Delay_ms @ 0xffffff80085ed074 ==== */
void Delay_ms(u16 param_1)

{
  msleep(param_1);
  return;
}

/* ==== BD_heartbp_init @ 0xffffff80085ede20 (factory verbatim, 15 assigns)
 * r45 P0: was missing entirely - all vars stayed BSS-zero, which masked the
 * wear-judge baseline (sta_static_data etc) after the DC chain fix. ==== */
u8 BD_heartbp_init(void)
{
	malloc_flag = 1;
	gensor_data_count = 0;
	time_count = 0;
	all_code_count = 0;
	all_code_count1 = 0;
	sta_basic_value = 0x4b;
	run_basic_value = 0x5a;
	user_count = 0;
	no_touch_flag = 0;
	no_touch_count[0] = 6;	/* factory strb to base = elem[0]; Ghidra flattened array */
	status_count0 = 0;
	status_count1 = 0;
	lcd_temp_data = 0x4b;
	sta_static_data = 0x4b;
	run_static_data = 0x4b;
	return 1;
}

/* ==== BD_Sensor_Init @ 0xffffff80085edf00 ==== */
u8 BD_Sensor_Init(s32 param_1,u8 *param_2)

{
  u8 bVar1;
  s8 cVar2;
  s8 cVar3;
  u8 uStack_1;
  
  led0_current = *param_2;
  led1_current = param_2[1];
  led2_current = param_2[2];
  ch_A_expot = 0x60;
  ch_B_expot = 0x60;
  ch_C_expot = 0x60;
  if (param_1 == 2) {
    cVar2 = BD_I2C_Write(0,0xe8,0);
    cVar3 = BD_I2C_Write(0,0xee,7);
    if ((cVar2 == '\0') || (cVar3 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    Delay_ms(10);
    cVar2 = BD_I2C_Write(0,0xe8,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,3,0x80);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,4,5);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,5,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x12,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x13,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,8,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,9,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,10,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xb,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xc,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xd,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xe,0x28);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xf,0x28);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x10,10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x11,0x77);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x14,led0_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x15,led1_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x16,led2_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x17,(s8)ch_A_expot);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x18,(s8)((u16)ch_A_expot >> 8));
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x19,(s8)ch_B_expot);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1a,(s8)((u16)ch_B_expot >> 8));
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1b,0x10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1c,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1d,0x4f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1e,0x2f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1f,0x4f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x22,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x26,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xeb,0x80);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,7,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      uStack_1 = 0;
    }
    else {
      uStack_1 = 1;
    }
  }
  else if (param_1 == 0xd) {
    ch_A_expot = 0x78;
    cVar2 = BD_I2C_Write(0,0xe8,0);
    cVar3 = BD_I2C_Write(0,0xee,7);
    if ((cVar2 == '\0') || (cVar3 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    Delay_ms(10);
    cVar2 = BD_I2C_Write(0,0xe8,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,3,0xa0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,4,5);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,5,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x12,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x13,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,8,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,9,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,10,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xb,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xc,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xd,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xe,5);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xf,10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x10,10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x11,0x33);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x14,led0_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x15,led1_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x16,led2_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x17,(s8)ch_A_expot);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x18,(s8)((u16)ch_A_expot >> 8));
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x19,0x10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1a,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1b,0x10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1c,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1d,0x1f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1e,0x4f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1f,0x2f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x22,0xf0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x26,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xeb,0x80);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,7,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      uStack_1 = 0;
    }
    else {
      uStack_1 = 1;
    }
  }
  else {
    cVar2 = BD_I2C_Write(0,0xe8,0);
    cVar3 = BD_I2C_Write(0,0xee,7);
    if ((cVar2 == '\0') || (cVar3 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    Delay_ms(10);
    cVar2 = BD_I2C_Write(0,0xe8,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,3,0xa0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,4,5);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,5,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x12,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x13,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,8,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,9,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,10,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xb,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xc,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xd,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xe,0x14);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xf,10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x10,10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x11,0x33);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x14,led0_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x15,led1_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x16,led2_current);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x17,(s8)ch_A_expot);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x18,(s8)((u16)ch_A_expot >> 8));
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x19,0x10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1a,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1b,0x10);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1c,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1d,0x1f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1e,0x4f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x1f,0x2f);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x22,0xf0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0x26,0);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,0xeb,0x80);
    if ((!bVar1) || (cVar2 == '\0')) {
      bVar1 = false;
    }
    else {
      bVar1 = true;
    }
    cVar2 = BD_I2C_Write(0,7,1);
    if ((!bVar1) || (cVar2 == '\0')) {
      uStack_1 = 0;
    }
    else {
      uStack_1 = 1;
    }
  }
  return uStack_1;
}

/* ==== BD_Sensor_Start @ 0xffffff80085ef904 ==== */
u8 BD_Sensor_Start(u64 param_1,u32 *param_2)

{
  u16 uStack_4;
  u8 bStack_1;
  
  for (uStack_4 = 0; uStack_4 < 0x100; uStack_4 = uStack_4 + 1) {
  }
  for (uStack_4 = 0; uStack_4 < 0x80; uStack_4 = uStack_4 + 1) {
    *(u16 *)((char *)bd_gensor_buf + (s64)(s32)(u32)uStack_4 * 2) = 0;
  }
  for (uStack_4 = 0; uStack_4 < 0x100; uStack_4 = uStack_4 + 1) {
    *(u32 *)((char *)green_iir_out + (s64)(s32)(u32)uStack_4 * 4) = 0;
    *(u32 *)((char *)red_iir_out + (s64)(s32)(u32)uStack_4 * 4) = 0;
    *(u32 *)((char *)ir_iir_out + (s64)(s32)(u32)uStack_4 * 4) = 0;
  }
  for (uStack_4 = 0; uStack_4 < 800; uStack_4 = uStack_4 + 1) {
    *(u8 *)((s64)(s32)(u32)uStack_4 + wave_data2) = 0;
  }
  for (uStack_4 = 0; uStack_4 < 8; uStack_4 = uStack_4 + 1) {
    *(u16 *)((char *)bpm_wavedatabuf + (s64)(s32)(u32)uStack_4 * 2) = 0;
  }
  for (bStack_1 = 0; bStack_1 < 6; bStack_1 = bStack_1 + 1) {
    *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result) = 0;
    *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_test_buf) = 0;
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u16 *)((char *)hrv_disp_buf + (s64)(s32)(u32)bStack_1 * 2) = 0;
  }
  for (bStack_1 = 0; bStack_1 < 0x19; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)RAW_A_buf + (s64)(s32)(u32)bStack_1 * 4) = 0;
    *(u32 *)((char *)RAW_B_buf + (s64)(s32)(u32)bStack_1 * 4) = 0;
    *(u32 *)((char *)RAW_C_buf + (s64)(s32)(u32)bStack_1 * 4) = 0;
    *(u32 *)((char *)RAW_A_data + (s64)(s32)(u32)bStack_1 * 4) = 0;
    *(u32 *)((char *)RAW_B_data + (s64)(s32)(u32)bStack_1 * 4) = 0;
    *(u32 *)((char *)RAW_C_data + (s64)(s32)(u32)bStack_1 * 4) = 0;
    *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)bStack_1 * 2) = 0;
    *(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)bStack_1 * 2) = 0;
    *(u16 *)((char *)ch_C_buf + (s64)(s32)(u32)bStack_1 * 2) = 0;
  }
  red_ir_ac_old = 0;
  new_data_flag = 0;
  power_unormal_count = 0;
  spo2_power_count = 0;
  power_kal = 0;
  gensor_data_count = 0;
  gensor_state_count = 0;
  speed_state_count = 0;
  gensor_power_freq_ave = 0;
  bp_gensor_power = 0;
  (*(u16 *)((s8 *)&bp_gensor_power + 2)) = 0;
  bp_gensor_data[0] = 0;
  (*(u16 *)((s8 *)bp_gensor_data + 2)) = 0;
  bp_gensor_flag = 0;
  red_setflag = 1;
  ir_setflag = 1;
  ch00_middata = 0;
  ch01_middata = 0;
  ch02_middata = 0;
  all_code_count = 0;
  all_code_count1 = 0;
  jump_delta = 0;
  jump_delta1 = 0;
  jump_delta2 = 0;
  fifo_iir_data = 0;
  fifo_iir_data1 = 0;
  fifo_iir_data2 = 0;
  run_data_buf[0] = 0;
  run_data_buf[1] = 0;
  weak_sport_count = 0;
  weak_sport_flag = 0;
  sport_stable_flag = 0;
  bpm_high_flag = 0;
  bpm_sum_flag = 0;
  bpm_unormal_count = 0;
  bpm_start_data = 0;
  bpm_now_data0 = 0;
  bpm_now_data1 = 0;
  bpm_run_count = 0;
  bpm_static_count = 0;
  bp_gensor_oldflag = 0;
  bpm_stable_data = 0;
  time_count = 0;
  user_count = 0;
  lcd_flag[0] = 0;
  unormal_flag = 0;
  unormal_count = 0;
  unormal_count1 = 0;
  lcd_disdata = 0;
  hrv_avedata_ms = 0;
  hrv_data_ms = 0;
  hrv_data1_ms = 0;
  hrv_rmssd_data = 0;
  hrv_press_data = 0;
  bpm_wavedata = 0;
  bpm_disdata = 0;
  status_flag = 0;
  reflect_abssum = 0;
  debug_count = 0;
  gensor_state = 2;
  bp_varrange[0] = 0;
  bd44_guard = 0;
  DC_refless_data1 = 0;
  no_touch_flag = 0;
  no_touch_count[0]= 6;
  status_count0 = 0;
  status_count1 = 0;
  state_stable_count = 0;
  test_pro_count = 0;
  test_pro_count1 = 0;
  lcd_temp_data = 0x4b;
  run_init_data = 0x4b;
  hardware_flag1 = 1;
  Product_old_status = 2;
  gensor_stateflag = 0;
  test_pro_flag = 0;
  speed_state_flag = 0;
  walk_state_count = 0;
  green_dc_value = 0;
  red_dc_value = 0;
  ir_dc_value = 0;
  first_data = 0;
  second_data = 0;
  now_data = 0;
  cha_offflag = 0;
  chb_offflag = 0;
  chc_offflag = 0;
  ch_A_setflag = 0;
  ch_B_setflag = 0;
  ch_C_setflag = 0;
  phdata_count = 0;
  green_dc_power = 0;
  red_dc_power = 0;
  ir_dc_power = 0;
  green_ac_power = 0;
  red_ac_power = 0;
  ir_ac_power = 0;
  green_old_data = 0;
  red_old_data = 0;
  ir_old_data = 0;
  wear_count = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u16 *)((char *)gensor_power_buf + (s64)(s32)(u32)bStack_1 * 2) = 0;
  }
  Gensor_init();
  Product_test_status = 2;
  wear_ref1 = *param_2;
  wear_ref2 = param_2[1];
  wear_ref3 = param_2[2];
  return 1;
}

/* ==== BD_Sensor_Close @ 0xffffff80085f0108 ==== */
u8 BD_Sensor_Close(void)

{
  s8 cVar1;
  s8 cVar2;
  u8 uVar3;
  
  time_count = 0;
  hardware_flag1 = 0;
  malloc_flag = 0;
  cVar1 = BD_I2C_Write(0,7,0);
  cVar2 = BD_I2C_Write(0,0xe8,1);
  if ((cVar2 == '\0') || (cVar1 == '\0')) {
    uVar3 = 0;
  }
  else {
    uVar3 = 1;
  }
  return uVar3;
}

/* ==== BD_Sensor_Data_Read @ 0xffffff80085f0194 ==== */
u8 BD_Sensor_Data_Read(s32 param_1)

{
  s8 cVar1;
  s8 cStack_8;
  u8 uStack_7;
  u8 uStack_6;
  u8 bStack_5;
  u8 bStack_4;
  u8 uStack_3;
  s8 cStack_2;
  u8 bStack_1;
  
  uStack_3 = 0;
  cStack_2 = '\x01';
  if (hardware_flag1 == '\x01') {
    if (param_1 == 2) {
      bStack_4 = BD_I2C_Read(0,0x27,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x28,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x29,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_A_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x2e,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x2f,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      if (phdata_count == 0) {
        cha_offflag = 0;
      }
      if (ch_A_setflag == '\x01') {
        cha_offflag = 1;
        if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x2904) ||
           (0x34bc < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))) {
          if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x2904) &&
             (ch_A_expot < 0x78)) {
            if (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 7000) {
              ch_A_expot = ch_A_expot + 5;
            }
            else {
              ch_A_expot = ch_A_expot + 1;
            }
          }
          else if ((0x34bc < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))
                  && (6 < ch_A_expot)) {
            if (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x3c8d) {
              ch_A_expot = ch_A_expot - 1;
            }
            else {
              ch_A_expot = ch_A_expot - 5;
            }
          }
        }
        else {
          ch_A_setflag = '\0';
        }
        if (ch_A_setflag == '\x01') {
          uStack_7 = (u8)ch_A_expot;
          cStack_8 = BD_I2C_Write(0,0x17,uStack_7);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
          uStack_7 = (u8)(ch_A_expot >> 8);
          cStack_8 = BD_I2C_Write(0,0x18,uStack_7);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
          cStack_8 = BD_I2C_Write(0,0xeb,0x80);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
      }
      else if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x2134) ||
              (15000 < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))) {
        ch_A_setflag = '\x01';
      }
      bStack_4 = BD_I2C_Read(0,0x32,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x33,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x34,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_B_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x39,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x3a,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      if (phdata_count == 0) {
        chb_offflag = 0;
      }
      if (ch_B_setflag == '\x01') {
        chb_offflag = 1;
        if ((*(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2) < 0x2904) ||
           (0x34bc < *(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2))) {
          if ((*(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2) < 0x2904) &&
             (ch_B_expot < 0x78)) {
            if (*(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2) < 7000) {
              ch_B_expot = ch_B_expot + 5;
            }
            else {
              ch_B_expot = ch_B_expot + 1;
            }
          }
          else if ((0x34bc < *(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2))
                  && (6 < ch_B_expot)) {
            if (*(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2) < 0x3c8d) {
              ch_B_expot = ch_B_expot - 1;
            }
            else {
              ch_B_expot = ch_B_expot - 5;
            }
          }
        }
        else {
          ch_B_setflag = '\0';
        }
        if (ch_B_setflag == '\x01') {
          uStack_7 = (u8)ch_B_expot;
          cStack_8 = BD_I2C_Write(0,0x19,uStack_7);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
          uStack_7 = (u8)(ch_B_expot >> 8);
          cStack_8 = BD_I2C_Write(0,0x1a,uStack_7);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
          cStack_8 = BD_I2C_Write(0,0xeb,0x80);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
      }
      else if ((*(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2) < 0x2134) ||
              (15000 < *(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2))) {
        ch_B_setflag = '\x01';
      }
      bStack_4 = BD_I2C_Read(0,0x3d,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x3e,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x3f,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_C_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x44,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x45,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_C_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      if (Product_old_status != Product_test_status) {
        Product_test_status = Product_old_status;
        if (Product_old_status == 1) {
          cStack_8 = BD_I2C_Write(0,0x15,led1_current & 0xe0);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
        else {
          cStack_8 = BD_I2C_Write(0,0x15,led1_current);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
        cVar1 = BD_I2C_Write(0,0xeb,0x80);
        if ((cStack_2 == '\0') || (cVar1 == '\0')) {
          cStack_2 = '\0';
        }
        else {
          cStack_2 = '\x01';
        }
      }
      if (cStack_2 == '\x01') {
        if (phdata_count < 0x13) {
          phdata_count = phdata_count + 1;
        }
        else {
          RAW_A_data[20] = 0;
          RAW_B_data[20] = 0;
          RAW_C_data[20] = 0;
          for (bStack_1 = 0; bStack_1 < 0x14; bStack_1 = bStack_1 + 1) {
            *(u32 *)((char *)RAW_A_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_A_buf + (s64)(s32)(u32)bStack_1 * 4);
            *(u32 *)((char *)RAW_B_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_B_buf + (s64)(s32)(u32)bStack_1 * 4);
            *(u32 *)((char *)RAW_C_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_C_buf + (s64)(s32)(u32)bStack_1 * 4);
            if (0xf < bStack_1) {
              RAW_A_data[20] =
                   RAW_A_data[20] +
                   (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
              RAW_B_data[20] =
                   RAW_B_data[20] +
                   (*(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
              RAW_C_data[20] =
                   RAW_C_data[20] +
                   (*(u16 *)((char *)ch_C_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
            }
          }
          RAW_A_data[21] = (u32)ch_A_expot;
          RAW_B_data[21] = (u32)ch_B_expot;
          RAW_C_data[21] = (u32)ch_C_expot;
          RAW_A_data[22] = (u32)cha_offflag;
          RAW_B_data[22] = (u32)chb_offflag;
          RAW_C_data[22] = (u32)chc_offflag;
          uStack_3 = 1;
          phdata_count = 0;
        }
      }
    }
    else if (param_1 == 0xd) {
      bStack_4 = BD_I2C_Read(0,0x27,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x28,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x29,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_A_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x2e,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x2f,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      if (ch_A_setflag == '\x01') {
        if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x1d4c) ||
           (0x34bc < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))) {
          if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x1d4c) &&
             (ch_A_expot < 0x78)) {
            if (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 6000) {
              ch_A_expot = ch_A_expot + 5;
            }
            else {
              ch_A_expot = ch_A_expot + 1;
            }
          }
          else if ((0x34bc < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))
                  && (6 < ch_A_expot)) {
            if (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x3c8d) {
              ch_A_expot = ch_A_expot - 1;
            }
            else {
              ch_A_expot = ch_A_expot - 5;
            }
          }
        }
        else {
          ch_A_setflag = '\0';
        }
        if (ch_A_setflag == '\x01') {
          uStack_7 = (u8)ch_A_expot;
          cStack_8 = BD_I2C_Write(0,0x17,uStack_7);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
          uStack_7 = (u8)(ch_A_expot >> 8);
          cStack_8 = BD_I2C_Write(0,0x18,uStack_7);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
          cStack_8 = BD_I2C_Write(0,0xeb,0x80);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
      }
      else if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 7000) ||
              (15000 < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))) {
        ch_A_setflag = '\x01';
      }
      bStack_4 = BD_I2C_Read(0,0x32,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x33,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x34,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_B_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x39,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x3a,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      bStack_4 = BD_I2C_Read(0,0x3d,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x3e,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x3f,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_C_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x44,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x45,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_C_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      if (Product_old_status != Product_test_status) {
        Product_test_status = Product_old_status;
        if (Product_old_status == 1) {
          cStack_8 = BD_I2C_Write(0,0x14,led0_current & 0xe0);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
        else {
          cStack_8 = BD_I2C_Write(0,0x14,led0_current);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
        cVar1 = BD_I2C_Write(0,0xeb,0x80);
        if ((cStack_2 == '\0') || (cVar1 == '\0')) {
          cStack_2 = '\0';
        }
        else {
          cStack_2 = '\x01';
        }
      }
      if (cStack_2 == '\x01') {
        if (phdata_count < 0x13) {
          phdata_count = phdata_count + 1;
        }
        else {
          RAW_A_data[20] = 0;
          RAW_B_data[20] = 0;
          RAW_C_data[20] = 0;
          for (bStack_1 = 0; bStack_1 < 0x14; bStack_1 = bStack_1 + 1) {
            *(u32 *)((char *)RAW_A_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_A_buf + (s64)(s32)(u32)bStack_1 * 4);
            *(u32 *)((char *)RAW_B_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_B_buf + (s64)(s32)(u32)bStack_1 * 4);
            *(u32 *)((char *)RAW_C_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_C_buf + (s64)(s32)(u32)bStack_1 * 4);
            if (0xf < bStack_1) {
              RAW_A_data[20] =
                   RAW_A_data[20] +
                   (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
              RAW_B_data[20] =
                   RAW_B_data[20] +
                   (*(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
              RAW_C_data[20] =
                   RAW_C_data[20] +
                   (*(u16 *)((char *)ch_C_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
            }
          }
          RAW_A_data[21] = (u32)ch_A_expot;
          RAW_B_data[21] = (u32)ch_B_expot;
          RAW_C_data[21] = (u32)ch_C_expot;
          uStack_3 = 1;
          phdata_count = 0;
        }
      }
    }
    else {
      bStack_4 = BD_I2C_Read(0,0x27,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x28,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x29,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_A_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x2e,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x2f,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      if (ch_A_setflag == '\x01') {
        if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x1d4c) ||
           (0x34bc < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))) {
          if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x1d4c) &&
             (ch_A_expot < 0x78)) {
            if (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 6000) {
              ch_A_expot = ch_A_expot + 3;
            }
            else {
              ch_A_expot = ch_A_expot + 1;
            }
          }
          else if ((0x34bc < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))
                  && (6 < ch_A_expot)) {
            if (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 0x3c8d) {
              ch_A_expot = ch_A_expot - 1;
            }
            else {
              ch_A_expot = ch_A_expot - 3;
            }
          }
        }
        else {
          ch_A_setflag = '\0';
        }
        if (ch_A_setflag == '\x01') {
          uStack_7 = (u8)ch_A_expot;
          cStack_8 = BD_I2C_Write(0,0x17,uStack_7);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
          uStack_7 = (u8)(ch_A_expot >> 8);
          cStack_8 = BD_I2C_Write(0,0x18,uStack_7);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
          cStack_8 = BD_I2C_Write(0,0xeb,0x80);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
      }
      else if ((*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2) < 7000) ||
              (15000 < *(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)phdata_count * 2))) {
        ch_A_setflag = '\x01';
      }
      bStack_4 = BD_I2C_Read(0,0x32,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x33,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x34,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_B_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x39,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x3a,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      bStack_4 = BD_I2C_Read(0,0x3d,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x3e,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      uStack_6 = BD_I2C_Read(0,0x3f,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u32 *)((char *)RAW_C_buf + (s64)(s32)(u32)phdata_count * 4) =
           (u32)(((( uStack_6 ) & 0xff) << 16) | (( (((( bStack_5 ) & 0xff) << 8) | (( bStack_4 ) & 0xff)) ) & 0xffff));
      bStack_4 = BD_I2C_Read(0,0x44,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      bStack_5 = BD_I2C_Read(0,0x45,&cStack_8);
      if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
        cStack_2 = '\0';
      }
      else {
        cStack_2 = '\x01';
      }
      *(u16 *)((char *)ch_C_buf + (s64)(s32)(u32)phdata_count * 2) =
           (u16)(bStack_4 >> 2) + (u16)bStack_5 * 0x40;
      if (Product_old_status != Product_test_status) {
        Product_test_status = Product_old_status;
        if (Product_old_status == 1) {
          cStack_8 = BD_I2C_Write(0,0x14,led0_current & 0xe0);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
        else {
          cStack_8 = BD_I2C_Write(0,0x14,led0_current);
          if ((cStack_2 == '\0') || (cStack_8 == '\0')) {
            cStack_2 = '\0';
          }
          else {
            cStack_2 = '\x01';
          }
        }
        cVar1 = BD_I2C_Write(0,0xeb,0x80);
        if ((cStack_2 == '\0') || (cVar1 == '\0')) {
          cStack_2 = '\0';
        }
        else {
          cStack_2 = '\x01';
        }
      }
      if (cStack_2 == '\x01') {
        if (phdata_count < 0x13) {
          phdata_count = phdata_count + 1;
        }
        else {
          RAW_A_data[20] = 0;
          RAW_B_data[20] = 0;
          RAW_C_data[20] = 0;
          for (bStack_1 = 0; bStack_1 < 0x14; bStack_1 = bStack_1 + 1) {
            *(u32 *)((char *)RAW_A_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_A_buf + (s64)(s32)(u32)bStack_1 * 4);
            *(u32 *)((char *)RAW_B_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_B_buf + (s64)(s32)(u32)bStack_1 * 4);
            *(u32 *)((char *)RAW_C_data + (s64)(s32)(u32)bStack_1 * 4) =
                 *(u32 *)((char *)RAW_C_buf + (s64)(s32)(u32)bStack_1 * 4);
            if (0xf < bStack_1) {
              RAW_A_data[20] =
                   RAW_A_data[20] +
                   (*(u16 *)((char *)ch_A_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
              RAW_B_data[20] =
                   RAW_B_data[20] +
                   (*(u16 *)((char *)ch_B_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
              RAW_C_data[20] =
                   RAW_C_data[20] +
                   (*(u16 *)((char *)ch_C_buf + (s64)(s32)(u32)bStack_1 * 2) >> 2);
            }
          }
          RAW_A_data[21] = (u32)ch_A_expot;
          RAW_B_data[21] = (u32)ch_B_expot;
          RAW_C_data[21] = (u32)ch_C_expot;
          uStack_3 = 1;
          phdata_count = 0;
        }
      }
    }
    if (cStack_2 != '\x01') {
      uStack_3 = 2;
    }
  }
  else {
    uStack_3 = 0;
  }
  return uStack_3;
}

/* ==== Add_Complex @ 0xffffff80085f29b4 ==== */
void Add_Complex(s32 *param_1,s32 *param_2,s32 *param_3)

{
  param_3[1] = param_1[1] + param_2[1];
  *param_3 = *param_1 + *param_2;
  return;
}

/* ==== Sub_Complex @ 0xffffff80085f2a08 ==== */
void Sub_Complex(s32 *param_1,s32 *param_2,s32 *param_3)

{
  param_3[1] = param_1[1] - param_2[1];
  *param_3 = *param_1 - *param_2;
  return;
}

/* ==== Multy_Complex @ 0xffffff80085f2a5c ==== */
void Multy_Complex(s32 *param_1,s32 *param_2,s32 *param_3)

{
  s32 iVar1;
  s32 iVar2;
  s32 iVar3;
  s32 iVar4;
  
  iVar1 = *param_1;
  iVar2 = *param_2;
  iVar3 = param_1[1];
  iVar4 = param_2[1];
  param_3[1] = (iVar1 * iVar4 + iVar2 * iVar3) >> 6;
  *param_3 = (iVar1 * iVar2 - iVar3 * iVar4) >> 6;
  return;
}

/* ==== sin_look @ 0xffffff80085f2b08 ==== */
s64 sin_look(u16 param_1,s64 param_2)

{
  return (s64)*(s16 *)(param_2 + (u64)param_1 * 2);
}

/* ==== cos_look @ 0xffffff80085f2b30 ==== */
s64 cos_look(u16 param_1,s64 param_2,u16 param_3)

{
  u32 uVar1;
  s32 iVar2;
  u32 uVar3;
  u32 uVar4;
  s32 iVar5;
  
  uVar1 = (u32)param_1 * 4 + (u32)param_3;
  uVar3 = (u32)param_3 * 4;
  uVar4 = 0;
  if (param_3 != 0) {
    uVar4 = uVar1 / uVar3;
  }
  iVar5 = uVar1 - uVar4 * uVar3;
  iVar2 = iVar5 + 3;
  if (-1 < iVar5) {
    iVar2 = iVar5;
  }
  return (s64)*(s16 *)(param_2 + (u64)(u16)(iVar2 >> 2) * 2);
}

/* ==== getWN @ 0xffffff80085f2b94 ==== */
void getWN(u16 param_1,u64 param_2,s32 *param_3,u16 param_4)

{
  s16 sVar1;
  
  sVar1 = sin_look(param_1,param_2);
  param_3[1] = -(s32)sVar1;
  sVar1 = cos_look(param_1,param_2,param_4);
  *param_3 = (s32)sVar1;
  return;
}

/* ==== fft_test_bitreverse @ 0xffffff80085f2bf4 ==== */
void fft_test_bitreverse(void *param_1,void *param_2,u16 param_3)

{
  u32 uVar1;
  u16 uStack_8;
  u16 uStack_6;
  u16 uStack_4;
  u16 uStack_2;
  
  for (uStack_2 = 0; uStack_2 < param_3; uStack_2 = uStack_2 + 1) {
    if (Product_old_status == 2) {
      if (uStack_2 < param_3 >> 1) {
        *(u32 *)(param_2 + (u64)uStack_2 * 8) =
             *(u32 *)(param_1 + (s64)(s32)((u32)uStack_2 + (u32)(param_3 >> 1)) * 4);
      }
      else {
        *(u32 *)(param_2 + (u64)uStack_2 * 8) =
             *(u32 *)(param_1 + (u64)param_3 * 4 + -4);
      }
    }
    else {
      *(u32 *)(param_2 + (u64)uStack_2 * 8) =
           *(u32 *)(param_1 + (u64)uStack_2 * 4);
    }
    *(u32 *)(param_2 + (u64)uStack_2 * 8 + 4) = 0;
  }
  for (uStack_2 = 1; uStack_2 <= (u16)(param_3 - 1); uStack_2 = uStack_2 + 1) {
    uStack_6 = uStack_2;
    uStack_4 = 0;
    for (uStack_8 = param_3 - 1; uStack_8 != 0; uStack_8 = uStack_8 >> 1) {
      uStack_4 = uStack_4 * 2 + (uStack_6 & 1);
      uStack_6 = uStack_6 >> 1;
    }
    if (uStack_2 < uStack_4) {
      uVar1 = *(u32 *)(param_2 + (u64)uStack_2 * 8);
      *(u32 *)(param_2 + (u64)uStack_2 * 8) =
           *(u32 *)(param_2 + (u64)uStack_4 * 8);
      *(u32 *)(param_2 + (u64)uStack_4 * 8) = uVar1;
    }
  }
  return;
}

/* ==== fft_bitreverse @ 0xffffff80085f2e04 ==== */
void fft_bitreverse(void *param_1,void *param_2,u16 param_3)

{
  u32 uVar1;
  u16 uStack_8;
  u16 uStack_6;
  u16 uStack_4;
  u16 uStack_2;
  
  for (uStack_2 = 0; uStack_2 < param_3; uStack_2 = uStack_2 + 1) {
    if (Product_old_status == 2) {
      *(s32 *)(param_2 + (u64)uStack_2 * 8) = (s32)*(s16 *)(param_1 + (u64)uStack_2 * 2);
    }
    else {
      *(s32 *)(param_2 + (u64)uStack_2 * 8) = (s32)*(s16 *)(param_1 + (u64)uStack_2 * 2);
    }
    *(u32 *)(param_2 + (u64)uStack_2 * 8 + 4) = 0;
  }
  for (uStack_2 = 1; uStack_2 <= (u16)(param_3 - 1); uStack_2 = uStack_2 + 1) {
    uStack_6 = uStack_2;
    uStack_4 = 0;
    for (uStack_8 = param_3 - 1; uStack_8 != 0; uStack_8 = uStack_8 >> 1) {
      uStack_4 = uStack_4 * 2 + (uStack_6 & 1);
      uStack_6 = uStack_6 >> 1;
    }
    if (uStack_2 < uStack_4) {
      uVar1 = *(u32 *)(param_2 + (u64)uStack_2 * 8);
      *(u32 *)(param_2 + (u64)uStack_2 * 8) =
           *(u32 *)(param_2 + (u64)uStack_4 * 8);
      *(u32 *)(param_2 + (u64)uStack_4 * 8) = uVar1;
    }
  }
  return;
}

/* ==== FFT @ 0xffffff80085f2fbc ==== */
void FFT(u64 param_1,void *param_2,u16 param_3)

{
  u32 uVar1;
  u32 uVar2;
  u32 uVar3;
  s32 auStack_20[2];
  u32 uStack_18;
  u32 uStack_14;
  u16 uStack_a;
  u16 uStack_8;
  u16 uStack_6;
  u16 uStack_4;
  u16 uStack_2;
  
  uStack_4 = 0;
  uStack_2 = param_3;
  while (uStack_2 = uStack_2 >> 1, uStack_2 != 0) {
    uStack_4 = uStack_4 + 1;
  }
  uStack_2 = uStack_4;
  for (uStack_6 = 0; uStack_6 < uStack_2; uStack_6 = uStack_6 + 1) {
    uStack_4 = 0;
    for (uStack_8 = 0; uStack_8 < param_3; uStack_8 = uStack_8 + 1) {
      uVar2 = (s32)(u32)uStack_8 >> (uStack_6 & 0x1f);
      uVar1 = uVar2 & 1;
      uVar3 = -uVar1;
      if (-1 < (s32)uVar2) {
        uVar3 = uVar1;
      }
      if (uVar3 == 1) {
        getWN(uStack_4,param_1,auStack_20,param_3);
        Multy_Complex((s32 *)(param_2 + (u64)uStack_8 * 8),auStack_20,(s32 *)(param_2 + (u64)uStack_8 * 8));
        uStack_4 = (s16)(1 << (u64)((((u32)uStack_2 - (u32)uStack_6) - 1) & 0x1f)) + uStack_4;
        uStack_a = uStack_8 - (s16)(1 << (u64)(uStack_6 & 0x1f));
        uStack_18 = *(u32 *)(param_2 + (u64)uStack_a * 8);
        uStack_14 = *(u32 *)(param_2 + (u64)uStack_a * 8 + 4);
        Add_Complex(&uStack_18,(s32 *)(param_2 + (u64)uStack_8 * 8),(s32 *)(param_2 + (u64)uStack_a * 8));
        Sub_Complex(&uStack_18,(s32 *)(param_2 + (u64)uStack_8 * 8),(s32 *)(param_2 + (u64)uStack_8 * 8));
      }
      else {
        uStack_4 = 0;
      }
    }
  }
  return;
}

/* ==== fft_mtest1 @ 0xffffff80085f31b8 ==== */



void fft_mtest1(u16 param_1,s64 param_2,s64 param_3,u16 param_4)

{
  s32 iVar1;
  s32 iVar2;
  u32 uVar3;
  u32 auStack_90[24];
  u16 auStack_30[4];
  u32 uStack_28;
  s32 iStack_24;
  u32 uStack_20;
  u32 uStack_1c;
  u16 uStack_16;
  s32 iStack_14;
  s32 iStack_10;
  s32 iStack_c;
  s16 sStack_6;
  u16 uStack_4;
  u16 uStack_2;
  
  sStack_6 = 0;
  iStack_c = 0;
  for (uStack_2 = 0; uStack_2 < 4; uStack_2 = uStack_2 + 1) {
    auStack_90[(s64)(s32)(u32)uStack_2 * 3] = 1;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 1] = 1;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 2] = 1;
    auStack_30[(s32)(u32)uStack_2] = 0;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 0xc] = 0;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 0xd] = 0;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 0xe] = 0;
  }
  for (uStack_2 = 0; uStack_2 < 2; uStack_2 = uStack_2 + 1) {
    iStack_10 = *(s32 *)(param_2 + (u64)uStack_2 * 8) >> 6;
    iStack_14 = *(s32 *)(param_2 + (u64)uStack_2 * 8 + 4) >> 6;
    auStack_90[(s64)(s32)(uStack_2 + 1) * 3 + 0xd] = iStack_10 * iStack_10 + iStack_14 * iStack_14;
    if (uStack_2 == 0) {
      DC_power[0] = auStack_90[0x10];
    }
  }
  for (uStack_2 = 1; uVar3 = auStack_90[0x13], auStack_90[0xf] = auStack_90[0x10],
      (s32)(u32)uStack_2 < (s32)((param_4 >> 1) - 1); uStack_2 = uStack_2 + 1) {
    sStack_6 = 0;
    auStack_90[0xd] = auStack_90[0x10];
    auStack_90[0x10] = auStack_90[0x13];
    iStack_10 = *(s32 *)(param_2 + ((u64)uStack_2 + 1) * 8) >> 6;
    iStack_14 = *(s32 *)(param_2 + ((u64)uStack_2 + 1) * 8 + 4) >> 6;
    auStack_90[0x13] = iStack_10 * iStack_10 + iStack_14 * iStack_14;
    auStack_90[0x11] = auStack_90[0x13];
    if ((uVar3 < auStack_90[0xf]) || (uVar3 < auStack_90[0x13])) {
      sStack_6 = 1;
    }
    if (sStack_6 == 0) {
      iStack_c = 0;
      if (param_4 != 0) {
        iStack_c = (s32)((u32)param_1 * (u32)uStack_2 * 0x3c + (u32)(param_4 >> 1)) /
                   (s32)(u32)param_4;
      }
      if ((0x31 < iStack_c) && (iStack_c < 0x105)) {
        for (uStack_4 = 4; uStack_4 != 0; uStack_4 = uStack_4 - 1) {
          if (auStack_90[(s64)(s32)(uStack_4 - 1) * 3 + 1] < auStack_90[0x10]) {
            if (uStack_4 < 4) {
              *(u64 *)(auStack_90 + (s64)(s32)(u32)uStack_4 * 3) =
                   *(u64 *)(auStack_90 + (s64)(s32)(uStack_4 - 1) * 3);
              auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 2] =
                   auStack_90[(s64)(s32)(uStack_4 - 1) * 3 + 2];
              *(u64 *)(auStack_90 + (s64)(s32)(uStack_4 - 1) * 3) =
                   (((((u64)( auStack_90[0x10] )) << 32) | ((u64)( auStack_90[0xf] ) & 0xffffffffULL)));
              auStack_90[(s64)(s32)(uStack_4 - 1) * 3 + 2] = auStack_90[0x11];
              auStack_30[(s32)(u32)uStack_4] = auStack_30[(s32)(uStack_4 - 1)];
              auStack_30[(s32)(uStack_4 - 1)] = uStack_2;
            }
            else {
              uVar3 = (u32)uStack_4;
              *(u64 *)(auStack_90 + (s64)(s32)(uVar3 - 1) * 3) =
                   (((((u64)( auStack_90[0x10] )) << 32) | ((u64)( auStack_90[0xf] ) & 0xffffffffULL)));
              auStack_90[(s64)(s32)(uVar3 - 1) * 3 + 2] = auStack_90[0x11];
              auStack_30[(s32)(uStack_4 - 1)] = uStack_2;
            }
          }
        }
      }
    }
  }
  for (uStack_4 = 0; uStack_4 < 4; uStack_4 = uStack_4 + 1) {
    uStack_16 = auStack_30[(s32)(u32)uStack_4];
    if (uStack_16 == 0) {
      *(u32 *)(param_3 + (u64)uStack_4 * 4) = 0;
      *(u32 *)(param_3 + (s64)(s32)(uStack_4 + 4) * 4) = 0;
    }
    else {
      uStack_1c = auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 2];
      uStack_20 = auStack_90[(s64)(s32)(u32)uStack_4 * 3];
      uVar3 = 0;
      if (param_4 != 0) {
        uVar3 = ((u32)param_1 * 0x3c0) / (u32)param_4;
      }
      iStack_24 = 0;
      if (auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 1] != 0) {
        iStack_24 = (s32)(uVar3 * (auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 2] -
                                  auStack_90[(s64)(s32)(u32)uStack_4 * 3])) /
                    (s32)auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 1];
      }
      iVar2 = 0;
      if (param_4 != 0) {
        iVar2 = (s32)((u32)param_1 * 0x3c0 * (u32)uStack_16) / (s32)(u32)param_4;
      }
      iVar2 = iVar2 + iStack_24;
      iVar1 = iVar2 + 0xf;
      if (-1 < iVar2) {
        iVar1 = iVar2;
      }
      iStack_c = iVar1 >> 4;
      *(s32 *)(param_3 + (u64)uStack_4 * 4) = iVar1 >> 4;
      uStack_28 = auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 1];
      *(u32 *)(param_3 + (s64)(s32)(uStack_4 + 4) * 4) =
           auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 1] >> 6;
    }
  }
  return;
}

/* ==== fft_mtest2 @ 0xffffff80085f3844 ==== */
void fft_mtest2(u16 param_1,s64 param_2,s64 param_3,u16 param_4)

{
  s32 iVar1;
  s32 iVar2;
  u32 uVar3;
  u32 auStack_90[24];
  u16 auStack_30[4];
  u32 uStack_28;
  s32 iStack_24;
  u32 uStack_20;
  u32 uStack_1c;
  u16 uStack_16;
  s32 iStack_14;
  s32 iStack_10;
  s32 iStack_c;
  s16 sStack_6;
  u16 uStack_4;
  u16 uStack_2;
  
  sStack_6 = 0;
  iStack_c = 0;
  for (uStack_2 = 0; uStack_2 < 4; uStack_2 = uStack_2 + 1) {
    auStack_90[(s64)(s32)(u32)uStack_2 * 3] = 1;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 1] = 1;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 2] = 1;
    auStack_30[(s32)(u32)uStack_2] = 0;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 0xc] = 1;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 0xd] = 1;
    auStack_90[(s64)(s32)(u32)uStack_2 * 3 + 0xe] = 1;
  }
  for (uStack_2 = 0; uStack_2 < 2; uStack_2 = uStack_2 + 1) {
    iStack_10 = *(s32 *)(param_2 + (u64)uStack_2 * 8) >> 4;
    iStack_14 = *(s32 *)(param_2 + (u64)uStack_2 * 8 + 4) >> 4;
    auStack_90[(s64)(s32)(uStack_2 + 1) * 3 + 0xd] = iStack_10 * iStack_10 + iStack_14 * iStack_14;
  }
  for (uStack_2 = 1; uVar3 = auStack_90[0x13], auStack_90[0xf] = auStack_90[0x10],
      (s32)(u32)uStack_2 < (s32)((param_4 >> 1) - 1); uStack_2 = uStack_2 + 1) {
    sStack_6 = 0;
    auStack_90[0xd] = auStack_90[0x10];
    auStack_90[0x10] = auStack_90[0x13];
    iStack_10 = *(s32 *)(param_2 + ((u64)uStack_2 + 1) * 8) >> 4;
    iStack_14 = *(s32 *)(param_2 + ((u64)uStack_2 + 1) * 8 + 4) >> 4;
    auStack_90[0x13] = iStack_10 * iStack_10 + iStack_14 * iStack_14;
    auStack_90[0x11] = auStack_90[0x13];
    if ((uVar3 < auStack_90[0xf]) || (uVar3 < auStack_90[0x13])) {
      sStack_6 = 1;
    }
    if (sStack_6 == 0) {
      iStack_c = 0;
      if (param_4 != 0) {
        iStack_c = (s32)((u32)param_1 * (u32)uStack_2 * 0x3c + (u32)(param_4 >> 1)) /
                   (s32)(u32)param_4;
      }
      if ((0x31 < iStack_c) && (iStack_c < 0x105)) {
        for (uStack_4 = 4; uStack_4 != 0; uStack_4 = uStack_4 - 1) {
          if (auStack_90[(s64)(s32)(uStack_4 - 1) * 3 + 1] < auStack_90[0x10]) {
            if (uStack_4 < 4) {
              *(u64 *)(auStack_90 + (s64)(s32)(u32)uStack_4 * 3) =
                   *(u64 *)(auStack_90 + (s64)(s32)(uStack_4 - 1) * 3);
              auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 2] =
                   auStack_90[(s64)(s32)(uStack_4 - 1) * 3 + 2];
              *(u64 *)(auStack_90 + (s64)(s32)(uStack_4 - 1) * 3) =
                   (((((u64)( auStack_90[0x10] )) << 32) | ((u64)( auStack_90[0xf] ) & 0xffffffffULL)));
              auStack_90[(s64)(s32)(uStack_4 - 1) * 3 + 2] = auStack_90[0x11];
              auStack_30[(s32)(u32)uStack_4] = auStack_30[(s32)(uStack_4 - 1)];
              auStack_30[(s32)(uStack_4 - 1)] = uStack_2;
            }
            else {
              uVar3 = (u32)uStack_4;
              *(u64 *)(auStack_90 + (s64)(s32)(uVar3 - 1) * 3) =
                   (((((u64)( auStack_90[0x10] )) << 32) | ((u64)( auStack_90[0xf] ) & 0xffffffffULL)));
              auStack_90[(s64)(s32)(uVar3 - 1) * 3 + 2] = auStack_90[0x11];
              auStack_30[(s32)(uStack_4 - 1)] = uStack_2;
            }
          }
        }
      }
    }
  }
  for (uStack_4 = 0; uStack_4 < 4; uStack_4 = uStack_4 + 1) {
    uStack_16 = auStack_30[(s32)(u32)uStack_4];
    if (uStack_16 == 0) {
      *(u32 *)(param_3 + (u64)uStack_4 * 4) = 0;
      *(u32 *)(param_3 + (s64)(s32)(uStack_4 + 4) * 4) = 0;
    }
    else {
      uStack_1c = auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 2];
      uStack_20 = auStack_90[(s64)(s32)(u32)uStack_4 * 3];
      uVar3 = 0;
      if (param_4 != 0) {
        uVar3 = ((u32)param_1 * 0x3c0) / (u32)param_4;
      }
      iStack_24 = 0;
      if (auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 1] != 0) {
        iStack_24 = (s32)(uVar3 * (auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 2] -
                                  auStack_90[(s64)(s32)(u32)uStack_4 * 3])) /
                    (s32)auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 1];
      }
      iVar2 = 0;
      if (param_4 != 0) {
        iVar2 = (s32)((u32)param_1 * 0x3c0 * (u32)uStack_16) / (s32)(u32)param_4;
      }
      iVar2 = iVar2 + iStack_24;
      iVar1 = iVar2 + 0xf;
      if (-1 < iVar2) {
        iVar1 = iVar2;
      }
      iStack_c = iVar1 >> 4;
      *(s32 *)(param_3 + (u64)uStack_4 * 4) = iVar1 >> 4;
      uStack_28 = auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 1];
      *(u32 *)(param_3 + (s64)(s32)(uStack_4 + 4) * 4) =
           auStack_90[(s64)(s32)(u32)uStack_4 * 3 + 1] >> 6;
    }
  }
  return;
}

/* ==== gen_data_iir_pro @ 0xffffff80085f3ea0 ==== */
s64 gen_data_iir_pro(u16 param_1)

{
  u32 uVar1;
  s32 iVar2;
  s16 sStack_2;
  
  iir_gen_data[4] = (u32)param_1 - iir_gen_data[1];
  iir_gen_data[1] = iir_gen_data[0];
  iir_gen_data[0] = (u32)param_1;
  iir_gen_data[4] =
       iir_gen_data[4] - ((iir_gen_data[2] * -0x69 + iir_gen_data[3] * 0x2e) >> 6);
  iir_gen_data[3] = iir_gen_data[2];
  iir_gen_data[2] = iir_gen_data[4];
  iVar2 = iir_gen_data[4] - iir_gen_data[6];
  iir_gen_data[6] = iir_gen_data[5];
  iir_gen_data[5] = iir_gen_data[4];
  iir_gen_data[9] =
       iVar2 - ((s32)(iir_gen_data[7] * -8 + iir_gen_data[8] * 0x11) >> 6);
  iir_gen_data[8] = iir_gen_data[7];
  iir_gen_data[7] = iir_gen_data[9];
  uVar1 = -iir_gen_data[9];
  if (-1 < (s32)iir_gen_data[9]) {
    uVar1 = iir_gen_data[9];
  }
  if (uVar1 < 0x8000) {
    sStack_2 = (s16)iir_gen_data[9];
  }
  else if ((s32)iir_gen_data[9] < 0) {
    sStack_2 = -0x8000;
  }
  else {
    sStack_2 = 0x7fff;
  }
  return (s64)sStack_2;
}

/* ==== in_gensor_adcdata @ 0xffffff80085f40b8 ==== */
u64 in_gensor_adcdata(u8 param_1,s64 param_2,void *param_3)

{
  s32 iVar1;
  s32 iVar2;
  u16 uVar3;
  u32 uVar4;
  s32 iStack_8;
  u16 uStack_4;
  u16 uStack_2;
  
  gensor_new_datalength = 0;
  for (uStack_2 = 0; uStack_2 < param_1; uStack_2 = uStack_2 + 1) {
    if (gensor_data_count < 3) {
      gensor_data_count = gensor_data_count + 1;
    }
    else {
      uStack_2 = uStack_2 + 1;
      gensor_data_count = 0;
    }
    if ((param_1 <= uStack_2) || (0x3f < uStack_2)) break;
    *(u16 *)((char *)gesnor_fifo_databuf + (s64)(s32)(u32)gensor_new_datalength * 2) =
         *(u16 *)(param_2 + (u64)uStack_2 * 2);
    gensor_new_datalength = gensor_new_datalength + 1;
  }
  if (all_code_count < 3) {
    iStack_8 = 0;
    for (uStack_4 = 0; uStack_4 < gensor_new_datalength; uStack_4 = uStack_4 + 1) {
      if (uStack_4 != 0) {
        iVar2 = (u32)*(u16 *)((char *)gesnor_fifo_databuf + (s64)(s32)(u32)uStack_4 * 2) -
                (u32)*(u16 *)((char *)gesnor_fifo_databuf + (s64)(s32)(uStack_4 - 1) * 2);
        iVar1 = -iVar2;
        if (-1 < iVar2) {
          iVar1 = iVar2;
        }
        iStack_8 = iStack_8 + iVar1;
      }
      gen_data_iir_pro(*(u16 *)((char *)gesnor_fifo_databuf + (s64)(s32)(u32)uStack_4 * 2));
    }
  }
  else {
    for (uStack_2 = 0; (s32)(u32)uStack_2 < (s32)(0x80 - (u32)gensor_new_datalength);
        uStack_2 = uStack_2 + 1) {
      *(u16 *)(param_3 + (u64)uStack_2 * 2) =
           *(u16 *)(param_3 + (s64)(s32)((u32)uStack_2 + (u32)gensor_new_datalength) * 2);
    }
    iStack_8 = 0;
    for (uStack_4 = 0; uStack_4 < gensor_new_datalength; uStack_4 = uStack_4 + 1) {
      if (uStack_4 != 0) {
        iVar2 = (u32)*(u16 *)((char *)gesnor_fifo_databuf + (s64)(s32)(u32)uStack_4 * 2) -
                (u32)*(u16 *)((char *)gesnor_fifo_databuf + (s64)(s32)(uStack_4 - 1) * 2);
        iVar1 = -iVar2;
        if (-1 < iVar2) {
          iVar1 = iVar2;
        }
        iStack_8 = iStack_8 + iVar1;
      }
      uVar4 = (u32)gensor_new_datalength;
      uVar3 = gen_data_iir_pro(*(u16 *)((char *)gesnor_fifo_databuf + (s64)(s32)(u32)uStack_4 * 2));
      *(u16 *)(param_3 + (s64)(s32)((0x80 - uVar4) + (u32)uStack_4) * 2) = uVar3;
    }
  }
  if (gensor_new_datalength < 2) {
    gensor_power = 1;
  }
  else {
    gensor_power = 0;
    if (gensor_new_datalength - 1 != 0) {
      gensor_power = (u32)(iStack_8 * 0x2b) / (gensor_new_datalength - 1);
    }
  }
  for (uStack_2 = 0; uStack_2 < 3; uStack_2 = uStack_2 + 1) {
    *(u16 *)((char *)gensor_power_buf + (s64)(s32)(u32)uStack_2 * 2) =
         *(u16 *)((char *)gensor_power_buf + (s64)(s32)(uStack_2 + 1) * 2);
  }
  if (gensor_power < 0xffff) {
    (*(u16 *)((s8 *)gensor_power_buf + 6)) = (u16)gensor_power;
  }
  else {
    (*(u16 *)((s8 *)gensor_power_buf + 6)) = 0xffff;
  }
  gensor_power_new[0] = 0;
  for (uStack_2 = 0; uStack_2 < 4; uStack_2 = uStack_2 + 1) {
    gensor_power_new[0] =
         (*(u16 *)((char *)gensor_power_buf + (s64)(s32)(u32)uStack_2 * 2) >> 2) + gensor_power_new[0];
  }
  if (gensor_power < 0x9c4) {
    if (weak_sport_count == 0) {
      weak_sport_flag = 0;
    }
    else {
      weak_sport_count = weak_sport_count - 1;
    }
  }
  else if (weak_sport_count < 8) {
    weak_sport_count = weak_sport_count + 4;
  }
  if (gensor_stateflag == '\0') {
    if (gensor_power < 0xbb9) {
      if (gensor_sport_count < 2) {
        gensor_sport_count = 0;
      }
      else {
        gensor_sport_count = gensor_sport_count - 2;
      }
    }
    else {
      gensor_sport_count = gensor_sport_count + 1;
      if (5 < gensor_sport_count) {
        if (lcd_flag[0] == '\0') {
          run_init_data1 = 0x50;
          Run_data = 0x50;
        }
        gensor_sport_count = 0;
        gensor_stateflag = '\x01';
        gensor_state = 5;
      }
    }
  }
  else if (gensor_power < 0xbb9) {
    gensor_static_count = gensor_static_count + 1;
    if (3 < gensor_static_count) {
      gensor_static_count = 0;
      gensor_stateflag = '\0';
      gensor_state = 2;
    }
  }
  else if (gensor_static_count < 2) {
    gensor_static_count = 0;
  }
  else {
    gensor_static_count = gensor_static_count - 2;
  }
  return 1;
}

/* ==== data_bpmiir_pro @ 0xffffff80085f47b4 ==== */
s32 data_bpmiir_pro(s32 param_1,s32 param_2)

{
  s32 iVar1;
  s32 iVar2;
  
  iVar1 = param_2 - fifo_iir_data;
  iVar2 = -iVar1;
  if (-1 < iVar1) {
    iVar2 = iVar1;
  }
  if (0xdac < iVar2) {
    jump_delta = (jump_delta + param_2) - fifo_iir_data;
  }
  iVar2 = param_2 - jump_delta;
  if (param_1 == 7) {
    if (Product_old_status == 3) {
      iVar1 = iir_data[2] * -0x6a + iir_data[3] * 0x2f;
    }
    else {
      iVar1 = iir_data[2] * -0x6d + iir_data[3] * 0x31;
    }
    iir_data[4] = (iVar2 - iir_data[1]) - (iVar1 >> 6);
    if (Product_old_status == 3) {
      iVar1 = iir_data[7] * -0x16 + iir_data[8] * 0x13;
    }
    else {
      iVar1 = iir_data[7] * -0x3f + iir_data[8] * 0x1f;
    }
    iir_data[9] = (iir_data[4] - iir_data[6]) - (iVar1 >> 6);
  }
  else {
    if (Product_old_status == 3) {
      iVar1 = iir_data[2] * -0x69 + iir_data[3] * 0x2e;
    }
    else {
      iVar1 = iir_data[2] * -0x6d + iir_data[3] * 0x31;
    }
    iir_data[4] = (iVar2 - iir_data[1]) - (iVar1 >> 6);
    if (Product_old_status == 3) {
      iVar1 = iir_data[7] * -8 + iir_data[8] * 0x11;
    }
    else {
      iVar1 = iir_data[7] * -0x3f + iir_data[8] * 0x1f;
    }
    iir_data[9] = (iir_data[4] - iir_data[6]) - (iVar1 >> 6);
  }
  iir_data[8] = iir_data[7];
  iir_data[6] = iir_data[5];
  iir_data[3] = iir_data[2];
  iir_data[1] = iir_data[0];
  iir_data[0] = iVar2;
  iir_data[2] = iir_data[4];
  iir_data[5] = iir_data[4];
  iir_data[7] = iir_data[9];
  fifo_iir_data = param_2;
  return iir_data[9];
}

/* ==== data_iir_pro @ 0xffffff80085f4d20 ==== */
s32 data_iir_pro(s32 param_1)

{
  s32 iVar1;
  s32 iVar2;
  
  iVar2 = param_1 - fifo_iir_data;
  iVar1 = -iVar2;
  if (-1 < iVar2) {
    iVar1 = iVar2;
  }
  if (0x1194 < iVar1) {
    jump_delta = (jump_delta + param_1) - fifo_iir_data;
  }
  iir_data[4] = (param_1 - jump_delta) - iir_data[1];
  iir_data[1] = iir_data[0];
  iir_data[0] = param_1 - jump_delta;
  iir_data[4] =
       iir_data[4] - ((iir_data[2] * -0x68 + iir_data[3] * 0x2f) >> 6);
  iir_data[3] = iir_data[2];
  iir_data[2] = iir_data[4];
  iir_data[9] = iir_data[4] - iir_data[6];
  iir_data[6] = iir_data[5];
  iir_data[5] = iir_data[4];
  iir_data[9] =
       iir_data[9] - ((iir_data[7] * -0x33 + iir_data[8] * 0x1d) >> 6);
  iir_data[8] = iir_data[7];
  iir_data[7] = iir_data[9];
  fifo_iir_data = param_1;
  return iir_data[9];
}

/* ==== data1_iir_pro @ 0xffffff80085f4f54 ==== */
s32 data1_iir_pro(s32 param_1)

{
  s32 iVar1;
  s32 iVar2;
  
  iVar2 = param_1 - fifo_iir_data1;
  iVar1 = -iVar2;
  if (-1 < iVar2) {
    iVar1 = iVar2;
  }
  if (0x1194 < iVar1) {
    jump_delta1 = (jump_delta1 + param_1) - fifo_iir_data1;
  }
  iir_data1[4] = (param_1 - jump_delta1) - iir_data1[1];
  iir_data1[1] = iir_data1[0];
  iir_data1[0] = param_1 - jump_delta1;
  iir_data1[4] =
       iir_data1[4] - ((iir_data1[2] * -0x68 + iir_data1[3] * 0x2f) >> 6);
  iir_data1[3] = iir_data1[2];
  iir_data1[2] = iir_data1[4];
  iir_data1[9] = iir_data1[4] - iir_data1[6];
  iir_data1[6] = iir_data1[5];
  iir_data1[5] = iir_data1[4];
  iir_data1[9] =
       iir_data1[9] - ((iir_data1[7] * -0x33 + iir_data1[8] * 0x1d) >> 6);
  iir_data1[8] = iir_data1[7];
  iir_data1[7] = iir_data1[9];
  fifo_iir_data1 = param_1;
  return iir_data1[9];
}

/* ==== data2_iir_pro @ 0xffffff80085f5188 ==== */
s32 data2_iir_pro(s32 param_1)

{
  s32 iVar1;
  s32 iVar2;
  
  iVar2 = param_1 - fifo_iir_data2;
  iVar1 = -iVar2;
  if (-1 < iVar2) {
    iVar1 = iVar2;
  }
  if (0x1194 < iVar1) {
    jump_delta2 = (jump_delta2 + param_1) - fifo_iir_data2;
  }
  iir_data2[4] = (param_1 - jump_delta2) - iir_data2[1];
  iir_data2[1] = iir_data2[0];
  iir_data2[0] = param_1 - jump_delta2;
  iir_data2[4] =
       iir_data2[4] - ((iir_data2[2] * -0x68 + iir_data2[3] * 0x2f) >> 6);
  iir_data2[3] = iir_data2[2];
  iir_data2[2] = iir_data2[4];
  iir_data2[9] = iir_data2[4] - iir_data2[6];
  iir_data2[6] = iir_data2[5];
  iir_data2[5] = iir_data2[4];
  iir_data2[9] =
       iir_data2[9] - ((iir_data2[7] * -0x33 + iir_data2[8] * 0x1d) >> 6);
  iir_data2[8] = iir_data2[7];
  iir_data2[7] = iir_data2[9];
  fifo_iir_data2 = param_1;
  return iir_data2[9];
}

/* ==== data_hrviir_pro @ 0xffffff80085f53bc ==== */
s32 data_hrviir_pro(s32 param_1)

{
  s32 iVar1;
  s32 iVar2;
  
  iVar2 = param_1 - fifo_iir_data;
  iVar1 = -iVar2;
  if (-1 < iVar2) {
    iVar1 = iVar2;
  }
  if (0xdac < iVar1) {
    jump_delta = (jump_delta + param_1) - fifo_iir_data;
  }
  iir_data[4] = (param_1 - jump_delta) - iir_data[1];
  iir_data[1] = iir_data[0];
  iir_data[0] = param_1 - jump_delta;
  iir_data[4] =
       iir_data[4] - ((iir_data[2] * -0x77 + iir_data[3] * 0x38) >> 6);
  iir_data[3] = iir_data[2];
  iir_data[2] = iir_data[4];
  iir_data[9] = iir_data[4] - iir_data[6];
  iir_data[6] = iir_data[5];
  iir_data[5] = iir_data[4];
  iir_data[9] =
       iir_data[9] - ((iir_data[7] * -0x54 + iir_data[8] * 0x23) >> 6);
  iir_data[8] = iir_data[7];
  iir_data[7] = iir_data[9];
  fifo_iir_data = param_1;
  return iir_data[9];
}

/* ==== data_bpmlpf_pro @ 0xffffff80085f55f4 ==== */
s32 data_bpmlpf_pro(u32 param_1)

{
  s32 iStack_c;
  s32 iStack_8;
  u8 bStack_1;
  
  if (all_code_count < 2) {
    for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
      *(u32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) = param_1;
      *(u32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) = param_1;
    }
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)red_flt_input + (s64)(s32)(bStack_1 + 1) * 4);
    *(u32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)red_flt_output + (s64)(s32)(bStack_1 + 1) * 4);
  }
  red_flt_input[4] = param_1;
  iStack_8 = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    iStack_8 = iStack_8 +
               *(s32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&b_bpmlpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  iStack_c = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    iStack_c = iStack_c +
               *(s32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&a_bpmlpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  red_flt_output[4] = (iStack_8 >> 10) - (iStack_c >> 10);
  return red_flt_output[4];
}

/* ==== data_hrvlpf_pro @ 0xffffff80085f57ec ==== */
s32 data_hrvlpf_pro(u32 param_1)

{
  s32 iStack_c;
  s32 iStack_8;
  u8 bStack_1;
  
  if (all_code_count < 2) {
    for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
      *(u32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) = param_1;
      *(u32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) = param_1;
    }
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)red_flt_input + (s64)(s32)(bStack_1 + 1) * 4);
    *(u32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)red_flt_output + (s64)(s32)(bStack_1 + 1) * 4);
  }
  red_flt_input[4] = param_1;
  iStack_8 = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    iStack_8 = iStack_8 +
               *(s32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&b_hrvlpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  iStack_c = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    iStack_c = iStack_c +
               *(s32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&a_hrvlpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  red_flt_output[4] = (iStack_8 >> 10) - (iStack_c >> 10);
  return red_flt_output[4];
}

/* ==== datared_lpf_pro @ 0xffffff80085f59e4 ==== */
s32 datared_lpf_pro(u32 param_1)

{
  s32 iStack_c;
  s32 iStack_8;
  u8 bStack_1;
  
  if (all_code_count == 1) {
    for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
      *(u32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) = param_1;
      *(u32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) = param_1;
    }
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)red_flt_input + (s64)(s32)(bStack_1 + 1) * 4);
    *(u32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)red_flt_output + (s64)(s32)(bStack_1 + 1) * 4);
  }
  red_flt_input[4] = param_1;
  iStack_8 = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    iStack_8 = iStack_8 +
               *(s32 *)((char *)red_flt_input + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&b_lpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  iStack_c = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    iStack_c = iStack_c +
               *(s32 *)((char *)red_flt_output + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&a_lpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  red_flt_output[4] = (iStack_8 >> 10) - (iStack_c >> 10);
  return red_flt_output[4];
}

/* ==== datared_hpf_pro @ 0xffffff80085f5bdc ==== */
s32 datared_hpf_pro(u32 param_1)

{
  s32 iStack_c;
  s32 iStack_8;
  u8 bStack_1;
  
  if (all_code_count == 1) {
    for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
      *(u32 *)((char *)red_hflt_input + (s64)(s32)(u32)bStack_1 * 4) = param_1;
      *(u32 *)((char *)red_hflt_output + (s64)(s32)(u32)bStack_1 * 4) = 0;
    }
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)red_hflt_input + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)red_hflt_input + (s64)(s32)(bStack_1 + 1) * 4);
    *(u32 *)((char *)red_hflt_output + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)red_hflt_output + (s64)(s32)(bStack_1 + 1) * 4);
  }
  red_hflt_input[4] = param_1;
  iStack_8 = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    iStack_8 = iStack_8 +
               *(s32 *)((char *)red_hflt_input + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&b_hpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  iStack_c = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    iStack_c = iStack_c +
               *(s32 *)((char *)red_hflt_output + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&a_hpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  red_hflt_output[4] = (iStack_8 >> 10) - (iStack_c >> 10);
  return red_hflt_output[4];
}

/* ==== datair_lpf_pro @ 0xffffff80085f5dd0 ==== */
s32 datair_lpf_pro(u32 param_1)

{
  s32 iStack_c;
  s32 iStack_8;
  u8 bStack_1;
  
  if (all_code_count == 1) {
    for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
      *(u32 *)((char *)ir_flt_input + (s64)(s32)(u32)bStack_1 * 4) = param_1;
      *(u32 *)((char *)ir_flt_output + (s64)(s32)(u32)bStack_1 * 4) = param_1;
    }
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)ir_flt_input + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)ir_flt_input + (s64)(s32)(bStack_1 + 1) * 4);
    *(u32 *)((char *)ir_flt_output + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)ir_flt_output + (s64)(s32)(bStack_1 + 1) * 4);
  }
  ir_flt_input[4] = param_1;
  iStack_8 = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    iStack_8 = iStack_8 +
               *(s32 *)((char *)ir_flt_input + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&b_lpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  iStack_c = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    iStack_c = iStack_c +
               *(s32 *)((char *)ir_flt_output + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&a_lpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  ir_flt_output[4] = (iStack_8 >> 10) - (iStack_c >> 10);
  return ir_flt_output[4];
}

/* ==== datair_hpf_pro @ 0xffffff80085f5fc8 ==== */
s32 datair_hpf_pro(u32 param_1)

{
  s32 iStack_c;
  s32 iStack_8;
  u8 bStack_1;
  
  if (all_code_count == 1) {
    for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
      *(u32 *)((char *)ir_hflt_input + (s64)(s32)(u32)bStack_1 * 4) = param_1;
      *(u32 *)((char *)ir_hflt_output + (s64)(s32)(u32)bStack_1 * 4) = 0;
    }
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)ir_hflt_input + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)ir_hflt_input + (s64)(s32)(bStack_1 + 1) * 4);
    *(u32 *)((char *)ir_hflt_output + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)ir_hflt_output + (s64)(s32)(bStack_1 + 1) * 4);
  }
  ir_hflt_input[4] = param_1;
  iStack_8 = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    iStack_8 = iStack_8 +
               *(s32 *)((char *)ir_hflt_input + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&b_hpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  iStack_c = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    iStack_c = iStack_c +
               *(s32 *)((char *)ir_hflt_output + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&a_hpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  ir_hflt_output[4] = (iStack_8 >> 10) - (iStack_c >> 10);
  return ir_hflt_output[4];
}

/* ==== datagreen_lpf_pro @ 0xffffff80085f61bc ==== */
s32 datagreen_lpf_pro(u32 param_1)

{
  s32 iStack_c;
  s32 iStack_8;
  u8 bStack_1;
  
  if (all_code_count == 1) {
    for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
      *(u32 *)((char *)green_flt_input + (s64)(s32)(u32)bStack_1 * 4) = param_1;
      *(u32 *)((char *)green_flt_output + (s64)(s32)(u32)bStack_1 * 4) = param_1;
    }
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)green_flt_input + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)green_flt_input + (s64)(s32)(bStack_1 + 1) * 4);
    *(u32 *)((char *)green_flt_output + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)green_flt_output + (s64)(s32)(bStack_1 + 1) * 4);
  }
  green_flt_input[4] = param_1;
  iStack_8 = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    iStack_8 = iStack_8 +
               *(s32 *)((char *)green_flt_input + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&b_lpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  iStack_c = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    iStack_c = iStack_c +
               *(s32 *)((char *)green_flt_output + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&a_lpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  green_flt_output[4] = (iStack_8 >> 10) - (iStack_c >> 10);
  return green_flt_output[4];
}

/* ==== datagreen_hpf_pro @ 0xffffff80085f63b4 ==== */
s32 datagreen_hpf_pro(u32 param_1)

{
  s32 iStack_c;
  s32 iStack_8;
  u8 bStack_1;
  
  if (all_code_count == 1) {
    for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
      *(u32 *)((char *)green_hflt_input + (s64)(s32)(u32)bStack_1 * 4) = param_1;
      *(u32 *)((char *)green_hflt_output + (s64)(s32)(u32)bStack_1 * 4) = 0;
    }
  }
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    *(u32 *)((char *)green_hflt_input + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)green_hflt_input + (s64)(s32)(bStack_1 + 1) * 4);
    *(u32 *)((char *)green_hflt_output + (s64)(s32)(u32)bStack_1 * 4) =
         *(u32 *)((char *)green_hflt_output + (s64)(s32)(bStack_1 + 1) * 4);
  }
  green_hflt_input[4] = param_1;
  iStack_8 = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    iStack_8 = iStack_8 +
               *(s32 *)((char *)green_hflt_input + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&b_hpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  iStack_c = 0;
  for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
    iStack_c = iStack_c +
               *(s32 *)((char *)green_hflt_output + (s64)(s32)(u32)bStack_1 * 4) *
               (s32)*(s16 *)(&a_hpfref + (s64)(s32)(u32)bStack_1 * 2);
  }
  green_hflt_output[4] = (iStack_8 >> 10) - (iStack_c >> 10);
  return green_hflt_output[4];
}

/* ==== BD18XX_Wear_pro @ 0xffffff80085f65a8 ==== */
u64 BD18XX_Wear_pro(u16 param_1,void *param_2)

{
  u32 uStack_c;
  u32 uStack_8;
  u16 uStack_2;
  
  uStack_8 = 0;
  uStack_c = 0;
  for (uStack_2 = 0; uStack_2 < param_1; uStack_2 = uStack_2 + 1) {
    uStack_8 = uStack_8 + *(s32 *)(param_2 + (u64)uStack_2 * 4);
    uStack_c = uStack_c + *(s32 *)((char *)RAW_B_data + (s64)(s32)(u32)uStack_2 * 4);
  }
  if (param_1 == 0) {
    DC_voltage = 0;
    DC_voltage1 = 0;
  }
  else {
    DC_voltage = 0;
    if (param_1 != 0) {
      DC_voltage = uStack_8 / param_1;
    }
    DC_voltage1 = 0;
    if (param_1 != 0) {
      DC_voltage1 = uStack_c / param_1;
    }
  }
  return 1;
}

/* ==== in_BPM_adcdata @ 0xffffff80085f6684 ==== */
u64 in_BPM_adcdata(u32 param_1,u16 param_2,s32 *param_3,void *param_4)

{
  s32 iVar1;
  s32 iVar2;
  u32 uVar3;
  s32 iStack_10;
  u32 uStack_c;
  u32 uStack_8;
  u16 uStack_4;
  u16 uStack_2;
  
  uStack_8 = 0;
  uStack_c = 0;
  for (uStack_2 = 0; uStack_2 < param_2; uStack_2 = uStack_2 + 1) {
    uStack_8 = uStack_8 + param_3[uStack_2];
    uStack_c = uStack_c + *(s32 *)((char *)RAW_B_data + (s64)(s32)(u32)uStack_2 * 4);
  }
  if (param_2 == 0) {
    DC_voltage = 0;
    DC_voltage1 = 0;
  }
  else {
    DC_voltage = 0;
    if (param_2 != 0) {
      DC_voltage = uStack_8 / param_2;
    }
    DC_voltage1 = 0;
    if (param_2 != 0) {
      DC_voltage1 = uStack_c / param_2;
    }
  }
  if (all_code_count < 3) {
    if (all_code_count < 2) {
      iVar1 = *param_3;
      first_data = iVar1;
      second_data = iVar1;
      now_data = iVar1;
      for (uStack_4 = 0; uStack_4 < 0x32; uStack_4 = uStack_4 + 1) {
        data_bpmiir_pro(param_1,iVar1);
      }
    }
    for (uStack_4 = 0; iVar1 = second_data, uStack_4 < param_2; uStack_4 = uStack_4 + 1) {
      first_data = second_data;
      second_data = now_data;
      iVar2 = param_3[uStack_4];
      if ((iVar1 >> 8 < iVar1 - now_data) && (iVar2 >> 8 < iVar2 - now_data)
         ) {
        iStack_10 = (iVar2 >> 1) + (iVar1 >> 1);
      }
      else {
        iStack_10 = now_data;
      }
      now_data = iVar2;
      uVar3 = data_bpmiir_pro(param_1,iStack_10);
      *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar3;
      *(s32 *)((char *)in_testbuf1 + (s64)(s32)(u32)uStack_4 * 4) = iStack_10;
      *(u32 *)((char *)in_testbuf2 + (s64)(s32)(u32)uStack_4 * 4) =
           *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
    }
    if (all_code_count == 2) {
      for (uStack_2 = 0; uStack_2 < 0x100; uStack_2 = uStack_2 + 1) {
        *(u32 *)(param_4 + (u64)uStack_2 * 4) =
             *(u32 *)((char *)red_flt_buf + (s64)(s32)(param_2 - 1) * 4);
      }
    }
  }
  else {
    for (uStack_2 = 0; (s32)(u32)uStack_2 < (s32)(0x100 - (u32)param_2); uStack_2 = uStack_2 + 1)
    {
      *(u32 *)(param_4 + (u64)uStack_2 * 4) =
           *(u32 *)(param_4 + (s64)(s32)((u32)uStack_2 + (u32)param_2) * 4);
    }
    for (uStack_4 = 0; iVar1 = second_data, uStack_4 < param_2; uStack_4 = uStack_4 + 1) {
      first_data = second_data;
      second_data = now_data;
      iVar2 = param_3[uStack_4];
      if ((iVar1 >> 8 < iVar1 - now_data) && (iVar2 >> 8 < iVar2 - now_data)
         ) {
        iStack_10 = (iVar2 >> 1) + (iVar1 >> 1);
      }
      else {
        iStack_10 = now_data;
      }
      now_data = iVar2;
      uVar3 = data_bpmiir_pro(param_1,iStack_10);
      *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar3;
      *(s32 *)((char *)in_testbuf1 + (s64)(s32)(u32)uStack_4 * 4) = iStack_10;
      *(u32 *)((char *)in_testbuf2 + (s64)(s32)(u32)uStack_4 * 4) =
           *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
      *(u32 *)(param_4 + (s64)(s32)((0x100 - (u32)param_2) + (u32)uStack_4) * 4) =
           *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
    }
  }
  return 1;
}

/* ==== in_spo2test_adcdata @ 0xffffff80085f6c34 ==== */
u64
in_spo2test_adcdata(u16 param_1,u32 *param_2,s64 param_3,u32 *param_4,s64 param_5
                   ,u32 *param_6,s64 param_7)

{
  s32 iVar1;
  u32 uVar2;
  s32 iVar3;
  u32 uVar4;
  u32 uVar5;
  u32 uStack_10;
  u32 uStack_c;
  u32 uStack_8;
  u16 uStack_4;
  u16 uStack_2;
  
  uStack_8 = 0;
  uStack_c = 0;
  uStack_10 = 0;
  for (uStack_2 = 0; uStack_2 < param_1; uStack_2 = uStack_2 + 1) {
    uStack_8 = uStack_8 + param_2[uStack_2];
    uStack_c = uStack_c + param_4[uStack_2];
    uStack_10 = uStack_10 + param_6[uStack_2];
  }
  if (all_code_count < 2) {
    if (all_code_count == 1) {
      uVar4 = *param_2;
      uVar5 = *param_4;
      uVar2 = *param_6;
      for (uStack_2 = 0; uStack_2 < 5; uStack_2 = uStack_2 + 1) {
        *(u32 *)((char *)green_flt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)green_flt_output + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)green_hflt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)green_hflt_output + (s64)(s32)(u32)uStack_2 * 4) = 0;
        *(u32 *)((char *)red_flt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar5;
        *(u32 *)((char *)red_flt_output + (s64)(s32)(u32)uStack_2 * 4) = uVar5;
        *(u32 *)((char *)red_hflt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar5;
        *(u32 *)((char *)red_hflt_output + (s64)(s32)(u32)uStack_2 * 4) = 0;
        *(u32 *)((char *)ir_flt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar2;
        *(u32 *)((char *)ir_flt_output + (s64)(s32)(u32)uStack_2 * 4) = uVar2;
        *(u32 *)((char *)ir_hflt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar2;
        *(u32 *)((char *)ir_hflt_output + (s64)(s32)(u32)uStack_2 * 4) = 0;
      }
      for (uStack_4 = 0; uStack_4 < 0x32; uStack_4 = uStack_4 + 1) {
        data_iir_pro(uVar4);
        data1_iir_pro(uVar5);
        data2_iir_pro(uVar2);
      }
      for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
        uVar4 = datagreen_lpf_pro(param_2[uStack_4]);
        uVar5 = datagreen_hpf_pro(uVar4);
        *(u32 *)((char *)green_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
        uVar5 = data_iir_pro(uVar4);
        *(u32 *)((char *)green_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
        *(u32 *)((char *)green_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
        uVar4 = datared_lpf_pro(param_4[uStack_4]);
        uVar5 = datared_hpf_pro(uVar4);
        *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
        uVar5 = data1_iir_pro(uVar4);
        *(u32 *)((char *)red_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
        *(u32 *)((char *)red_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
        uVar4 = datair_lpf_pro(param_6[uStack_4]);
        uVar5 = datair_hpf_pro(uVar4);
        *(u32 *)((char *)ir_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
        uVar5 = data2_iir_pro(uVar4);
        *(u32 *)((char *)ir_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
        *(u32 *)((char *)ir_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
      }
    }
  }
  else {
    green_ac_power = 0;
    red_ac_power = 0;
    ir_ac_power = 0;
    for (uStack_2 = 0; (s32)(u32)uStack_2 < (s32)(0x100 - (u32)param_1); uStack_2 = uStack_2 + 1)
    {
      *(u32 *)(param_3 + (u64)uStack_2 * 4) =
           *(u32 *)(param_3 + (s64)(s32)((u32)uStack_2 + (u32)param_1) * 4);
      iVar3 = *(s32 *)(param_3 + (u64)uStack_2 * 4);
      iVar1 = -iVar3;
      if (-1 < iVar3) {
        iVar1 = iVar3;
      }
      green_ac_power = iVar1 + green_ac_power;
      *(u32 *)(param_5 + (u64)uStack_2 * 4) =
           *(u32 *)(param_5 + (s64)(s32)((u32)uStack_2 + (u32)param_1) * 4);
      iVar3 = *(s32 *)(param_5 + (u64)uStack_2 * 4);
      iVar1 = -iVar3;
      if (-1 < iVar3) {
        iVar1 = iVar3;
      }
      red_ac_power = iVar1 + red_ac_power;
      *(u32 *)(param_7 + (u64)uStack_2 * 4) =
           *(u32 *)(param_7 + (s64)(s32)((u32)uStack_2 + (u32)param_1) * 4);
      iVar3 = *(s32 *)(param_7 + (u64)uStack_2 * 4);
      iVar1 = -iVar3;
      if (-1 < iVar3) {
        iVar1 = iVar3;
      }
      ir_ac_power = iVar1 + ir_ac_power;
    }
    for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
      uVar4 = datagreen_lpf_pro(param_2[uStack_4]);
      uVar5 = datagreen_hpf_pro(uVar4);
      *(u32 *)((char *)green_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
      uVar5 = data_iir_pro(uVar4);
      *(u32 *)((char *)green_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
      *(u32 *)((char *)green_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
      uVar4 = datared_lpf_pro(param_4[uStack_4]);
      uVar5 = datared_hpf_pro(uVar4);
      *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
      uVar5 = data1_iir_pro(uVar4);
      *(u32 *)((char *)red_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
      *(u32 *)((char *)red_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
      uVar4 = datair_lpf_pro(param_6[uStack_4]);
      uVar5 = datair_hpf_pro(uVar4);
      *(u32 *)((char *)ir_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
      uVar5 = data2_iir_pro(uVar4);
      *(u32 *)((char *)ir_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar5;
      *(u32 *)((char *)ir_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
      *(u32 *)(param_3 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4) =
           *(u32 *)((char *)green_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
      iVar3 = *(s32 *)(param_3 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4);
      iVar1 = -iVar3;
      if (-1 < iVar3) {
        iVar1 = iVar3;
      }
      green_ac_power = iVar1 + green_ac_power;
      *(u32 *)(param_5 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4) =
           *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
      iVar3 = *(s32 *)(param_5 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4);
      iVar1 = -iVar3;
      if (-1 < iVar3) {
        iVar1 = iVar3;
      }
      red_ac_power = iVar1 + red_ac_power;
      *(u32 *)(param_7 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4) =
           *(u32 *)((char *)ir_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
      iVar3 = *(s32 *)(param_7 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4);
      iVar1 = -iVar3;
      if (-1 < iVar3) {
        iVar1 = iVar3;
      }
      ir_ac_power = iVar1 + ir_ac_power;
    }
  }
  if (param_1 == 0) {
    green_dc_power = 1;
    red_dc_power = 1;
    ir_dc_power = 1;
    green_ac_power = 1;
    red_ac_power = 1;
    ir_ac_power = 1;
  }
  else {
    green_dc_power = 0;
    if (param_1 != 0) {
      green_dc_power = uStack_8 / param_1;
    }
    iVar1 = green_ac_power + 0xff;
    if (-1 < green_ac_power) {
      iVar1 = green_ac_power;
    }
    green_ac_power = iVar1 >> 8;
    red_dc_power = 0;
    if (param_1 != 0) {
      red_dc_power = uStack_c / param_1;
    }
    iVar1 = red_ac_power + 0xff;
    if (-1 < red_ac_power) {
      iVar1 = red_ac_power;
    }
    red_ac_power = iVar1 >> 8;
    ir_dc_power = 0;
    if (param_1 != 0) {
      ir_dc_power = uStack_10 / param_1;
    }
    iVar1 = ir_ac_power + 0xff;
    if (-1 < ir_ac_power) {
      iVar1 = ir_ac_power;
    }
    ir_ac_power = iVar1 >> 8;
  }
  return 1;
}

/* ==== in_spo2test1_adcdata @ 0xffffff80085f76ac ==== */











u64
in_spo2test1_adcdata(u16 param_1,void *param_2,void *param_3,void *param_4,void *param_5,void *param_6,void *param_7)

{
  s32 iVar1;
  s32 iVar2;
  u32 uVar3;
  u32 uVar4;
  u32 uStack_10;
  u32 uStack_c;
  u32 uStack_8;
  u16 uStack_4;
  u16 uStack_2;
  
  uStack_8 = 0;
  uStack_c = 0;
  uStack_10 = 0;
  for (uStack_2 = 0; uStack_2 < param_1; uStack_2 = uStack_2 + 1) {
    uStack_8 = uStack_8 + *(s32 *)(param_2 + (u64)uStack_2 * 4);
    uStack_c = uStack_c + *(s32 *)(param_4 + (u64)uStack_2 * 4);
    uStack_10 = uStack_10 + *(s32 *)(param_6 + (u64)uStack_2 * 4);
  }
  if (param_1 == 0) {
    DC_voltage = 0;
    DC_voltage1 = 0;
  }
  else {
    DC_voltage = 0;
    if (param_1 != 0) {
      DC_voltage = uStack_c / param_1;
    }
    DC_voltage1 = 0;
    if (param_1 != 0) {
      DC_voltage1 = uStack_10 / param_1;
    }
  }
  if (all_code_count != 0) {
    green_ac_power = 0;
    red_ac_power = 0;
    ir_ac_power = 0;
    for (uStack_2 = 0; (s32)(u32)uStack_2 < (s32)(0x100 - (u32)param_1); uStack_2 = uStack_2 + 1)
    {
      *(u32 *)(param_3 + (u64)uStack_2 * 4) =
           *(u32 *)(param_3 + (s64)(s32)((u32)uStack_2 + (u32)param_1) * 4);
      iVar1 = *(s32 *)(param_3 + (u64)uStack_2 * 4);
      iVar2 = -iVar1;
      if (-1 < iVar1) {
        iVar2 = iVar1;
      }
      green_ac_power = iVar2 + green_ac_power;
      *(u32 *)(param_5 + (u64)uStack_2 * 4) =
           *(u32 *)(param_5 + (s64)(s32)((u32)uStack_2 + (u32)param_1) * 4);
      iVar1 = *(s32 *)(param_5 + (u64)uStack_2 * 4);
      iVar2 = -iVar1;
      if (-1 < iVar1) {
        iVar2 = iVar1;
      }
      red_ac_power = iVar2 + red_ac_power;
      *(u32 *)(param_7 + (u64)uStack_2 * 4) =
           *(u32 *)(param_7 + (s64)(s32)((u32)uStack_2 + (u32)param_1) * 4);
      iVar1 = *(s32 *)(param_7 + (u64)uStack_2 * 4);
      iVar2 = -iVar1;
      if (-1 < iVar1) {
        iVar2 = iVar1;
      }
      ir_ac_power = iVar2 + ir_ac_power;
    }
    if ((*(s32 *)(param_2 + 0x58) == 1) && (ch_A_expot < 0x78)) {
      uVar4 = *(u32 *)(param_2 + (u64)param_1 * 4 + -4);
      for (uStack_2 = 0; uStack_2 < 5; uStack_2 = uStack_2 + 1) {
        *(u32 *)((char *)green_flt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)green_flt_output + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)green_hflt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)green_hflt_output + (s64)(s32)(u32)uStack_2 * 4) = 0;
      }
      for (uStack_4 = 0; uStack_4 < 0x32; uStack_4 = uStack_4 + 1) {
        data_iir_pro(uVar4);
      }
      for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
        *(u32 *)((char *)green_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = 0;
        *(u32 *)((char *)green_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = 0;
      }
    }
    else {
      for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
        uVar4 = *(u32 *)(param_2 + (u64)uStack_4 * 4);
        iVar2 = datagreen_lpf_pro(uVar4);
        uVar3 = datagreen_hpf_pro(uVar4);
        *(u32 *)((char *)green_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar3;
        uVar4 = data_iir_pro(uVar4);
        *(u32 *)((char *)green_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
        *(s32 *)((char *)green_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = iVar2 + -200000;
      }
    }
    if (*(s32 *)(param_4 + 0x58) == 1) {
      uVar4 = *(u32 *)(param_4 + (u64)param_1 * 4 + -4);
      for (uStack_2 = 0; uStack_2 < 5; uStack_2 = uStack_2 + 1) {
        *(u32 *)((char *)red_flt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)red_flt_output + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)red_hflt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)red_hflt_output + (s64)(s32)(u32)uStack_2 * 4) = 0;
      }
      for (uStack_4 = 0; uStack_4 < 0x32; uStack_4 = uStack_4 + 1) {
        data1_iir_pro(uVar4);
      }
      for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
        *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = 0;
        *(u32 *)((char *)red_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = 0;
      }
    }
    else {
      for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
        uVar4 = *(u32 *)(param_4 + (u64)uStack_4 * 4);
        iVar2 = datared_lpf_pro(uVar4);
        uVar3 = datared_hpf_pro(uVar4);
        *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar3;
        uVar4 = data1_iir_pro(uVar4);
        *(u32 *)((char *)red_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
        *(s32 *)((char *)red_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = iVar2 + -200000;
      }
    }
    if (*(s32 *)(param_6 + 0x58) == 1) {
      uVar4 = *(u32 *)(param_6 + (u64)param_1 * 4 + -4);
      for (uStack_2 = 0; uStack_2 < 5; uStack_2 = uStack_2 + 1) {
        *(u32 *)((char *)ir_flt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)ir_flt_output + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)ir_hflt_input + (s64)(s32)(u32)uStack_2 * 4) = uVar4;
        *(u32 *)((char *)ir_hflt_output + (s64)(s32)(u32)uStack_2 * 4) = 0;
      }
      for (uStack_4 = 0; uStack_4 < 0x32; uStack_4 = uStack_4 + 1) {
        data2_iir_pro(uVar4);
      }
      for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
        *(u32 *)((char *)ir_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = 0;
        *(u32 *)((char *)ir_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = 0;
      }
    }
    else {
      for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
        uVar4 = *(u32 *)(param_6 + (u64)uStack_4 * 4);
        iVar2 = datair_lpf_pro(uVar4);
        uVar3 = datair_hpf_pro(uVar4);
        *(u32 *)((char *)ir_flt_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar3;
        uVar4 = data2_iir_pro(uVar4);
        *(u32 *)((char *)ir_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = uVar4;
        *(s32 *)((char *)ir_flt2_buf + (s64)(s32)(u32)uStack_4 * 4) = iVar2 + -200000;
      }
    }
    for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
      *(u32 *)(param_3 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4) =
           *(u32 *)((char *)green_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
      iVar1 = *(s32 *)(param_3 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4);
      iVar2 = -iVar1;
      if (-1 < iVar1) {
        iVar2 = iVar1;
      }
      green_ac_power = iVar2 + green_ac_power;
      *(u32 *)(param_5 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4) =
           *(u32 *)((char *)red_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
      iVar1 = *(s32 *)(param_5 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4);
      iVar2 = -iVar1;
      if (-1 < iVar1) {
        iVar2 = iVar1;
      }
      red_ac_power = iVar2 + red_ac_power;
      *(u32 *)(param_7 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4) =
           *(u32 *)((char *)ir_flt_buf + (s64)(s32)(u32)uStack_4 * 4);
      iVar1 = *(s32 *)(param_7 + (s64)(s32)((0x100 - (u32)param_1) + (u32)uStack_4) * 4);
      iVar2 = -iVar1;
      if (-1 < iVar1) {
        iVar2 = iVar1;
      }
      ir_ac_power = iVar2 + ir_ac_power;
    }
  }
  if (param_1 == 0) {
    green_dc_power = 1;
    red_dc_power = 1;
    ir_dc_power = 1;
    green_ac_power = 1;
    red_ac_power = 1;
    ir_ac_power = 1;
  }
  else {
    green_dc_power = 0;
    if (param_1 != 0) {
      green_dc_power = uStack_8 / param_1;
    }
    iVar2 = green_ac_power + 0xff;
    if (-1 < green_ac_power) {
      iVar2 = green_ac_power;
    }
    green_ac_power = iVar2 >> 8;
    red_dc_power = 0;
    if (param_1 != 0) {
      red_dc_power = uStack_c / param_1;
    }
    iVar2 = red_ac_power + 0xff;
    if (-1 < red_ac_power) {
      iVar2 = red_ac_power;
    }
    red_ac_power = iVar2 >> 8;
    ir_dc_power = 0;
    if (param_1 != 0) {
      ir_dc_power = uStack_10 / param_1;
    }
    iVar2 = ir_ac_power + 0xff;
    if (-1 < ir_ac_power) {
      iVar2 = ir_ac_power;
    }
    ir_ac_power = iVar2 >> 8;
  }
  return 1;
}

/* ==== in_HRV_adcdata @ 0xffffff80085f855c ==== */
u64 in_HRV_adcdata(u16 param_1,u32 *param_2,void *param_3)

{
  s32 iVar1;
  u32 uVar2;
  s32 iVar3;
  s32 iStack_c;
  u32 uStack_8;
  u16 uStack_4;
  u16 uStack_2;
  
  uStack_8 = 0;
  for (uStack_2 = 0; uStack_2 < param_1; uStack_2 = uStack_2 + 1) {
    uStack_8 = uStack_8 + param_2[uStack_2];
  }
  DC_voltage = 0;
  if (param_1 != 0) {
    DC_voltage = uStack_8 / param_1;
  }
  hrv_data_buf[0] = (u32)(param_1 >> 1);
  if (all_code_count < 4) {
    if (all_code_count < 2) {
      iVar3 = *param_2;
      first_data = iVar3;
      second_data = iVar3;
      now_data = iVar3;
      for (uStack_4 = 0; uStack_4 < 0x32; uStack_4 = uStack_4 + 1) {
        uVar2 = data_hrvlpf_pro(iVar3);
        data_hrviir_pro(uVar2);
      }
    }
    for (uStack_4 = 0; iVar3 = second_data, uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
      first_data = second_data;
      second_data = now_data;
      iVar1 = param_2[uStack_4];
      if ((iVar3 >> 8 < iVar3 - now_data) && (iVar1 >> 8 < iVar1 - now_data)
         ) {
        iStack_c = (iVar1 >> 1) + (iVar3 >> 1);
      }
      else {
        iStack_c = now_data;
      }
      now_data = iVar1;
      uVar2 = data_hrvlpf_pro(iStack_c);
      iVar3 = data_hrviir_pro(uVar2);
      if ((uStack_4 & 1) != 0) {
        *(s32 *)((char *)hrv_data_buf + (s64)(s32)((uStack_4 >> 1) + 1) * 4) = iVar3 >> 2;
      }
    }
  }
  else {
    if ((unormal_flag == '\0') || (3 < unormal_count1)) {
      for (uStack_2 = 0; (s32)(u32)uStack_2 < (s32)(200 - (u32)param_1); uStack_2 = uStack_2 + 1)
      {
        *(u32 *)(param_3 + (u64)uStack_2 * 4) =
             *(u32 *)(param_3 + (s64)(s32)((u32)uStack_2 + (u32)param_1) * 4);
      }
    }
    for (uStack_4 = 0; iVar3 = second_data, uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
      first_data = second_data;
      second_data = now_data;
      iVar1 = param_2[uStack_4];
      if ((iVar3 >> 8 < iVar3 - now_data) && (iVar1 >> 8 < iVar1 - now_data)
         ) {
        iStack_c = (iVar1 >> 1) + (iVar3 >> 1);
      }
      else {
        iStack_c = now_data;
      }
      now_data = iVar1;
      uVar2 = data_hrvlpf_pro(iStack_c);
      iVar3 = data_hrviir_pro(uVar2);
      *(s32 *)(param_3 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4) = iVar3 >> 2;
      if ((uStack_4 & 1) != 0) {
        *(s32 *)((char *)hrv_data_buf + (s64)(s32)((uStack_4 >> 1) + 1) * 4) = iVar3 >> 2;
      }
    }
  }
  return 1;
}

/* ==== Wear_SPO2_judge @ 0xffffff80085f8a5c ==== */
void Wear_SPO2_judge(void)

{
  if (Product_old_status == 2) {
    if ((DC_voltage1 < wear_ref1) ||
       (red_dc_power < wear_ref2)) {
      if ((DC_voltage1 < wear_ref1) ||
         (red_dc_power < wear_ref2)) {
        status_count0 = status_count0 + 3;
      }
      else {
        status_count0 = status_count0 + 1;
      }
      if (2 < status_count0) {
        Product_old_status = 1;
        status_count0 = 0;
      }
    }
    else if (status_count0 != 0) {
      status_count0 = status_count0 - 1;
    }
  }
  else if (Product_old_status == 1) {
    if (DC_voltage1 < wear_ref1) {
      if (status_count1 != 0) {
        status_count1 = status_count1 - 1;
      }
    }
    else {
      if (DC_voltage1 < wear_ref1) {
        status_count1 = status_count1 + 1;
      }
      else {
        status_count1 = status_count1 + 3;
      }
      if (2 < status_count1) {
        Product_old_status = 2;
        status_count1 = 0;
        all_code_count = 0;
      }
    }
  }
  return;
}

/* ==== Wear_Hrv_judge @ 0xffffff80085f8d54 ==== */
void Wear_Hrv_judge(void)

{
  u32 uVar1;
  u32 uStack_4;
  
  uVar1 = 0;
  if (DC_refless_data1 + 1 != 0) {
    uVar1 = (bd44_guard * 10) / (DC_refless_data1 + 1);
  }
  if (DC_refless_data1 < bd44_guard) {
    uStack_4 = bd44_guard - DC_refless_data1;
  }
  else {
    uStack_4 = 0;
  }
  if (Product_old_status == 2) {
    if ((((wear_ref1 < uVar1) || (300 < DC_refless_data1)) &&
        (wear_ref2 < uStack_4)) &&
       (((299 < DC_voltage || (1 < green_dc_value)) &&
        ((DC_voltage < 0xf619 || (green_dc_value < 0xfe)))))) {
      if (status_count0 != 0) {
        status_count0 = status_count0 - 1;
      }
    }
    else {
      if ((((wear_ref1 < uVar1) || (300 < DC_refless_data1)) &&
          ((wear_ref2 < uStack_4 &&
           ((0x129 < DC_voltage || (1 < green_dc_value)))))) &&
         ((DC_voltage < 0xf67d || (green_dc_value < 0xfe)))) {
        status_count0 = status_count0 + 1;
      }
      else {
        status_count0 = status_count0 + 3;
      }
      if (5 < status_count0) {
        Product_old_status = 1;
        status_count0 = 0;
      }
    }
  }
  else if (Product_old_status == 1) {
    if ((((wear_ref1 < uVar1) && (DC_refless_data1 < 0x12d)) &&
        (wear_ref2 < uStack_4)) ||
       ((300 < DC_refless_data1 && (wear_ref2 < uStack_4)))) {
      if ((((wear_ref1 < uVar1) && (DC_refless_data1 < 0x12d)) &&
          (wear_ref2 < uStack_4)) ||
         ((300 < DC_refless_data1 && (wear_ref2 < uStack_4)))) {
        status_count1 = status_count1 + 3;
      }
      else {
        status_count1 = status_count1 + 1;
      }
      if (5 < status_count1) {
        Product_old_status = 2;
        status_count1 = 0;
        all_code_count = 0;
      }
    }
    else if (status_count1 != 0) {
      status_count1 = status_count1 - 1;
    }
  }
  return;
}

/* ==== Wear_judge @ 0xffffff80085f9188 ==== */
void Wear_judge(void)

{
  if (Product_old_status == 2) {
    if ((DC_voltage1 < wear_ref1) ||
       ((ch_A_expot < wear_ref3 &&
        (wear_ref2 < DC_voltage)))) {
      if ((DC_voltage1 < wear_ref1) ||
         ((ch_A_expot < wear_ref3 &&
          (wear_ref2 < DC_voltage)))) {
        status_count0 = status_count0 + 3;
      }
      else {
        status_count0 = status_count0 + 1;
      }
      if (2 < status_count0) {
        Product_old_status = 1;
        status_count0 = 0;
      }
    }
    else if (status_count0 != 0) {
      status_count0 = status_count0 - 1;
    }
  }
  else if (Product_old_status == 1) {
    if (DC_voltage1 < wear_ref1) {
      if (status_count1 != 0) {
        status_count1 = status_count1 - 1;
      }
    }
    else {
      if (DC_voltage1 < wear_ref1) {
        status_count1 = status_count1 + 1;
      }
      else {
        status_count1 = status_count1 + 3;
      }
      if (2 < status_count1) {
        Product_old_status = 2;
        status_count1 = 0;
        all_code_count = 0;
      }
    }
  }
  return;
}

/* ==== Product_state_judge2 @ 0xffffff80085f94b4 ==== */
void Product_state_judge2(void *param_1)

{
  s32 iVar1;
  s32 iVar2;
  u32 uVar3;
  s16 asStack_28[12];
  s32 iStack_10;
  u16 uStack_c;
  u16 uStack_a;
  u16 uStack_8;
  s16 sStack_6;
  u8 bStack_4;
  u8 bStack_3;
  u8 bStack_2;
  u8 bStack_1;
  
  asStack_28[0] = 0;
  asStack_28[1] = 0;
  asStack_28[2] = 0;
  asStack_28[3] = 0;
  for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
    *(u16 *)((char *)mid_heart_sbp + (s64)(s32)(u32)bStack_1 * 2) =
         *(u16 *)((char *)mid_heart_sbp + (s64)(s32)(bStack_1 + 1) * 2);
  }
  bStack_1 = 0;
  sStack_6 = 0;
  do {
    if (3 < bStack_1) {
LAB_ffffff80085f95cc:
      if (sStack_6 == 0) {
        sStack_6 = 0x4e;
      }
      (*(u16 *)((s8 *)mid_heart_sbp + 10)) = sStack_6;
      uStack_8 = mid_heart_sbp[0];
      uStack_a = mid_heart_sbp[0];
      bStack_3 = 0;
      bStack_4 = 0;
      for (bStack_1 = 0; bStack_1 < 6; bStack_1 = bStack_1 + 1) {
        if (uStack_a < *(u16 *)((char *)mid_heart_sbp + (s64)(s32)(u32)bStack_1 * 2)) {
          uStack_a = *(u16 *)((char *)mid_heart_sbp + (s64)(s32)(u32)bStack_1 * 2);
          bStack_3 = bStack_1;
        }
        if (*(u16 *)((char *)mid_heart_sbp + (s64)(s32)(u32)bStack_1 * 2) < uStack_8) {
          uStack_8 = *(u16 *)((char *)mid_heart_sbp + (s64)(s32)(u32)bStack_1 * 2);
          bStack_4 = bStack_1;
        }
      }
      iVar2 = (u32)uStack_a - (u32)uStack_8;
      iVar1 = -iVar2;
      if (-1 < iVar2) {
        iVar1 = iVar2;
      }
      bp_varrange[0] = (u16)iVar1;
      bStack_2 = 0;
      for (bStack_1 = 0; bStack_1 < 6; bStack_1 = bStack_1 + 1) {
        if ((bStack_1 != bStack_4) && (bStack_1 != bStack_3)) {
          asStack_28[(s32)(u32)bStack_2] =
               *(s16 *)((char *)mid_heart_sbp + (s64)(s32)(u32)bStack_1 * 2);
          bStack_2 = bStack_2 + 1;
        }
      }
      uStack_c = 0;
      for (bStack_1 = 0; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
        uStack_c = asStack_28[(s32)(u32)bStack_1] + uStack_c;
      }
      heart_sbp_ave = uStack_c >> 2;
      real_freq_data = sStack_6;
      iStack_10 = 0;
      for (bStack_1 = 0; uVar3 = state_judge_power[1], bStack_1 < 4; bStack_1 = bStack_1 + 1) {
        *(u32 *)((char *)freq_maxpower + (s64)(s32)(u32)bStack_1 * 4) =
             *(u32 *)((char *)freq_maxpower + (s64)(s32)(bStack_1 + 1) * 4);
        iStack_10 = iStack_10 + *(s32 *)((char *)freq_maxpower + (s64)(s32)(u32)bStack_1 * 4);
      }
      freq_maxpower[4] = *(u32 *)(param_1 + 0x10) >> 3;
      freq_powerave = (freq_maxpower[4] + iStack_10) >> 4;
      state_judge_power[0] = state_judge_power[1];
      state_judge_power[1] = freq_powerave;
      if ((uVar3 + (uVar3 >> 3) < freq_powerave) && (200 < freq_powerave)) {
        if (uVar3 + (uVar3 >> 2) < freq_powerave) {
          if (state_judge_count < 0x10) {
            state_judge_count = state_judge_count + 2;
          }
          else {
            state_judge_count = 0x10;
          }
        }
        else if (state_judge_count < 0x10) {
          state_judge_count = state_judge_count + 1;
        }
        else {
          state_judge_count = 0x10;
        }
        if (0xd < state_judge_count) {
          state_judge_flag = 1;
        }
      }
      else if ((freq_powerave < uVar3 - (uVar3 >> 3)) && (200 < freq_powerave)) {
        if (freq_powerave < uVar3 - (uVar3 >> 2)) {
          if (state_judge_count < 5) {
            state_judge_count = 4;
          }
          else {
            state_judge_count = state_judge_count - 2;
          }
        }
        else if (state_judge_count < 5) {
          state_judge_count = 4;
        }
        else {
          state_judge_count = state_judge_count - 1;
        }
        if (state_judge_count < 7) {
          state_judge_flag = 2;
        }
      }
      else if ((uVar3 < freq_powerave) && (10 < state_judge_count)) {
        state_judge_count = state_judge_count - 1;
      }
      else if ((freq_powerave < uVar3) && (state_judge_count < 10)) {
        state_judge_count = state_judge_count + 1;
      }
      return;
    }
    if ((*(u32 *)(param_1 + (u64)bStack_1 * 4) < 0x73) ||
       ((*(u32 *)(param_1 + (u64)bStack_1 * 4) < 0x9b &&
        (0x7d < *(u32 *)(param_1 + (u64)bStack_1 * 4))))) {
      sStack_6 = (s16)*(u32 *)(param_1 + (u64)bStack_1 * 4);
      goto LAB_ffffff80085f95cc;
    }
    bStack_1 = bStack_1 + 1;
  } while( true );
}

/* ==== Data_sequence @ 0xffffff80085f9bdc ==== */
void Data_sequence(void *param_1,void *param_2)

{
  u16 uVar1;
  s8 acStack_9[7];
  u8 bStack_2;
  u8 bStack_1;
  
  for (bStack_1 = 0; bStack_1 < 3; bStack_1 = bStack_1 + 1) {
    *(u16 *)(param_2 + (u64)bStack_1 * 2) = 0;
  }
  for (bStack_1 = 0; bStack_1 < 3; bStack_1 = bStack_1 + 1) {
    uVar1 = (u16)*(u32 *)(param_1 + (u64)bStack_1 * 4);
    for (bStack_2 = 3; bStack_2 != 0; bStack_2 = bStack_2 - 1) {
      if (*(u16 *)(param_2 + (u64)bStack_2 * 2 + -2) < uVar1) {
        if (bStack_2 < 3) {
          *(u16 *)(param_2 + (u64)bStack_2 * 2) =
               *(u16 *)(param_2 + (u64)bStack_2 * 2 + -2);
          *(u16 *)(param_2 + (u64)bStack_2 * 2 + -2) = uVar1;
          acStack_9[(s64)(s32)(u32)bStack_2 + 1] = acStack_9[(s64)(s32)(bStack_2 - 1) + 1];
          acStack_9[(s64)(s32)(bStack_2 - 1) + 1] = bStack_1 + 1;
        }
        else {
          acStack_9[(s64)(s32)(u32)bStack_1 + 1] = bStack_1 + 1;
          *(u16 *)(param_2 + (u64)bStack_2 * 2 + -2) = uVar1;
        }
      }
    }
  }
  *(u16 *)(param_2 + 6) =
       (u16)(u8)acStack_9[3] +
       (u16)(u8)acStack_9[1] * 100 + (u16)(u8)acStack_9[2] * 10;
  return;
}

/* ==== Gensor_sport_freq @ 0xffffff80085f9dd4 ==== */
u16 Gensor_sport_freq(u32 *param_1)

{
  s32 iVar1;
  s32 iVar2;
  s32 iVar3;
  u16 uStack_2;
  
  if (((s32)(*param_1 + param_1[1] * -2) < -0x10) || (0x10 < (s32)(*param_1 + param_1[1] * -2))) {
    if (((s32)(param_1[1] + *param_1 * -2) < -0x10) || (0x10 < (s32)(param_1[1] + *param_1 * -2))) {
      if (((heart_stable_count < 5) || ((s32)(*param_1 + (u32)heart_olddata_fifo[0] * -2) < -0x10)
          ) || (0x10 < (s32)(*param_1 + (u32)heart_olddata_fifo[0] * -2))) {
        if (((heart_stable_count < 5) ||
            ((s32)(param_1[1] + (u32)heart_olddata_fifo[0] * -2) < -0x10)) ||
           (0x10 < (s32)(param_1[1] + (u32)heart_olddata_fifo[0] * -2))) {
          if (((s32)((param_1[1] * 2 + param_1[1]) - *param_1) < -0x18) ||
             (0x18 < (s32)((param_1[1] * 2 + param_1[1]) - *param_1))) {
            if (((s32)((*param_1 * 2 + *param_1) - param_1[1]) < -0x18) ||
               (0x18 < (s32)((*param_1 * 2 + *param_1) - param_1[1]))) {
              if (gensor_power_new[0] < 0x61a9) {
                if (*param_1 < 0x51) {
                  if ((param_1[1] < 0x51) || (0x9f < param_1[1])) {
                    uStack_2 = (u16)((*param_1 & 0x7fff) << 1);
                  }
                  else {
                    uStack_2 = (u16)param_1[1];
                  }
                }
                else {
                  uStack_2 = (u16)*param_1;
                }
                test_pro_state1 = 7;
              }
              else if (gensor_power_new[0] < 45000) {
                test_pro_state1 = 9;
                if ((((heart_stable_count < 5) || (0x78 < *param_1)) ||
                    ((s32)(*param_1 - (u32)heart_olddata_fifo[0]) < -8)) ||
                   (8 < (s32)(*param_1 - (u32)heart_olddata_fifo[0]))) {
                  if (((heart_stable_count < 5) || (0x78 < param_1[1])) ||
                     (((s32)(param_1[1] - (u32)heart_olddata_fifo[0]) < -8 ||
                      (8 < (s32)(param_1[1] - (u32)heart_olddata_fifo[0]))))) {
                    if (((*param_1 < 0x51) ||
                        ((s32)(((*param_1 >> 1) + *param_1) - param_1[1]) < -0x12)) ||
                       (0x12 < (s32)(((*param_1 >> 1) + *param_1) - param_1[1]))) {
                      if (((param_1[1] < 0x51) ||
                          ((s32)(((param_1[1] >> 1) + param_1[1]) - *param_1) < -0x12)) ||
                         (0x12 < (s32)(((param_1[1] >> 1) + param_1[1]) - *param_1))) {
                        if (((*param_1 < 0x5b) || (199 < *param_1)) ||
                           ((199 < param_1[1] || (param_1[1] < 0x5b)))) {
                          if (*param_1 < param_1[1]) {
                            if ((param_1[1] < 0x65) || (0xd1 < param_1[1])) {
                              uStack_2 = (u16)((*param_1 & 0x7fff) << 1);
                            }
                            else {
                              uStack_2 = (u16)param_1[1];
                            }
                          }
                          else if ((*param_1 < 0x65) || (0xd1 < *param_1)) {
                            uStack_2 = (u16)((param_1[1] & 0x7fff) << 1);
                          }
                          else {
                            uStack_2 = (u16)*param_1;
                          }
                          test_pro_state1 = 0xd;
                        }
                        else {
                          iVar2 = *param_1 - (u32)gensor_freq_data[1];
                          iVar1 = -iVar2;
                          if (-1 < iVar2) {
                            iVar1 = iVar2;
                          }
                          iVar3 = param_1[1] - (u32)gensor_freq_data[1];
                          iVar2 = -iVar3;
                          if (-1 < iVar3) {
                            iVar2 = iVar3;
                          }
                          if (iVar1 < iVar2) {
                            uStack_2 = (u16)*param_1;
                          }
                          else {
                            uStack_2 = (u16)param_1[1];
                          }
                          test_pro_state1 = 0xc;
                        }
                      }
                      else {
                        uStack_2 = (u16)param_1[1];
                        test_pro_state1 = 0xb;
                      }
                    }
                    else {
                      uStack_2 = (u16)*param_1;
                      test_pro_state1 = 10;
                    }
                  }
                  else {
                    uStack_2 = (u16)((param_1[1] & 0x7fff) << 1);
                  }
                }
                else {
                  uStack_2 = (u16)((*param_1 & 0x7fff) << 1);
                }
              }
              else {
                if (*param_1 < 0xa0) {
                  if (((param_1[1] < 0xa0) ||
                      ((s32)(param_1[1] - (u32)gensor_freq_data[1]) < -0x10)) ||
                     (0x10 < (s32)(param_1[1] - (u32)gensor_freq_data[1]))) {
                    uStack_2 = (u16)((*param_1 & 0x7fff) << 1);
                  }
                  else {
                    uStack_2 = (u16)param_1[1];
                  }
                }
                else {
                  uStack_2 = (u16)*param_1;
                }
                test_pro_state1 = 8;
              }
            }
            else {
              uStack_2 = (u16)((*param_1 & 0x7fff) << 1);
              test_pro_state1 = 6;
            }
          }
          else {
            uStack_2 = (u16)((param_1[1] & 0x7fff) << 1);
            test_pro_state1 = 5;
          }
        }
        else {
          uStack_2 = (u16)param_1[1];
          test_pro_state1 = 4;
        }
      }
      else {
        uStack_2 = (u16)*param_1;
        test_pro_state1 = 3;
      }
    }
    else {
      if ((gensor_power_new[0] < 18000) && (0x9f < param_1[1])) {
        uStack_2 = (u16)*param_1;
      }
      else {
        uStack_2 = (u16)param_1[1];
      }
      test_pro_state1 = 2;
    }
  }
  else {
    if ((gensor_power_new[0] < 18000) && (0x9f < *param_1)) {
      uStack_2 = (u16)param_1[1];
    }
    else {
      uStack_2 = (u16)*param_1;
    }
    test_pro_state1 = 1;
  }
  return uStack_2;
}

/* ==== Heart_real_math @ 0xffffff80085fa69c ==== */
void Heart_real_math(u32 *param_1,u32 *param_2)

{
  u32 uVar1;
  u16 uVar2;
  s16 sVar3;
  s16 sVar4;
  u16 uVar5;
  s32 iVar6;
  s32 iVar7;
  u8 bVar8;
  u8 bVar9;
  s32 iVar10;
  u32 uVar11;
  u16 uStack_40;
  u16 uStack_3e;
  u16 uStack_3c;
  s16 sStack_24;
  s16 sStack_20;
  u8 bStack_17;
  u16 uStack_16;
  u16 uStack_12;
  u16 uStack_10;
  u16 uStack_e;
  u16 uStack_c;
  u16 uStack_a;
  u16 uStack_8;
  u16 uStack_6;
  u16 uStack_4;
  
  if (Product_old_status == 1) {
    if (no_touch_count[0]< 8) {
      state_judge_count = 10;
      no_touch_count[0]= no_touch_count[0]+ 1;
      user_count = 0;
      lcd_flag[0] = 0;
      Run_data = 0x4b;
      new_data_flag = 0;
      sta_data_flag = 0;
      math_work_count0 = 0;
      math_work_count1 = 0;
      math_work_count2 = 0;
      run_to_sta_count = 0;
      sport_to_sta_flag = 0;
      no_touch_flag = 1;
      heart_stable_count = 0;
      state_stable_count = 0;
      test_pro_flag = 0;
      test_pro_count = 0;
      test_pro_count1 = 0;
      speed_state_flag = 0;
      gensor_state_count = 0;
      run_init_data = sta_static_data + 10;
      bpm_stable_data = 0;
      bp_gensor_oldflag = 0;
      bpm_run_count = 0;
      bpm_static_count = 0;
      sta_basic_value = sta_static_data;
      Sta_down_count = 0;
      Sta_temp_data = sta_static_data;
      Sta_temp_data1 = sta_static_data;
      return;
    }
    state_judge_count = 10;
    no_touch_count[0]= 8;
    user_count = 0;
    lcd_flag[0] = 0;
    Run_data = 0x4b;
    new_data_flag = 0;
    sta_data_flag = 0;
    math_work_count0 = 0;
    math_work_count1 = 0;
    math_work_count2 = 0;
    run_to_sta_count = 0;
    sport_to_sta_flag = 0;
    no_touch_flag = 1;
    heart_stable_count = 0;
    state_stable_count = 0;
    test_pro_flag = 0;
    test_pro_count = 0;
    test_pro_count1 = 0;
    speed_state_flag = 0;
    gensor_state_count = 0;
    run_init_data = sta_static_data + 10;
    bpm_stable_data = 0;
    bp_gensor_oldflag = 0;
    bpm_run_count = 0;
    bpm_static_count = 0;
    sta_basic_value = sta_static_data;
    Sta_down_count = 0;
    Sta_temp_data = sta_static_data;
    Sta_temp_data1 = sta_static_data;
    return;
  }
  if (Product_old_status == 2) {
    if (5 < no_touch_count[0]) {
      no_touch_flag = 0;
    }
    no_touch_count[0]= 0;
    state_stable_count = 0;
    gensor_state_count = 0;
    bp_gensor_power = 0;
    (*(u16 *)((s8 *)&bp_gensor_power + 2)) = 0;
    bp_gensor_data[0] = 0;
    (*(u16 *)((s8 *)bp_gensor_data + 2)) = 0;
    test_pro_count = 0;
    sport_stable_flag = 0;
    bpm_high_flag = 0;
    bpm_sum_flag = 0;
    bpm_unormal_count = 0;
    test_pro_count1 = 0;
    heart_stable_count = 0;
    if (real_freq_data < 0x32) {
      uStack_8 = 0x32;
    }
    else if (real_freq_data < 0xa6) {
      uStack_8 = real_freq_data;
    }
    else {
      uStack_8 = 0xa5;
    }
    sta_data_buf[0] = sta_data_buf[1];
    uVar11 = *param_2;
    uVar2 = (u16)uVar11;
    bVar8 = false;
    if (sta_data_flag == '\0') {
      if (sta_basic_value < 0x65) {
        iVar6 = param_2[1] - (u32)sta_basic_value;
        iVar10 = -iVar6;
        if (-1 < iVar6) {
          iVar10 = iVar6;
        }
        if ((u16)iVar10 < 6) {
          uStack_12 = (u16)param_2[1];
          bVar8 = true;
        }
        else if (sta_basic_value + 0x28 < (u32)uStack_8) {
          uStack_12 = sta_basic_value;
        }
        else if (uStack_8 + 0x1e < (u32)sta_basic_value) {
          uStack_12 = sta_basic_value;
        }
        else {
          uStack_12 = uStack_8;
          bVar8 = true;
        }
      }
      else if ((sta_basic_value < 0x65) || (0x73 < sta_basic_value)) {
        if (sta_basic_value + 0x2d < (u32)uStack_8) {
          uStack_12 = sta_basic_value;
        }
        else if (uStack_8 + 0x14 < (u32)sta_basic_value) {
          uStack_12 = sta_basic_value;
        }
        else {
          uStack_12 = uStack_8;
          bVar8 = true;
        }
      }
      else if (sta_basic_value + 0x32 < (u32)uStack_8) {
        uStack_12 = sta_basic_value;
      }
      else if (uStack_8 + 0x19 < (u32)sta_basic_value) {
        uStack_12 = sta_basic_value;
      }
      else {
        uStack_12 = uStack_8;
        bVar8 = true;
      }
    }
    else {
      sta_data_count = sta_data_count + '\x01';
      if (sta_data_count != '\0') {
        sta_data_flag = '\0';
      }
      if (Sta_real_value + 0x3c < (u32)uStack_8) {
        uStack_12 = Sta_real_value;
      }
      else if (uStack_8 + 0x3c < (u32)Sta_real_value) {
        uStack_12 = Sta_real_value;
      }
      else {
        uStack_12 = uStack_8;
        bVar8 = true;
      }
    }
    Sta_real_value = uStack_12;
    if (bVar8) {
      if (uStack_12 == sta_basic_value) {
        sta_basic_value = uStack_12;
      }
      else if ((state_judge_flag == '\x01') && (sta_basic_value < 0x65)) {
        if (math_work_count0 < 3) {
          math_work_count0 = math_work_count0 + 1;
        }
        else {
          math_work_count0 = 0;
          state_judge_flag = '\0';
        }
      }
      else {
        math_work_count0 = 0;
        state_judge_flag = '\0';
        if ((sta_basic_value < 0x47) && (uStack_12 < 0x47)) {
          iVar6 = (u32)sta_basic_value - param_2[1];
          iVar10 = -iVar6;
          if (-1 < iVar6) {
            iVar10 = iVar6;
          }
          if (((u16)iVar10 < 0xb) && ((u16)param_2[1] < sta_basic_value)) {
            if (sta_basic_value + 0x28 < (u32)uStack_8) {
              uStack_12 = sta_basic_value;
            }
            else if (uStack_8 + 0x1e < (u32)sta_basic_value) {
              uStack_12 = sta_basic_value;
            }
            else {
              uStack_12 = uStack_8;
            }
            Sta_real_value = uStack_12;
          }
          iVar6 = (u32)sta_basic_value - (u32)Sta_real_value;
          iVar10 = -iVar6;
          if (-1 < iVar6) {
            iVar10 = iVar6;
          }
          if (3 < (u16)iVar10) {
            iVar6 = (u32)sta_basic_value - (u32)Sta_real_value;
            iVar10 = -iVar6;
            if (-1 < iVar6) {
              iVar10 = iVar6;
            }
            sVar3 = 0;
            iVar10 = (s32)(s16)((s16)iVar10 + 1);
            if (iVar10 != 0) {
              sVar3 = (s16)((((s16)Sta_real_value + 1) - (s32)(s16)sta_basic_value)
                             / iVar10);
            }
            sta_basic_value =
                 (s16)((s32)(((u32)sta_basic_value * 0x20 - (u32)sta_basic_value) +
                               (u32)Sta_real_value + 1) >> 5) + sVar3;
          }
        }
        else {
          iVar6 = (u32)uStack_12 - (u32)sta_basic_value;
          iVar10 = -iVar6;
          if (-1 < iVar6) {
            iVar10 = iVar6;
          }
          if ((((u16)iVar10 < 0xb) && (sta_basic_value < 0x65)) ||
             ((uStack_8 == uStack_12 && (100 < sta_basic_value)))) {
            sta_basic_value =
                 (u16)((s32)((u32)sta_basic_value * 2 + (u32)sta_basic_value +
                               (u32)uStack_12) >> 2);
          }
          else {
            iVar6 = (u32)sta_basic_value - (u32)uStack_12;
            iVar10 = -iVar6;
            if (-1 < iVar6) {
              iVar10 = iVar6;
            }
            sVar3 = 0;
            iVar10 = (s32)(s16)((s16)iVar10 + 3);
            if (iVar10 != 0) {
              sVar3 = (s16)((((s16)uStack_12 + 1) - (s32)(s16)sta_basic_value) / iVar10);
            }
            sta_basic_value =
                 (s16)((s32)(((u32)sta_basic_value * 0x10 - (u32)sta_basic_value) +
                               (u32)uStack_12 + 1) >> 4) + sVar3;
          }
        }
      }
      math_work_count1 = 0;
    }
    else if ((0x31 < uStack_8) && (uStack_8 < 0xa6)) {
      if (math_work_count1 < 2) {
        math_work_count1 = math_work_count1 + 1;
      }
      else {
        math_work_count1 = 0;
        iVar6 = (u32)sta_basic_value - (u32)uStack_8;
        iVar10 = -iVar6;
        if (-1 < iVar6) {
          iVar10 = iVar6;
        }
        sVar3 = 0;
        iVar10 = (s32)(s16)((s16)iVar10 + 3);
        if (iVar10 != 0) {
          sVar3 = (s16)((((s16)uStack_8 + 1) - (s32)(s16)sta_basic_value) / iVar10);
        }
        sta_basic_value =
             (s16)((s32)(((u32)sta_basic_value * 0x20 - (u32)sta_basic_value) +
                           (u32)uStack_8 + 1) >> 5) + sVar3;
      }
    }
    if (sta_data_flag == '\x01') {
      sta_basic_value = sta_basic_value2;
    }
    if ((sport_to_sta_flag == '\x01') && (sta_basic_value < sta_static_data)) {
      sport_to_sta_flag = '\0';
    }
    iVar6 = (u32)sta_basic_value - (u32)sta_static_data;
    iVar10 = -iVar6;
    if (-1 < iVar6) {
      iVar10 = iVar6;
    }
    if (((new_data_flag == '\x01') && (sta_data_flag == '\0')) &&
       ((((6 < (u16)iVar10 && (sta_static_data < 0x65)) ||
         ((8 < (u16)iVar10 && (100 < sta_static_data)))) && (sport_to_sta_flag == '\0')))
       ) {
      sta_basic_value =
           (u16)((s32)((u32)sta_static_data * 2 + (u32)sta_basic_value +
                         (u32)sta_static_data) >> 2);
    }
    iVar6 = (uVar11 & 0xffff) - (u32)sta_data_buf[1];
    iVar10 = -iVar6;
    if (-1 < iVar6) {
      iVar10 = iVar6;
    }
    uVar1 = (s32)((u32)sta_data_buf[1] + (uVar11 & 0xffff)) >> 1;
    Sta_temp_data1 = (u16)uVar1;
    if (((((u16)iVar10 < 0xb) && (Sta_temp_data1 < 0x74)) && (6 < user_count)) &&
       (weak_sport_count < 2)) {
      if (Sta_down_count < 7) {
        Sta_down_count = Sta_down_count + 1;
      }
      else {
        Sta_down_count = '\0';
        if (sport_to_sta_flag == '\x01') {
          if (uVar1 <= sta_static_data + 0x14) {
            if (Sta_temp_data1 < 0x74) {
              if ((s32)(sta_static_data - 8) <= (s32)uVar1) {
                math_work_count2 = 0;
                new_data_flag = '\x01';
                sport_to_sta_flag = '\0';
                sta_basic_value = Sta_temp_data1;
                Sta_temp_data = Sta_temp_data1;
                sta_static_data = Sta_temp_data1;
              }
            }
          }
        }
        else if (new_data_flag == '\0') {
          math_work_count2 = 0;
          new_data_flag = '\x01';
          sta_basic_value = Sta_temp_data1;
          Sta_temp_data = Sta_temp_data1;
          sta_static_data = Sta_temp_data1;
        }
        else {
          iVar6 = uVar1 - sta_static_data;
          iVar10 = -iVar6;
          if (-1 < iVar6) {
            iVar10 = iVar6;
          }
          if ((10 < (u16)iVar10) &&
             (((u16)iVar10 < 0xb || (Sta_temp_data1 <= sta_static_data)))) {
            sta_temp_data_buf = (*(u16 *)((s8 *)&sta_temp_data_buf + 2));
            iVar6 = uVar1 - (*(u16 *)((s8 *)&sta_temp_data_buf + 2));
            iVar10 = -iVar6;
            if (-1 < iVar6) {
              iVar10 = iVar6;
            }
            (*(u16 *)((s8 *)&sta_temp_data_buf + 2)) = Sta_temp_data1;
            if ((math_work_count2 == 0) || (9 < (u16)iVar10)) {
              if (math_work_count2 < 2) {
                math_work_count2 = math_work_count2 + 1;
              }
              else {
                math_work_count2 = 2;
              }
              goto LAB_ffffff80085fb828;
            }
          }
          math_work_count2 = 0;
          sta_basic_value = Sta_temp_data1;
          Sta_temp_data = Sta_temp_data1;
          sta_static_data = Sta_temp_data1;
        }
      }
    }
    else {
      Sta_down_count = '\0';
    }
LAB_ffffff80085fb828:
    if ((uVar2 < sta_data_buf[1]) && (0x7d < uVar2)) {
      if (run_to_sta_count < 6) {
        run_to_sta_count = run_to_sta_count + 1;
      }
    }
    else if ((sta_data_buf[1] < uVar2) && (run_to_sta_count != 0)) {
      run_to_sta_count = run_to_sta_count - 1;
    }
    if (2 < run_to_sta_count) {
      sta_basic_value = (u16)((s32)((u32)sta_data_buf[1] + (uVar11 & 0xffff)) >> 1);
      run_to_sta_count = 0;
      sport_to_sta_flag = '\x01';
      math_work_count2 = 0;
      Sta_temp_data = sta_basic_value;
    }
    if ((10 < user_count) && (lcd_flag[0] == '\0')) {
      if (new_data_flag == '\x01') {
        sta_basic_value = sta_static_data;
      }
      else if (bp_varrange[0] < 0x10) {
        sta_basic_value = heart_sbp_ave;
      }
      else {
        sta_basic_value = (u16)((s32)(heart_sbp_ave + 0xea) >> 2);
      }
      if (sta_basic_value < 0x37) {
        sta_basic_value = 0x37;
      }
      else if (0x73 < sta_basic_value) {
        sta_basic_value = 0x73;
      }
      Sta_temp_data = sta_basic_value;
      blood_lcd_data = sta_basic_value;
      no_touch_flag = 0;
      lcd_flag[0] = '\x01';
    }
    sta_basic_value2 = sta_basic_value;
    iVar6 = (u32)sta_basic_value - (u32)Sta_temp_data;
    iVar10 = -iVar6;
    if (-1 < iVar6) {
      iVar10 = iVar6;
    }
    sVar3 = (s16)iVar10 + 1;
    if (sta_basic_value < 100) {
      sStack_20 = (s16)((s32)(((u32)Sta_temp_data * 0x10 - (u32)Sta_temp_data) +
                                (u32)sta_basic_value + 1) >> 4);
      sStack_24 = 0;
      if (sVar3 != 0) {
        sStack_24 = (s16)((((s16)sta_basic_value + 1) - (s32)(s16)Sta_temp_data) /
                           (s32)sVar3);
      }
    }
    else {
      sStack_20 = (s16)((s32)(((u32)Sta_temp_data * 4 - (u32)Sta_temp_data) +
                                (u32)sta_basic_value + 1) >> 2);
      sStack_24 = 0;
      if (sVar3 != 0) {
        sStack_24 = (s16)((((s16)sta_basic_value + 1) - (s32)(s16)Sta_temp_data) /
                           (s32)sVar3);
      }
    }
    Sta_temp_data = sStack_20 + sStack_24;
    if ((1 < weak_sport_count) && (lcd_flag[0] == '\x01')) {
      if (Sta_temp_data < Sta_data) {
        if (((weak_sport_flag == '\0') && (Sta_temp_data < 0x6e)) &&
           (Sta_data < 0x73)) {
          Sta_temp_data = Sta_data + 4;
          weak_sport_flag = '\x01';
        }
        else {
          Sta_temp_data = Sta_data;
        }
      }
      sta_basic_value = run_basic_value;
    }
    Sta_data = Sta_temp_data;
    run_basic_value = sta_basic_value;
    Run_data = Sta_temp_data + 6;
    run_static_data = sta_static_data + 6;
    run_init_data = Sta_temp_data + 6;
    test_pro_state = 1;
    test_pro_state1 = 0;
    test_pro_flag = 0;
    speed_state_flag = 0;
    sta_data_buf[1] = uVar2;
    run_data_buf[1] = Run_data;
    for (bStack_17 = 0; bStack_17 < 4; bStack_17 = bStack_17 + 1) {
      *(u16 *)((char *)gensor_freq_data + (s64)(s32)(u32)bStack_17 * 2) = 0;
    }
    for (bStack_17 = 0; bStack_17 < 8; bStack_17 = bStack_17 + 1) {
      *(u16 *)((char *)speed_state_buf + (s64)(s32)(u32)bStack_17 * 2) = 0;
    }
    if (bpm_static_count < 200) {
      bpm_static_count = bpm_static_count + 1;
    }
    else {
      bpm_static_count = -0x38;
    }
    speed_state_count = 0;
    bp_gensor_flag = 1;
    speed_data_buf = 0;
    (*(u16 *)((s8 *)&speed_data_buf + 2)) = 0;
    bpm_now_data0 = 0;
    bpm_now_data1 = 0;
    walk_state_count = 0;
    return;
  }
  if (Product_old_status != 3) {
    Sta_data = 0;
    Run_data = 0;
    return;
  }
  if (4 < no_touch_count[0]) {
    no_touch_flag = 0;
  }
  if (gensor_power_new[0] < 23000) {
    if (run_init_data < 0x5f) {
      run_init_data1 = run_init_data;
    }
    else {
      run_init_data1 = 0x5f;
    }
  }
  else if (run_init_data < 0x6e) {
    run_init_data1 = run_init_data;
  }
  else {
    run_init_data1 = 0x6e;
  }
  weak_sport_flag = 1;
  no_touch_count[0]= 0;
  Sta_down_count = 0;
  sta_data_count = 0;
  math_work_count2 = 0;
  run_to_sta_count = 0;
  sta_data_flag = 1;
  sport_to_sta_flag = 1;
  lcd_flag[0] = 1;
  test_pro_state1 = 0;
  Data_sequence(param_2,&uStack_40);
  uVar2 = heart_olddata_fifo[0];
  heart_olddata_fifo[2] = heart_olddata_fifo[1];
  heart_olddata_fifo[1] = heart_olddata_fifo[0];
  if (((s32)((*param_2 >> 1) - param_2[1]) < -6) || (6 < (s32)((*param_2 >> 1) - param_2[1]))) {
    if (((s32)((param_2[1] >> 1) - *param_2) < -6) || (6 < (s32)((param_2[1] >> 1) - *param_2))) {
      if (((((s32)((u32)(uStack_40 >> 1) - (u32)uStack_3c) < -6) ||
           (6 < (s32)((u32)(uStack_40 >> 1) - (u32)uStack_3c))) ||
          ((s32)((u32)heart_olddata_fifo[0] - (u32)uStack_3c) < -6)) ||
         (6 < (s32)((u32)heart_olddata_fifo[0] - (u32)uStack_3c))) {
        heart_olddata_fifo[0] = (u16)*param_2;
      }
      else {
        heart_olddata_fifo[0] = uStack_3c;
      }
    }
    else {
      heart_olddata_fifo[0] = (u16)*param_2;
    }
  }
  else {
    heart_olddata_fifo[0] = (u16)param_2[1];
  }
  if (((s32)((u32)heart_olddata_fifo[0] - (u32)uVar2) < -6) ||
     (6 < (s32)((u32)heart_olddata_fifo[0] - (u32)uVar2))) {
    heart_stable_count = 0;
  }
  else if (heart_stable_count < 100) {
    heart_stable_count = heart_stable_count + 1;
  }
  for (bStack_17 = 0; bStack_17 < 3; bStack_17 = bStack_17 + 1) {
    *(u16 *)((char *)gensor_freq_data + (s64)(s32)(u32)bStack_17 * 2) =
         *(u16 *)((char *)gensor_freq_data + (s64)(s32)(bStack_17 + 1) * 2);
  }
  (*(u16 *)((s8 *)gensor_freq_data + 6)) = Gensor_sport_freq(param_1);
  sVar3 = (*(u16 *)((s8 *)gensor_freq_data + 6)) - gensor_freq_data[1];
  sVar4 = gensor_freq_data[1] - (*(u16 *)((s8 *)gensor_freq_data + 2));
  if ((s16)((sVar3 >> 2) * (sVar4 >> 2)) < 0) {
    iVar10 = -(s32)sVar4;
    if (-1 < sVar4) {
      iVar10 = (s32)sVar4;
    }
    if (0x1d < iVar10) {
      iVar10 = -(s32)sVar3;
      if (-1 < sVar3) {
        iVar10 = (s32)sVar3;
      }
      if (0x1d < iVar10) {
        gensor_freq_data[1] = (*(u16 *)((s8 *)gensor_freq_data + 2));
      }
    }
  }
  if (((s32)((u32)gensor_freq_data[1] - (u32)(*(u16 *)((s8 *)gensor_freq_data + 2))) < -0xb) ||
     (0xb < (s32)((u32)gensor_freq_data[1] - (u32)(*(u16 *)((s8 *)gensor_freq_data + 2))))) {
    state_stable_count = 0;
  }
  else if (state_stable_count < 0x3c) {
    state_stable_count = state_stable_count + 1;
  }
  uStack_4 = 0;
  for (bStack_17 = 0; bStack_17 < 4; bStack_17 = bStack_17 + 1) {
    uStack_4 = *(s16 *)((char *)gensor_freq_data + (s64)(s32)(u32)bStack_17 * 2) + uStack_4;
  }
  gensor_freq_ave = uStack_4 >> 2;
  uVar2 = gensor_freq_data[1] >> 1;
  uVar11 = (u32)gensor_power_new[0];
  bStack_17 = 0;
  while( true ) {
    speed_data_buf = (*(u16 *)((s8 *)&speed_data_buf + 2));
    uVar5 = (*(u16 *)((s8 *)bp_data_buf + 2));
    if (6 < bStack_17) break;
    *(u16 *)((char *)speed_state_buf + (s64)(s32)(u32)bStack_17 * 2) =
         *(u16 *)((char *)speed_state_buf + (s64)(s32)(bStack_17 + 1) * 2);
    bStack_17 = bStack_17 + 1;
  }
  speed_state_buf[7] = (u16)(uVar2 * uVar11 >> 0xe);
  if (1000 < speed_state_buf[7]) {
    speed_state_buf[7] = 1000;
  }
  uStack_16 = 0;
  for (bStack_17 = 0; bStack_17 < 4; bStack_17 = bStack_17 + 1) {
    uStack_16 = (*(u16 *)((char *)speed_state_buf + (s64)(s32)(bStack_17 + 4) * 2) >> 2) + uStack_16;
  }
  gensor_power_freq_ave = -uStack_16;
  if (-1 < (s16)uStack_16) {
    gensor_power_freq_ave = uStack_16;
  }
  sVar3 = speed_state_buf[7] - speed_state_buf[0];
  if ((((*(u16 *)((s8 *)&speed_data_buf + 2)) < 0) && (sVar3 < 0)) || ((0 < sVar3 && (0 < (*(u16 *)((s8 *)&speed_data_buf + 2)))))) {
    iVar10 = -(s32)sVar3;
    if (-1 < sVar3) {
      iVar10 = (s32)sVar3;
    }
    if (iVar10 < 0x28) goto LAB_ffffff80085fc6fc;
    if (speed_state_count < 100) {
      speed_state_count = speed_state_count + 1;
    }
  }
  else {
LAB_ffffff80085fc6fc:
    speed_state_count = 0;
    test_pro_flag = '\0';
    speed_state_flag = '\0';
  }
  if (speed_state_count != 0) {
    iVar10 = -(s32)sVar3;
    if (-1 < sVar3) {
      iVar10 = (s32)sVar3;
    }
    if (0x32 < iVar10) {
      test_pro_flag = '\x01';
    }
  }
  if (((5 < speed_state_count) && (test_pro_flag == '\x01')) &&
     (speed_state_flag == '\0')) {
    if (sVar3 < 1) {
      bp_gensor_flag = '\x02';
    }
    else {
      bp_gensor_flag = '\x01';
    }
    gensor_state_count = 0;
    speed_state_flag = '\x01';
  }
  if (gensor_state_count < 0x5a) {
    gensor_state_count = gensor_state_count + 1;
  }
  else {
    gensor_state_count = 0x3d;
  }
  if (gensor_state_count == 1) {
    bpm_start_data = gensor_power_freq_ave;
  }
  if (bpm_run_count < 200) {
    bpm_run_count = bpm_run_count + 1;
  }
  else {
    bpm_run_count = 200;
  }
  if ((bpm_static_count < 0x28) && (bpm_static_count != 0)) {
    bpm_run_count = bpm_run_count + bpm_static_count;
    bpm_static_count = 0;
  }
  else if (0x27 < bpm_static_count) {
    bpm_run_count = 0;
    bpm_static_count = 0;
    bpm_stable_data = 0;
  }
  uVar2 = bpm_now_data1;
  if (((((u32)gensor_state_count) + ((gensor_state_count / 5) * -5)) & 0xff) == 0) {
    bpm_now_data0 = bpm_now_data1;
    uVar2 = gensor_power_freq_ave;
    if (0x27 < gensor_state_count) {
      bpm_stable_data = bpm_now_data1;
      bpm_run_count = 0;
      bp_gensor_oldflag = bp_gensor_flag;
    }
  }
  bpm_now_data1 = uVar2;
  if (((gensor_state_count == 1) && (bpm_stable_data != 0)) && (bpm_run_count < 0x28))
  {
    if (((s32)((u32)bpm_start_data - (u32)bpm_stable_data) < -0x31) ||
       (0x31 < (s32)((u32)bpm_start_data - (u32)bpm_stable_data))) {
      if (bpm_start_data < bpm_stable_data) {
        bp_gensor_flag = '\x02';
      }
      else {
        bp_gensor_flag = '\x01';
      }
    }
    else {
      bp_gensor_flag = bp_gensor_oldflag;
    }
  }
  if (gensor_state_count == 1) {
    run_data_buf[0] = run_data_buf[1];
    run_data_buf[1] = Run_data;
    sport_stable_flag = '\0';
    bpm_high_flag = 0;
    bpm_sum_flag = '\0';
    bpm_unormal_count = 0;
  }
  uVar2 = gensor_freq_data[1] >> 1;
  bVar8 = false;
  test_pro_state = '\0';
  if (gensor_power_freq_ave < 0x50) {
    bp_lim_data[0] = run_init_data1;
  }
  else if (gensor_power_freq_ave < 0x8c) {
    bp_lim_data[0] = run_init_data1 + 0x14;
  }
  else if (gensor_power_freq_ave < 0xe6) {
    bp_lim_data[0] = run_init_data1 + 0x1e;
  }
  else {
    bp_lim_data[0] = run_init_data1 + 0x28;
  }
  if (gensor_state_count < 0x29) {
    if (bp_gensor_flag == '\x01') {
      if (gensor_power_freq_ave < 0xc9) {
        if (gensor_power_freq_ave < 100) {
          uStack_a = 0xf;
          uStack_e = 0xf;
        }
        else {
          uStack_a = 0xc;
          uStack_e = 0x19;
        }
      }
      else {
        uStack_a = 8;
        uStack_e = 0x28;
      }
    }
    else if (bp_gensor_flag == '\x02') {
      if (gensor_power_freq_ave < 0xc9) {
        if (gensor_power_freq_ave < 0x65) {
          if (gensor_state_count < 0x1e) {
            uStack_a = 0x17;
          }
          else {
            uStack_a = 0x23;
          }
          uStack_e = 6;
        }
        else {
          if (gensor_state_count < 0x1e) {
            uStack_a = 0x12;
          }
          else {
            uStack_a = 0x19;
          }
          uStack_e = 10;
        }
      }
      else {
        uStack_a = 0xd;
        uStack_e = 10;
      }
    }
    else {
      uStack_a = 0xf;
      uStack_e = 0xf;
    }
  }
  else {
    uStack_a = 10;
    uStack_e = 10;
  }
  if (gensor_state_count < 10) {
    uStack_c = 0x28;
  }
  else if (gensor_state_count < 0x14) {
    uStack_c = 0x37;
  }
  else {
    uStack_c = 0x3c;
  }
  if ((((state_stable_count < 3) || (uStack_3e < 0x47)) || (uStack_3e <= run_init_data1)) ||
     ((0x22 < gensor_state_count && ((gensor_state_count < 0x24 || (heart_stable_count < 7))))
     )) {
    test_pro_count = 0;
    test_pro_count1 = 0;
    goto LAB_ffffff80085fd370;
  }
  if ((((s32)((u32)(uStack_40 >> 1) - (u32)uStack_3c) < -6) ||
      (6 < (s32)((u32)(uStack_40 >> 1) - (u32)uStack_3c))) ||
     (((s32)((u32)uStack_3c - (u32)uVar2) < -6 || (6 < (s32)((u32)uStack_3c - (u32)uVar2))))) {
    test_pro_count = 0;
    test_pro_count1 = 0;
    goto LAB_ffffff80085fd370;
  }
  if ((uStack_3c + 0x14 < (u32)uStack_3e) && (uStack_3e < 0xbe)) {
    iVar6 = (u32)uStack_40 - (u32)uStack_3e;
    iVar10 = -iVar6;
    if (-1 < iVar6) {
      iVar10 = iVar6;
    }
    if (10 < iVar10) {
      bp_data_buf[0] = (*(u16 *)((s8 *)bp_data_buf + 2));
      (*(u16 *)((s8 *)bp_data_buf + 2)) = uStack_3e;
      if (((((s32)((u32)uStack_3e - (u32)uVar5) < -0xb) ||
           (0xb < (s32)((u32)uStack_3e - (u32)uVar5))) || (uStack_3e < bp_lim_data[0])) ||
         (((bp_gensor_flag != '\x02' || (run_data_buf[1] <= uStack_3e)) &&
          ((bp_gensor_flag != '\x01' || (uStack_3e <= run_data_buf[1])))))) {
LAB_ffffff80085fd15c:
        test_pro_count = 0;
      }
      else {
        iVar6 = (u32)uStack_3e - (u32)Run_data;
        iVar10 = -iVar6;
        if (-1 < iVar6) {
          iVar10 = iVar6;
        }
        if ((s32)(u32)uStack_c < iVar10) goto LAB_ffffff80085fd15c;
        test_pro_count = test_pro_count + 1;
        if ((bpm_high_flag + 2 <= (u32)test_pro_count) &&
           (((bp_gensor_flag == '\x02' && ((u32)uStack_3e < Run_data + 0xd)) ||
            ((bp_gensor_flag == '\x01' &&
             ((s32)(Run_data - 0xd) < (s32)(u32)uStack_3e)))))) {
          Run_data = (u16)((s32)((u32)uStack_3e + (u32)uVar5) >> 1);
          bVar8 = true;
          test_pro_count = 0;
          test_pro_state = '\x03';
        }
      }
      if (((-10 < (s32)((u32)uStack_3e - (u32)uVar5)) &&
          ((s32)((u32)uStack_3e - (u32)uVar5) < 10)) && (bp_lim_data[0] <= uStack_3e)) {
        iVar6 = (u32)uStack_3e - (u32)Run_data;
        iVar10 = -iVar6;
        if (-1 < iVar6) {
          iVar10 = iVar6;
        }
        if (iVar10 <= (s32)(u32)uStack_c) {
          test_pro_count1 = test_pro_count1 + 1;
          if (4 < test_pro_count1) {
            Run_data = (u16)((s32)((u32)uStack_3e + (u32)uVar5) >> 1);
            run_static_data = uStack_3e;
            bVar8 = true;
            test_pro_count1 = 0;
            test_pro_state = '\f';
            sport_stable_flag = '\x01';
            if (((0x86 < uStack_3e) && (-0x11 < (s32)((u32)uStack_40 - (u32)uStack_3e))) &&
               ((s32)((u32)uStack_40 - (u32)uStack_3e) < 0x11)) {
              bpm_high_flag = 2;
            }
          }
          goto LAB_ffffff80085fd370;
        }
      }
      test_pro_count1 = 0;
      goto LAB_ffffff80085fd370;
    }
  }
  test_pro_count = 0;
  test_pro_count1 = 0;
LAB_ffffff80085fd370:
  if (!bVar8) {
    bVar9 = false;
    uStack_10 = 0x2d;
    for (bStack_17 = 0; bStack_17 < 4; bStack_17 = bStack_17 + 1) {
      uStack_6 = (u16)param_2[bStack_17];
      if (((0x42 < param_2[bStack_17]) && (param_2[bStack_17] < 0xbe)) &&
         (((u32)uVar2 < param_2[bStack_17] && ((u32)run_init_data1 < param_2[bStack_17]))))
      {
        if (heart_stable_count < 6) {
          iVar6 = param_2[bStack_17] - (u32)uVar2;
          iVar10 = -iVar6;
          if (-1 < iVar6) {
            iVar10 = iVar6;
          }
          if (3 < iVar10) {
            iVar6 = param_2[bStack_17] + (u32)uVar2 * -2;
            iVar10 = -iVar6;
            if (-1 < iVar6) {
              iVar10 = iVar6;
            }
            if (((5 < iVar10) && ((u32)Run_data < param_2[bStack_17] + (u32)uStack_a))
               && (param_2[bStack_17] < (u32)Run_data + (u32)uStack_e)) {
              if (!bVar9) {
                bVar9 = true;
                test_pro_state = '\x05';
                uStack_4 = uStack_6;
              }
              iVar6 = param_2[bStack_17] - (u32)Run_data;
              iVar10 = -iVar6;
              if (-1 < iVar6) {
                iVar10 = iVar6;
              }
              uVar5 = (u16)iVar10;
              if (uVar5 < 10) {
                test_pro_state = '\x06';
                if (uVar5 <= uStack_10) {
                  uStack_10 = uVar5;
                  uStack_4 = uStack_6;
                }
              }
            }
          }
        }
        else {
          iVar6 = param_2[bStack_17] - (u32)Run_data;
          iVar10 = -iVar6;
          if (-1 < iVar6) {
            iVar10 = iVar6;
          }
          iVar7 = param_2[bStack_17] - (u32)uVar2;
          iVar6 = -iVar7;
          if (-1 < iVar7) {
            iVar6 = iVar7;
          }
          if (3 < iVar6) {
            iVar7 = param_2[bStack_17] + (u32)uVar2 * -2;
            iVar6 = -iVar7;
            if (-1 < iVar7) {
              iVar6 = iVar7;
            }
            if (((5 < iVar6) && ((u32)Run_data < param_2[bStack_17] + (u32)uStack_a))
               && ((param_2[bStack_17] < (u32)Run_data + (u32)uStack_e &&
                   ((u16)iVar10 <= uStack_10)))) {
              bVar9 = true;
              test_pro_state = '\x04';
              uStack_10 = (u16)iVar10;
              uStack_4 = uStack_6;
            }
          }
        }
      }
    }
    if (bVar9) {
      Run_data =
           (u16)((s32)((u32)Run_data * 2 + (u32)Run_data +
                         (u32)uStack_4) >> 2);
    }
    else {
      uStack_10 = 10;
      for (bStack_17 = 0; bStack_17 < 4; bStack_17 = bStack_17 + 1) {
        if ((u32)run_init_data1 < param_2[bStack_17]) {
          uStack_6 = (u16)param_2[bStack_17];
          iVar6 = param_2[bStack_17] - (u32)Run_data;
          iVar10 = -iVar6;
          if (-1 < iVar6) {
            iVar10 = iVar6;
          }
          uVar2 = (u16)iVar10;
          if ((uVar2 < 9) && (uVar2 <= uStack_10)) {
            bVar9 = true;
            test_pro_state = '\a';
            uStack_10 = uVar2;
            uStack_4 = uStack_6;
          }
        }
      }
      if (bVar9) {
        Run_data =
             (u16)((s32)((u32)Run_data * 2 + (u32)Run_data +
                           (u32)uStack_4) >> 2);
      }
    }
    if ((sport_stable_flag == '\x01') &&
       (((bp_gensor_flag == '\x02' &&
         (run_static_data + 0x14 < (u32)Run_data)) ||
        ((bp_gensor_flag == '\x01' &&
         (Run_data + 0xd < (u32)run_static_data)))))) {
      Run_data = run_static_data;
      test_pro_state = '\r';
    }
  }
  if ((test_pro_state == '\0') && (bpm_unormal_count < 0x1e)) {
    bpm_unormal_count = bpm_unormal_count + 2;
  }
  else if (test_pro_state == '\f') {
    bpm_unormal_count = 0;
  }
  else if (test_pro_state != '\0') {
    if (bpm_unormal_count < 4) {
      bpm_unormal_count = 0;
    }
    else {
      bpm_unormal_count = bpm_unormal_count - 3;
    }
  }
  if ((0x13 < gensor_state_count) &&
     (((((u32)gensor_state_count) + ((gensor_state_count / 0x14) * -0x14)) & 0xff) == 0)) {
    if ((*(u16 *)((s8 *)&bp_gensor_power + 2)) == 0) {
      bp_gensor_power = (*(u16 *)((s8 *)&bp_gensor_power + 2));
      bp_gensor_data[0] = bp_lim_data[0];
    }
    else if (((*(u16 *)((s8 *)&bp_gensor_power + 2)) < 0x50) && (0x78 < gensor_power_freq_ave)) {
      bp_gensor_power = (*(u16 *)((s8 *)&bp_gensor_power + 2));
      bp_gensor_data[0] = bp_lim_data[0];
    }
    else if ((0x78 < (*(u16 *)((s8 *)&bp_gensor_power + 2))) && (gensor_power_freq_ave < 0x50)) {
      bp_gensor_power = (*(u16 *)((s8 *)&bp_gensor_power + 2));
      bp_gensor_data[0] = bp_lim_data[0];
    }
    (*(u16 *)((s8 *)bp_gensor_data + 2)) = Run_data;
    (*(u16 *)((s8 *)&bp_gensor_power + 2)) = gensor_power_freq_ave;
    if (!bVar8) {
      if ((bp_gensor_power == 0) && (Run_data + 8 < (u32)bp_lim_data[0])) {
        Run_data = bp_lim_data[0];
        test_pro_state = '\b';
      }
      else if ((((gensor_power_freq_ave < 0x79) || (0x4f < bp_gensor_power)) ||
               ((u32)bp_gensor_data[0] <= Run_data + 10)) ||
              (((s32)((u32)Run_data - (u32)run_data_buf[1]) < -0xc ||
               (0xc < (s32)((u32)Run_data - (u32)run_data_buf[1]))))) {
        if ((((0x78 < bp_gensor_power) &&
             ((gensor_power_freq_ave < 0x50 &&
              (bp_gensor_data[0] + 10 < (u32)Run_data)))) &&
            (-0xd < (s32)((u32)Run_data - (u32)run_data_buf[1]))) &&
           ((s32)((u32)Run_data - (u32)run_data_buf[1]) < 0xd)) {
          Run_data = Run_data - 5;
          test_pro_state = '\n';
        }
      }
      else {
        Run_data = Run_data + 8;
        test_pro_state = '\t';
      }
    }
  }
  if (!bVar8) {
    if ((((gensor_state_count < 3) || (0x13 < gensor_state_count)) ||
        ((s32)((u32)Run_data - (u32)run_data_buf[1]) < -0xc)) ||
       (0xc < (s32)((u32)Run_data - (u32)run_data_buf[1]))) {
      if (((gensor_state_count == 0x14) &&
          (-10 < (s32)((u32)Run_data - (u32)run_data_buf[1]))) &&
         ((s32)((u32)Run_data - (u32)run_data_buf[1]) < 10)) {
        if (bp_gensor_flag == '\x02') {
          Run_data = run_data_buf[1] - 5;
        }
        else if (bp_gensor_flag == '\x01') {
          Run_data = run_data_buf[1] + 5;
        }
      }
    }
    else if ((bp_gensor_flag == '\x01') && (gensor_state_count < 9)) {
      if (gensor_power_freq_ave < 0xfb) {
        if (gensor_power_freq_ave < 0x97) {
          if (gensor_power_freq_ave < 0x65) {
            Run_data = Run_data + 1;
          }
          else {
            Run_data = Run_data + 2;
          }
        }
        else {
          Run_data = Run_data + 3;
        }
      }
      else {
        Run_data = Run_data + 4;
      }
    }
    else if ((((bp_gensor_flag == '\x02') && (10 < gensor_state_count)) &&
             (gensor_state_count < 0xf)) && (gensor_power_freq_ave < 0x97)) {
      if (gensor_power_freq_ave < 0x65) {
        if (gensor_power_freq_ave < 0x3d) {
          Run_data = Run_data - 3;
        }
        else {
          Run_data = Run_data - 2;
        }
      }
      else {
        Run_data = Run_data - 1;
      }
    }
  }
  if (((0x1e < state_stable_count) && (0x23 < gensor_state_count)) &&
     ((0x8c < Run_data &&
      ((-0xf < (s32)((u32)Run_data - (u32)gensor_freq_data[1]) &&
       ((s32)((u32)Run_data - (u32)gensor_freq_data[1]) < 0xf)))))) {
    bpm_sum_flag = '\x01';
  }
  if ((((bpm_sum_flag == '\x01') && (0xd < bpm_unormal_count)) &&
      (speed_state_count < 3)) &&
     (((-0xb < (s32)((u32)uStack_40 - (u32)gensor_freq_data[1]) &&
       ((s32)((u32)uStack_40 - (u32)gensor_freq_data[1]) < 0xb)) ||
      ((-0xb < (s32)((u32)uStack_3e - (u32)gensor_freq_data[1]) &&
       ((s32)((u32)uStack_3e - (u32)gensor_freq_data[1]) < 0xb)))))) {
    Run_data = gensor_freq_data[1];
    bpm_unormal_count = 0;
    test_pro_state = '\x10';
    bpm_sum_flag = '\0';
  }
  if (((test_pro_state == '\0') && (10 < bpm_unormal_count)) &&
     ((0x14 < state_stable_count &&
      (((0x14 < gensor_state_count && (Run_data < 0x7d)) &&
       (gensor_power_freq_ave < 0x41)))))) {
    bVar8 = false;
    for (bStack_17 = 0; bStack_17 < 4; bStack_17 = bStack_17 + 1) {
      if (((u32)Run_data <= param_2[bStack_17] + 6) &&
         (param_2[bStack_17] <= Run_data + 0xf)) {
        uVar1 = param_2[bStack_17] - (u32)Run_data;
        uVar11 = -uVar1;
        if (-1 < (s32)uVar1) {
          uVar11 = uVar1;
        }
        if ((uVar11 & 0xffff) < 0x15) {
          uStack_6 = (u16)param_2[bStack_17];
        }
        bVar8 = true;
      }
    }
    if (bVar8) {
      if (((s32)((u32)uStack_6 - (u32)gensor_freq_data[1]) < -9) ||
         (9 < (s32)((u32)uStack_6 - (u32)gensor_freq_data[1]))) {
        Run_data = uStack_6;
        walk_state_count = 0;
        bpm_unormal_count = 0;
        test_pro_state = '\x12';
      }
      else {
        walk_state_count = walk_state_count + 1;
        if (2 < walk_state_count) {
          Run_data = uStack_6;
          walk_state_count = 0;
          bpm_unormal_count = 0;
          test_pro_state = '\x11';
        }
      }
    }
    if (0x19 < bpm_unormal_count) {
      for (bStack_17 = 0; bStack_17 < 4; bStack_17 = bStack_17 + 1) {
        if ((((-10 < (s32)(param_2[bStack_17] - (u32)gensor_freq_data[1])) &&
             ((s32)(param_2[bStack_17] - (u32)gensor_freq_data[1]) < 10)) &&
            (-0x1e < (s32)(param_2[bStack_17] - (u32)Run_data))) &&
           ((s32)(param_2[bStack_17] - (u32)Run_data) < 0x1e)) {
          Run_data = (u16)param_2[bStack_17];
          walk_state_count = 0;
          bpm_unormal_count = 0;
          test_pro_state = '\x13';
        }
      }
    }
  }
  if (bpm_high_flag == 2) {
    test_pro_state = test_pro_state + '\x14';
  }
  if (0xc4 < Run_data) {
    Run_data = 0xc4;
  }
  if (blood_lcd_data == 0) {
    sta_basic_value2 = Run_data;
  }
  else {
    sta_basic_value2 = blood_lcd_data;
  }
  Sta_temp_data = Run_data;
  sta_basic_value = Run_data;
  (*(u16 *)((s8 *)&speed_data_buf + 2)) = sVar3;
  return;
}

/* ==== BP_blood_user @ 0xffffff80085fe73c ==== */
u16 BP_blood_user(u32 param_1)

{
  u8 bVar1;
  u16 uStack_2;
  
  if (user_count < 0x640) {
    user_count = user_count + 1;
  }
  else {
    user_count = 7;
  }
  if (user_count == 6) {
    if (Product_old_status == 2) {
      blood_data = Sta_data;
      if (Sta_data < 0x65) {
        if (Sta_data < 0x32) {
          blood_lcd_data = 0x32;
        }
        else {
          blood_lcd_data = Sta_data;
        }
      }
      else {
        blood_lcd_data = 100;
      }
    }
    else if (Product_old_status == 3) {
      blood_data = Run_data;
      if (Run_data < 0x83) {
        if (Run_data < 0x5f) {
          blood_lcd_data = 0x5f;
        }
        else {
          blood_lcd_data = Run_data;
        }
      }
      else {
        blood_lcd_data = 0x82;
      }
    }
    else if (Product_old_status == 1) {
      blood_data = 0;
      blood_lcd_data = 0;
    }
    else {
      blood_data = 0;
      blood_lcd_data = 0;
    }
    Product_now_status = Product_old_status;
    lcd_data = blood_lcd_data;
  }
  else if (user_count < 7) {
    blood_data = 0;
    blood_lcd_data = 0;
    lcd_data = 0;
  }
  else if (Product_old_status == Product_now_status) {
    if (((u8)user_count & 1) == 1) {
      if (Product_old_status == 2) {
        blood_data = Sta_data;
        if (blood_lcd_data == Sta_data) {
          bVar1 = (s8)param_1 + (s8)(param_1 / 3) * -3;
          if (bVar1 < 2) {
            uStack_2 = bVar1 + blood_lcd_data;
          }
          else {
            uStack_2 = (blood_lcd_data - bVar1) + 1;
          }
          lcd_data = uStack_2;
        }
        else {
          if (Sta_data + 4 < (u32)blood_lcd_data) {
            blood_lcd_data = blood_lcd_data - 3;
          }
          else if ((s32)(u32)blood_lcd_data < (s32)(Sta_data - 4)) {
            blood_lcd_data = blood_lcd_data + 3;
          }
          else if (Sta_data < blood_lcd_data) {
            blood_lcd_data = blood_lcd_data - 1;
          }
          else {
            blood_lcd_data = blood_lcd_data + 1;
          }
          lcd_data = blood_lcd_data;
        }
      }
      else if (Product_old_status == 3) {
        blood_data = Run_data;
        if (blood_lcd_data == Run_data) {
          bVar1 = (s8)param_1 + (s8)(param_1 / 5) * -5;
          if (bVar1 < 3) {
            uStack_2 = bVar1 + blood_lcd_data;
          }
          else {
            uStack_2 = (blood_lcd_data - bVar1) + 2;
          }
          lcd_data = uStack_2;
        }
        else {
          if (Run_data + 5 < (u32)blood_lcd_data) {
            if (Run_data < 0x74) {
              blood_lcd_data = blood_lcd_data - 2;
            }
            else {
              blood_lcd_data = blood_lcd_data - 4;
            }
          }
          else if ((s32)(u32)blood_lcd_data < (s32)(Run_data - 5)) {
            blood_lcd_data = blood_lcd_data + 4;
          }
          else if (Run_data < blood_lcd_data) {
            blood_lcd_data = blood_lcd_data - 2;
          }
          else {
            blood_lcd_data = blood_lcd_data + 2;
          }
          lcd_data = blood_lcd_data;
        }
      }
      else if (Product_old_status == 1) {
        blood_data = 0;
        blood_lcd_data = 0;
        lcd_data = 0;
      }
      else {
        blood_data = 0;
        blood_lcd_data = 0;
        lcd_data = 0;
      }
    }
    else if ((blood_lcd_data + 1 < (u32)blood_data) ||
            ((s32)(u32)blood_data < (s32)(blood_lcd_data - 1))) {
      if (Product_old_status == 2) {
        if ((u32)blood_lcd_data < blood_data + 5) {
          if ((s32)(blood_data - 5) < (s32)(u32)blood_lcd_data) {
            if (blood_data < blood_lcd_data) {
              blood_lcd_data = blood_lcd_data - 1;
            }
            else {
              blood_lcd_data = blood_lcd_data + 1;
            }
          }
          else {
            blood_lcd_data = blood_lcd_data + 3;
          }
        }
        else {
          blood_lcd_data = blood_lcd_data - 3;
        }
        lcd_data = blood_lcd_data;
      }
      else if (Product_old_status == 3) {
        if ((u32)blood_lcd_data < blood_data + 6) {
          if ((s32)(blood_data - 6) < (s32)(u32)blood_lcd_data) {
            if (blood_data < blood_lcd_data) {
              blood_lcd_data = blood_lcd_data - 2;
            }
            else {
              blood_lcd_data = blood_lcd_data + 2;
            }
          }
          else {
            blood_lcd_data = blood_lcd_data + 4;
          }
        }
        else if (blood_data < 0x74) {
          blood_lcd_data = blood_lcd_data - 2;
        }
        else {
          blood_lcd_data = blood_lcd_data - 4;
        }
        lcd_data = blood_lcd_data;
      }
    }
  }
  else {
    if (Product_old_status == 2) {
      blood_data = Sta_data;
      if (lcd_data == 0) {
        if (Sta_data < 0x65) {
          if (Sta_data < 0x32) {
            blood_lcd_data = 0x32;
          }
          else {
            blood_lcd_data = Sta_data;
          }
        }
        else {
          blood_lcd_data = 100;
        }
      }
      else if (Sta_data + 4 < (u32)blood_lcd_data) {
        blood_lcd_data = blood_lcd_data - 4;
      }
      else if ((s32)(u32)blood_lcd_data < (s32)(Sta_data - 4)) {
        blood_lcd_data = blood_lcd_data + 4;
      }
      else {
        blood_lcd_data = Sta_data;
      }
    }
    else if (Product_old_status == 3) {
      blood_data = Run_data;
      if (lcd_data == 0) {
        blood_lcd_data = Run_data;
      }
      else if (Run_data + 5 < (u32)blood_lcd_data) {
        blood_lcd_data = blood_lcd_data - 5;
      }
      else if ((s32)(u32)blood_lcd_data < (s32)(Run_data - 5)) {
        blood_lcd_data = blood_lcd_data + 5;
      }
      else {
        blood_lcd_data = Run_data;
      }
    }
    else if (Product_old_status == 1) {
      blood_data = 0;
      blood_lcd_data = 0;
    }
    else {
      blood_data = 0;
      blood_lcd_data = 0;
    }
    Product_now_status = Product_old_status;
    lcd_data = blood_lcd_data;
  }
  if (Product_old_status == 1) {
    lcd_data = 0;
  }
  else if (lcd_flag[0] == '\0') {
    lcd_data = 0;
  }
  else {
    lcd_temp_data = lcd_data;
  }
  return lcd_data;
}

/* ==== Gensor_init @ 0xffffff80085ff530 ==== */
void Gensor_init(void)

{
  gensor_static_count = 0;
  gensor_sport_count = 0;
  gensor_stateflag = 0;
  return;
}

/* ==== Gensor_Date_Pro @ 0xffffff80085ff55c ==== */
u64 Gensor_Date_Pro(void)

{
  return 2;
}

/* ==== Gensor_State_Read @ 0xffffff80085ff57c ==== */
void Gensor_State_Read(s8 param_1)

{
  if (Product_old_status == 1) {
    Product_old_status = 1;
  }
  else if ((((param_1 == '\x03') || (param_1 == '\x04')) || (param_1 == '\x05')) &&
          (0xb < user_count)) {
    Product_old_status = 3;
  }
  else if ((param_1 == '\x02') || (param_1 == '\0')) {
    Product_old_status = 2;
  }
  return;
}

/* ==== BD_Spo2test_math @ 0xffffff80085ff62c ==== */
void BD_Spo2test_math(u32 *param_1,u32 *param_2,u32 *param_3,u32 *param_4)

{
  u16 uVar1;
  u32 uStack_14;
  u32 uStack_10;
  u32 uStack_c;
  u16 uStack_8;
  u16 uStack_6;
  u16 uStack_4;
  u8 bStack_1;
  
  if (all_code_count < 6) {
    *param_4 = 0;
    param_4[9] = 0;
    param_4[10] = 0;
    param_4[0xb] = 0;
    param_4[0xc] = 0;
  }
  else {
    if (((*param_1 < 0x79) || ((s32)(param_1[1] - (*param_1 >> 1)) < -4)) ||
       (4 < (s32)(param_1[1] - (*param_1 >> 1)))) {
      uStack_8 = (u16)*param_1;
      uStack_14 = param_1[4];
    }
    else {
      uStack_8 = (u16)param_1[1];
      uStack_14 = param_1[5];
    }
    param_4[10] = uStack_14;
    *param_4 = (u32)uStack_8;
    uStack_4 = (s16)*param_2 - uStack_8;
    uStack_c = param_2[4];
    uStack_6 = 1;
    for (bStack_1 = 1; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
      uVar1 = (s16)param_2[bStack_1] - uStack_8;
      if (uVar1 < uStack_4) {
        uStack_c = param_2[(u64)bStack_1 + 4];
        uStack_6 = bStack_1 + 1;
        uStack_4 = uVar1;
      }
    }
    if (uStack_4 < 7) {
      param_4[0xb] = uStack_c;
    }
    else {
      param_4[0xb] = 0;
      uStack_6 = 0;
    }
    param_4[9] = (u32)uStack_6 * 10;
    uStack_4 = (s16)*param_3 - uStack_8;
    uStack_10 = param_3[4];
    uStack_6 = 1;
    for (bStack_1 = 1; bStack_1 < 4; bStack_1 = bStack_1 + 1) {
      uVar1 = (s16)param_3[bStack_1] - uStack_8;
      if (uVar1 < uStack_4) {
        uStack_10 = param_3[(u64)bStack_1 + 4];
        uStack_6 = bStack_1 + 1;
        uStack_4 = uVar1;
      }
    }
    if (uStack_4 < 7) {
      param_4[0xc] = uStack_10;
    }
    else {
      param_4[0xc] = 0;
      uStack_6 = 0;
    }
    param_4[9] = param_4[9] + (u32)uStack_6;
  }
  return;
}

/* ==== BD_Spo2_math @ 0xffffff80085ff958 ==== */
void BD_Spo2_math(u64 param_1,u64 param_2,u32 *param_3)

{
  s32 iVar1;
  u32 uVar2;
  u32 uVar3;
  s32 iVar4;
  u16 uVar5;
  u16 uVar6;
  u16 uVar7;
  s32 iVar8;
  s32 iVar9;
  u32 uVar10;
  u8 bVar11;
  s32 iVar12;
  s32 iVar13;
  u16 uStack_8;
  u16 uStack_6;
  u8 bStack_4;
  u8 bStack_3;
  u8 bStack_2;
  u8 bStack_1;
  
  if ((gensor_power < 0x1389) || (7 < spo2_power_count)) {
    if ((gensor_power < 5000) && (spo2_power_count != 0)) {
      spo2_power_count = spo2_power_count - 1;
    }
  }
  else {
    spo2_power_count = spo2_power_count + 4;
  }
  if (all_code_count < 0xb) {
    *param_3 = 0;
    param_3[1] = 0;
    param_3[2] = 0;
    param_3[3] = 0;
  }
  else {
    iVar4 = spo2_data[4] * -0xa0c7;
    iVar9 = spo2_data[6] * 4 - spo2_data[8];
    iVar12 = (s32)spo2_data[1];
    iVar13 = (s32)spo2_data[2];
    iVar1 = spo2_data[1] * 10 + 1;
    iVar8 = 0;
    if (iVar1 != 0) {
      iVar8 = (spo2_data[2] * -0x25f5b) / iVar1;
    }
    uVar2 = (s32)(red_ir_ac_new[0] * -0xa80b) / 1000 + 0x1f914 +
            (spo2_data[6] * 3 + spo2_data[8] * -2) / 10 + iVar8;
    uVar3 = (s32)(red_ir_ac_new[0] * -0xa0c7) / 1000 + 0x181f0 +
            (spo2_data[6] * 4 - spo2_data[8]) / 10 +
            (spo2_data[1] * 0x38f + spo2_data[2] * -0x1a6) / 10;
    bVar11 = (u8)((s32)(uVar2 + uVar3 + 1000) / 2000);
    iVar8 = red_ir_ac_new[0] - spo2_data[4];
    iVar1 = -iVar8;
    if (-1 < iVar8) {
      iVar1 = iVar8;
    }
    uVar5 = 0;
    if (red_ir_ac_new[0] + 1 != 0) {
      uVar5 = (u16)((iVar1 * 100) / (s32)(red_ir_ac_new[0] + 1));
    }
    uVar10 = 0;
    if (ir_data_ave + 1U != 0) {
      uVar10 = (u32)(red_data_ave * 1000) / (ir_data_ave + 1U);
    }
    iVar8 = red_ir_ac_new[0] - uVar10;
    iVar1 = -iVar8;
    if (-1 < iVar8) {
      iVar1 = iVar8;
    }
    uVar6 = 0;
    if (red_ir_ac_new[0] + 1 != 0) {
      uVar6 = (u16)((iVar1 * 100) / (s32)(red_ir_ac_new[0] + 1));
    }
    uVar10 = 0;
    if (ir_data_ave + 1U != 0) {
      uVar10 = (u32)(red_data_ave * 1000) / (ir_data_ave + 1U);
    }
    iVar8 = spo2_data[4] - uVar10;
    iVar1 = -iVar8;
    if (-1 < iVar8) {
      iVar1 = iVar8;
    }
    uVar7 = 0;
    if (spo2_data[4] + 1 != 0) {
      uVar7 = (u16)((iVar1 * 100) / (spo2_data[4] + 1));
    }
    if (all_code_count == 0xc) {
      spo2_data[0] = bVar11;
    }
    else if (((uVar5 < 10) && (uVar6 < 10)) && (uVar7 < 10)) {
      spo2_data[0] = bVar11;
    }
    if (spo2_data[0] < 100) {
      if ((spo2_data[0] < 0x4e) && (spo2_data[0] != 0)) {
        param_3[1] = 0x4e;
      }
      else {
        param_3[1] = (u32)spo2_data[0];
      }
    }
    else {
      param_3[1] = 99;
    }
    if (((power_unormal_flag == 0) && (power_unormal_count == '\0')) &&
       (0x28 < non_zero_count)) {
      for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
        *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result) =
             *(u8 *)((s64)(s32)(bStack_1 + 1) + spo2_fifo_result);
      }
      spo2_fifo_result[5] = (u8)param_3[1];
    }
    bStack_2 = 0;
    uStack_6 = 0;
    for (bStack_1 = 0; bStack_1 < 6; bStack_1 = bStack_1 + 1) {
      if (*(s8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result) != '\0') {
        if (bStack_2 == 0) {
          bStack_3 = *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result);
          bStack_4 = *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result);
        }
        else {
          if (bStack_3 < *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result)) {
            bStack_3 = *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result);
          }
          if (*(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result) < bStack_4) {
            bStack_4 = *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result);
          }
        }
        bStack_2 = bStack_2 + 1;
        uStack_6 = *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_fifo_result) + uStack_6;
      }
    }
    if (bStack_2 < 3) {
      param_3[1] = 0;
    }
    else {
      uVar10 = 0;
      if (bStack_2 - 2 != 0) {
        uVar10 = (s32)(((u32)uStack_6 - (u32)bStack_3) - (u32)bStack_4) / (s32)(bStack_2 - 2);
      }
      param_3[1] = uVar10;
    }
    if (Product_old_status == 1) {
      *param_3 = 0;
    }
    else if (all_code_count < 0x10) {
      *param_3 = 0;
    }
    else {
      uStack_8 = 0;
      for (bStack_1 = 0; bStack_1 < 5; bStack_1 = bStack_1 + 1) {
        *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_test_buf) =
             *(u8 *)((s64)(s32)(bStack_1 + 1) + spo2_test_buf);
        uStack_8 = *(u8 *)((s64)(s32)(u32)bStack_1 + spo2_test_buf) + uStack_8;
      }
      if (spo2_power_count < 3) {
        spo2_test_buf[5] = (spo2_data[0] & 3) + 0x60;
      }
      else {
        spo2_test_buf[5] = (u8)uVar3 & 3;
        if (-1 < (s32)-uVar3) {
          spo2_test_buf[5] = -((u8)-uVar3 & 3);
        }
        spo2_test_buf[5] = spo2_test_buf[5] + 0x5f;
      }
      if ((uStack_8 < 0x1c2) || (0x1ef < uStack_8)) {
        *param_3 = 0;
      }
      else {
        *param_3 = ((u32)uStack_8 + (u32)spo2_test_buf[5]) / 6;
      }
      if (((0xfff9 < DC_refless_data2) && (red_dc_value == -1)) && (*param_3 != 0)) {
        *param_3 = 99;
      }
    }
    param_3[1] = (u32)spo2_data[0];
    param_3[2] = iVar4 / 1000 + 0x181f0 + iVar9 / 10 + (iVar12 * 0x38f + iVar13 * -0x1a6) / 10;
    param_3[3] = uVar2;
    param_3[5] = uVar3;
    param_3[6] = ir_temp_chage;
    param_3[7] = red_power_s;
    param_3[8] = ir_power_s;
    param_3[0xb] = (u32)power_unormal_flag;
    param_3[0xc] = red_ir_ac_old;
    param_3[0xd] = red_ir_ac_new[0];
    uVar2 = 0;
    if (ir_data_ave + 1U != 0) {
      uVar2 = (u32)(red_data_ave * 1000) / (ir_data_ave + 1U);
    }
    param_3[0xe] = uVar2;
  }
  param_3[0xf] = (u32)reflect_abssum;
  param_3[0x10] = bd44_guard;
  param_3[0x11] = DC_refless_data1;
  param_3[9] = (s32)spo2_data[1];
  param_3[10] = (s32)spo2_data[2];
  param_3[4] = Product_old_status;
  param_3[0x12] = (u32)spo2_power_count;
  param_3[0x13] = (u32)all_code_count;
  param_3[0x14] = Product_old_status;
  return;
}

/* ==== BD_HRV_math @ 0xffffff8008600494 ==== */
void BD_HRV_math(u16 param_1,void *param_2,void *param_3)

{
  s32 iVar1;
  u16 uVar2;
  u16 uVar3;
  s32 iVar4;
  s16 asStack_80[46];
  u32 uStack_24;
  u32 uStack_20;
  s32 iStack_1c;
  s32 iStack_18;
  u32 uStack_14;
  u32 uStack_10;
  u16 uStack_c;
  u16 uStack_a;
  u16 uStack_8;
  u16 uStack_6;
  u16 uStack_4;
  s8 cStack_2;
  u8 bStack_1;
  
  asStack_80[0] = 0;
  asStack_80[1] = 0;
  asStack_80[2] = 0;
  asStack_80[3] = 0;
  asStack_80[4] = 0;
  asStack_80[5] = 0;
  asStack_80[6] = 0;
  asStack_80[7] = 0;
  asStack_80[8] = 0;
  asStack_80[9] = 0;
  asStack_80[10] = 0;
  asStack_80[0xb] = 0;
  asStack_80[0xc] = 0;
  asStack_80[0xd] = 0;
  asStack_80[0xe] = 0;
  asStack_80[0xf] = 0;
  asStack_80[0x10] = 0;
  asStack_80[0x11] = 0;
  asStack_80[0x12] = 0;
  asStack_80[0x13] = 0;
  asStack_80[0x14] = 0;
  asStack_80[0x15] = 0;
  asStack_80[0x16] = 0;
  asStack_80[0x17] = 0;
  asStack_80[0x18] = 0;
  asStack_80[0x19] = 0;
  asStack_80[0x1a] = 0;
  asStack_80[0x1b] = 0;
  asStack_80[0x1c] = 0;
  asStack_80[0x1d] = 0;
  asStack_80[0x1e] = 0;
  asStack_80[0x1f] = 0;
  asStack_80[0x20] = 0;
  asStack_80[0x21] = 0;
  asStack_80[0x22] = 0;
  asStack_80[0x23] = 0;
  asStack_80[0x24] = 0;
  asStack_80[0x25] = 0;
  asStack_80[0x26] = 0;
  asStack_80[0x27] = 0;
  if (Product_old_status == 2) {
    uStack_6 = 0;
    iStack_1c = 0;
    uStack_20 = 0;
    for (uStack_4 = 0; uStack_4 < 200; uStack_4 = uStack_4 + 1) {
      if (*(s32 *)(param_2 + (u64)uStack_4 * 4) != 0) {
        iStack_1c = iStack_1c + *(s32 *)(param_2 + (u64)uStack_4 * 4);
        iVar4 = *(s32 *)(param_2 + (u64)uStack_4 * 4);
        iVar1 = -iVar4;
        if (-1 < iVar4) {
          iVar1 = iVar4;
        }
        uStack_20 = uStack_20 + iVar1;
        uStack_6 = uStack_6 + 1;
      }
    }
    if (uStack_6 == 0) {
      uStack_10 = 0;
      iStack_18 = 0;
    }
    else {
      iStack_18 = 0;
      if (uStack_6 != 0) {
        iStack_18 = iStack_1c / (s32)(u32)uStack_6;
      }
      uStack_10 = 0;
      if (uStack_6 != 0) {
        uStack_10 = uStack_20 / uStack_6;
      }
    }
    uStack_6 = 0;
    uStack_20 = 0;
    for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
      if (*(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4) != 0) {
        iVar4 = *(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4);
        iVar1 = -iVar4;
        if (-1 < iVar4) {
          iVar1 = iVar4;
        }
        uStack_20 = uStack_20 + iVar1;
        uStack_6 = uStack_6 + 1;
      }
    }
    if (uStack_6 == 0) {
      uStack_14 = 0;
    }
    else {
      uStack_14 = 0;
      if (uStack_6 != 0) {
        uStack_14 = uStack_20 / uStack_6;
      }
    }
    if ((uStack_10 << 1 < uStack_14) || (uStack_14 < uStack_10 >> 2)) {
      unormal_flag = '\x01';
      unormal_count = 0;
    }
    if (unormal_flag == '\x01') {
      if (unormal_count1 < 6) {
        unormal_count1 = unormal_count1 + 1;
      }
    }
    else if (unormal_count1 < 2) {
      unormal_count1 = 0;
    }
    else {
      unormal_count1 = unormal_count1 - 2;
    }
    if ((unormal_flag == '\0') && (weak_sport_count < 2)) {
      for (uStack_4 = 0; (s32)(u32)uStack_4 < (s32)(800 - (u32)param_1); uStack_4 = uStack_4 + 1)
      {
        *(u8 *)((s64)(s32)(u32)uStack_4 + wave_data2) =
             *(u8 *)((s64)(s32)((u32)uStack_4 + (u32)param_1) + wave_data2);
      }
      for (uStack_4 = 0; uStack_4 < param_1; uStack_4 = uStack_4 + 1) {
        if ((s32)(iStack_18 + (uStack_14 >> 2)) <
            *(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4)) {
          *(u8 *)((s64)(s32)((800 - (u32)param_1) + (u32)uStack_4) + wave_data2) = 0;
        }
        else if (*(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4) <
                 (s32)(iStack_18 - (uStack_14 >> 2))) {
          if ((*(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4 + -4) <
               *(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4)) &&
             ((*(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4 + -4) <
               *(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4 + -8) ||
              ((*(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4 + -4) ==
                *(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4 + -8) &&
               (*(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4 + -4) <
                *(s32 *)(param_2 + (s64)(s32)((200 - (u32)param_1) + (u32)uStack_4) * 4 + -0xc)))
              )))) {
            bStack_1 = 0;
            for (uStack_6 = 0; uStack_6 < 5; uStack_6 = uStack_6 + 1) {
              if (*(s32 *)(param_2 +
                          (s64)(s32)((((u32)uStack_4 - (u32)param_1) + 199) - (u32)uStack_6) * 4
                          ) <
                  *(s32 *)(param_2 +
                          (s64)(s32)((((u32)uStack_4 - (u32)param_1) + 0xc6) - (u32)uStack_6) *
                          4)) {
                bStack_1 = bStack_1 + 1;
              }
            }
            if (bStack_1 < 3) {
              *(u8 *)((s64)(s32)((800 - (u32)param_1) + (u32)uStack_4) + wave_data2) =
                   0;
            }
            else {
              *(u8 *)((s64)(s32)((800 - (u32)param_1) + (u32)uStack_4) + wave_data2) =
                   0x9c;
            }
          }
          else {
            *(u8 *)((s64)(s32)((800 - (u32)param_1) + (u32)uStack_4) + wave_data2) = 0
            ;
          }
        }
        else {
          *(u8 *)((s64)(s32)((800 - (u32)param_1) + (u32)uStack_4) + wave_data2) = 0;
        }
      }
    }
    else {
      for (uStack_6 = 0; uStack_6 < 800; uStack_6 = uStack_6 + 1) {
        *(u8 *)((s64)(s32)(u32)uStack_6 + wave_data2) = 0;
      }
    }
    for (uStack_4 = 0; uStack_4 < 0x28; uStack_4 = uStack_4 + 1) {
      asStack_80[(s32)(u32)uStack_4] = 0;
    }
    uStack_6 = 0;
    cStack_2 = '\0';
    uStack_a = 0;
    uStack_c = 0;
    for (uStack_4 = 0; uStack_4 < 800; uStack_4 = uStack_4 + 1) {
      if (*(s8 *)((s64)(s32)(u32)uStack_4 + wave_data2) == -100) {
        uStack_6 = uStack_6 + 1;
        if (cStack_2 == '\0') {
          uStack_a = uStack_4;
        }
        else {
          uStack_c = uStack_4;
        }
        cStack_2 = -100;
        for (uStack_8 = 0; uStack_8 < 0x27; uStack_8 = uStack_8 + 1) {
          asStack_80[(s32)(u32)uStack_8] = asStack_80[(s32)(uStack_8 + 1)];
        }
        asStack_80[0x27] = uStack_4;
      }
    }
    if (uStack_a < uStack_c) {
      bpm_wavedata = 0;
      if ((u32)uStack_c - (u32)uStack_a != 0) {
        bpm_wavedata =
             (u16)(((uStack_6 - 1) * 0x960) / ((u32)uStack_c - (u32)uStack_a));
      }
      iVar4 = (u32)(u16)asStack_80[0x27] - (u32)(u16)asStack_80[0x26];
      iVar1 = -iVar4;
      if (-1 < iVar4) {
        iVar1 = iVar4;
      }
      (*(s16 *)&uStack_24) = (s16)iVar1 * 0x19;
    }
    else {
      bpm_wavedata = 0;
      (*(s16 *)&uStack_24) = 0;
    }
    if (all_code_count < 8) {
      bpm_disdata = 0;
    }
    else {
      for (uStack_4 = 0; uStack_4 < 7; uStack_4 = uStack_4 + 1) {
        *(u16 *)((char *)bpm_wavedatabuf + (s64)(s32)(u32)uStack_4 * 2) =
             *(u16 *)((char *)bpm_wavedatabuf + (s64)(s32)(uStack_4 + 1) * 2);
      }
      if ((bpm_wavedata == 0) || (0x31 < bpm_wavedata)) {
        if (bpm_wavedata < 0x83) {
          bpm_wavedatabuf[7] = bpm_wavedata;
        }
        else {
          bpm_wavedatabuf[7] = 0x82;
        }
      }
      else {
        bpm_wavedatabuf[7] = 0x32;
      }
      uStack_4 = 0;
      uStack_20 = 0;
      while ((uStack_4 < 8 && (*(s16 *)((char *)bpm_wavedatabuf + (s64)(s32)(7 - (u32)uStack_4) * 2) != 0)
             )) {
        uStack_20 = uStack_20 + *(u16 *)((char *)bpm_wavedatabuf + (s64)(s32)(7 - (u32)uStack_4) * 2);
        uStack_4 = uStack_4 + 1;
      }
      if (uStack_4 == 0) {
        bpm_disdata = 0;
      }
      else {
        bpm_disdata = 0;
        if (uStack_4 != 0) {
          bpm_disdata = (u16)(uStack_20 / uStack_4);
        }
      }
    }
    if (unormal_flag == '\x01') {
      if (unormal_count < 2) {
        unormal_count = unormal_count + 1;
      }
      else {
        unormal_flag = '\0';
      }
    }
    status_flag = '\x01';
    if (all_code_count < 0xb) {
      lcd_disdata = 0;
      hrv_avedata_ms = 0;
      hrv_data_ms = 0;
      hrv_data1_ms = 0;
      hrv_rmssd_data = 0;
    }
    else {
      lcd_disdata = bpm_disdata;
      lcd_flag[0] = 1;
      if (bpm_disdata == 0) {
        hrv_avedata_ms = 0;
      }
      else {
        hrv_avedata_ms = 0;
        if (bpm_disdata != 0) {
          hrv_avedata_ms = (u16)(60000 / bpm_disdata);
        }
      }
      hrv_data_ms = (s16)uStack_24;
      uStack_8 = 0x28;
      uStack_4 = 0;
      uStack_24 = 0;
      max_ms = 0;
      min_ms = 0;
      while ((2 < uStack_8 && (asStack_80[(s32)(uStack_8 - 3)] != 0))) {
        uVar2 = (asStack_80[(s32)(uStack_8 - 1)] - asStack_80[(s32)(uStack_8 - 2)]) * 0x19;
        uVar3 = (asStack_80[(s32)(uStack_8 - 2)] - asStack_80[(s32)(uStack_8 - 3)]) * 0x19;
        iVar4 = (s32)(s16)uVar2 - (s32)(s16)uVar3;
        iVar1 = -iVar4;
        if (-1 < iVar4) {
          iVar1 = iVar4;
        }
        uStack_24 = uStack_24 + iVar1;
        uStack_4 = uStack_4 + 1;
        if (uStack_4 == 1) {
          iVar4 = (s32)(s16)uVar2 - (s32)(s16)uVar3;
          iVar1 = -iVar4;
          if (-1 < iVar4) {
            iVar1 = iVar4;
          }
          max_ms = (u16)iVar1;
          min_ms = max_ms;
          if ((s16)uVar3 < (s16)uVar2) {
            max_heart_ms = uVar2;
            min_heart_ms = uVar3;
          }
          else {
            max_heart_ms = uVar3;
            min_heart_ms = uVar2;
          }
        }
        else {
          iVar4 = (s32)(s16)uVar2 - (s32)(s16)uVar3;
          iVar1 = -iVar4;
          if (-1 < iVar4) {
            iVar1 = iVar4;
          }
          if ((s32)(u32)max_ms < iVar1) {
            iVar4 = (s32)(s16)uVar2 - (s32)(s16)uVar3;
            iVar1 = -iVar4;
            if (-1 < iVar4) {
              iVar1 = iVar4;
            }
            max_ms = (u16)iVar1;
          }
          else {
            iVar4 = (s32)(s16)uVar2 - (s32)(s16)uVar3;
            iVar1 = -iVar4;
            if (-1 < iVar4) {
              iVar1 = iVar4;
            }
            if (iVar1 < (s32)(u32)min_ms) {
              iVar4 = (s32)(s16)uVar2 - (s32)(s16)uVar3;
              iVar1 = -iVar4;
              if (-1 < iVar4) {
                iVar1 = iVar4;
              }
              min_ms = (u16)iVar1;
            }
          }
          if ((s32)(u32)max_heart_ms < (s32)(s16)uVar3) {
            max_heart_ms = uVar3;
          }
          if ((s32)(s16)uVar3 < (s32)(u32)min_heart_ms) {
            min_heart_ms = uVar3;
          }
        }
        uStack_8 = uStack_8 - 1;
      }
      if (uStack_4 < 10) {
        hrv_data1_ms = 0;
      }
      else {
        hrv_data1_ms = 0;
        if (uStack_4 - 2 != 0) {
          hrv_data1_ms =
               (s16)(((uStack_24 - max_ms) - (u32)min_ms) /
                      (uStack_4 - 2));
        }
      }
      if (hrv_data1_ms != 0) {
        for (uStack_6 = 0; uStack_6 < 3; uStack_6 = uStack_6 + 1) {
          *(u16 *)((char *)hrv_disp_buf + (s64)(s32)(u32)uStack_6 * 2) =
               *(u16 *)((char *)hrv_disp_buf + (s64)(s32)(uStack_6 + 1) * 2);
        }
        (*(u16 *)((s8 *)hrv_disp_buf + 6)) = hrv_data1_ms;
      }
      uStack_6 = 4;
      uStack_24 = 0;
      uStack_8 = 0;
      while ((uStack_6 != 0 && (*(s16 *)((char *)hrv_disp_buf + (s64)(s32)(uStack_6 - 1) * 2) != 0))) {
        uStack_24 = uStack_24 + *(u16 *)((char *)hrv_disp_buf + (s64)(s32)(uStack_6 - 1) * 2);
        uStack_8 = uStack_8 + 1;
        uStack_6 = uStack_6 - 1;
      }
      if (uStack_8 == 0) {
        hrv_rmssd_data = 0;
      }
      else {
        hrv_rmssd_data = 0;
        if (uStack_8 != 0) {
          hrv_rmssd_data = (u16)(uStack_24 / uStack_8);
        }
      }
      time_count = (u8)uStack_4;
    }
    if (hrv_rmssd_data < 0x4c) {
      if (hrv_rmssd_data < 2) {
        hrv_press_data = 0;
      }
      else {
        hrv_press_data = 0x55 - hrv_rmssd_data;
      }
    }
    else {
      hrv_press_data = 10;
    }
  }
  else if (Product_old_status == 1) {
    if (status_flag == '\x01') {
      for (uStack_6 = 0; uStack_6 < 200; uStack_6 = uStack_6 + 1) {
        *(u32 *)((char *)green_iir_out + (s64)(s32)(u32)uStack_6 * 4) = 0;
      }
      for (uStack_6 = 0; uStack_6 < 800; uStack_6 = uStack_6 + 1) {
        *(u8 *)((s64)(s32)(u32)uStack_6 + wave_data2) = 0;
      }
      for (uStack_6 = 0; uStack_6 < 8; uStack_6 = uStack_6 + 1) {
        *(u16 *)((char *)bpm_wavedatabuf + (s64)(s32)(u32)uStack_6 * 2) = 0;
      }
      status_flag = '\0';
    }
    for (uStack_6 = 0; uStack_6 < 4; uStack_6 = uStack_6 + 1) {
      *(u16 *)((char *)hrv_disp_buf + (s64)(s32)(u32)uStack_6 * 2) = 0;
    }
    hrv_rmssd_data = 0;
    hrv_press_data = 0;
    lcd_disdata = 0;
    hrv_avedata_ms = 0;
    hrv_data_ms = 0;
    lcd_flag[0] = 0;
    bpm_disdata = 0;
    unormal_flag = '\0';
    unormal_count = 0;
    unormal_count1 = 0;
  }
  return;
}

/* ==== BD1860_Wear_Check @ 0xffffff8008601674 ==== */
void BD1860_Wear_Check(void)

{
  return;
}

/* ==== BD_health_pro @ 0xffffff8008601688 ==== */
u32 BD_health_pro(u32 param_1,u8 param_2,u64 param_3,u64 param_4,
                  u32 *param_5)

{
  u32 uVar1;
  s32 iVar2;
  s32 iVar3;
  s32 iStack_8;
  u8 bStack_1;
  
  if (hardware_flag1 != '\x01') {
    return 0;
  }
  if (all_code_count < 0x7d1) {
    all_code_count = all_code_count + 1;
  }
  else {
    all_code_count = 100;
  }
  if (all_code_count1 < 0x7d1) {
    all_code_count1 = all_code_count1 + 1;
  }
  else {
    all_code_count1 = 100;
  }
  if (param_1 < 5) {
    if (2 < param_1) {
      in_HRV_adcdata(0x14,RAW_A_data,green_iir_out);
      in_gensor_adcdata(param_2,param_3,bd_gensor_buf);
      Wear_judge();
      BD_HRV_math(0x14,green_iir_out,param_5);
      if (param_1 == 3) {
        *param_5 = (u32)hrv_rmssd_data;
        param_5[1] = (u32)hrv_press_data;
      }
      else {
        *param_5 = (u32)hrv_press_data;
        param_5[1] = (u32)hrv_rmssd_data;
      }
      param_5[2] = (u32)hrv_data_ms;
      param_5[3] = (u32)bpm_wavedata;
      param_5[4] = Product_old_status;
      param_5[5] = (u32)hrv_avedata_ms;
      param_5[6] = (u32)unormal_flag;
      param_5[7] = (u32)min_ms;
      param_5[8] = (u32)max_ms;
      param_5[9] = (u32)min_heart_ms;
      param_5[10] = (u32)max_heart_ms;
      param_5[0xb] = (u32)time_count;
      param_5[0xc] = gensor_power;
      param_5[0xd] = (u32)weak_sport_count;
      param_5[0xe] = DC_voltage;
      param_5[0xf] = bd44_guard;
      param_5[0x10] = DC_refless_data1;
      param_5[0x11] = (u32)green_dc_value;
      param_5[0x12] = (u32)reflect_abssum;
      param_5[0x13] = (u32)all_code_count;
      param_5[0x14] = Product_old_status;
      goto LAB_ffffff8008602854;
    }
    if (param_1 == 2) {
      in_spo2test1_adcdata
                (0x14,RAW_A_data,green_iir_out,RAW_B_data,red_iir_out,
                 RAW_C_data,ir_iir_out);
      if (2 < all_code_count) {
        fft_test_bitreverse(green_iir_out,cmplex_out,0x100);
        FFT((u64)(unsigned long)&sin_tab,cmplex_out,0x100);
        fft_mtest1(0x14,(s64)(long)cmplex_out,(s64)(long)green_result,0x100);
        fft_test_bitreverse(red_iir_out,cmplex_out,0x100);
        FFT((u64)(unsigned long)&sin_tab,cmplex_out,0x100);
        fft_mtest1(0x14,(s64)(long)cmplex_out,(s64)(long)red_result,0x100);
        fft_test_bitreverse(ir_iir_out,cmplex_out,0x100);
        FFT((u64)(unsigned long)&sin_tab,cmplex_out,0x100);
        fft_mtest1(0x14,(s64)(long)cmplex_out,(s64)(long)ir_result,0x100);
      }
      Wear_SPO2_judge();
      BD_Spo2test_math(green_result,red_result,ir_result,param_5);
      param_5[4] = Product_old_status;
      param_5[5] = green_dc_power;
      param_5[6] = red_dc_power;
      param_5[7] = ir_dc_power;
      param_5[1] = green_ac_power;
      param_5[2] = red_ac_power;
      param_5[3] = ir_ac_power;
      if (param_5[0xb] == 0) {
        param_5[0xd] = 0;
      }
      else {
        uVar1 = 0;
        if (param_5[0xb] != 0) {
          uVar1 = (param_5[10] * 100) / param_5[0xb];
        }
        param_5[0xd] = uVar1;
      }
      if (param_5[0xc] == 0) {
        param_5[0xe] = 0;
        param_5[0xf] = 0;
      }
      else {
        uVar1 = 0;
        if (param_5[0xc] != 0) {
          uVar1 = (param_5[10] * 100) / param_5[0xc];
        }
        param_5[0xe] = uVar1;
        uVar1 = 0;
        if (param_5[0xc] != 0) {
          uVar1 = (param_5[0xb] * 100) / param_5[0xc];
        }
        param_5[0xf] = uVar1;
      }
      if (red_ac_power == 0) {
        param_5[0x10] = 0;
      }
      else {
        uVar1 = 0;
        if (red_ac_power != 0) {
          uVar1 = (s32)(green_ac_power * 100) / (s32)red_ac_power;
        }
        param_5[0x10] = uVar1;
      }
      if (ir_ac_power == 0) {
        param_5[0x11] = 0;
        param_5[0x12] = 0;
      }
      else {
        uVar1 = 0;
        if (ir_ac_power != 0) {
          uVar1 = (s32)(green_ac_power * 100) / (s32)ir_ac_power;
        }
        param_5[0x11] = uVar1;
        uVar1 = 0;
        if (ir_ac_power != 0) {
          uVar1 = (s32)(red_ac_power * 100) / (s32)ir_ac_power;
        }
        param_5[0x12] = uVar1;
      }
      if (green_dc_power == 0) {
        param_5[0x14] = 0;
        param_5[0x15] = 0;
      }
      else {
        uVar1 = 0;
        if (green_dc_power != 0) {
          uVar1 = (param_5[0xd] * red_dc_power) / green_dc_power;
        }
        param_5[0x14] = uVar1;
        uVar1 = 0;
        if (green_dc_power != 0) {
          uVar1 = (param_5[0xe] * ir_dc_power) / green_dc_power;
        }
        param_5[0x15] = uVar1;
      }
      if (red_dc_power == 0) {
        param_5[0x16] = 0;
      }
      else {
        uVar1 = 0;
        if (red_dc_power != 0) {
          uVar1 = (param_5[0xf] * ir_dc_power) / red_dc_power;
        }
        param_5[0x16] = uVar1;
      }
      iStack_8 = 0;
      for (bStack_1 = 0; bStack_1 < 3; bStack_1 = bStack_1 + 1) {
        *(u32 *)((char *)spo2_data_buf + (s64)(s32)(u32)bStack_1 * 4) =
             *(u32 *)((char *)spo2_data_buf + (s64)(s32)(bStack_1 + 1) * 4);
        iStack_8 = iStack_8 + *(s32 *)((char *)spo2_data_buf + (s64)(s32)(u32)bStack_1 * 4);
      }
      spo2_data_buf[3] = param_5[0x16] % 3 + 0x60;
      uVar1 = iStack_8 + spo2_data_buf[3];
      if ((all_code_count < 0x15) || (Product_old_status == 1)) {
        *param_5 = 0;
      }
      else {
        *param_5 = uVar1 >> 2;
      }
      param_5[1] = uVar1 >> 2;
      param_5[2] = spo2_data_buf[3];
      param_5[0x13] = DC_voltage1;
      param_5[0x17] = (u32)all_code_count;
      goto LAB_ffffff8008602854;
    }
  }
  else if (param_1 == 0xd) {
    BD18XX_Wear_pro(4,RAW_A_data);
    Wear_judge();
    if (Product_old_status == 1) {
      param_5[4] = 1;
      wear_count = 0;
    }
    else {
      if (wear_count < 10) {
        wear_count = wear_count + 1;
      }
      if ((1 < wear_count) || (all_code_count1 < 2)) {
        param_5[4] = 2;
      }
    }
    *param_5 = param_5[4];
    param_5[1] = (u32)all_code_count;
    param_5[2] = 0xd;
    param_5[3] = DC_voltage;
    param_5[5] = DC_voltage1;
    param_5[6] = Product_old_status;
    param_5[0xd] = param_5[4];
    param_5[7] = wear_ref1;
    param_5[8] = wear_ref2;
    param_5[9] = wear_ref3;
    param_5[10] = (u32)led0_current;
    param_5[0xb] = (u32)led1_current;
    param_5[0xc] = (u32)led2_current;
    goto LAB_ffffff8008602854;
  }
  in_BPM_adcdata(param_1,0x14,RAW_A_data,green_iir_out);
  in_gensor_adcdata(param_2,param_3,bd_gensor_buf);
  fft_bitreverse(bd_gensor_buf,cmplex_gensor_out,0x80);
  FFT((u64)(unsigned long)&gen_sin_tab,cmplex_gensor_out,0x80);
  fft_mtest1(0x14,(s64)(long)cmplex_gensor_out,(s64)(long)gensor_freq_buf,0x80);
  fft_test_bitreverse(green_iir_out,cmplex_out,0x100);
  FFT((u64)(unsigned long)&sin_tab,cmplex_out,0x100);
  fft_mtest1(0x14,(s64)(long)cmplex_out,(s64)(long)param_5,0x100);
  green_dc_power = DC_power[0];
  Product_state_judge2(param_5);
  Wear_judge();
  Gensor_State_Read(gensor_state);
  Heart_real_math(gensor_freq_buf,param_5);
  lcd_data = BP_blood_user(param_5[4]);
  param_5[5] = *param_5;
  param_5[6] = param_5[1];
  param_5[7] = param_5[2];
  param_5[8] = param_5[3];
  *param_5 = (u32)lcd_data;
  if (lcd_data < 0x32) {
    param_5[1] = 0;
  }
  else if (lcd_data < 0x41) {
    param_5[1] = 0x72;
  }
  else if (lcd_data < 0x74) {
    iVar3 = lcd_data - 0x41;
    iVar2 = -iVar3;
    if (-1 < iVar3) {
      iVar2 = iVar3;
    }
    param_5[1] = iVar2 / 3 + 0x73;
  }
  else {
    param_5[1] = 0x84;
  }
  if (param_5[1] == 0) {
    param_5[2] = 0;
  }
  else {
    iVar3 = param_5[1] * 3 + -0x10e;
    iVar2 = -iVar3;
    if (-1 < iVar3) {
      iVar2 = iVar3;
    }
    param_5[2] = iVar2 / 5 + 0x3c;
  }
  if (0x27 < *param_5) {
    if ((*param_5 < 0x28) || (0x3c < *param_5)) {
      iVar3 = *param_5 * 0xe + -0x348;
      iVar2 = -iVar3;
      if (-1 < iVar3) {
        iVar2 = iVar3;
      }
      power_kal = iVar2 + power_kal + 0x48;
    }
    else {
      power_kal = power_kal + 0x48;
    }
  }
  param_5[3] = (u32)Run_data;
  if (Product_old_status == 1) {
    param_5[4] = 1;
    wear_count = 0;
  }
  else {
    if (wear_count < 10) {
      wear_count = wear_count + 1;
    }
    if ((1 < wear_count) || (all_code_count1 < 2)) {
      param_5[4] = 2;
    }
  }
  param_5[9] = gensor_power;
  param_5[10] = gensor_freq_buf[0];
  param_5[0xb] = gensor_freq_buf[1];
  param_5[0xc] = gensor_freq_buf[2];
  param_5[0xd] = (u32)speed_state_buf[7];
  param_5[0xe] = (u32)test_pro_state;
  param_5[0xf] = (u32)gensor_state_count;
  param_5[0x10] = (u32)heart_stable_count;
  param_5[0x11] = (u32)bp_gensor_flag;
  param_5[0x12] = DC_voltage;
  param_5[0x13] = DC_voltage1;
  param_5[0x14] = (u32)all_code_count;
  param_5[0x15] = (u32)Run_data;
  param_5[0x16] = Product_old_status;
  param_5[0x17] = wear_ref1;
  param_5[0x18] = (u32)ch_A_expot;
LAB_ffffff8008602854:
  return param_1 & 0xff;
}

/* ---- debug accessors (driver shell r43-raw line) ---- */
/* r46 fix attempt read RAW_A_data -- but RAW_A_data[0..19] is itself a
 * per-frame copy of RAW_A_buf (alg line ~1543), and RAW_A_buf is never
 * written: zombie chain, ppgA stayed 0. r47: raw_a reads the LIVE
 * per-sample ch_A_buf (written at alg line ~1308); dc_a = RAW_A_data[20]
 * per-frame DC accumulator (live). green_iir_out only fills in HRV/BPM
 * paths; its accessor is kept for ABI. */
u32 bd44_dbg_raw_a(int i)
{
	if (i < 0 || i > 27)
		return 0;
	return (u32)ch_A_buf[i];
}

u32 bd44_dbg_dc_a(void) { return RAW_A_data[20]; }

u16 bd44_dbg_cur(int ch)
{
    return ch == 0 ? ch_A_expot : (ch == 1 ? ch_B_expot : ch_C_expot);
}

/* r51: full-array accessor for the DC-chain forensics dump (bd1860.c).
 * DC_voltage  = sum(RAW_A_data[0..19])/20  (in_BPM_adcdata uStack_8)
 * DC_voltage1 = sum(RAW_B_data[0..19])/20  (in_BPM_adcdata uStack_c)
 * r50 live read: DC_voltage1 = 83M -- impossible for a 24-bit PPG join
 * (max 0xFFFFFF = 16.7M). Dump the arrays to find the polluter. */
u32 bd44_dbg_raw_arr(int ch, int i)
{
	if (i < 0 || i > 25)
		return 0;
	if (ch == 0)
		return RAW_A_data[i];
	if (ch == 1)
		return RAW_B_data[i];
	return RAW_C_data[i];
}

/* r52: write-side (RAW_x_buf) accessor + phdata_count export.
 * r51 live evidence: RAW_A_data[0..19] shows a stride-4 fill pattern
 * (values only at slots 0/4/8/12/16, everything else 0) and RAW_B_data
 * contains impossible >24-bit values (0x2Bxxxxxx ~736M, max legal join
 * = 0xFFFFFF). The data-side copy loop is provably faithful to factory
 * assembly, so the pollution must enter at the RAW_x_buf write side or
 * the phdata_count index. Dump both sides to catch it red-handed. */
u32 bd44_dbg_buf_arr(int ch, int i)
{
	if (i < 0 || i > 25)
		return 0;
	if (ch == 0)
		return RAW_A_buf[i];
	if (ch == 1)
		return RAW_B_buf[i];
	return RAW_C_buf[i];
}

u8 bd44_dbg_phcount(void)
{
	return phdata_count;
}

u32 bd44_dbg_dcvolt(int which)
{
	return which == 0 ? DC_voltage : DC_voltage1;
}
