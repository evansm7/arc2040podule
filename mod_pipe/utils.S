#include "riscos_defs.h"

        .text
        .globl strcpy
        .globl print_hex8
        .globl print_hex8slz
        .globl print_hex32

        //////////////////////////////////////////////////////////////////////
        // Utils

        // r0 = dest
        // r1 = src
strcpy: stmfd   r13!, {r2}
1:      ldrb    r2, [r1], #1
        strb    r2, [r0], #1
        cmp     r2, #0
        bne     1b
        ldmfd   r13!, {r2}
        movs    pc, lr

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

        .end
