#include "../podule_regs.h"
#include "riscos_defs.h"
#include "module.h"

        .text
        .globl pipe_packet_tx
        .globl pipe_packet_rx

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
#if DEBUG > 3
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
        add     r13, r13, #4
        ldmfd   r13!, {r1-r12,lr}
        orrs    pc, lr, #V_BIT


        //////////////////////////////////////////////////////////////////////

        // Wait for a packet.
        // r0 = packet data buffer (of max size), r12 = workspace
        // Returns r0 = 0, r1 = len, r2 = CID
        // Or, V set for error and r0 = error.
pipe_packet_rx:
        stmfd   r13!, {r3-r12, lr}      // FIXME: Reduce regs

        ldr     r10, [r12, #WS_HW]
        add     r10, r10, #PR_BASE                      // r10 = registers
        // Regs are lower 8b of each 32b word in this space.

        mov     r11, r0
        // Find tail index:
        ldr     r8, [r12, #WS_RX_TAIL]
#if DEBUG > 3
        mov     r9, r0
        mov     r0, r8
        bl      print_hex8
        swi     SWI_OS_NEWLINE
        mov     r0, r9
#endif
        // Find appropriate RX descriptor:
        add     r2, r10, #PR_RX0_0 << 2                 // r2 = Addr of RX descr 0
        add     r3, r2, r8, lsl#2+2                     // r3 = Descr N

#if DEBUG > 3
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
#if DEBUG > 3
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
        // Done!
        // r1 = len, r2 = CID, buffer filled
        ldmfd   r13!, {r3-r12, pc}^

rx_timeout:
        adr     r0, err_rx_timeout
        ldmfd   r13!, {r3-r12,lr}
        orrs    pc, lr, #V_BIT


err_tx_timeout:
        .long   ERR_BASE + 0
        .asciz  "Timed out waiting for TX descriptor"
        .align

err_rx_timeout:
        .long   ERR_BASE + 1
        .asciz  "Timed out waiting for RX descriptor"
        .align

        .end
