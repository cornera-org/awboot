/*
 * Allwinner T113-sx CPUCFG and PRCM register definitions
 *
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __T113_SUNXI_CPUCFG_H__
#define __T113_SUNXI_CPUCFG_H__

#define SYS_CONTROL_REG_BASE	0x03000000u
#define SYS_SID_BASE					0x03006000u

#define SUNXI_R_CPUCFG_BASE	0x07000400u
#define SUNXI_R_CPUCFG_SUP_STAN_FLAG (0x1d4)

#define SUNXI_R_PRCM_BASE		0x07010000u

#define SUNXI_PRCM_CPU_SOFT_ENTRY_REG	(SUNXI_R_PRCM_BASE + 0x0164u)
#define SUNXI_CPUCFG_BASE			0x09010000u
#define SUNXI_CPUMBIST_BASE		0x09020000u

#endif /* __T113_SUNXI_CPUCFG_H__ */
