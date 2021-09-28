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

        unsigned int rx_total;
        unsigned int rx_pos;
        unsigned int rx_last_descr;
        bool rx_packet_pending;
        uint8_t rx_buf[512 + 3];
} pp_state_t;

static pp_state_t state;

#define PKT_HDR_SIZE    3


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
        state.rx_pos = 0;
        state.tx_pos = 0;

        state.rx_last_descr = 0;
        state.rx_packet_pending = false;
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

        memcpy(&state.tx_buf[PKT_HDR_SIZE], tx_data, len);
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


/* Add packet to RX buffer.  Returns true if accepted/there is space,
 * allowing rudimentary flow-control/backpressure.
 */
static bool     pipe_rx_packet(uint8_t cid, uint16_t len, uint8_t *data)
{
        volatile uint8_t *r = podule_if_get_regs();

        // FIXME: Multiple RX buffers!
        unsigned int last = state.rx_last_descr;
        unsigned int head = r[PR_RX_HEAD];

        /* Before then, this is a hack.  It makes sure there are no
         * unconsumed RX descriptors (checking that the last created
         * has been consumed), because an unconsumed descriptor means
         * an unconsumed RX buffer (and there's only one of those).
         */

        if (PR_DESCR_IS_READY(PR_RX_DESCR(r, last))) {
                static int last_last = -1;
                if (last_last != last) {
                        // Dumb rate-limiting
                        printf("[pipe RX: No room for packet]\n");
                        last_last = last;
                }
                return false;
        }

        if (len > PR_RX_TX_BUFSZ) {
                printf("[pipe RX ERROR: Packet size %d is too large! Dropping.]\n",
                       len);
                return true;
        }

        /* Create a descriptor at PR_RX_HEAD: */
        unsigned int addr = 0;  // Right at the start of the RX buffer

        memcpy((void *)&r[PR_RX_BUFFERS + addr], data, len);
        // Once data is in place, construct the descriptor -- making it ready:
        PR_RX_DESCR(r, head) = ((uint32_t)cid << PR_DESCR_CID_SHIFT) |
                ((uint32_t)len << PR_DESCR_SIZE_SHIFT) |
                (addr << PR_DESCR_ADDR_SHIFT) |
                PR_DESCR_READY;

        state.rx_last_descr = head;
        // Move on head pointer:
        head = (head + 1) & 7;
        r[PR_RX_HEAD] = head;

        return true;
}

/* Assembles a single packet from possibly multi-chunk multi-receives,
 * staging the data into the state.rx_buf buffer until it's complete, then
 * copying that into the RX buffer area.
 *
 * This could be made more complex and zero-copy by receiving just the header
 * (to work out size) then allocating a correctly-sized block in the RX area,
 * but "meh" and we're not close to needing to scrimp on memory either.
 */
static void     pipe_rx(void)
{
        unsigned int len = 0;
        static unsigned int pkt_counter = 0;

        /* If a packet's pending, try to deliver it without receiving more USB
         * data.
         */
        if (!state.rx_packet_pending) {
                /* If we're in the middle of a packet, state.rx_pos is
                 * non-zero so that this receive places data at the end of
                 * previous data: */
                len = tud_cdc_n_read(0, &state.rx_buf[state.rx_pos],
                                       sizeof(state.rx_buf) - state.rx_pos);
#if DEBUG > 0
                printf("[pipe RX %d bytes]\n", len);
#endif

                state.rx_pos += len;
        }

        // Check for packet partial/complete:
        if (state.rx_pos >= PKT_HDR_SIZE) {
                // Decode size
                uint16_t data_len = state.rx_buf[1] | ((uint32_t)state.rx_buf[2] << 8);
                state.rx_total = PKT_HDR_SIZE + data_len;
#if DEBUG > 2
                printf("[pipe RX packet header: CID%d, size %d]\n",
                       state.rx_buf[0], state.rx_total);
#endif
                // Did we get all of it?
                if (state.rx_pos >= state.rx_total) {
                        // Pop it into the RX buffer/descriptor for Arc to see:
                        bool accepted = pipe_rx_packet(state.rx_buf[0],
                                                       data_len,
                                                       &state.rx_buf[PKT_HDR_SIZE]);
#if DEBUG > 1
                        if (accepted) {
                                printf("[pipe RX packet complete: CID%d, size %d]\n",
                                       state.rx_buf[0], state.rx_total);
                        } else {
                                // Rate-limit this!
                                static int last_count = ~0;
                                if (pkt_counter != last_count) {
                                        printf("[pipe RX packet stalled: "
                                               "CID%d, size %d]\n",
                                               state.rx_buf[0], state.rx_total);
                                        last_count = pkt_counter;
                                }
                        }
#endif
                        /* If it wasn't accepted (no room in RX queue,
                         * Arc isn't listening) then just quit now.
                         * It's safe, because next loop we'll try this
                         * all over again.  In the meantime, USB reads
                         * aren't performed so the backpressure
                         * propagates to the host.
                         */
                        if (!accepted) {
                                state.rx_packet_pending = true;
                                return;
                        } else {
                                pkt_counter++;
                                state.rx_packet_pending = false;
                        }

                        // Did we read too much/some of the next packet?
                        if (state.rx_pos > state.rx_total) {
                                /* Move the excess data read to the
                                 * bottom of the buffer, so that the
                                 * next read just tacks data onto the
                                 * end:
                                 */
                                unsigned int excess = state.rx_total -
                                        state.rx_pos;
                                memmove(&state.rx_buf[0],
                                        &state.rx_buf[state.rx_total],
                                        excess);
                                state.rx_pos = excess;
#if DEBUG > 1
                                printf("[pipe RX packet excess %d]\n",
                                       excess);
#endif
                        } else {
                                /* We read an exact amount,  Complete now,
                                 * and next RX occurs at the start, afresh.
                                 */
                                state.rx_pos = 0;
                        }
                }
        }
        // Else do nothing, next reception we'll check again.
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

        if ((cdc_connected && tud_cdc_n_available(0)) || state.rx_packet_pending) {
                pipe_rx();
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
