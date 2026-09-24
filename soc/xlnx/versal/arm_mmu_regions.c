/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/arch/arm64/arm_mmu.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#define DEVICE_ATTR (MT_DEVICE_nGnRnE | MT_P_RW_U_NA | MT_DEFAULT_SECURE_STATE)

static const struct arm_mmu_region mmu_regions[] = {
	MMU_REGION_DT_COMPAT_FOREACH_FLAT_ENTRY(cdns_i2c, DEVICE_ATTR)
};

const struct arm_mmu_config mmu_config = {
	.num_regions = ARRAY_SIZE(mmu_regions),
	.mmu_regions = mmu_regions,
};
