/*
 * Copyright (c) 2026 Artem Yurov
 * SPDX-License-Identifier: MIT
 */

/* Fixed-point (Q16) rotation of relative motion. No Zephyr dependencies. */

#ifndef CIRQUE_ROTATE_MATH_H_
#define CIRQUE_ROTATE_MATH_H_

#include <stdbool.h>
#include <stdint.h>

#define ROTATE_Q16_ONE 65536
/* Half a count: makes the floor division round to nearest. */
#define ROTATE_Q16_HALF 32768

struct rotate_math {
	int32_t sin_q16;
	int32_t cos_q16;
	int32_t rem_x;
	int32_t rem_y;
};

/* Quarter wave, one entry per degree. */
static const int32_t rotate_sine_q16[] = {
	0,     1144,  2287,  3430,  4572,  5712,  6850,  7987,  9121,  10252, 11380,
	12505, 13626, 14742, 15855, 16962, 18064, 19161, 20252, 21336, 22415, 23486,
	24550, 25607, 26656, 27697, 28729, 29753, 30767, 31772, 32768, 33754, 34729,
	35693, 36647, 37590, 38521, 39441, 40348, 41243, 42126, 42995, 43852, 44695,
	45525, 46341, 47143, 47930, 48703, 49461, 50203, 50931, 51643, 52339, 53020,
	53684, 54332, 54963, 55578, 56175, 56756, 57319, 57865, 58393, 58903, 59396,
	59870, 60326, 60764, 61183, 61584, 61966, 62328, 62672, 62997, 63303, 63589,
	63856, 64104, 64332, 64540, 64729, 64898, 65048, 65177, 65287, 65376, 65446,
	65496, 65526, 65536,
};

/* angle is in 0..359 */
static inline int32_t rotate_sin_q16(int32_t angle)
{
	if (angle <= 90) {
		return rotate_sine_q16[angle];
	}
	if (angle <= 180) {
		return rotate_sine_q16[180 - angle];
	}
	if (angle <= 270) {
		return -rotate_sine_q16[angle - 180];
	}
	return -rotate_sine_q16[360 - angle];
}

static inline void rotate_math_reset(struct rotate_math *m)
{
	m->rem_x = ROTATE_Q16_HALF;
	m->rem_y = ROTATE_Q16_HALF;
}

static inline void rotate_math_init(struct rotate_math *m, int32_t angle_deg)
{
	int32_t angle = angle_deg % 360;

	if (angle < 0) {
		angle += 360;
	}

	m->sin_q16 = rotate_sin_q16(angle);
	m->cos_q16 = rotate_sin_q16((angle + 90) % 360);
	rotate_math_reset(m);
}

/* Floor division by 2^16, leaving a nonnegative remainder in *rem. */
static inline int32_t rotate_floor_q16(int32_t n, int32_t *rem)
{
	int32_t q = n / ROTATE_Q16_ONE;
	int32_t r = n % ROTATE_Q16_ONE;

	if (r < 0) {
		r += ROTATE_Q16_ONE;
		q -= 1;
	}
	*rem = r;
	return q;
}

/* |dx|, |dy| <= 256 keeps the products within int32_t. */
static inline void rotate_math_apply(struct rotate_math *m, int32_t dx, int32_t dy, int32_t *ox,
				     int32_t *oy)
{
	int32_t nx = m->cos_q16 * dx - m->sin_q16 * dy + m->rem_x;
	int32_t ny = m->sin_q16 * dx + m->cos_q16 * dy + m->rem_y;

	*ox = rotate_floor_q16(nx, &m->rem_x);
	*oy = rotate_floor_q16(ny, &m->rem_y);
}

#endif /* CIRQUE_ROTATE_MATH_H_ */
