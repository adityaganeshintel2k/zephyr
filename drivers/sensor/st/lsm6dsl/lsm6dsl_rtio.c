/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lsm6dsl.h"
#include "lsm6dsl_rtio.h"
#include "lsm6dsl_decoder.h"

#include <zephyr/rtio/work.h>
#include <zephyr/drivers/sensor_clock.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(LSM6DSL_RTIO, CONFIG_SENSOR_LOG_LEVEL);

#define LSM6DSL_RTIO_DEFAULT_ODR_HZ 104

/*
 * When CONFIG_LSM6DSL_TRIGGER is enabled, the data-ready interrupt handler
 * races with this poll and almost always clears XLDA/GDA first, so waiting
 * on those flags here would spuriously time out. Instead just sleep one ODR
 * period (yielding the CPU, unlike a busy-wait) so each read is from a new
 * sample, whatever last latched it.
 */
static void lsm6dsl_wait_one_odr_period(const struct device *dev)
{
	struct lsm6dsl_data *data = dev->data;
	uint16_t freq = data->accel_freq != 0 ? data->accel_freq : LSM6DSL_RTIO_DEFAULT_ODR_HZ;

	k_sleep(K_USEC(USEC_PER_SEC / freq + 1000));
}

/*
 * Minimal one-shot RTIO read: reads gyro (6 bytes) + accel (6 bytes) in a
 * single burst starting at OUTX_L_G, since the two blocks are contiguous
 * in the register map.
 */
static int lsm6dsl_rtio_read_sample(const struct device *dev, struct lsm6dsl_rtio_sample *sample)
{
	struct lsm6dsl_data *data = dev->data;
	uint8_t buf[12];

	lsm6dsl_wait_one_odr_period(dev);

	if (data->hw_tf->read_data(dev, LSM6DSL_REG_OUTX_L_G, buf, sizeof(buf)) < 0) {
		return -EIO;
	}

	sample->gyro[0] = (int16_t)sys_get_le16(&buf[0]);
	sample->gyro[1] = (int16_t)sys_get_le16(&buf[2]);
	sample->gyro[2] = (int16_t)sys_get_le16(&buf[4]);
	sample->accel[0] = (int16_t)sys_get_le16(&buf[6]);
	sample->accel[1] = (int16_t)sys_get_le16(&buf[8]);
	sample->accel[2] = (int16_t)sys_get_le16(&buf[10]);

	return 0;
}

static void lsm6dsl_submit_sync(struct rtio_iodev_sqe *iodev_sqe)
{
	const struct sensor_read_config *cfg = iodev_sqe->sqe.iodev->data;
	const struct device *dev = cfg->sensor;
	struct lsm6dsl_data *data = dev->data;
	struct lsm6dsl_encoded_data *edata;
	uint32_t min_buf_len = sizeof(struct lsm6dsl_encoded_data);
	uint8_t *buf;
	uint32_t buf_len;
	uint64_t cycles;
	int rc;

	LOG_INF("RTIO submit running on workq thread, %d samples", CONFIG_LSM6DSL_RTIO_SAMPLE_COUNT);

	rc = rtio_sqe_rx_buf(iodev_sqe, min_buf_len, min_buf_len, &buf, &buf_len);
	if (rc != 0) {
		LOG_ERR("Failed to get a read buffer of size %u bytes", min_buf_len);
		rtio_iodev_sqe_err(iodev_sqe, rc);
		return;
	}

	edata = (struct lsm6dsl_encoded_data *)buf;
	edata->accel_sensitivity = data->accel_sensitivity;
	edata->gyro_sensitivity = data->gyro_sensitivity;
	edata->num_samples = CONFIG_LSM6DSL_RTIO_SAMPLE_COUNT;

	for (int i = 0; i < CONFIG_LSM6DSL_RTIO_SAMPLE_COUNT; i++) {
		rc = lsm6dsl_rtio_read_sample(dev, &edata->samples[i]);
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

void lsm6dsl_submit(const struct device *dev, struct rtio_iodev_sqe *iodev_sqe)
{
	struct rtio_work_req *req = rtio_work_req_alloc();

	LOG_INF("lsm6dsl_submit() called for %s", dev->name);

	if (req == NULL) {
		LOG_ERR("RTIO work item allocation failed. Consider to increase "
			"CONFIG_RTIO_WORKQ_POOL_ITEMS.");
		rtio_iodev_sqe_err(iodev_sqe, -ENOMEM);
		return;
	}

	rtio_work_req_submit(req, iodev_sqe, lsm6dsl_submit_sync);
}
