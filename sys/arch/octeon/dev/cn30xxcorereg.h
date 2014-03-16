/*	$OpenBSD: cn30xxcorereg.h,v 1.1 2014/03/13 02:17:13 yasuoka Exp $	*/

/*
 * Copyright (c) 2014 YASUOKA Masahiko <yasuoka@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _CN30XXCOREREG_H_
#define _CN30XXCOREREG_H_

/*
 * Cavium Networks OCTEON CN30XX Hardware Reference Manual CN30XX-HM-1.0
 * 4. cnMIPS Cores
 */

/* 4.11 Core Coprocessor 0 Privileged Registers */

/* CvmCtl Registers */
#define COP_0_CVMCTL_FUSE_START_BIT	0x80000000
#define COP_0_CVMCTL_NOFDA_CP2		0x10000000
#define COP_0_CVMCTL_NOMUL		0x08000000
#define COP_0_CVMCTL_NOCRYPTO		0x04000000
#define COP_0_CVMCTL_RST_SHT		0x02000000
#define COP_0_CVMCTL_BIST_DIS		0x01000000
#define COP_0_CVMCTL_DISSETPRED		0x00800000
#define COP_0_CVMCTL_DISJRPRED		0x00400000
#define COP_0_CVMCTL_DISICACHE		0x00200000
#define COP_0_CVMCTL_DISWAIT		0x00100000
#define COP_0_CVMCTL_DEFET		0x00080000
#define COP_0_CVMCTL_DISCO		0x00040000
#define COP_0_CVMCTL_DISCE		0x00020000
#define COP_0_CVMCTL_DDCLK		0x00010000
#define COP_0_CVMCTL_DCICLK		0x00008000
#define COP_0_CVMCTL_REPUN		0x00004000
#define COP_0_CVMCTL_IPREF		0x00002000
#define COP_0_CVMCTL_USEUN		0x00001000
#define COP_0_CVMCTL_DISIOCACHE		0x00000800
#define COP_0_CVMCTL_IRAND		0x00000400
#define COP_0_CVMCTL_IPPCI		0x00000380
#define COP_0_CVMCTL_IPTI		0x00000070
#define COP_0_CVMCTL_LE			0x00000002
#define COP_0_CVMCTL_USELY		0x00000001

#endif /* _CN30XXCOREREG_H_ */
