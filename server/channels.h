/*
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

#ifndef CHANNELS_H
#define CHANNELS_H

// Channel types
#define CID_IGNORE                      0
#define CID_HOSTINFO                    1
#define CID_HOSTINFO_PROTO_VERSION      1
#define CID_HOSTINFO_STRING             "ArcPipePodule host server" // 28 max
#define CID_RAWFILE                     2
#define CID_RAWFILE_INIT_READ           0
#define CID_RAWFILE_READ_BLOCK          1
#define CID_RAWFILE_CLOSE               4

extern void     channel_hostinfo_rx(uint8_t *data, unsigned int len);

extern void     channel_rawfile_init(void);
extern void     channel_rawfile_rx(uint8_t *data, unsigned int len);

extern void     send_packet(unsigned int cid, unsigned int len, uint8_t *data);

#endif
