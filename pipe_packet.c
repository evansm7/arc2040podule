/* Talk to the USB link, and interface to the packet descriptor queues in
 * shared memory space.
 *
 * ME 27 Sep 2021
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


#define DEBUG 2

typedef struct {
        bool tx_ongoing;
        unsigned int tx_total;
        unsigned int tx_pos;
        uint8_t tx_buf[512 + 3];

        bool rx_ongoing;
} pp_state_t;

static pp_state_t state;
static uint8_t rx_buf[1024];


// Called at init, but can also be requested by loader on soft reset:
void    pipe_init(void)
{
        volatile uint8_t *r = podule_if_get_regs();

        // Reset packet registers
        memset((void *)&r[PR_TX0_0], 0, 8*4*2);
        r[PR_TX_TAIL] = 0;
        r[PR_RX_HEAD] = 0;

        // Reset state
        state.tx_ongoing = false;
        state.rx_ongoing = false;
        state.tx_pos = 0;
}

static void     pipe_tx_done(void)
{
        volatile uint8_t *r = podule_if_get_regs();
        unsigned int tail = r[PR_TX_TAIL];

        // Descriptor clean/non-ready:
        PR_TX_DESCR(r, tail) = PR_TX_DESCR(r, tail) & ~(PR_DESCR_READY);
        // Move on tail pointer:
        tail = (tail + 1) & 7;
        r[PR_TX_TAIL] = tail;
}

static void     pipe_tx_start(uint32_t descr)
{
        volatile uint8_t *r = podule_if_get_regs();

        unsigned int len = PR_DESCR_SIZE(descr);
        unsigned int cid = PR_DESCR_CID(descr);
        unsigned int addr = PR_DESCR_ADDR(descr);
        uint8_t *tx_data = (uint8_t *)((uintptr_t)&r[PR_TX_BUFFERS] +
                                       (uintptr_t)addr);

        if (addr + len >= PR_RX_TX_BUFSZ) {
                printf("[pipe TX ERROR: TX off end of buffer! "
                       "%08x, CID%d, addr %d, len %d - dropping packet]\n",
                       descr, cid, addr, len);
                pipe_tx_done();
                return;
        }

#if DEBUG > 0
        printf("[pipe TX start: %08x: CID%d, addr %d/%p, len %d]\n",
               descr, cid, addr, tx_data, len);
#endif
        /* One wart is that we send a little header before the data.
         *
         * An approach to this (given that the tud_cdc_n_write() below
         * might just accept 1 byte right now) is to have a FSM
         * tracking how much of the header's been sent, moving onto
         * the data eventually.
         *
         * It's much easier, though, to just assemble the outgoing
         * data into a buffer (memcpy the payload) and track progress
         * through that.
         */

        state.tx_buf[0] = cid;
        state.tx_buf[1] = len & 0xff;
        state.tx_buf[2] = (len >> 8) & 0xff;

        memcpy(&state.tx_buf[3], tx_data, len);
        state.tx_total = len + 3;

        /* Try to queue as much as possible in the USB TX FIFOs: */
        unsigned int tx_written = tud_cdc_n_write(0, state.tx_buf, state.tx_total);
        tud_cdc_n_write_flush(0);

#if DEBUG > 2
        for (int i = 0; i < tx_written; i++) {
                printf("%02x ", state.tx_buf[i]);
        }
        printf("\n");
#endif
        if (tx_written == state.tx_total) {
#if DEBUG > 1
                printf("[pipe TX done: submitted %d in one go]\n", tx_written);
#endif
                /* Submitted entire packet, now it's SEP. */
                state.tx_ongoing = false;
                pipe_tx_done();
        } else {
#if DEBUG > 1
                printf("[pipe TX ongoing: submitted %d, %d total]\n",
                       tx_written, state.tx_total);
#endif
                /* We're not done with the packet, there's more work
                 * to do later on.
                 *
                 * The descriptor is still marked ready in the queue,
                 * but this state change inhibits the register i/f
                 * continually starting a new tx, and the rest of the
                 * work occurs via pipe_tx_continue().
                 */
                state.tx_ongoing = true;
                state.tx_pos = tx_written;

                /* The packet descriptor remains ready/unconsumed;
                 * it's consumed in pipe_tx_done().  We could consume
                 * it here just as easily.
                 *
                 * Note this would be a different semantic of queue
                 * view, where consuming a packet doesn't mean it's
                 * hit USB yet.
                 */
        }
}

static void     pipe_tx_continue(void)
{
        if (!state.tx_ongoing) {
                printf("[pipe TX ERROR: continue, but no work to do!\n");
                return;
        }

        /* Send more data: */
        unsigned int tx_written = tud_cdc_n_write(0,
                                                  &state.tx_buf[state.tx_pos],
                                                  state.tx_total - state.tx_pos);
        tud_cdc_n_write_flush(0);

        state.tx_pos += tx_written;
        if (state.tx_pos >= state.tx_total) {
#if DEBUG > 1
                printf("[pipe TX complete: submitted %d of %d total]\n",
                               tx_written, state.tx_total);
#endif
                state.tx_ongoing = false;
                pipe_tx_done();
        } else {
                if (tx_written != 0) {
                        // If it's really busy, and repeatedly writing 0, be quiet.
#if DEBUG > 1
                        printf("[pipe TX ongoing2: submitted %d, now %d of %d total]\n",
                               tx_written, state.tx_pos, state.tx_total);
#endif
                }

        }
}

static void dump_regs()
{
        volatile uint8_t *r = podule_if_get_regs();

        for (int i = 0; i < 2048; i += 16) {
                printf("%03x: ", i);
                for (int j = 0; j < 16; j++) {
                        printf("%02x ", r[i+j]);
                }
                printf("  ");
                for (int j = 0; j < 16; j++) {
                        uint8_t c = r[i+j];
                        printf("%c", c >= 32 ? c : '.');
                }
                printf("\n");
        }
}

// Check whether the packet descriptors have some work for us (or an ongoing transfer)
void pipe_poll(void)
{
        volatile uint8_t *r = podule_if_get_regs();

        /* Check USB */
        bool cdc_connected = tud_cdc_n_connected(0);
        static bool last_connected = false;

        if (!cdc_connected && last_connected) {
                // Disconnect occurred!
                printf("[pipe disconnected]\n");
        }
        last_connected = cdc_connected;

        //////////////////////////////////////////////////////////////////////
        // Receive

        /* FIXME: Simple data sink/debugging for now.  Implement RX!
         */
        if (cdc_connected && tud_cdc_n_available(0)) {
                unsigned int count = tud_cdc_n_read(0, rx_buf, sizeof(rx_buf));
#if DEBUG > 0
                printf("+++ Rx'd %d bytes from USB\n", count);
#endif
#if DEBUG > 2
                for (int i = 0; i < count; i++) {
                        uint8_t c = rx_buf[i];
                        printf("%02x(%c) ", c, c >= 32 ? c : '.');
                }
                printf("\r\n");
#endif
                if (rx_buf[0] == '^')
                        dump_regs();
        }

        //////////////////////////////////////////////////////////////////////
        // Transmit

        if (state.tx_ongoing) {
                if (cdc_connected) {
                        pipe_tx_continue();
                } else {
                        state.tx_ongoing = false;
                }
        } else {
                /* Check registers: */

                unsigned int tail = r[PR_TX_TAIL];
                uint32_t descr = PR_TX_DESCR(r, tail);

                if (PR_DESCR_IS_READY(descr)) {
                        if (cdc_connected)
                                pipe_tx_start(descr);
                        else
                                pipe_tx_done(); // Consume immediately
                }
        }
}
