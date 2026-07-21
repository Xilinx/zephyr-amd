# Copyright (c) 2026 Advanced Micro Devices, Inc.
#
# SPDX-License-Identifier: Apache-2.0

find_package(Dtc 1.4.6 REQUIRED)

if(EXISTS "${BOARD_DIR}/${BOARD}-qemu.dts")
  if(DTC_FOUND)
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
      COMMAND ${DTC} -I dts -O dtb -q "${BOARD_DIR}/${BOARD}-qemu.dts"
                     -o "${PROJECT_BINARY_DIR}/${BOARD}-qemu.dtb"
    )
  else()
    message(FATAL_ERROR "DTC not found, required for compiling ${BOARD}-qemu.dts")
  endif()
endif()
