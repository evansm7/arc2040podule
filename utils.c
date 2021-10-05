/* Misc utilities for arc2040podule firmware
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

void    pretty_hexdump(uint8_t *r, unsigned int len, int zeroidx)
{
        unsigned int left = len;
        uintptr_t idx = zeroidx ? 0 : (uintptr_t)r;

        for (int i = 0; i < len; i += 16) {
                unsigned int rowlen = left > 16 ? 16 : left;
                printf("%08x: ", idx + i);
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
