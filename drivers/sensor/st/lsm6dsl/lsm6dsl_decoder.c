/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT st_lsm6dsl

#include "lsm6dsl_decoder.h"

#include <zephyr/sys/util.h>

/*
 * Fixed, conservative shifts covering the full LSM6DSL FS range without
 * overflow (16g accel and 2000dps gyro): kept simple since this decoder only
 * needs to be good enough to validate the RTIO read path.
 */
#define LSM6DSL_ACCEL_SHIFT 8
#define LSM6DSL_GYRO_SHIFT  6

static void lsm6dsl_accel_raw_to_q31(int16_t raw, float sensitivity, q31_t *out)
{
	struct sensor_value val;
	int64_t dval = (int64_t)raw * sensitivity;
	int64_t intermediate;

	sensor_ug_to_ms2(dval, &val);

	intermediate = (int64_t)val.val1 * INT64_C(1000000) + val.val2;
	intermediate = intermediate * ((int64_t)INT32_MAX + 1) /
		       ((1 << LSM6DSL_ACCEL_SHIFT) * INT64_C(1000000));
	*out = CLAMP(intermediate, INT32_MIN, INT32_MAX);
}

static void lsm6dsl_gyro_raw_to_q31(int16_t raw, float sensitivity, q31_t *out)
{
	struct sensor_value val;
	int64_t dval = (int64_t)raw * sensitivity / 10;
	int64_t intermediate;

	sensor_10udegrees_to_rad(dval, &val);

	intermediate = (int64_t)val.val1 * INT64_C(1000000) + val.val2;
	intermediate = intermediate * ((int64_t)INT32_MAX + 1) /
		       ((1 << LSM6DSL_GYRO_SHIFT) * INT64_C(1000000));
	*out = CLAMP(intermediate, INT32_MIN, INT32_MAX);
}

static int lsm6dsl_decoder_get_frame_count(const uint8_t *buffer,
					   struct sensor_chan_spec chan_spec,
					   uint16_t *frame_count)
{
	const struct lsm6dsl_encoded_data *edata = (const struct lsm6dsl_encoded_data *)buffer;

	if (chan_spec.chan_idx != 0) {
		return -ENOTSUP;
	}

	switch (chan_spec.chan_type) {
	case SENSOR_CHAN_ACCEL_XYZ:
	case SENSOR_CHAN_GYRO_XYZ:
		*frame_count = edata->num_samples;
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int lsm6dsl_decoder_get_size_info(struct sensor_chan_spec chan_spec, size_t *base_size,
					 size_t *frame_size)
{
	switch (chan_spec.chan_type) {
	case SENSOR_CHAN_ACCEL_XYZ:
	case SENSOR_CHAN_GYRO_XYZ:
		*base_size = sizeof(struct sensor_three_axis_data);
		*frame_size = sizeof(struct sensor_three_axis_sample_data);
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int lsm6dsl_decoder_decode(const uint8_t *buffer, struct sensor_chan_spec chan_spec,
				  uint32_t *fit, uint16_t max_count, void *data_out)
{
	const struct lsm6dsl_encoded_data *edata = (const struct lsm6dsl_encoded_data *)buffer;
	struct sensor_three_axis_data *out = data_out;
	bool is_accel;
	uint16_t count = 0;

	if (chan_spec.chan_idx != 0) {
		return -ENOTSUP;
	}

	switch (chan_spec.chan_type) {
	case SENSOR_CHAN_ACCEL_XYZ:
		is_accel = true;
		out->shift = LSM6DSL_ACCEL_SHIFT;
		break;
	case SENSOR_CHAN_GYRO_XYZ:
		is_accel = false;
		out->shift = LSM6DSL_GYRO_SHIFT;
		break;
	default:
		return -ENOTSUP;
	}

	out->header.base_timestamp_ns = edata->timestamp;

	while (*fit < edata->num_samples && count < max_count) {
		const struct lsm6dsl_rtio_sample *sample = &edata->samples[*fit];

		out->readings[count].timestamp_delta = 0;

		for (int i = 0; i < 3; i++) {
			if (is_accel) {
				lsm6dsl_accel_raw_to_q31(sample->accel[i],
							  edata->accel_sensitivity,
							  &out->readings[count].values[i]);
			} else {
				lsm6dsl_gyro_raw_to_q31(sample->gyro[i], edata->gyro_sensitivity,
							&out->readings[count].values[i]);
			}
		}

		(*fit)++;
		count++;
	}

	out->header.reading_count = count;

	return count;
}

SENSOR_DECODER_API_DT_DEFINE() = {
	.get_frame_count = lsm6dsl_decoder_get_frame_count,
	.get_size_info = lsm6dsl_decoder_get_size_info,
	.decode = lsm6dsl_decoder_decode,
};

int lsm6dsl_get_decoder(const struct device *dev, const struct sensor_decoder_api **decoder)
{
	ARG_UNUSED(dev);
	*decoder = &SENSOR_DECODER_NAME();

	return 0;
}
