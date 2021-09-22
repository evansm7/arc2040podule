/* arc_pipe_podule
 *
 * Firmware for Acorn Archimedes pipe podule, bit-banged expansion card bus.
 * The function of the card will be at least a ROM card, and hopefully a USB link to a host,
 * over which messages are passed to implement filesystem/network ops on the host.
 *
 * 7 Sept 2021
 * Matt Evans
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "podule_interface.h"
#include "hw.h"
#include "version.h"
#include "podule_interface.h"


static void init_podule_space(void)
{
        /* Initialise the loader & reg spaces: */

        // FIXME: copy loader[]
        uint8_t *p = podule_if_get_memspace();

        memset(&p[PODULE_MEM_LOADER], 0, 1024);

        /* Product info: */
        p[3] = 0xab;
        p[4] = 0xcd;
        /* Manuf info: */
        p[5] = 0xb3;
        p[6] = 0x3f;

        /* Overall layout of regs:
         * +0   PAGE_REG_L
         *      Write page number 0-2047 & 0xff
         * +1   PAGE_REG_H
         *      Write page number 0-2047 >> 8
         *      Set bit 7 to load; cleared once page is loaded
         */
}

int main()
{
	stdio_init_all();
	io_init();

	printf("[Arc pipe podule firmware version " VERSION      \
               ", built " __TIME__ " " __DATE__ "]\n");

        podule_if_init();

	while (true) {
                led_on();
		sleep_ms(100);
                led_off();
		sleep_ms(200);

                podule_if_debug();
	}

	return 0;
}