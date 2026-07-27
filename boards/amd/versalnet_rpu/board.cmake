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

if(CONFIG_SDMMC_STACK OR CONFIG_MMC_STACK)
  set(QEMU_SD_IMG ${ZEPHYR_BINARY_DIR}/sd.img)

  # The SD/eMMC QEMU image is generated with the host tools 'dd' and
  # 'mkfs.vfat' (from dosfstools). These are not provided by the Zephyr SDK,
  # so they must be available on the host running QEMU. Guard for them here so
  # a missing tool produces a clear configure-time error instead of an obscure
  # failure at the PRE_QEMU stage. The guard only applies to the SD/eMMC tests.
  find_program(DD_EXE dd)
  find_program(MKFS_VFAT_EXE mkfs.vfat)
  if(NOT DD_EXE OR NOT MKFS_VFAT_EXE)
    message(FATAL_ERROR
      "SD/eMMC QEMU tests require the host tools 'dd' and 'mkfs.vfat' "
      "(dosfstools) to create the QEMU SD image. Please install them.")
  endif()

  list(APPEND PRE_QEMU_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E remove -f ${QEMU_SD_IMG}
    COMMAND ${DD_EXE} if=/dev/zero of=${QEMU_SD_IMG} bs=256M count=1
    COMMAND ${MKFS_VFAT_EXE} ${QEMU_SD_IMG}
  )

  if(CONFIG_SDMMC_STACK)
    list(APPEND QEMU_EXTRA_FLAGS
      -drive file=${QEMU_SD_IMG},if=sd,format=raw,index=0
    )
  endif()

  if(CONFIG_MMC_STACK)
    list(APPEND QEMU_EXTRA_FLAGS
      -drive file=${QEMU_SD_IMG},if=sd,format=raw,index=3
    )
  endif()
endif()

set(QEMU_KERNEL_OPTION
  -device loader,cpu-num=16,file=\$<TARGET_FILE:\${logical_target_for_zephyr_elf}>
)
