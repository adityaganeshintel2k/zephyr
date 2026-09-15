/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_LIS3MDL_LIS3MDL_DECODER_H_
#define ZEPHYR_DRIVERS_SENSOR_LIS3MDL_LIS3MDL_DECODER_H_

#include <stdint.h>
#include <zephyr/drivers/sensor.h>

/* One raw magnetometer reading, in the sensor's native LSB units */
struct lis3mdl_rtio_sample {
	int16_t mag[3];
} __packed;

/*
 * Buffer produced by lis3mdl_submit() and consumed by the decoder.
 * Packed because the backing buffer isn't guaranteed to be aligned for the
 * uint64_t timestamp field, and unaligned 64-bit access faults on Cortex-M.
 */
struct lis3mdl_encoded_data {
	uint64_t timestamp;
	uint16_t magn_gain;
	uint8_t num_samples;
	struct lis3mdl_rtio_sample samples[CONFIG_LIS3MDL_RTIO_SAMPLE_COUNT];
} __packed;

int lis3mdl_get_decoder(const struct device *dev, const struct sensor_decoder_api **decoder);

#endif /* ZEPHYR_DRIVERS_SENSOR_LIS3MDL_LIS3MDL_DECODER_H_ */
