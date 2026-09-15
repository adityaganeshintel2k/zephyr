/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT st_lis3mdl_magn

#include "lis3mdl_decoder.h"

#include <zephyr/sys/util.h>

/* Conservative shift covering the full +/-16 gauss FS range without overflow */
#define LIS3MDL_MAGN_SHIFT 5

static void lis3mdl_raw_to_q31(int16_t raw, uint16_t gain, q31_t *out)
{
	int64_t whole = raw / (int32_t)gain;
	int64_t fraction = (((int64_t)raw % gain) * INT64_C(1000000)) / gain;
	int64_t intermediate = whole * INT64_C(1000000) + fraction;

	intermediate = intermediate * ((int64_t)INT32_MAX + 1) /
		       ((1 << LIS3MDL_MAGN_SHIFT) * INT64_C(1000000));
	*out = CLAMP(intermediate, INT32_MIN, INT32_MAX);
}

static int lis3mdl_decoder_get_frame_count(const uint8_t *buffer, struct sensor_chan_spec chan_spec,
					   uint16_t *frame_count)
{
	const struct lis3mdl_encoded_data *edata = (const struct lis3mdl_encoded_data *)buffer;

	if (chan_spec.chan_idx != 0) {
		return -ENOTSUP;
	}

	if (chan_spec.chan_type != SENSOR_CHAN_MAGN_XYZ) {
		return -ENOTSUP;
	}

	*frame_count = edata->num_samples;
	return 0;
}

static int lis3mdl_decoder_get_size_info(struct sensor_chan_spec chan_spec, size_t *base_size,
					 size_t *frame_size)
{
	if (chan_spec.chan_type != SENSOR_CHAN_MAGN_XYZ) {
		return -ENOTSUP;
	}

	*base_size = sizeof(struct sensor_three_axis_data);
	*frame_size = sizeof(struct sensor_three_axis_sample_data);
	return 0;
}

static int lis3mdl_decoder_decode(const uint8_t *buffer, struct sensor_chan_spec chan_spec,
				  uint32_t *fit, uint16_t max_count, void *data_out)
{
	const struct lis3mdl_encoded_data *edata = (const struct lis3mdl_encoded_data *)buffer;
	struct sensor_three_axis_data *out = data_out;
	uint16_t count = 0;

	if (chan_spec.chan_idx != 0 || chan_spec.chan_type != SENSOR_CHAN_MAGN_XYZ) {
		return -ENOTSUP;
	}

	out->header.base_timestamp_ns = edata->timestamp;
	out->shift = LIS3MDL_MAGN_SHIFT;

	while (*fit < edata->num_samples && count < max_count) {
		const struct lis3mdl_rtio_sample *sample = &edata->samples[*fit];

		out->readings[count].timestamp_delta = 0;

		for (int i = 0; i < 3; i++) {
			lis3mdl_raw_to_q31(sample->mag[i], edata->magn_gain,
					   &out->readings[count].values[i]);
		}

		(*fit)++;
		count++;
	}

	out->header.reading_count = count;

	return count;
}

SENSOR_DECODER_API_DT_DEFINE() = {
	.get_frame_count = lis3mdl_decoder_get_frame_count,
	.get_size_info = lis3mdl_decoder_get_size_info,
	.decode = lis3mdl_decoder_decode,
};

int lis3mdl_get_decoder(const struct device *dev, const struct sensor_decoder_api **decoder)
{
	ARG_UNUSED(dev);
	*decoder = &SENSOR_DECODER_NAME();

	return 0;
}
