/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>

#define HAS_ACCEL0 (DT_HAS_ALIAS(accel0) && DT_NODE_HAS_STATUS(DT_ALIAS(accel0), okay))

#ifdef CONFIG_SENSOR_ASYNC_API
#include <zephyr/rtio/rtio.h>
#include <zephyr/drivers/sensor_data_types.h>

RTIO_DEFINE(lis3mdl_rtio_ctx, 1, 1);
SENSOR_DT_READ_IODEV(lis3mdl_rtio_iodev, DT_ALIAS(magn0), {SENSOR_CHAN_MAGN_XYZ, 0});

/* Proves the driver's submit()/get_decoder() (RTIO) path runs, as opposed to sample_fetch() */
static void lis3mdl_rtio_test(const struct device *dev)
{
	static uint8_t buf[512];
	const struct sensor_decoder_api *decoder;
	struct sensor_chan_spec magn_spec = {SENSOR_CHAN_MAGN_XYZ, 0};
	uint32_t fit = 0;
	struct sensor_three_axis_data data;
	int rc = sensor_read(&lis3mdl_rtio_iodev, &lis3mdl_rtio_ctx, buf, sizeof(buf));

	if (rc != 0) {
		printk("RTIO sensor_read() failed: %d\n", rc);
		return;
	}

	rc = sensor_get_decoder(dev, &decoder);
	if (rc != 0) {
		printk("sensor_get_decoder() failed: %d\n", rc);
		return;
	}

	printk("--- RTIO path: decoding buffered magn samples ---\n");
	while (decoder->decode(buf, magn_spec, &fit, 1, &data) == 1) {
		double scale = (double)(1U << (31 - data.shift));

		printk("RTIO magn[%u]: x=%.3f y=%.3f z=%.3f gauss\n", fit - 1,
		       data.readings[0].x / scale, data.readings[0].y / scale,
		       data.readings[0].z / scale);
	}
}
#endif /* CONFIG_SENSOR_ASYNC_API */

int main(void)
{
	const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(magn0));
	struct sensor_value value_x, value_y, value_z;
#if HAS_ACCEL0
	const struct device *const accel_dev = DEVICE_DT_GET(DT_ALIAS(accel0));
	struct sensor_value accel[3], gyro[3];
	struct sensor_value odr_attr = {
		.val1 = 104,
	};
#endif
	int ret;

	if (!device_is_ready(dev)) {
		printk("sensor: device not ready.\n");
		return 0;
	}

#if HAS_ACCEL0
	if (!device_is_ready(accel_dev)) {
		printk("%s: device not ready.\n", accel_dev->name);
		return 0;
	}

	if (sensor_attr_set(accel_dev, SENSOR_CHAN_ACCEL_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set sampling frequency for accelerometer.\n");
		return 0;
	}

	if (sensor_attr_set(accel_dev, SENSOR_CHAN_GYRO_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set sampling frequency for gyro.\n");
		return 0;
	}
#endif

	printk("Polling magnetometer data from %s.\n", dev->name);

	while (1) {
#ifdef CONFIG_SENSOR_ASYNC_API
		lis3mdl_rtio_test(dev);
#endif

		ret = sensor_sample_fetch(dev);
		if (ret) {
			printk("sensor_sample_fetch failed ret %d\n", ret);
			return 0;
		}

		ret = sensor_channel_get(dev, SENSOR_CHAN_MAGN_X, &value_x);
		ret = sensor_channel_get(dev, SENSOR_CHAN_MAGN_Y, &value_y);
		ret = sensor_channel_get(dev, SENSOR_CHAN_MAGN_Z, &value_z);
		printf("( x y z ) = ( %f  %f  %f )\n",
		       sensor_value_to_double(&value_x),
		       sensor_value_to_double(&value_y),
		       sensor_value_to_double(&value_z));

#if HAS_ACCEL0
		ret = sensor_sample_fetch(accel_dev);
		if (ret) {
			printk("lsm6dsl sensor_sample_fetch failed ret %d\n", ret);
			return 0;
		}

		sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
		sensor_channel_get(accel_dev, SENSOR_CHAN_GYRO_XYZ, gyro);
		printf("LSM6DSL accel (m/s^2): x=%f y=%f z=%f\n",
		       sensor_value_to_double(&accel[0]), sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
		printf("LSM6DSL gyro (rad/s): x=%f y=%f z=%f\n",
		       sensor_value_to_double(&gyro[0]), sensor_value_to_double(&gyro[1]),
		       sensor_value_to_double(&gyro[2]));
#endif

		k_sleep(K_MSEC(500));
	}
	return 0;
}
