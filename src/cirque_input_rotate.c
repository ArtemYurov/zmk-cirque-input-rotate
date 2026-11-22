/*
 * Copyright (c) 2026 Artem Yurov
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_cirque_input_rotate

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cirque_input_rotate, CONFIG_INPUT_LOG_LEVEL);

#include "rotate_math.h"

/* A longer pause between packets means a new stroke. */
#define STROKE_GAP_MS 50

struct rotate_config {
	const struct device *input_dev;
	int32_t angle;
};

struct rotate_data {
	struct rotate_math math;
	int32_t pending_x;
	/* Rotated motion that could not be queued; added to the next packet. */
	int32_t carry_x;
	int32_t carry_y;
	bool has_x;
	uint32_t last_ms;
};

static void rotate_emit(const struct device *dev, uint16_t code, int32_t value, bool sync,
			int32_t *carry)
{
	/*
	 * K_NO_WAIT: this runs in the input thread, the only consumer of the
	 * input queue, so blocking on a full queue would deadlock.
	 */
	int ret = input_report(dev, INPUT_EV_REL, code, value, sync, K_NO_WAIT);

	if (ret < 0) {
		*carry += value;
		LOG_WRN("rotate: dropped %s (%d)", code == INPUT_REL_X ? "x" : "y", ret);
	}
}

static void rotate_flush(const struct device *dev, int32_t dx, int32_t dy, bool sync)
{
	struct rotate_data *data = dev->data;
	uint32_t now = k_uptime_get_32();
	int32_t x, y;

	if (now - data->last_ms > STROKE_GAP_MS) {
		rotate_math_reset(&data->math);
		data->carry_x = 0;
		data->carry_y = 0;
	} else if (dx == 0 && dy == 0) {
		rotate_math_reset(&data->math);
	}
	data->last_ms = now;

	rotate_math_apply(&data->math, dx, dy, &x, &y);
	x += data->carry_x;
	y += data->carry_y;
	data->carry_x = 0;
	data->carry_y = 0;

	rotate_emit(dev, INPUT_REL_X, x, false, &data->carry_x);
	rotate_emit(dev, INPUT_REL_Y, y, sync, &data->carry_y);
}

static void cirque_input_rotate_cb(struct input_event *evt, void *user_data)
{
	const struct device *dev = user_data;
	struct rotate_data *data = dev->data;

	if (evt->type == INPUT_EV_REL && evt->code == INPUT_REL_X) {
		data->pending_x = evt->value;
		data->has_x = true;
		return;
	}

	if (evt->type == INPUT_EV_REL && evt->code == INPUT_REL_Y) {
		int32_t dx = data->has_x ? data->pending_x : 0;

		data->has_x = false;
		rotate_flush(dev, dx, evt->value, evt->sync);
		return;
	}

	/* X without a following Y: flush it before forwarding this event. */
	if (data->has_x) {
		data->has_x = false;
		rotate_flush(dev, data->pending_x, 0, false);
	}

	int ret = input_report(dev, evt->type, evt->code, evt->value, evt->sync, K_NO_WAIT);

	if (ret < 0) {
		LOG_WRN("rotate: dropped event type %d code %d (%d)", evt->type, evt->code, ret);
	}
}

static int cirque_input_rotate_init(const struct device *dev)
{
	const struct rotate_config *cfg = dev->config;
	struct rotate_data *data = dev->data;

	if (!device_is_ready(cfg->input_dev)) {
		LOG_ERR("input device not ready");
		return -ENODEV;
	}

	rotate_math_init(&data->math, cfg->angle);
	LOG_INF("angle %d sin %d cos %d", cfg->angle, data->math.sin_q16, data->math.cos_q16);

	return 0;
}

#define CIRQUE_INPUT_ROTATE_DEFINE(inst)                                                           \
	INPUT_CALLBACK_DEFINE_NAMED(DEVICE_DT_GET(DT_INST_PHANDLE(inst, input)),                   \
				    cirque_input_rotate_cb, (void *)DEVICE_DT_INST_GET(inst),      \
				    cirque_input_rotate_cb_##inst);                                \
                                                                                                   \
	static struct rotate_data rotate_data_##inst;                                              \
                                                                                                   \
	static const struct rotate_config rotate_config_##inst = {                                 \
		.input_dev = DEVICE_DT_GET(DT_INST_PHANDLE(inst, input)),                          \
		/* A negative devicetree int is emitted as an unsigned constant. */                \
		.angle = (int32_t)DT_INST_PROP(inst, angle),                                       \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, cirque_input_rotate_init, NULL, &rotate_data_##inst,           \
			      &rotate_config_##inst, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY,      \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(CIRQUE_INPUT_ROTATE_DEFINE)
