/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PRANDOM_H
#define _LINUX_PRANDOM_H

/*
 * W527 4.14.336-merge shim.
 *
 * Upstream created this file after 4.14.98 by splitting the prandom API
 * out of <linux/random.h>.  The W527 vendor tree is 4.14.98-based and its
 * ABI (and CRCs of the exported prandom symbols) follow the 4.14.98
 * declarations that still live in <linux/random.h>.  Keep this header a
 * thin wrapper around <linux/random.h> so that merged 4.14.336 sources
 * which #include <linux/prandom.h> keep compiling against the vendor ABI.
 *
 * The only post-4.14.98 addition that merged sources actually use is
 * prandom_u32_max(); it is a pure inline helper on top of prandom_u32()
 * and introduces no new exported symbol, so it is reproduced here.
 */

#include <linux/random.h>

/**
 * prandom_u32_max - returns a pseudo-random number in interval [0, ep_ro)
 * @ep_ro: right open interval endpoint
 *
 * Returns a pseudo-random number that is in interval [0, ep_ro). This
 * uses the maximally equidistributed combined Tausworthe generator
 * prandom_u32(). For a more general solution see random.h's
 * get_random_u32_above/below helpers or use prandom_u32() directly.
 */
static inline u32 prandom_u32_max(u32 ep_ro)
{
	return (u32)(((u64) prandom_u32() * ep_ro) >> 32);
}

#endif /* _LINUX_PRANDOM_H */
