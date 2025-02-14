#
# Copyright (c) 2024 - 2025 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

include(${ZEPHYR_BASE}/boards/common/xsdb.board.cmake)
set(SUPPORTED_EMU_PLATFORMS qemu)
set(QEMU_ARCH xilinx-riscv32)
set(QEMU_CPU_TYPE_${ARCH} riscv32)

set(QEMU_FLAGS_${ARCH}
	-machine riscv-fdt
	-hw-dtb ${PROJECT_BINARY_DIR}/${BOARD}-qemu.dtb
	-nographic
	-net nic,netdev=eth0 -netdev user,id=eth0
	-m 2g
)

set(QEMU_KERNEL_OPTION
	-device loader,cpu-num=0,file=\$<TARGET_FILE:\${logical_target_for_zephyr_elf}>
)
