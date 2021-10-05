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
