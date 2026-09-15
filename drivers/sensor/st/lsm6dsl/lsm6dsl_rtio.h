/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_LSM6DSL_LSM6DSL_RTIO_H_
#define ZEPHYR_DRIVERS_SENSOR_LSM6DSL_LSM6DSL_RTIO_H_

#include <zephyr/device.h>
#include <zephyr/rtio/rtio.h>

void lsm6dsl_submit(const struct device *dev, struct rtio_iodev_sqe *iodev_sqe);

#endif /* ZEPHYR_DRIVERS_SENSOR_LSM6DSL_LSM6DSL_RTIO_H_ */
