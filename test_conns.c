/* test_conns
 *
 * A super-simple connection tester: puts all GPIO into input, and prints which
 * pins change due to external (likely human-led) probing.
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

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"


static void dump_state(uint32_t now, uint32_t changed)
{
	// Row of bit numbers
	printf("     ");
	for (int w = 0; w < 30; w++) {
		printf("%d ", w / 10);
	}
	printf("\n     ");
	for (int w = 0; w < 30; w++) {
		printf("%d ", w % 10);
	}
	printf("\nNow: ");
	for (int w = 0; w < 30; w++) {
		printf("%d ", !!(now & (1 << w)));
	}
	printf("\nCh:  ");
	for (int w = 0; w < 30; w++) {
		printf("%d ", !!(changed & (1 << w)));
	}
	printf("\n\n");
}

int main()
{
	stdio_init_all();

	sleep_ms(1000);
	printf("Connection tester, ohai.\n");

	// All GPIOs input with a weak pullup:
	for (int g = 0; g < 30; g++) {
		gpio_set_function(g, GPIO_FUNC_NULL);
		gpio_pull_up(g);
		gpio_set_input_enabled(g, true);
	}
	sleep_ms(1000);

	uint32_t initial_gpio = gpio_get_all();
	uint32_t cumulative = initial_gpio;
	uint32_t changed = 0;

	while (true) {
		uint32_t curr = gpio_get_all();

		changed |= (curr ^ initial_gpio);

		dump_state(curr, changed);
		sleep_ms(100);
	}

	return 0;
}
