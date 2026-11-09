// SPDX-License-Identifier: GPL-2.0
/*
 * W527 compat: closed-source vendor modules (built against 4.14.98) call
 * the out-of-line ll_sc cmpxchg helpers with byte-size suffixes
 * (__cmpxchg_case_mb_4 etc).  Stable backports renamed them to bit-width
 * suffixes (__cmpxchg_case_mb_32 etc) with identical semantics
 * (4 bytes == 32 bits).  Provide the old names via tail-call wrappers
 * so the modules resolve on this kernel.
 */
#include <linux/export.h>

extern unsigned long __ll_sc___cmpxchg_case_mb_32(volatile void *ptr,
						  unsigned long old,
						  unsigned long new);

unsigned long __ll_sc___cmpxchg_case_mb_4(volatile void *ptr,
					  unsigned long old,
					  unsigned long new)
{
	return __ll_sc___cmpxchg_case_mb_32(ptr, old, new);
}
EXPORT_SYMBOL(__ll_sc___cmpxchg_case_mb_4);
