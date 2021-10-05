/*
 * Podule loader for arc2040podule
 *
 * Run by RISC OS, interfaces between RISC OS and the bank-switch protocol in
 * arc_pipe_podule.  The loader is an "accessor" with a "get byte" routine to
 * abstract away hardware details of the podule.
 *
 * See https://www.chiark.greenend.org.uk/~theom/riscos/docs/PodLoader.txt for
 * info on RISC OS podule loaders.
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

#include "../podule_regs.h"

#define V_bit           (1<<28)

        .text
        .org    0
        .globl _start
_start:
        //////////////////////////////////////////////////////////////////////
        // Podule loader header:
        b       read_byte
        b       write_byte
        b       reset
        bics    pc, lr, #V_bit                 // CallLoader


        //////////////////////////////////////////////////////////////////////
reset:
        ldr     r2, mask
        and     r2, r11, r2
        add     r2, r2, #PR_BASE
        add     r2, r2, #PR_RESET << 2
        ldrb    r3, [r2]
        add     r3, r3, #1
        strb    r3, [r2]

        mvn     r3, #0
        str     r3, current_page
        bics    pc, lr, #V_bit


        //////////////////////////////////////////////////////////////////////
read_byte:
        cmp     r1, #(32*1024*1024)
        adrge   r0, too_big
        orrges  pc, lr, #V_bit

        ldr     r2, mask
        and     r2, r11, r2                     // r2=Podule base addr

        mov     r3, r1, asr#10                  // r3=1KB page idx
        ldr     r10, current_page
        cmp     r3, r10
        beq     page_ok

        // Change page:
        add     r2, r2, #PR_BASE
        str     r3, current_page
        mov     r10, r3, asr#8                  // High byte of page number
        orr     r10, r10, #0x80                 // Set handshake flag
        strb    r3, [r2, #PR_PAGE_L << 2]       // Store low byte
        strb    r10, [r2, #PR_PAGE_H << 2]      // Store high byte

        // Now, poll for completion:
        mov     r3, #1024*1024                  // Timeout
2:      ldrb    r10, [r2, #PR_PAGE_H << 2]      // Read flag
        tst     r10, #0x80
        beq     1f                              // if flag clear, success
        subs    r3, r3, #1
        beq     timeout
        b       2b
1:
        sub     r2, r2, #PR_BASE

page_ok:
        mov     r3, r1, lsl#22                  // Get bottom 10 bits of addr
        mov     r3, r3, lsr#22                  // r1 is offset into data window
        add     r3, r3, #1024
        ldrb    r0, [r2, r3, lsl#2]             // Convert byte idx into words

        bics    pc, lr, #V_bit

timeout:
        adr     r0, page_timeout
        orrs    pc, lr, #V_bit


        //////////////////////////////////////////////////////////////////////
write_byte:
        adr     r0, cant_write
        orrs    pc, lr, #V_bit


        //////////////////////////////////////////////////////////////////////
        // Vars
        /* Mask: applied to address, can be used to clear [20:19], forcing
         * a slow cycle.  The default (sync?) access is OK on stock A3000 tho.
         */
mask:           .long   0x03fff000
        /* Slow cycles: .long   0x03fff000 & ~(3<<19) */
current_page:   .long   ~0


        //////////////////////////////////////////////////////////////////////
        // Errors
cant_write:
        .long   0xda7adead
        .asciz "Not writeable"
        .align

too_big:
        .long   0xda7adecc
        .asciz "Address too big (>32M)"
        .align

page_timeout:
        .long   0xda7adedd
        .asciz "Timeout waiting for page-change completion!"
        .align

        .end
