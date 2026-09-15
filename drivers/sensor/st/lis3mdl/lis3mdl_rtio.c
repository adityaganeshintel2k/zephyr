/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lis3mdl.h"
#include "lis3mdl_rtio.h"
#include "lis3mdl_decoder.h"

#include <zephyr/rtio/work.h>
#include <zephyr/drivers/sensor_clock.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(LIS3MDL_RTIO, CONFIG_SENSOR_LOG_LEVEL);

/* No runtime ODR change (unlike lsm6dsl), so just wait one period of the compile-time ODR */
static void lis3mdl_wait_one_odr_period(const struct device *dev)
{
	struct lis3mdl_data *data = dev->data;

	k_sleep(K_USEC((uint32_t)(1000000000ULL / data->odr_mhz) + 1000));
}

static int lis3mdl_rtio_read_sample(const struct device *dev, struct lis3mdl_rtio_sample *sample)
{
	const struct lis3mdl_config *config = dev->config;
	uint8_t buf[6];

	lis3mdl_wait_one_odr_period(dev);

	if (i2c_burst_read_dt(&config->i2c, LIS3MDL_REG_SAMPLE_START, buf, sizeof(buf)) < 0) {
		return -EIO;
	}

	sample->mag[0] = (int16_t)sys_get_le16(&buf[0]);
	sample->mag[1] = (int16_t)sys_get_le16(&buf[2]);
	sample->mag[2] = (int16_t)sys_get_le16(&buf[4]);

	return 0;
}

static void lis3mdl_submit_sync(struct rtio_iodev_sqe *iodev_sqe)
{
	const struct sensor_read_config *cfg = iodev_sqe->sqe.iodev->data;
	const struct device *dev = cfg->sensor;
	struct lis3mdl_encoded_data *edata;
	uint32_t min_buf_len = sizeof(struct lis3mdl_encoded_data);
	uint8_t *buf;
	uint32_t buf_len;
	uint64_t cycles;
	int rc;

	LOG_INF("lis3mdl_submit() running on workq thread, %d samples",
		CONFIG_LIS3MDL_RTIO_SAMPLE_COUNT);

	rc = rtio_sqe_rx_buf(iodev_sqe, min_buf_len, min_buf_len, &buf, &buf_len);
	if (rc != 0) {
		LOG_ERR("Failed to get a read buffer of size %u bytes", min_buf_len);
		rtio_iodev_sqe_err(iodev_sqe, rc);
		return;
	}

	edata = (struct lis3mdl_encoded_data *)buf;
	edata->magn_gain = lis3mdl_magn_gain[LIS3MDL_FS_IDX];
	edata->num_samples = CONFIG_LIS3MDL_RTIO_SAMPLE_COUNT;

	for (int i = 0; i < CONFIG_LIS3MDL_RTIO_SAMPLE_COUNT; i++) {
		rc = lis3mdl_rtio_read_sample(dev, &edata->samples[i]);
		if (rc != 0) {
			LOG_ERR("Failed to read sample %d", i);
			rtio_iodev_sqe_err(iodev_sqe, rc);
			return;
		}
	}

	rc = sensor_clock_get_cycles(&cycles);
	if (rc != 0) {
		rtio_iodev_sqe_err(iodev_sqe, rc);
		return;
	}
	edata->timestamp = sensor_clock_cycles_to_ns(cycles);

	rtio_iodev_sqe_ok(iodev_sqe, 0);
}

void lis3mdl_submit(const struct device *dev, struct rtio_iodev_sqe *iodev_sqe)
{
	struct rtio_work_req *req = rtio_work_req_alloc();

	if (req == NULL) {
		LOG_ERR("RTIO work item allocation failed. Consider to increase "
			"CONFIG_RTIO_WORKQ_POOL_ITEMS.");
		rtio_iodev_sqe_err(iodev_sqe, -ENOMEM);
		return;
	}

	rtio_work_req_submit(req, iodev_sqe, lis3mdl_submit_sync);
}
