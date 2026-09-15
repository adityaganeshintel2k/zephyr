/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_LSM6DSL_LSM6DSL_DECODER_H_
#define ZEPHYR_DRIVERS_SENSOR_LSM6DSL_LSM6DSL_DECODER_H_

#include <stdint.h>
#include <zephyr/drivers/sensor.h>

/* One raw accel+gyro reading, in the sensor's native LSB units */
struct lsm6dsl_rtio_sample {
	int16_t accel[3];
	int16_t gyro[3];
} __packed;

/*
 * Buffer produced by lsm6dsl_submit() and consumed by the decoder.
 * Packed because the backing buffer (app-provided or RTIO mempool) is not
 * guaranteed to be aligned for the uint64_t timestamp field, and unaligned
 * 64-bit access faults on Cortex-M.
 */
struct lsm6dsl_encoded_data {
	uint64_t timestamp;
	float accel_sensitivity;
	float gyro_sensitivity;
	uint8_t num_samples;
	struct lsm6dsl_rtio_sample samples[CONFIG_LSM6DSL_RTIO_SAMPLE_COUNT];
} __packed;

int lsm6dsl_get_decoder(const struct device *dev, const struct sensor_decoder_api **decoder);

#endif /* ZEPHYR_DRIVERS_SENSOR_LSM6DSL_LSM6DSL_DECODER_H_ */
