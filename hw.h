/*
 * Hardware-specific defines for arc2040podule
 *
 * MIT License
 *
 * Copyright (c) 2021 Matt Evans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef HW_H
#define HW_H

#include "hardware/gpio.h"

#define GPIO_D0		0	// to 7 for D0
#define GPIO_NSEL 	8
#define GPIO_HIRQ	9
#define GPIO_HRST	10
#define GPIO_NRD	11
#define GPIO_NWR	12
#if BOARD_HW == 1
#define GPIO_A2		13      // to 15 for A4
#define GPIO_NRST_I	16
#define GPIO_A5		17	// to 25 for A13
#define GPIO_LED	29

#elif BOARD_HW == 2
#define GPIO_A2		13      // to 24 for A13
#define GPIO_LED	25

#else
#error "Unknown board HW revision"
#endif


static void	led_state(bool b)
{
	gpio_put(GPIO_LED, b);
}

static void	led_on(void)
{
	led_state(true);
}

static void	led_off(void)
{
	led_state(false);
}

/* Init the LED; the rest is done via the podule i/f code. */
static void	io_init(void)
{
	gpio_init(GPIO_LED);
	gpio_set_dir(GPIO_LED, GPIO_OUT);
	led_off();
}

#endif
