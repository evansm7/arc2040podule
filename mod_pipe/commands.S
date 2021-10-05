/* Module command table, and simple commands
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
#include "riscos_defs.h"
#include "module.h"

        .text
        .globl cmd_table

        //////////////////////////////////////////////////////////////////////
        // Commands:
cmd_table:
        .asciz  "pt"    // "pipe test"
        .align  2       // Word-align
        .long   cmd_pipetest
        // Flags word:
        .byte   0       // Min params
        .byte   0       // No GSTrans
        .byte   0       // Max params
        .byte   0       // Flags
        .long   0       // No syntax error message
        .long   0       // No help message

        .asciz  "pi"    // "pipe info"
        .align  2       // Word-align
        .long   cmd_pipeinfo
        // Flags word:
        .byte   0       // Min params
        .byte   0       // No GSTrans
        .byte   0       // Max params
        .byte   0       // Flags
        .long   0       // No syntax error message
        .long   0       // No help message

        .asciz  "pcpl"  // "pipe copy to local (from remote)"
        .align  2       // Word-align
        .long   cmd_pipe_copy_to_local
        // Flags word:
        .byte   2       // Min params
        .byte   0x02    // No GSTrans on param 0, GSTrans on param 1
        .byte   2       // Max params
        .byte   0       // Flags
        .long   str_pcpl_syntax
        .long   str_pcpl_help

        .long   0       // End


        //////////////////////////////////////////////////////////////////////
cmd_pipetest:
        // r0 = command tail (preserve)
        // r1 = number of OSCLI parameters
        // r12 = pointer to module private word

        stmfd   r13!, {r0-r12, lr}

        // Print some info:
        adr     r0, str_hw_at
        swi     SWI_OS_WRITE0 | SWI_X
        bvs     99f

        ldr     r12, [r12]      // Workspace pointer

        ldr     r0, [r12, #WS_HW]
        bl      print_hex32

        swi     SWI_OS_NEWLINE | SWI_X
        bvs     99f

        // Send a test packet.

        // Construct data to send (nice if it's generally human-readable):
        add     r0, r12, #WS_SCRATCH
        mov     r1, #32
        mov     r2, #0
1:      strb    r1, [r0, r2]
        add     r1, r1, #1
        cmp     r1, #128
        movge   r1, #32
        add     r2, r2, #1
        cmp     r2, #256
        bne     1b

        // r0 = pointer, r1 = len, r2 = CID, r12=workspace
        mov     r1, #256
        mov     r2, #0

        bl      pipe_packet_tx

        ldmfd   r13!, {r0-r12, pc}^

99:     add     r13, r13, #4
        ldmfd   r13!, {r1-r12,lr}
        orrs    pc, lr, #V_BIT

str_hw_at:      .asciz  "Hardware address: "
        .align


        //////////////////////////////////////////////////////////////////////

cmd_pipeinfo:
        // r0 = command tail (preserve)
        // r1 = number of OSCLI parameters
        // r12 = pointer to module private word

        stmfd   r13!, {r0-r12, lr}

        ldr     r12, [r12]      // Workspace pointer

        // Construct data to send (nice if it's generally human-readable):
        add     r0, r12, #WS_SCRATCH

        // Message 0 on CID 1 = GetInfo
        mov     r2, #0
        strb    r2, [r0]
        // r0 = pointer, r1 = len, r2 = CID, r12=workspace
        mov     r1, #1
        mov     r2, #1
        bl      pipe_packet_tx

#if DEBUG > 2
        ES("Tx done\r\n")
#endif

        add     r0, r12, #WS_SCRATCH
        bl      pipe_packet_rx
        // FIXME: Assert returned CID matches 1 too?  Expecting GetInfoResp
#if DEBUG > 2
        ES("Rx done\r\n")
#endif

        // Print CID:
        mov     r0, r2
        bl      print_hex8
        mov     r0, #':'
        swi     SWI_OS_WRITEC | SWI_X
        // Print len:
        mov     r0, r1
        bl      print_hex32
        mov     r0, #':'
        swi     SWI_OS_WRITEC | SWI_X
        // Print protocol version:
        add     r10, r12, #WS_SCRATCH
        ldr     r0, [r10, #0]
        bl      print_hex32
        mov     r0, #':'
        swi     SWI_OS_WRITEC | SWI_X
        // Print version string:
        add     r0, r10, #4
        swi     SWI_OS_WRITE0 | SWI_X
        swi     SWI_OS_NEWLINE | SWI_X

        ldmfd   r13!, {r0-r12, pc}^


        .end
