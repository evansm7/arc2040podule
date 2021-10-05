/* RawFile channel commands
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
        .globl cmd_pipe_copy_to_local
        .globl str_pcpl_help
        .globl str_pcpl_syntax

cmd_pipe_copy_to_local:
        stmfd   r13!, {r0-r12, lr}

        // r0 = command tail (preserve)
        // r1 = number of OSCLI parameters
        // r12 = pointer to module private word
        ldr     r12, [r12]
        mov     r10, r0

        cmp     r1, #2                          // OS should've checked arg nr!
        adrne   r0, err_pcpl_wrong_params
        bne     98f

#if DEBUG > 2
        ES("Cmd tail ptr:")
        mov     r0, r10
        bl      print_hex32
        swi     SWI_OS_NEWLINE | SWI_X
#endif
        /* Separate the args.
         *
         * The args string is terminated by a 0x0d *not* a zero!  Arrrghhhh.
         * The string contains args separated by a single space.
         */

        /* FIXME: This just dumbly looks for a space separator and
	 * doesn't support spaces in the host filename.  Check for
	 * quotes, escapes etc.
         */

        /* FIXME: Take variable 1 or 2 arguments, and take first filename
         * as local one if second not provided.
         * FIXME: There's max scope for incompatible names in RISC OS.
         */

        // r10 = first argument
        mov     r0, r10
        mov     r2, #0

        // Find end of first argument, and zero-terminate it:
1:      ldrb    r1, [r0]
        cmp     r1, #' '
        addne   r0, r0, #1
        bne     1b
        strb    r2, [r0], #1

        // r11 = second argument
        mov     r11, r0

        // Find end of second argument, and zero-terminate it:
