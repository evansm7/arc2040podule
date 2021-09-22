#ifndef HW_H
#define HW_H

#include "hardware/gpio.h"

#define GPIO_D0		0	// to 7 for D0
#define GPIO_NSEL 	8
#define GPIO_HIRQ	9
#define GPIO_HRST	10
#define GPIO_NRD	11
#define GPIO_NWR	12
#define GPIO_A2		13      // to 15 for A4
#define GPIO_NRST_I	16
#define GPIO_A5		17	// to 25 for A13
#define GPIO_LED	29

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
        gpio_set_drive_strength(GPIO_LED, GPIO_DRIVE_STRENGTH_2MA);
	led_off();
}

#endif
