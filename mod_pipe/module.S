        /* arc_pipe_podule interface module
         *
         * Copyright 2021 Matt Evans
         *
         */

#include "../podule_regs.h"

        /* Tunables: */
#define DEBUG   1

        /* Defs: */
#define SWI_X           0x20000
#define SWI_OS_WRITEC   0x0
#define SWI_OS_WRITE0   0x2
#define SWI_OS_NEWLINE  0x3
#define SWI_OS_MODULE   0x1e

#define WORKSPACE_SZ    4096
#define WS_HW           0
#define WS_TX_HEAD      4
#define WS_RX_TAIL      8
#define WS_SCRATCH      3072

        .text
        .org    0
        .globl _start
_start:
        //////////////////////////////////////////////////////////////////////
        // Header
module_header:
        .long   0               // No run/start code
        .long   init
        .long   fini
        .long   0               // No service call
        .long   str_title
        .long   str_help
        .long   cmd_table
        .long   0               // No SWI Chunk number
        .long   0               // No SWI handler
        .long   0               // No SWI names
        .long   0               // No SWI decoder
        .long   0               // No messages
        .long   0               // No module flags


        //////////////////////////////////////////////////////////////////////
        // Fini
fini:
        stmfd   r13!, {r0-r12,r14}

        // If workspace was allocated, free it:
        ldr     r12, [r12]
        cmp     r12, #0
        beq     1f

        mov     r0, #7          // Free
        mov     r2, r12
        swi     SWI_OS_MODULE | SWI_X
        bvs     fail_err_return

1:      ldmfd   r13!, {r0-r12, pc}^


        //////////////////////////////////////////////////////////////////////
        // Initialisation

        /* Entry:
         *      r0 = environment/init parameters
         *      r11 = IO base/instantiation number
         *      r12 = private word
         *      r13 = SVC stack
         *      r14 = return
         *
         * Return:
         *      V clear on success
         *      V set, r0 = error block
         */
init:
        stmfd   r13!, {r0-r12,r14}

        // Is private word non-zero?
        ldr     r0, [r12]
        cmp     r0, #0
        bne     1f
        // If so, workspace already claimed & we're re-initing

        // Allocate workspace
        mov     r0, #6          // Claim space
        mov     r3, #WORKSPACE_SZ
        swi     SWI_OS_MODULE | SWI_X
        bvs     fail_err_return
        str     r2, [r12]
