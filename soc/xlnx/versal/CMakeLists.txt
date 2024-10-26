# Copyright (c) 2024 Northrop Grumman
#
# SPDX-License-Identifier: Apache-2.0

zephyr_sources(
  soc.c
)
zephyr_sources_ifdef(
  CONFIG_ARM_MPU
  arm_mpu_regions.c
)

zephyr_include_directories(.)

if(CONFIG_SOC_XILINX_VERSAL_RPU)
  set(SOC_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld CACHE INTERNAL "")
endif()
