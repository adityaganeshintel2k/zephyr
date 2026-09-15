/*
 * Copyright (c) 2018 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_SENSOR_ASYNC_API
#include <zephyr/rtio/rtio.h>
#include <zephyr/drivers/sensor_data_types.h>

RTIO_DEFINE(lsm6dsl_rtio_ctx, 1, 1);
SENSOR_DT_READ_IODEV(lsm6dsl_rtio_iodev, DT_COMPAT_GET_ANY_STATUS_OKAY(st_lsm6dsl),
		     {SENSOR_CHAN_ACCEL_XYZ, 0}, {SENSOR_CHAN_GYRO_XYZ, 0});

/* Proves the driver's submit()/get_decoder() (RTIO) path runs, as opposed to sample_fetch() */
static void lsm6dsl_rtio_test(const struct device *dev)
{
	static uint8_t buf[512];
	const struct sensor_decoder_api *decoder;
	struct sensor_chan_spec accel_spec = {SENSOR_CHAN_ACCEL_XYZ, 0};
	uint32_t fit = 0;
	struct sensor_three_axis_data data;
	int rc = sensor_read(&lsm6dsl_rtio_iodev, &lsm6dsl_rtio_ctx, buf, sizeof(buf));

	if (rc != 0) {
		printk("RTIO sensor_read() failed: %d\n", rc);
		return;
	}

	rc = sensor_get_decoder(dev, &decoder);
	if (rc != 0) {
		printk("sensor_get_decoder() failed: %d\n", rc);
		return;
	}

	printk("--- RTIO path: decoding buffered accel samples ---\n");
	while (decoder->decode(buf, accel_spec, &fit, 1, &data) == 1) {
		/* q31 -> physical value: raw / 2^(31 - shift) */
		double scale = (double)(1U << (31 - data.shift));

		printk("RTIO accel[%u]: x=%.3f y=%.3f z=%.3f m/s^2 (raw x=%d y=%d z=%d shift=%d)\n",
		       fit - 1, data.readings[0].x / scale, data.readings[0].y / scale,
		       data.readings[0].z / scale, data.readings[0].x, data.readings[0].y,
		       data.readings[0].z, data.shift);
	}
}
#endif /* CONFIG_SENSOR_ASYNC_API */

static int print_samples;
static int lsm6dsl_trig_cnt;

static struct sensor_value accel_x_out, accel_y_out, accel_z_out;
static struct sensor_value gyro_x_out, gyro_y_out, gyro_z_out;
#if defined(CONFIG_LSM6DSL_EXT0_LIS2MDL)
static struct sensor_value magn_x_out, magn_y_out, magn_z_out;
#endif
#if defined(CONFIG_LSM6DSL_EXT0_LPS22HB)
static struct sensor_value press_out, temp_out;
#endif

#ifdef CONFIG_LSM6DSL_TRIGGER
static void lsm6dsl_trigger_handler(const struct device *dev,
				    const struct sensor_trigger *trig)
{
	static struct sensor_value accel_x, accel_y, accel_z;
	static struct sensor_value gyro_x, gyro_y, gyro_z;
#if defined(CONFIG_LSM6DSL_EXT0_LIS2MDL)
	static struct sensor_value magn_x, magn_y, magn_z;
#endif
#if defined(CONFIG_LSM6DSL_EXT0_LPS22HB)
	static struct sensor_value press, temp;
#endif
	lsm6dsl_trig_cnt++;

	sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &accel_x);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &accel_y);
	sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &accel_z);

	/* lsm6dsl gyro */
	sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &gyro_x);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &gyro_y);
	sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &gyro_z);

#if defined(CONFIG_LSM6DSL_EXT0_LIS2MDL)
	/* lsm6dsl external magn */
	sensor_sample_fetch_chan(dev, SENSOR_CHAN_MAGN_XYZ);
	sensor_channel_get(dev, SENSOR_CHAN_MAGN_X, &magn_x);
	sensor_channel_get(dev, SENSOR_CHAN_MAGN_Y, &magn_y);
	sensor_channel_get(dev, SENSOR_CHAN_MAGN_Z, &magn_z);
#endif

