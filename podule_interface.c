#include <stdio.h>
#include <inttypes.h>
#include "podule_interface.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#define DEBUG 1


/* 4KB of addr space => 4KB of RAM.
 *
 * This space is arranged as:
 *
 *  +--------------------+ 0
 *  |                    |
 *  | RO                 |
 *  | Chunk dir., loader |
 *  |                    |
 *  +--------------------+ 1KB
 *  |                    |
 *  | RO                 |
 *  | Paged view of ROM  |
 *  |                    |
 *  +--------------------+ 2KB
 *  |                    |
 *  | RW                 |
 *  | Mailboxes, buffers |
 *  |                    |
 *  +                    + 3KB
 *  |                    |
 *  |                    |
 *  |                    |
 *  |                    |
 *  +--------------------+
 *
 *
 */
volatile uint8_t podule_space[4096];

#define CFG_INPUT(x) do { \
                gpio_init(x);   \
                gpio_set_dir(x, GPIO_IN);               \
        } while(0)

#define CFG_OUTPUT(x) do { \
                gpio_init(x);                           \
                gpio_set_dir(x, GPIO_OUT);              \
        } while(0)

static uint32_t get_addr(uint32_t i)
{
        uint32_t j = i & (7 << GPIO_A2);
        uint32_t k = i & (0x1ff << GPIO_A5);
        return (j >> GPIO_A2) | (k >> (GPIO_A5 - 3));
}

static uint8_t  get_data(uint32_t i)
{
        return (i & (0xff << GPIO_D0)) >> (GPIO_D0);
}

static bool     is_read(uint32_t i)
{
        return (i & ((1 << GPIO_NSEL) | (1 << GPIO_NRD))) == 0;
}

static bool     is_write(uint32_t i)
{
        return (i & ((1 << GPIO_NSEL) | (1 << GPIO_NWR))) == 0;
}

static bool     is_selected(uint32_t i)
{
        return (i & (1 << GPIO_NSEL)) == 0;
}

static void     data_inputs(void)
{
        gpio_set_dir_in_masked(0xff << GPIO_D0);
}

static void     data_outputs(void)
{
        gpio_set_dir_out_masked(0xff << GPIO_D0);
}

volatile uint32_t       dbg_info1 = 0;
volatile uint32_t       dbg_trx_count = 0;

static void podule_if_thread(void)
{
        save_and_disable_interrupts();
        gpio_clr_mask(0xff << GPIO_D0);
        while (1) {
                /* Simple bus interface operates as follows:
                 *
                 * Idle:        Wait for (/RD or /WR) and /SEL
                 *              Go to do_read or do_write
                 *
                 * do_read:     Acquire byte, set D as outputs, apply output data,
                 *              wait for /RD to go inactive. (Ideally, and /SEL)
                 *              D set as inputs.  Return to Idle.
                 *
                 * do_write:    Sample D inputs, store byte.  Wait for /WR (and /SEL)
                 *              to go inactive.  Return to idle.
                 *
                 * Synchronous cycles happen (e.g. PI/ECId); there's < 200ns between
                 * /RD |_ and data needing to be ready (>=50ns setup before _|).
                 *
                 * Looking at compiler output, there are a couple of things that could
                 * be improved using asm; this works but I'd like more margin.
                 */
                uint32_t io = gpio_get_all();

                if (is_read(io)) {
                        uint32_t addr = get_addr(io);
                        uint32_t data = podule_space[addr & 0xfff];

                        gpio_set_mask(data << GPIO_D0); // Faster than gpio_put_masked()
                        data_outputs();
#ifdef DEBUG
                        dbg_info1 = 0x80000000 | addr;
#endif

                        // Wait for read cycle to finish:
                        do {
                                io = gpio_get_all();
                        } while(is_selected(io)); // Hold after /RD rises

                        data_inputs();
                        gpio_clr_mask(0xff << GPIO_D0);
                        dbg_trx_count++;

                } else if (is_write(io)) {
                        uint32_t addr = get_addr(io);
                        uint32_t data;

                        io = gpio_get_all();    // resample WR data (wait a bit?)
                        data = get_data(io);
#ifdef DEBUG
                        dbg_info1 = 0xc0000000 | addr | (data << 16);
#endif
                        if (addr >= 2048 && addr < 4096) {
                                podule_space[addr] = data;
                        }
                        // Wait for write cycle to finish:
                        do {
                                io = gpio_get_all();
                        } while(is_write(io));

                        dbg_trx_count++;
                }
        }
}

void	podule_if_init(void)
{
        // Address/control inputs:
        for (int i = 0; i < 8; i++) {
                CFG_INPUT(GPIO_D0 + i);         // D0-D7
                gpio_set_drive_strength(GPIO_D0 + i, GPIO_DRIVE_STRENGTH_8MA);
        }
        CFG_INPUT(GPIO_NSEL);
        CFG_INPUT(GPIO_NRD);
        CFG_INPUT(GPIO_NWR);
        CFG_INPUT(GPIO_NRST_I);

        for (int i = 0; i < 3; i++) {
                CFG_INPUT(GPIO_A2 + i);         // A2-A4
        }
        for (int i = 0; i < 9; i++) {
                CFG_INPUT(GPIO_A5 + i);         // A5-A13
        }

        // Control outputs:
        gpio_put(GPIO_HIRQ, false);             // Don't assert IRQ
        CFG_OUTPUT(GPIO_HIRQ);
        gpio_put(GPIO_HRST, false);             // Don't reset...
        CFG_OUTPUT(GPIO_HRST);

        multicore_launch_core1(podule_if_thread);
}

void	podule_if_debug(void)
{
        uint32_t i = gpio_get_all();

        printf("A = %04x, D[7:0] = %02x, NSEL%d, NRD%d, NWR%d, NRST%d, %08x, %d\n",
               get_addr(i), get_data(i),
               gpio_get(GPIO_NSEL),
               gpio_get(GPIO_NRD),
               gpio_get(GPIO_NWR),
               gpio_get(GPIO_NRST_I),
               dbg_info1, dbg_trx_count);
}
