#
# Copyright (c) 2025-2026 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

if(CONFIG_BUILD_WITH_TFA)
  set(TFA_PLAT "versal2")
  # Add Versal Gen 2 specific TF-A build parameters
  set(TFA_EXTRA_ARGS "RESET_TO_BL31=1;PRELOADED_BL33_BASE=${CONFIG_SRAM_BASE_ADDRESS};TFA_NO_PM=1;")
  if(CONFIG_TFA_MAKE_BUILD_TYPE_DEBUG)
    set(BUILD_FOLDER "debug")
  else()
    set(BUILD_FOLDER "release")
  endif()
  set(XSDB_BL31_PATH ${PROJECT_BINARY_DIR}/../tfa/versal2/${BUILD_FOLDER}/bl31/bl31.elf)
  board_runner_args(xsdb "--bl31=${XSDB_BL31_PATH}")
endif()

include(${ZEPHYR_BASE}/boards/common/xsdb.board.cmake)
set(SUPPORTED_EMU_PLATFORMS qemu)
set(QEMU_ARCH xilinx-aarch64)
set(QEMU_CPU_TYPE_${ARCH} cortexa78)

set(QEMU_FLAGS_${ARCH}
  -machine arm-generic-fdt
  -hw-dtb ${PROJECT_BINARY_DIR}/${BOARD}-qemu.dtb
  -device loader,addr=0xEC200300,data=0x3EE,data-len=4
  -device loader,addr=0xEC200300,data=0x3EE,data-len=4
  -nographic
  -m 2g
)

# Add SMP support if configured maxcpus parameter value is aligned with QEMU DT
if(CONFIG_SMP AND CONFIG_MP_MAX_NUM_CPUS GREATER 1)
  list(APPEND QEMU_SMP_FLAGS -smp maxcpus=18)
endif()

set(QEMU_KERNEL_OPTION
  -device loader,cpu-num=0,file=${PROJECT_BINARY_DIR}/../tfa/versal2/${BUILD_FOLDER}/bl31/bl31.elf
  -device loader,file=\$<TARGET_FILE:\${logical_target_for_zephyr_elf}>
)
