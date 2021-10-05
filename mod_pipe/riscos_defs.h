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

#ifndef RISCOS_DEFS_H
#define RISCOS_DEFS_H

#define SWI_X           0x20000
#define SWI_OS_WRITEC   0x0
#define SWI_OS_WRITES   0x1
#define SWI_OS_WRITE0   0x2
#define SWI_OS_NEWLINE  0x3
#define SWI_OS_FILE     0x08
#define SWI_OS_GBPB     0x0c
#define SWI_OS_FIND     0x0d
#define SWI_OS_MODULE   0x1e
#define SWI_OS_GSTRANS  0x27

#define V_BIT           (1 << 28)

// Embedded string:
#define ES(str)        swi     SWI_OS_WRITES | SWI_X ; \
        .asciz str                                   ; \
        .align

#endif
