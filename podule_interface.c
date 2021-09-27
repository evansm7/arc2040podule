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


#ifdef DEBUG
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

struct _debug {
        volatile uint32_t       rd_addr;
        volatile uint32_t       rd_data;
        volatile uint32_t       rd_count;
        volatile uint32_t       wr_addr;
        volatile uint32_t       wr_data;
        volatile uint32_t       wr_count;
} debug;
#endif

static void __no_inline_not_in_flash_func(podule_if_thread)(void)
{
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
         * This thread runs from RAM (avoiding XIP/cache unpredictability), and
         * with IRQs off.
         */

        save_and_disable_interrupts();
        gpio_clr_mask(0xff << GPIO_D0);

        /* Asm is tuned for specific pins, assert this: */
#if GPIO_D0 != 0
#error "D0 not at 0"
#endif
        while (1) {
                /* Don't ask.  Though there's a loop in the asm (and
                 * this while shouldn't be necessary), I saw some reg
                 * allocation weirdness when this wasn't in a while
                 * block of its own.
                 */
                uint32_t unused1, unused2, unused3, unused4;
                asm __volatile__(
                        "pif_mainloop:                  \n"
                        "mov    %1, %[rd_sel_mask]      \n"
                        "mov    %2, %[wr_sel_mask]      \n"
                        "pif_wait_strobe:               \n"
                        "ldr    %0, [%[io], %[in]]      \n"
                        // Selected for read or write?
                        "tst    %0, %1                  \n"
                        "beq    pif_rd                  \n"
                        "tst    %0, %2                  \n"
                        "beq    pif_wr                  \n"
                        "b      pif_wait_strobe         \n"

                        "pif_rd:                        \n"
                        // Construct address (make these pins contiguous on V2!):
                        "mov    %2, %[addr_mask]        \n"
                        // Addr bits from input pins:
                        "and    %2, %2, %0              \n"
                        // Lower bits A[4:2]:
                        "mov    %1, %[addrl_mask]       \n"
                        "and    %1, %1, %0              \n"
                        // Trick: Shift A[4:2] field right by adding to self:
                        "add    %1, %1, %2              \n"
                        "asr    %1, %1, %[shr_a2]       \n"
                        // %1 now contains the address as A[11:0].  Get read data:
                        "ldrb   %0, [%[ps], %1]         \n"
                        // Set GPIO data output value:
                        "str    %0, [%[io], %[setmask]] \n"
                        // Set GPIO pins output:
                        "mov    %2, #0xff               \n" // D[7:0] mask
                        "str    %2, [%[io], %[oe_set]]  \n"

#ifdef DEBUG
                        // Debug:
                        "str    %1, [%[debug], %[dbg_r_addr]] \n" // Addr
                        "str    %0, [%[debug], %[dbg_r_data]] \n" // Data
                        "ldr    %0, [%[debug], %[dbg_r_count]] \n"
                        "add    %0, %0, #1              \n"
                        "str    %0, [%[debug], #8]      \n"
#endif
                        // Whew.  Now wait for SEL to go high:\n"
                        "mov    %1, %[sel_mask]         \n"
                        "pif_rd_done_wait:                  \n"
                        "ldr    %0, [%[io], %[in]]      \n"
                        "tst    %0, %1                  \n"
                        "beq    pif_rd_done_wait        \n"
                        // Finally, clear D, set pins inputs:
                        "str    %2, [%[io], %[oe_clr]]  \n"
                        "str    %2, [%[io], %[clrmask]] \n"
                        // Done!
                        "b      pif_mainloop            \n"

                        "pif_wr:                        \n"
                        // Construct address, as above:
                        "mov    %2, %[addr_mask]        \n"
                        "and    %2, %2, %0              \n"
                        "mov    %1, %[addrl_mask]       \n"
                        "and    %1, %1, %0              \n"
                        "add    %1, %1, %2              \n"
                        "asr    %1, %1, %[shr_a2]       \n"
                        // Extract data (valid at /WR falling edge):
                        "mov    %2, #0xff               \n" // D[7:0] mask
                        "and    %0, %0, %2              \n"
                        // Is addr >=2048? (0-2047 are R/O)
                        "mov    %2, #0x8                \n"
                        "lsl    %2, %2, #8              \n"
                        "cmp    %1, %2                  \n"
                        "blt    pif_wr_done             \n"
                        "strb   %0, [%[ps], %1]         \n"

                        "pif_wr_done:                   \n"
#ifdef DEBUG
                        // Debug:
                        "str    %1, [%[debug], %[dbg_w_addr]] \n"
                        "str    %0, [%[debug], %[dbg_w_data]] \n"
                        "ldr    %0, [%[debug], %[dbg_w_count]] \n"
                        "add    %0, %0, #1              \n"
                        "str    %0, [%[debug], %[dbg_w_count]] \n"
#endif
                        // Whew.  Now wait for SEL to go high:\n"
                        "mov    %1, %[sel_mask]         \n"
                        "pif_wr_done_wait:              \n"
                        "ldr    %0, [%[io], %[in]]      \n"
                        "tst    %0, %1                  \n"
                        "beq    pif_wr_done_wait        \n"
                        // Done!
                        "b      pif_mainloop            \n"

                        /* Constraints follow.
                         * Note, for thumb, h = high regs, l = low regs; LDR/STR
                         * can only use low regs.
                         */

                        /* Outputs: these are used as temporaries, %0-%3. */
                        :
                        "=l"(unused1), "=l"(unused2), "=l"(unused3), "=l"(unused4)

                        /* Inputs: */
                        :

                        /* Pointer to shared memory: */
                        [ps]"l"(podule_space),

                        /* GPIO registers: */
                        [io]"l"(sio_hw),

                        /* One 'l' register free. */

                        /* Various constants/masks to load into (hi) regs: */
                        [rd_sel_mask]"h"( (1 << GPIO_NSEL) | (1 << GPIO_NRD) ),
                        [wr_sel_mask]"h"( (1 << GPIO_NSEL) | (1 << GPIO_NWR) ),
                        [sel_mask]"h"( 1 << GPIO_NSEL ),
                        [addrl_mask]"h"( (7 << GPIO_A2) ),
                        [addr_mask]"h"( (0x1ff << GPIO_A5) | (7 << GPIO_A2) ),

                        /* Offsets of GPIO registers: */
                        [in]"i"(offsetof(sio_hw_t, gpio_in)),
                        [setmask]"i"(offsetof(sio_hw_t, gpio_set)),
                        [clrmask]"i"(offsetof(sio_hw_t, gpio_clr)),
                        [oe_set]"i"(offsetof(sio_hw_t, gpio_oe_set)),
                        [oe_clr]"i"(offsetof(sio_hw_t, gpio_oe_clr)),
#ifdef DEBUG
                        /* Debug counters: */
                        [debug]"l"(&debug),

                        /* Debug struct offsets: */
                        [dbg_r_addr]"i"(offsetof(struct _debug, rd_addr)),
                        [dbg_r_data]"i"(offsetof(struct _debug, rd_data)),
                        [dbg_r_count]"i"(offsetof(struct _debug, rd_count)),
                        [dbg_w_addr]"i"(offsetof(struct _debug, wr_addr)),
                        [dbg_w_data]"i"(offsetof(struct _debug, wr_data)),
                        [dbg_w_count]"i"(offsetof(struct _debug, wr_count)),
#endif
                        /* Misc constants: */
                        [shr_a2]"i"(GPIO_A2 + 1)
                        );
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

#ifdef DEBUG
        debug.rd_addr = 0;
        debug.rd_data = 0;
        debug.rd_count = 0;
        debug.wr_addr = 0;
        debug.wr_data = 0;
        debug.wr_count = 0;
#endif

        multicore_launch_core1(podule_if_thread);
}

void	podule_if_debug(void)
{
#ifdef DEBUG
        uint32_t i = gpio_get_all();

        printf("A:%04x, D:%02x, NSEL%d, NRD%d, NWR%d, NRST%d; R %04x, %02x, %d; W %04x, %02x, %d\n",
               get_addr(i), get_data(i),
               gpio_get(GPIO_NSEL),
               gpio_get(GPIO_NRD),
               gpio_get(GPIO_NWR),
               gpio_get(GPIO_NRST_I),
               debug.rd_addr, debug.rd_data, debug.rd_count,
               debug.wr_addr, debug.wr_data, debug.wr_count
                );
#endif
}

void    podule_if_reset_host(void)
{
        gpio_put(GPIO_HRST, true);
        sleep_ms(50);
        gpio_put(GPIO_HRST, false);
}
