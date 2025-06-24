#
# Copyright (c) 2025 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

include(${ZEPHYR_BASE}/boards/common/xsdb.board.cmake)
set(SUPPORTED_EMU_PLATFORMS qemu)
set(QEMU_ARCH xilinx-aarch64)
set(QEMU_CPU_TYPE_${ARCH} cortex-r52)

set(QEMU_FLAGS_${ARCH}
	-machine arm-generic-fdt
	-hw-dtb ${PROJECT_BINARY_DIR}/${BOARD}-qemu.dtb
	-device loader,addr=0xeb580000,data=0x1,data-len=4 -device loader,addr=0xeb5e0310,data=0xE,data-len=4
	-nographic
	-net nic,netdev=eth0 -netdev user,id=eth0 -net nic,netdev=eth1 -netdev user,id=eth1
	-m 2g
)

set(QEMU_KERNEL_OPTION
	-device loader,cpu-num=16,file=\$<TARGET_FILE:\${logical_target_for_zephyr_elf}>
)