1:
        ldr     r12, [r12]
        // Init workspace
        str     r11, [r12, #WS_HW]
        mov     r0, #0
        str     r0, [r12, #WS_TX_HEAD]
        str     r0, [r12, #WS_RX_TAIL]

        adr     r0, str_found
        swi     SWI_OS_WRITE0 | SWI_X
        bvs     fail_err_return

        // Finished!
        ldmfd   r13!,{r0-r12,pc}^

fail_err_return:
        add     r13, r13, #4
        ldmfd   r13!, {r1-r12,lr}
        orr     pc, lr, #0x10000000


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

        .long   0       // End

cmd_pipetest:
        // r0 = command tail (preserve)
        // r1 = number of OSCLI parameters
        // r12 = pointer to module private word

        stmfd   r13!, {r0-r12, lr}

        // Print some info:
        adr     r0, str_hw_at
        swi     SWI_OS_WRITE0 | SWI_X
        bvs     fail_err_return

        ldr     r12, [r12]      // Workspace pointer

        ldr     r0, [r12, #WS_HW]
        bl      print_hex32

        swi     SWI_OS_NEWLINE | SWI_X
        bvs     fail_err_return

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
        mov     r0, #'T'
        swi     SWI_OS_WRITEC | SWI_X
        swi     SWI_OS_NEWLINE | SWI_X
#endif

        add     r0, r12, #WS_SCRATCH
        bl      pipe_packet_rx
        // FIXME: Assert returned CID matches 1 too?  Expecting GetInfoResp
#if DEBUG > 2
        mov     r0, #'R'
        swi     SWI_OS_WRITEC | SWI_X
        swi     SWI_OS_NEWLINE | SWI_X
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


        //////////////////////////////////////////////////////////////////////
        // Pipe packet routines

        // r0 = packet data, r1 = len, r2 = CID, r12=workspace
pipe_packet_tx:
        stmfd   r13!, {r0-r12, lr}      // FIXME: Reduce regs

        // Copy packet to data buffer:
        ldr     r10, [r12, #WS_HW]
        add     r10, r10, #PR_BASE      // r10 = registers
        // Regs are lower 8b of each 32b word in this space.

        // Fill the TX buffer at index 0:
        add     r9, r10, #PR_TX_BUFFERS << 2
        // FIXME: Smart TX buffer allocation
        mov     r3, r1
1:      ldrb    r4, [r0], #1
        strb    r4, [r9], #4
        subs    r3, r3, #1
        bne     1b
        // FIXME: That's the worst memcpy evar.

        sub     r1, r1, #1                              // SIZE specified as (length-1)
        // Construct a TX descriptor:
        mov     r9, #0 << PR_DESCR_ADDR_SHIFT           // TX data at buffer idx 0
        mov     r1, r1, lsl#23                          // clear bits 9 up
        orr     r9, r9, r1, lsr#(23-PR_DESCR_SIZE_SHIFT)// TX len
        mov     r2, r2, lsl#25
        orr     r9, r9, r2, lsr#(25-PR_DESCR_CID_SHIFT) // CID
        orr     r9, r9, #PR_DESCR_READY                 // Go!

        // Write descriptor LSB-first to TX_HEAD descriptor.

        // Find head index:
        ldr     r1, [r12, #WS_TX_HEAD]
#if DEBUG > 1
        mov     r0, r1
        bl      print_hex8
        swi     SWI_OS_NEWLINE
#endif
        // Find appropriate TX descriptor:
        add     r2, r10, #PR_TX0_0 << 2                 // r2 = Addr of TX descr 0
        add     r3, r2, r1, lsl#2+2                     // r3 = Descr N

        // FIXME: When >1 outstanding is supported, check it's free before use.

        // OK, we got a descriptor to write to, so do so:
        strb    r9, [r3, #0]
        mov     r9, r9, lsr#8
        strb    r9, [r3, #4]
        mov     r9, r9, lsr#8
        strb    r9, [r3, #8]
        mov     r9, r9, lsr#8
        strb    r9, [r3, #12]

        // Descriptor written.  The card will now send the packet!

        /* FIXME: Dumb poll for completion.
         * Better would be to do this before sending the next (to ensure the TX
         * buffer is free for use).
         * Better still would be to choose different TX buffer addresses, with
         * multiple outstanding -- that's the point of the queues.
         */
        mov     r5, #0x100000                           // Long timeout
1:      ldrb    r4, [r3, #12]                           // Check top bit
        tst     r4, #0x80
        beq     2f
        subs    r5, r5, #1
        beq     tx_timeout
        b       1b
2:

        // Move on head pointer:
        add     r1, r1, #1
        and     r1, r1, #PR_DESCRS_MASK
        str     r1, [r12, #WS_TX_HEAD]

        ldmfd   r13!, {r0-r12, pc}^

tx_timeout:
        adr     r0, err_tx_timeout
        b       fail_err_return


        //////////////////////////////////////////////////////////////////////

        // Wait for a packet.
        // r0 = packet data buffer (of max size), r12 = workspace
        // Returns r1 = len, r2 = CID
pipe_packet_rx:
        stmfd   r13!, {r3-r12, lr}      // FIXME: Reduce regs

        ldr     r10, [r12, #WS_HW]
        add     r10, r10, #PR_BASE                      // r10 = registers
        // Regs are lower 8b of each 32b word in this space.

        mov     r11, r0
        // Find tail index:
        ldr     r8, [r12, #WS_RX_TAIL]
#if DEBUG > 1
        mov     r9, r0
        mov     r0, r8
        bl      print_hex8
        swi     SWI_OS_NEWLINE
        mov     r0, r9
#endif
        // Find appropriate RX descriptor:
        add     r2, r10, #PR_RX0_0 << 2                 // r2 = Addr of RX descr 0
        add     r3, r2, r8, lsl#2+2                     // r3 = Descr N

#if DEBUG > 2
        mov     r0, r3
        bl      print_hex32
        swi     SWI_OS_NEWLINE
#endif

        // Poll for ready:
        mov     r5, #0x100000                           // Long timeout
1:      ldrb    r6, [r3, #12]                           // Check top bit
        tst     r6, #0x80
        bne     2f
        subs    r5, r5, #1
        beq     rx_timeout
        b       1b
2:
        // Get full descriptor:
        ldrb    r4, [r3, #0]
        ldrb    r5, [r3, #4]
        orr     r4, r4, r5, lsl#8
        ldrb    r5, [r3, #8]
        orr     r4, r4, r5, lsl#16
        orr     r4, r4, r6, lsl#24
#if DEBUG > 2
        mov     r0, r4
        bl      print_hex32
        swi     SWI_OS_NEWLINE
#endif

        // Glean info from it:
        mov     r1, r4, lsl#32-PR_DESCR_SIZE_SHIFT-9
        mov     r1, r1, lsr#32-9
        add     r1, r1, #1                              // r1 = len
        mov     r2, r4, lsl#32-PR_DESCR_CID_SHIFT-7
        mov     r2, r2, lsr#32-7                        // r2 = CID
        mov     r4, r4, lsl#32-PR_DESCR_ADDR_SHIFT-9
        mov     r4, r4, lsr#32-7                        // r4 = addr

        mov     r0, r11
        // Copy packet from RX_BUFFER to data buffer @r0:
        add     r9, r10, #PR_RX_BUFFERS << 2
        add     r9, r9, r4                              // r9 = RX_buf + addr
        mov     r4, r1                                  // len
1:      ldrb    r5, [r9], #4
        strb    r5, [r0], #1
        subs    r4, r4, #1
        bgt     1b
        // FIXME: That's the worst memcpy evar.

        // Finally, consume the packet by clearing descr top bit
        strb    r4, [r3, #12]                           // r4 = 0

        // Move on tail pointer:
        add     r8, r8, #1
        and     r8, r8, #PR_DESCRS_MASK
        str     r8, [r12, #WS_RX_TAIL]
done:
        // Done!
        // r1 = len, r2 = CID, buffer filled
        ldmfd   r13!, {r3-r12, pc}^

rx_timeout:
        adr     r0, err_rx_timeout
        ldmfd   r13!, {r3-r12,lr}
        orr     pc, lr, #0x10000000


str_hw_at:      .asciz  "Hardware address: "
        .align

err_tx_timeout:
        .long   0xdeadf00d
        .asciz  "Timed out waiting for TX descriptor"
        .align

err_rx_timeout:
        .long   0xdeadf00e
        .asciz  "Timed out waiting for RX descriptor"
        .align

        //////////////////////////////////////////////////////////////////////
        // Utils

print_hex8:     // r0 = byte to print
        stmfd   r13!, {r1, lr}
        mov     r1, r0
        and     r0, r0, #0xf0
        mov     r0, r0, lsr#4
        cmp     r0, #9
        addgt   r0, r0, #'a'-10
        addle   r0, r0, #'0'
        swi     SWI_OS_WRITEC | SWI_X
        and     r0, r1, #0x0f
        cmp     r0, #9
        addgt   r0, r0, #'a'-10
        addle   r0, r0, #'0'
        swi     SWI_OS_WRITEC | SWI_X
        ldmfd   r13!, {r1, pc}^

        // Suppressing leading zero form:
print_hex8slz:     // r0 = byte to print
        stmfd   r13!, {r1, lr}
        mov     r1, r0
        and     r0, r0, #0xf0
        movs    r0, r0, lsr#4
        beq     1f
        cmp     r0, #9
        addgt   r0, r0, #'a'-10
        addle   r0, r0, #'0'
        swi     SWI_OS_WRITEC | SWI_X
1:      and     r0, r1, #0x0f
        cmp     r0, #9
        addgt   r0, r0, #'a'-10
        addle   r0, r0, #'0'
        swi     SWI_OS_WRITEC | SWI_X
        ldmfd   r13!, {r1, pc}^

print_hex32:    // r0 = word to print
        stmfd   r13!, {r10, lr}
        mov     r10, r0
        mov     r0, r10, lsr#24
        bl      print_hex8
        mov     r0, r10, lsr#16
        bl      print_hex8
        mov     r0, r10, lsr#8
        bl      print_hex8
        mov     r0, r10
        bl      print_hex8
        ldmfd   r13!, {r10, pc}^

        //////////////////////////////////////////////////////////////////////
        // Strings

str_found:
        .ascii "ArcPipePodule initialising, built "
        .ascii BUILD_DATE
        .asciz "\r\n"
        .align

str_title:      .asciz "ArcPipePodule"
str_help:       .asciz "ArcPipePodule	0.01 (" BUILD_DATE ")"
        .align

        ////////////////////////////////////////////////////////////////

.end