#if defined(CONFIG_LSM6DSL_EXT0_LPS22HB)
	/* lsm6dsl external press/temp */
	sensor_sample_fetch_chan(dev, SENSOR_CHAN_PRESS);
	sensor_channel_get(dev, SENSOR_CHAN_PRESS, &press);

	sensor_sample_fetch_chan(dev, SENSOR_CHAN_AMBIENT_TEMP);
	sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
#endif

	if (print_samples) {
		print_samples = 0;

		accel_x_out = accel_x;
		accel_y_out = accel_y;
		accel_z_out = accel_z;

		gyro_x_out = gyro_x;
		gyro_y_out = gyro_y;
		gyro_z_out = gyro_z;

#if defined(CONFIG_LSM6DSL_EXT0_LIS2MDL)
		magn_x_out = magn_x;
		magn_y_out = magn_y;
		magn_z_out = magn_z;
#endif

#if defined(CONFIG_LSM6DSL_EXT0_LPS22HB)
		press_out = press;
		temp_out = temp;
#endif
	}

}
#endif

int main(void)
{
	int cnt = 0;
	char out_str[64];
	struct sensor_value odr_attr;
	const struct device *const lsm6dsl_dev = DEVICE_DT_GET_ONE(st_lsm6dsl);

	if (!device_is_ready(lsm6dsl_dev)) {
		printk("sensor: device not ready.\n");
		return 0;
	}

	/* set accel/gyro sampling frequency to 104 Hz */
	odr_attr.val1 = 104;
	odr_attr.val2 = 0;

	if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_ACCEL_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set sampling frequency for accelerometer.\n");
		return 0;
	}

	if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_GYRO_XYZ,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		printk("Cannot set sampling frequency for gyro.\n");
		return 0;
	}

#ifdef CONFIG_LSM6DSL_TRIGGER
	struct sensor_trigger trig;

	trig.type = SENSOR_TRIG_DATA_READY;
	trig.chan = SENSOR_CHAN_ACCEL_XYZ;

	if (sensor_trigger_set(lsm6dsl_dev, &trig, lsm6dsl_trigger_handler) != 0) {
		printk("Could not set sensor type and channel\n");
		return 0;
	}
#endif

	if (sensor_sample_fetch(lsm6dsl_dev) < 0) {
		printk("Sensor sample update error\n");
		return 0;
	}

	while (1) {
		/* Erase previous */
		printk("\0033\014");
		printf("LSM6DSL sensor samples:\n\n");

#ifdef CONFIG_SENSOR_ASYNC_API
		lsm6dsl_rtio_test(lsm6dsl_dev);
#endif

		/* lsm6dsl accel */
		sprintf(out_str, "accel x:%f ms/2 y:%f ms/2 z:%f ms/2",
							  sensor_value_to_double(&accel_x_out),
							  sensor_value_to_double(&accel_y_out),
							  sensor_value_to_double(&accel_z_out));
		printk("%s\n", out_str);

		/* lsm6dsl gyro */
		sprintf(out_str, "gyro x:%f dps y:%f dps z:%f dps",
							   sensor_value_to_double(&gyro_x_out),
							   sensor_value_to_double(&gyro_y_out),
							   sensor_value_to_double(&gyro_z_out));
		printk("%s\n", out_str);

#if defined(CONFIG_LSM6DSL_EXT0_LIS2MDL)
		/* lsm6dsl external magn */
		sprintf(out_str, "magn x:%f G y:%f G z:%f G",
							   sensor_value_to_double(&magn_x_out),
							   sensor_value_to_double(&magn_y_out),
							   sensor_value_to_double(&magn_z_out));
		printk("%s\n", out_str);
#endif

#if defined(CONFIG_LSM6DSL_EXT0_LPS22HB)
		/* lsm6dsl external press/temp */
		sprintf(out_str, "press: %f kPa - temp: %f deg",
			sensor_value_to_double(&press_out), sensor_value_to_double(&temp_out));
		printk("%s\n", out_str);
#endif

		printk("loop:%d trig_cnt:%d\n\n", ++cnt, lsm6dsl_trig_cnt);

		print_samples = 1;
		k_sleep(K_MSEC(2000));
	}
}
