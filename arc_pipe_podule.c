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
#include "tusb.h"

#include "hw.h"
#include "version.h"
#include "podule_interface.h"
#include "podule_regs.h"


extern uint8_t podule_header[], podule_header_end[];
extern uint8_t podule_rom[], podule_rom_end[];

static void podule_rom_switch_page(uint16_t page)
{
        volatile uint8_t *p = podule_if_get_rom_window();

        if (page*1024 < (podule_rom_end - podule_rom)) {
                memcpy((void *)p, podule_rom + (page*1024), 1024);
        } else {
                printf("%s: Argh! page %d is off the end!\n", __FUNCTION__, page);
        }
}

static void init_podule_space(void)
{
        /* Initialise the loader & reg spaces: */

        volatile uint8_t *l = podule_if_get_loader();

        /* In first 1KB, header/loader live: */
        memcpy((void *)l, podule_header, podule_header_end - podule_header);

        /* In second KB, the ROM space paged window: */
        podule_rom_switch_page(0);

        /* In third/fourth KB, the registers. */

        volatile uint8_t *r = podule_if_get_regs();
        memset((void *)r, 0, 2048);
}

static unsigned int reset_generation = 0;

static void podule_poll(void)
{
        volatile uint8_t *r = podule_if_get_regs();

        /* Check for page register access:
         *
         * +0   PAGE_REG_L
         *      Write page number 0-2047 & 0xff
         * +1   PAGE_REG_H
         *      Write page number 0-2047 >> 8
         *      Set bit 7 to load; cleared once page is loaded
         */
        if (r[PR_PAGE_H] & 0x80) {
                uint16_t page = (((uint16_t)(r[PR_PAGE_H] & 0x7f)) << 8) |
                        r[PR_PAGE_L];
                podule_rom_switch_page(page);
                // barrier

                // Clear handshake flag:
                r[PR_PAGE_H] &= ~0x80;
                printf("-- Set page 0x%x\n", page);
        }

        // Check for reset request
        if (r[PR_RESET] != reset_generation) {
                reset_generation = r[PR_RESET];
        }
}

int main()
{
	stdio_init_all();
	io_init();
        tusb_init();

	printf("[Arc pipe podule firmware version " VERSION      \
               ", built " __TIME__ " " __DATE__ "]\n");

        podule_if_init();
        init_podule_space();

        printf("Initialised.\n");

        unsigned int loops = 0;
	while (true) {
                tud_task();
                podule_poll();
                comms_poll();

                if ((loops & 0x0fffff) == 0)
                        led_on();

                if ((loops & 0x0fffff) > 0x01000)
                        led_off();

                if ((loops & 0x1fffff) == 0) {
                        podule_if_debug();
                }
                loops++;
	}

	return 0;
}
