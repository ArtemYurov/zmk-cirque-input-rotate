/*
 * Copyright (c) 2026 Artem Yurov
 * SPDX-License-Identifier: MIT
 *
 * Run from the repository root:
 *   cc -std=c11 -Wall -Wextra -Werror -I src tests/rotate_math_test.c -o "$TMPDIR/rotate_math_test" && "$TMPDIR/rotate_math_test"
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "rotate_math.h"

static void near(int32_t got, int32_t want)
{
	assert(abs(got - want) <= 1);
}

static void sum_packets(int32_t angle, int32_t dx, int32_t dy, int n, bool reset_each,
			int32_t *sx, int32_t *sy)
{
	struct rotate_math m;
	int32_t ox, oy;

	rotate_math_init(&m, angle);
	*sx = 0;
	*sy = 0;
	for (int i = 0; i < n; i++) {
		if (reset_each) {
			rotate_math_reset(&m);
		}
		rotate_math_apply(&m, dx, dy, &ox, &oy);
		*sx += ox;
		*sy += oy;
	}
}

int main(void)
{
	struct rotate_math m;
	int32_t x, y, sx, sy;

	/* angle 0 is identity */
	rotate_math_init(&m, 0);
	rotate_math_apply(&m, 5, -3, &x, &y);
	assert(x == 5 && y == -3);
	rotate_math_apply(&m, -120, 77, &x, &y);
	assert(x == -120 && y == 77);
	rotate_math_apply(&m, 0, 0, &x, &y);
	assert(x == 0 && y == 0);

	/* angle 90: (1,0) -> (0,1) */
	rotate_math_init(&m, 90);
	rotate_math_apply(&m, 1, 0, &x, &y);
	assert(x == 0 && y == 1);

	/* -30 degrees */
	rotate_math_init(&m, -30);
	rotate_math_apply(&m, 100, 0, &x, &y);
	near(x, 87);
	near(y, -50);
	rotate_math_init(&m, -30);
	rotate_math_apply(&m, 0, 100, &x, &y);
	near(x, 50);
	near(y, 87);

	/* slow motion is not lost, in either direction */
	sum_packets(-30, 1, 0, 100, false, &sx, &sy);
	near(sx, 87);
	near(sy, -50);
	sum_packets(-30, -1, 0, 100, false, &sx, &sy);
	near(sx, -87);
	near(sy, 50);

	/* a reset before every packet keeps slow horizontal motion horizontal */
	sum_packets(-30, 1, 0, 10, true, &sx, &sy);
	assert(sx >= 8);

	rotate_math_init(&m, -30);
	m.rem_x = 0;
	m.rem_y = 0;
	rotate_math_reset(&m);
	assert(m.rem_x == 32768 && m.rem_y == 32768);

	/* 330 == -30 */
	struct rotate_math a, b;
	rotate_math_init(&a, 330);
	rotate_math_init(&b, -30);
	assert(a.sin_q16 == b.sin_q16 && a.cos_q16 == b.cos_q16);

	printf("ok\n");
	return 0;
}
