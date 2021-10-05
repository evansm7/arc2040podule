/* Host-side server for ArcPipePodule
 *
 * 27 Sep 2021
 * Very bare-bones late-night PoC!
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
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <inttypes.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <endian.h>

#include "channels.h"

#define DEBUG 2

#define TTY_DEVICE      "/dev/ttyACM0"  // FIXME: cmdline option!


/* Input state machine */
typedef enum {
        ST_IDLE=0,
        ST_READING_HDR,
        ST_READING_DATA
} rx_state_t;

static rx_state_t rxs = ST_IDLE;
static uint8_t rx_buffer[4096];
static unsigned int rx_pos = 0;

static uint8_t tx_buffer[4096];
static int tx_len = -1;
static unsigned int tx_pos = 0;

typedef struct {
        uint8_t cid;
        uint8_t sizel;
        uint8_t sizeh;
} pkt_header_t;


////////////////////////////////////////////////////////////////////////////////
// Utils

// scandir/opendir

static void     pretty_hexdump(uint8_t *r, unsigned int len)
{
        unsigned int left = len;
        for (int i = 0; i < len; i += 16) {
                unsigned int rowlen = left > 16 ? 16 : left;
                printf("%03x: ", i);
                for (int j = 0; j < rowlen; j++) {
                        printf("%02x ", r[i+j]);
                }
                printf("  ");
                for (int j = 0; j < rowlen; j++) {
                        uint8_t c = r[i+j];
                        printf("%c", c >= 32 ? c : '.');
                }
                printf("\n");
                left -= 16;
        }
}

void            send_packet(unsigned int cid, unsigned int len, uint8_t *data)
{
        if (tx_len != -1) {
                // FIXME, TX is rubbish, needs a queue
                printf("Yarrrgh! TX busy!\n");
        }
        pkt_header_t *pkt = (pkt_header_t *)&tx_buffer[0];
        pkt->cid = cid;
        pkt->sizel = len & 0xff;
        pkt->sizeh = len >> 8;
        memcpy(&tx_buffer[3], data, len);

        tx_pos = 0;
        tx_len = len + 3;

        // Main loop sorts it.
}

////////////////////////////////////////////////////////////////////////////////
// Channel Hostinfo

void            channel_hostinfo_rx(uint8_t *data, unsigned int len)
{
        if (data[0] == 0) {
#if DEBUG > 1
                printf("+++ hostinfo request (%d)\n", data[0]);
#endif
                // Format host info string
                // FIXME: Do protocol version, capabilities etc.
                struct {
                        uint32_t proto_ver;
                        char    hinfo[28];
                        uint32_t pad;
                } response;

                response.proto_ver = htole32(CID_HOSTINFO_PROTO_VERSION);
                memset(response.hinfo, 0, 28);
                strncpy(response.hinfo, CID_HOSTINFO_STRING, 28);
                response.pad = 0;

                send_packet(CID_HOSTINFO, sizeof(response), (uint8_t *)&response);
        } else {
                printf("hostinfo: Odd byte 0: 0x%x\n", data[0]);
        }
}

////////////////////////////////////////////////////////////////////////////////
// Core packet dispatch

static void     process_packet(unsigned int cid, unsigned int len, uint8_t *data)
{
#if DEBUG > 1
        printf("+++ Packet CID%d, len %d\n", cid, len);
#if DEBUG > 2
        pretty_hexdump(data, len);
#endif
#endif
        switch (cid) {
        case CID_IGNORE:
                break;

        case CID_HOSTINFO:
                channel_hostinfo_rx(data, len);
                break;

        case CID_RAWFILE:
                channel_rawfile_rx(data, len);
                break;
        }
}

////////////////////////////////////////////////////////////////////////////////
// Infra for input/output & main service loop

static void     process_input(int fd)
{
        int r;

        do {
                // Try a large read; O_NONBLOCK returns EAGAIN instead of blockin'
                r = read(fd, &rx_buffer[rx_pos],
                         sizeof(rx_buffer) - rx_pos);
                if (r < 0) {
#if DEBUG > 0
                        if (errno != EAGAIN)
                                printf("- Read error %d\n", errno);
#endif
                        return;
                }
#if DEBUG > 2
                printf("Received %d\n", r);
#endif
                rx_pos += r;

        packet_check:
                if (rx_pos > sizeof(pkt_header_t)) {
                        pkt_header_t *pkt = (pkt_header_t *)rx_buffer;
                        uint16_t data_len = pkt->sizel + (pkt->sizeh * 256);
                        unsigned int dend = sizeof(pkt_header_t) + data_len;

                        if (rx_pos >= dend) {
                                // We can access the entire packet.  Consume/process:
                                process_packet(pkt->cid, data_len,
                                               &rx_buffer[sizeof(pkt_header_t)]);
                        }
                        // Reset read buffer
                        if (rx_pos == dend) {
                                rx_pos = 0;
                        } else if (rx_pos > dend) {
                                // We read some of the next request too.
                                // Hacky, but shuffle that down to index 0...
                                unsigned int excess = dend - rx_pos;
                                memmove(&rx_buffer[0], &rx_buffer[dend], excess);
                                // And, we reset everything:
                                rx_pos = excess;
#if DEBUG > 1
                                printf("Read %d, pkt %d, excess %d\n",
                                       rx_pos, dend, excess);
#endif
                                goto packet_check;
                        }
                }
        } while (r > 0);
}

static void     process_output(int fd)
{
        int r;

        do {
                r = write(fd, &tx_buffer[tx_pos], tx_len - tx_pos);

                if (r < 0) {
#if DEBUG > 0
                        if (errno != EAGAIN)
                                printf("- Write error %d\n", errno);
#endif
                        return;
                }
#if DEBUG > 2
                printf("Wrote %d\n", r);
#endif
                tx_pos += r;

                if (tx_pos == tx_len) {
                        printf("+++ TX of %d complete\n", tx_len);

                        tx_len = -1;
                        return;
                }
        } while (r > 0);
}

static int      open_device(char *path)
{
        int r = open(path, O_RDWR);

        if (r < 0) {
                return r;
        }

        struct termios tos;
        if (tcgetattr(r, &tos) < 0) {
                perror("Can't tcgetattr:");
                return -1;
        }
        cfmakeraw(&tos);
        if (tcsetattr(r, TCSAFLUSH, &tos)) {
                perror("Can't tcsetattr:");
                return -1;
        }

        int f = fcntl(r, F_GETFL);
        fcntl(r, F_SETFL, f | O_NONBLOCK);

        return r;
}

static void     service_loop(int fd)
{
        channel_rawfile_init();

        while (1) {
                struct pollfd pfd = {
                        .fd = fd,
                        .events = POLLIN | POLLHUP,
                        .revents = 0
                };

                int r;
                r = poll(&pfd, 1, -1);

                if (pfd.revents & POLLHUP) {
                        close(fd);
                        break;
                } else if (pfd.revents & POLLIN) {
                        /* FIXME: inhibits this if there's TX pending, as
                         * it'll likely create more output...
                         */
                        if (tx_len == -1)
                                process_input(fd);
                }

                if (tx_len != -1) {
                        process_output(fd);
                }
        }
}

int             main(void)
{
        while (1) {
                int fd;
                printf("Opening %s\n", TTY_DEVICE);

                fd = open_device(TTY_DEVICE);

                if (fd < 0) {
                        perror("- Can't open device");
                        sleep(1);
                        continue;
                }
                printf("+++ Main loop, fd %d\n", fd);
                service_loop(fd);
        }
        return 0;
}