1:      ldrb    r1, [r0], #1
        cmp     r1, #0x0d
        bne     1b
        strb    r2, [r0, #-1]

        // The second argument might have a trailing space.  If so, remove that:
        ldrb    r1, [r0, #-2]
        cmp     r1, #' '
        streqb  r2, [r0, #-2]

#if DEBUG > 0
        // Print the args:
        ES("Copy from: ")
        mov     r0, r10
        swi     SWI_OS_WRITE0 | SWI_X
        swi     SWI_OS_NEWLINE | SWI_X
        ES("Copy to: ")
        mov     r0, r11
        swi     SWI_OS_WRITE0 | SWI_X
        swi     SWI_OS_NEWLINE | SWI_X
#endif

        /* OK.  Send a request to the host to open the file, which returns
         * the file size (FIXME: and type???).  We then make repeated requests
         * for file data, writing it locally.
         */
        add     r9, r12, #WS_SCRATCH

        mov     r2, #0                          // Message 0 on CID 2 = InitiateRead
        strb    r2, [r9]
        add     r0, r9, #1
        mov     r1, r10                         // Host filename from arg 1
        bl      strcpy                          // FIXME: this WILL overflow!

        // r0 = pointer, r1 = len, r2 = CID, r12=workspace
        mov     r0, r9
        mov     r1, #256
        add     r1, r1, #1                      // Len 257
        mov     r2, #2
        bl      pipe_packet_tx
        bvs     98f                             // Error?

        // Now wait for an InitiateReadResp packet:
        add     r0, r12, #WS_SCRATCH
        bl      pipe_packet_rx
        bvs     98f                             // Error?
        // FIXME: Assert returned CID matches 2.  Expecting InitiateReadResp

        ldrb    r1, [r9, #0]
        cmp     r1, #0
        beq     1f

        ES("Can't open file, error = ")
        mov     r0, r1
        bl      print_hex8
        swi     SWI_OS_NEWLINE | SWI_X

        b       99f

1:      // OK, success opening the thing.  Save some basic info:
        ldr     r8, [r9, #4]                    // r8 = total len
        ldr     r6, [r9, #8]                    // r6 = load address
        ldr     r7, [r9, #12]                   // r7 = exec address

#if DEBUG > 0
        ES("Remote file len,load,exec: ")
        mov     r0, r8
        bl      print_hex32
        mov     r0, #','
        swi     SWI_OS_WRITEC | SWI_X
        mov     r0, r6
        bl      print_hex32
        mov     r0, #','
        swi     SWI_OS_WRITEC | SWI_X
        mov     r0, r7
        bl      print_hex32
        swi     SWI_OS_NEWLINE | SWI_X
#endif
        // Now, try to open the local file (name in r11):
        mov     r0, #0x80                       // Create new file
        mov     r1, r11
        swi     SWI_OS_FIND | SWI_X
        bvc     1f
2:      // Error :(  Save error:
        mov     r8, r0
        mov     r0, r9
        bl      cmd_pipe_copy_to_local_exit_close_host
        mov     r0, r8
        b       98f
1:
        // Opened file!  Now we can read some blocks, and write 'em!
        mov     r10, r11                        // r10 = local name
        mov     r11, r0                         // r11 = file handle

        mov     r5, #0                          // File position
pcpl_get_block_loop:
        // Get block from host
#if DEBUG > 2
        ES("+ Requesting block for ")
        mov     r0, r5
        bl      print_hex32
        swi     SWI_OS_NEWLINE | SWI_X
#endif

        mov     r0, #1                          // ReadBlock
        strb    r0, [r9, #0]
        str     r5, [r9, #4]
        sub     r4, r8, r5
        cmp     r4, #512
        movgt   r4, #512
        str     r4, [r9, #8]                    // Block size
        mov     r0, r9
        mov     r1, #16
        mov     r2, #2
        bl      pipe_packet_tx
        bvs     cmd_pipe_copy_to_local_err_cleanup

#if DEBUG > 3
        ES("+ Requested, waiting for resp.\r\n")
#endif

        mov     r0, r9
        bl      pipe_packet_rx
        bvs     cmd_pipe_copy_to_local_err_cleanup
        // Get data back (r4 in length)

#if DEBUG > 3
        ES("+ Got block, writing it.\r\n")
#endif

        mov     r0, #1
        mov     r1, r11
        mov     r2, r9
        mov     r3, r4                          // Number of bytes
        mov     r4, r5                          // File offset
        swi     SWI_OS_GBPB | SWI_X
        // Error :(  Save error, close file, bomb out:
        bvs     cmd_pipe_copy_to_local_err_cleanup

        add     r5, r5, #512
        cmp     r5, r8
        blt     pcpl_get_block_loop

        /* Now, set the type as given by the server (r6,r7).
         * If top 12 bits of LA are all set, file has a type:
        */
        mvns    r0, r6, asr#20
        bne     1f                              // Zero if all set
#if DEBUG > 1
        ES("Setting type\r\n")
#endif
        mov     r0, #18                         // OS_File 18
        mov     r2, r6, lsl#12                  // File type in r2
        mov     r2, r2, lsr#20
        b       2f
1:
#if DEBUG > 1
        ES("Setting load/exec\r\n")
#endif
        mov     r0, #1                          // OS_File 1
        mov     r2, r6                          // Load in r2
        mov     r3, r7                          // Exec in r3
        mov     r5, #3                          // Attrs in r5: WR/--
2:      mov     r1, r10                         // Filename
        swi     SWI_OS_FILE | SWI_X
        bvs     cmd_pipe_copy_to_local_err_cleanup

#if DEBUG > 0
        ES("Transfer complete.\r\n")
#endif
        // Done

        mov     r0, r11
        bl      cmd_pipe_copy_to_local_exit_close_file

#if DEBUG > 0
        ES("Closed local file.\r\n")
#endif

        mov     r0, r9
        bl      cmd_pipe_copy_to_local_exit_close_host

#if DEBUG > 0
        ES("Closed host file.\r\n")
#endif

99:
        ldmfd   r13!, {r0-r12, pc}^
98:
        add     r13, r13, #4
        ldmfd   r13!, {r1-r12,lr}
        orrs    pc, lr, #V_BIT

cmd_pipe_copy_to_local_err_cleanup:
        mov     r8, r0
        mov     r0, r11
        bl      cmd_pipe_copy_to_local_exit_close_file
        mov     r0, r9
        bl      cmd_pipe_copy_to_local_exit_close_host
        mov     r0, r8

        b       98b

cmd_pipe_copy_to_local_err_param_too_long:
        adr     r0, err_pcpl_param_too_long
        b       98b

cmd_pipe_copy_to_local_exit_close_file:
        stmfd   r13!, {lr}
        // Close file:
        mov     r1, r0
        mov     r0, #0                          // Close
        swi     SWI_OS_FIND | SWI_X
        ldmfd   r13!, {pc}^

        // r0 = buffer for TX
cmd_pipe_copy_to_local_exit_close_host:
        stmfd   r13!, {r1,r2,lr}
        // Close host file:
        // Message 4 on CID 2 = Close
        mov     r2, #4
        strb    r2, [r0, #0]
        mov     r1, #1
        mov     r2, #2
        bl      pipe_packet_tx
        ldmfd   r13!, {r1,r2,pc}^


str_pcpl_help:
        .asciz "Pipe Copy to Local:  Copies a file from the remote pipe server to a local path"
str_pcpl_syntax:
        .asciz "Syntax: pcpl <host path> <local path>"
        .align
err_pcpl_wrong_params:
        .long   ERR_BASE + 2
        .asciz "Problem: Wrong number of params (shouldn't happen!)"
        .align
err_pcpl_param_too_long:
        .long   ERR_BASE + 3
        .asciz "Parameter too long"
        .align
