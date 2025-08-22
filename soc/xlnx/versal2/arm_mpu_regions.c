/*
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/arch/arm/mpu/arm_mpu.h>

#define DEVICE_REGION_START	0xE2000000U
#define DEVICE_REGION_END	0xF8000000U
#define DEVICE_LPD_PL_START	0x80000000U
#define DEVICE_LPD_PL_END	0x9FFFFFFFU
#define DEVICE_PCIE_START	0xA0000000U
#define DEVICE_PCIE_END		0xB7FFFFFFU
#define DEVICE_OSPI_START	0xC0000000U
#define DEVICE_OSPI_END		0xE0FFFFFFU

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ocm), okay)
#define OCM_START	DT_REG_ADDR_BY_IDX(DT_NODELABEL(ocm), 0)
#define OCM_END		(OCM_START + DT_REG_SIZE_BY_IDX(DT_NODELABEL(ocm), 0))
#endif

#if DT_HAS_CHOSEN(zephyr_ipc_shm)
/* Constants derived from device tree */
#define SHM_NODE		DT_CHOSEN(zephyr_ipc_shm)
#define SHARED_MEM_START	DT_REG_ADDR(SHM_NODE)
#define SHARED_MEM_END		(SHARED_MEM_START + DT_REG_SIZE(SHM_NODE))
#endif /* DT_HAS_CHOSEN(zephyr_ipc_shm) */

static const struct arm_mpu_region mpu_regions[] = {
	MPU_REGION_ENTRY("vector",
			 (uintptr_t)_vector_start,
			 REGION_RAM_TEXT_ATTR((uintptr_t)_vector_end)),

	MPU_REGION_ENTRY("SRAM_TEXT",
			 (uintptr_t)__text_region_start,
			 REGION_RAM_TEXT_ATTR((uintptr_t)__rodata_region_start)),

	MPU_REGION_ENTRY("SRAM_RODATA",
			 (uintptr_t)__rodata_region_start,
			 REGION_RAM_RO_ATTR((uintptr_t)__rodata_region_end)),

	MPU_REGION_ENTRY("SRAM_DATA",
			 (uintptr_t)__rom_region_end,
			 REGION_RAM_ATTR((uintptr_t)__kernel_ram_end)),

	MPU_REGION_ENTRY("DEVICE",
			 DEVICE_REGION_START,
			 REGION_DEVICE_ATTR(DEVICE_REGION_END)),
#if DT_HAS_CHOSEN(zephyr_ipc_shm)
	MPU_REGION_ENTRY("SHARED_MEM",
			 (uintptr_t)SHARED_MEM_START,
			 REGION_RAM_ATTR(SHARED_MEM_END)),
#endif /* DT_HAS_CHOSEN(zephyr_ipc_shm) */

	MPU_REGION_ENTRY("DEVICE_LPD_PL",
			 DEVICE_LPD_PL_START,
			 REGION_DEVICE_ATTR(DEVICE_LPD_PL_END)),

	MPU_REGION_ENTRY("DEVICE_PCIE",
			 DEVICE_PCIE_START,
			 REGION_DEVICE_ATTR(DEVICE_PCIE_END)),

	MPU_REGION_ENTRY("DEVICE_OSPI",
			 DEVICE_OSPI_START,
			 REGION_DEVICE_ATTR(DEVICE_OSPI_END)),

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ocm), okay)
	MPU_REGION_ENTRY("OCM",
			 OCM_START,
			 REGION_RAM_TEXT_ATTR(OCM_END)),
#endif
};

const struct arm_mpu_config mpu_config = {
	.num_regions = ARRAY_SIZE(mpu_regions),
	.mpu_regions = mpu_regions,
};
