/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

#ifdef CONFIG_SENSOR_ASYNC_API
#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/rtio/rtio.h>

RTIO_DEFINE(lsm6dsl_rtio_ctx, 1, 1);
RTIO_DEFINE(lis3mdl_rtio_ctx, 1, 1);
SENSOR_DT_READ_IODEV(lsm6dsl_rtio_iodev, DT_ALIAS(accel0),
		     {SENSOR_CHAN_ACCEL_XYZ, 0}, {SENSOR_CHAN_GYRO_XYZ, 0});
SENSOR_DT_READ_IODEV(lis3mdl_rtio_iodev, DT_ALIAS(magn0), {SENSOR_CHAN_MAGN_XYZ, 0});

#define LSM6DSL_RTIO_PRIO RTIO_PRIO_HIGH
#define LIS3MDL_RTIO_PRIO RTIO_PRIO_LOW

static double q31_to_double(q31_t value, int8_t shift)
{
	return value / (double)(1ULL << (31 - shift));
}

static int sensor_read_with_prio(const struct rtio_iodev *iodev, struct rtio *ctx,
				 uint8_t *buf, size_t buf_len, uint8_t prio)
{
	struct rtio_sqe *sqe = rtio_sqe_acquire(ctx);
	struct rtio_cqe *cqe;
	int res;

	if (sqe == NULL) {
		return -ENOMEM;
	}

	rtio_sqe_prep_read(sqe, iodev, prio, buf, buf_len, buf);
	rtio_submit(ctx, 0);

	cqe = rtio_cqe_consume_block(ctx);
	res = cqe->result;

	__ASSERT(cqe->userdata == buf,
		 "consumed non-matching completion for sensor read into buffer %p\n", buf);

	rtio_cqe_release(ctx, cqe);

	return res;
}

static void print_rtio_three_axis(const char *name, const struct device *dev,
				  struct rtio_iodev *iodev, struct rtio *ctx,
				  struct sensor_chan_spec chan_spec, const char *unit, uint8_t prio)
{
	static uint8_t buf[512];
	const struct sensor_decoder_api *decoder;
	struct sensor_three_axis_data data;
	uint32_t fit = 0;
	int rc;

	rc = sensor_read_with_prio(iodev, ctx, buf, sizeof(buf), prio);
	if (rc != 0) {
		printk("%s RTIO sensor_read(prio=%u) failed: %d\n", name, prio, rc);
		return;
	}

	rc = sensor_get_decoder(dev, &decoder);
	if (rc != 0) {
		printk("%s sensor_get_decoder() failed: %d\n", name, rc);
		return;
	}

	while (decoder->decode(buf, chan_spec, &fit, 1, &data) == 1) {
		printk("%s RTIO[%u] prio=%u: x=%.3f y=%.3f z=%.3f %s\n", name, fit - 1,
		       prio, q31_to_double(data.readings[0].x, data.shift),
		       q31_to_double(data.readings[0].y, data.shift),
		       q31_to_double(data.readings[0].z, data.shift), unit);
	}
}
#endif /* CONFIG_SENSOR_ASYNC_API */

static int configure_lsm6dsl(const struct device *dev)
{
	struct sensor_value odr_attr = {
		.val1 = 104,
		.val2 = 0,
	};

	if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set LSM6DSL accelerometer sampling frequency.\n");
		return -EIO;
	}

	if (sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set LSM6DSL gyro sampling frequency.\n");
		return -EIO;
	}

	return 0;
}

int main(void)
{
	const struct device *const lsm6dsl = DEVICE_DT_GET(DT_ALIAS(accel0));
	const struct device *const lis3mdl = DEVICE_DT_GET(DT_ALIAS(magn0));
	struct sensor_value accel[3];
	struct sensor_value gyro[3];
	struct sensor_value magn[3];
	int ret;

	if (!device_is_ready(lsm6dsl)) {
		printk("%s: device not ready.\n", lsm6dsl->name);
		return 0;
	}

	if (!device_is_ready(lis3mdl)) {
		printk("%s: device not ready.\n", lis3mdl->name);
		return 0;
	}

	if (configure_lsm6dsl(lsm6dsl) != 0) {
		return 0;
	}

	printk("DISCO-L475-IOT1 sensor sample: %s + %s\n", lsm6dsl->name, lis3mdl->name);

	while (1) {
		printk("\0033\014");
		printk("DISCO-L475-IOT1 sensors\n\n");

#ifdef CONFIG_SENSOR_ASYNC_API
		print_rtio_three_axis("LSM6DSL accel", lsm6dsl, &lsm6dsl_rtio_iodev,
				      &lsm6dsl_rtio_ctx,
				      (struct sensor_chan_spec){SENSOR_CHAN_ACCEL_XYZ, 0}, "m/s^2",
				      LSM6DSL_RTIO_PRIO);
		print_rtio_three_axis("LIS3MDL magn", lis3mdl, &lis3mdl_rtio_iodev,
				      &lis3mdl_rtio_ctx,
				      (struct sensor_chan_spec){SENSOR_CHAN_MAGN_XYZ, 0}, "gauss",
				      LIS3MDL_RTIO_PRIO);
		printk("\n");
#endif

		ret = sensor_sample_fetch(lsm6dsl);
		if (ret != 0) {
			printk("LSM6DSL sample fetch failed: %d\n", ret);
			return 0;
		}

		ret = sensor_sample_fetch(lis3mdl);
		if (ret != 0) {
			printk("LIS3MDL sample fetch failed: %d\n", ret);
			return 0;
		}

		sensor_channel_get(lsm6dsl, SENSOR_CHAN_ACCEL_XYZ, accel);
		sensor_channel_get(lsm6dsl, SENSOR_CHAN_GYRO_XYZ, gyro);
		sensor_channel_get(lis3mdl, SENSOR_CHAN_MAGN_XYZ, magn);

		printf("LSM6DSL accel: x=%.3f y=%.3f z=%.3f m/s^2\n",
		       sensor_value_to_double(&accel[0]), sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
		printf("LSM6DSL gyro:  x=%.3f y=%.3f z=%.3f rad/s\n",
		       sensor_value_to_double(&gyro[0]), sensor_value_to_double(&gyro[1]),
		       sensor_value_to_double(&gyro[2]));
		printf("LIS3MDL magn:  x=%.3f y=%.3f z=%.3f gauss\n",
		       sensor_value_to_double(&magn[0]), sensor_value_to_double(&magn[1]),
		       sensor_value_to_double(&magn[2]));

		k_sleep(K_SECONDS(2));
	}

	return 0;
}
