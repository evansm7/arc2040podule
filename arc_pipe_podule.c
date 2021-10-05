/* arc_pipe_podule
 *
 * Firmware for Acorn Archimedes pipe podule, bit-banged expansion card bus.
 * The function of the card will be at least a ROM card, and hopefully a USB
 * link to a host, over which messages are passed to implement
 * filesystem/network ops on the host.
 *
 * 7 Sept 2021
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
#include <unistd.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "hw.h"
#include "version.h"
#include "podule_interface.h"
#include "podule_regs.h"
#include "pipe_packet.h"


#define RESET_HOST_ON_STARTUP

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

        pipe_init();
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
                printf("-- Reset request\n");
                reset_generation = r[PR_RESET];
                pipe_init();
        }

        /* The more complex packet interface registers are dealt with in here.
         * NOTE:  The USB comms polling also occurs in here.
         */
        pipe_poll();
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

#ifdef RESET_HOST_ON_STARTUP
        podule_if_reset_host();
        printf("Host reset.\n");
#endif

        unsigned int loops = 0;
	while (true) {
                tud_task();
                podule_poll();

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
