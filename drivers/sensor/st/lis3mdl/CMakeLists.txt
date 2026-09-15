# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(lis3mdl.c)
zephyr_library_sources_ifdef(CONFIG_LIS3MDL_TRIGGER lis3mdl_trigger.c)
zephyr_library_sources_ifdef(CONFIG_SENSOR_ASYNC_API lis3mdl_rtio.c lis3mdl_decoder.c)
