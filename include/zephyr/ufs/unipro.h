/*
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_UFS_UNIPRO_H_
#define ZEPHYR_INCLUDE_UFS_UNIPRO_H_

/* PHY Adapter Attributes */
#define PA_ACTIVETXDATALANES	0x1560
#define PA_CONNECTEDTXDATALANES	0x1561
#define PA_TXGEAR		0x1568
#define PA_TXTERMINATION	0x1569
#define PA_HSSERIES		0x156A
#define PA_PWRMODE		0x1571
#define PA_ACTIVERXDATALANES	0x1580
#define PA_CONNECTEDRXDATALANES	0x1581
#define PA_RXGEAR		0x1583
#define PA_RXTERMINATION	0x1584
#define PA_MAXRXPWMGEAR		0x1586
#define PA_MAXRXHSGEAR		0x1587
#define PA_TXHSADAPTTYPE	0x15D4

/* Adapt type for PA_TXHSADAPTTYPE attribute */
#define PA_REFRESH_ADAPT       0x00
#define PA_INITIAL_ADAPT       0x01
#define PA_NO_ADAPT            0x03

/* Transport Layer Attributes */
#define T_CONNECTIONSTATE	0x4020

/* Other Attributes */
#define VS_MPHYCFGUPDT		0xD085
#define VS_MPHYDISABLE		0xD0C1

/* PA power modes */
enum ufs_pa_pwr_mode {
	FAST_MODE	= 1,
	SLOW_MODE	= 2,
	FASTAUTO_MODE	= 4,
	SLOWAUTO_MODE	= 5,
	UNCHANGED	= 7,
};

/* PA TX/RX Frequency Series */
enum ufs_hs_gear_rate {
	PA_HS_MODE_A	= 1,
	PA_HS_MODE_B	= 2,
};

enum ufs_pwm_gear_tag {
	UFS_PWM_DONT_CHANGE,	/* Don't change Gear */
	UFS_PWM_G1,		/* PWM Gear 1 (default for reset) */
	UFS_PWM_G2,		/* PWM Gear 2 */
	UFS_PWM_G3,		/* PWM Gear 3 */
	UFS_PWM_G4,		/* PWM Gear 4 */
	UFS_PWM_G5,		/* PWM Gear 5 */
	UFS_PWM_G6,		/* PWM Gear 6 */
	UFS_PWM_G7,		/* PWM Gear 7 */
};

enum ufs_hs_gear_tag {
	UFS_HS_DONT_CHANGE,	/* Don't change Gear */
	UFS_HS_G1,		/* HS Gear 1 (default for reset) */
	UFS_HS_G2,		/* HS Gear 2 */
	UFS_HS_G3,		/* HS Gear 3 */
	UFS_HS_G4,		/* HS Gear 4 */
	UFS_HS_G5		/* HS Gear 5 */
};

enum ufs_lanes {
	UFS_LANE_DONT_CHANGE,	/* Don't change Lane */
	UFS_LANE_1,		/* Lane 1 (default for reset) */
	UFS_LANE_2,		/* Lane 2 */
};

#endif /* ZEPHYR_INCLUDE_UFS_UNIPRO_H_ */
