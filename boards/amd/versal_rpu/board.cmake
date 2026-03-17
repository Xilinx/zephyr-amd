# Copyright (c) 2026, Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: Apache-2.0

include(${ZEPHYR_BASE}/boards/common/xsdb.board.cmake)
set(SUPPORTED_EMU_PLATFORMS qemu)
set(QEMU_ARCH xilinx-aarch64)
set(QEMU_CPU_TYPE_${ARCH} cortex-r5f)

set(QEMU_FLAGS_${ARCH}
  -machine arm-generic-fdt
  -hw-dtb ${PROJECT_BINARY_DIR}/${BOARD}-qemu.dtb
  -device loader,addr=0xff5e0300,data=0x16,data-len=4
  -device loader,addr=0xff9a0100,data=0x1,data-len=4
  -device loader,addr=0xff9a0000,data=0x48,data-len=4
  -nographic
  -m 2g
)

set(QEMU_KERNEL_OPTION
  -device loader,cpu-num=2,file=\$<TARGET_FILE:\${logical_target_for_zephyr_elf}>
)
