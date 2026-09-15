/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_LIS3MDL_LIS3MDL_RTIO_H_
#define ZEPHYR_DRIVERS_SENSOR_LIS3MDL_LIS3MDL_RTIO_H_

#include <zephyr/device.h>
#include <zephyr/rtio/rtio.h>

void lis3mdl_submit(const struct device *dev, struct rtio_iodev_sqe *iodev_sqe);

#endif /* ZEPHYR_DRIVERS_SENSOR_LIS3MDL_LIS3MDL_RTIO_H_ */
