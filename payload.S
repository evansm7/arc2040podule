
        .section .rodata

        .global podule_header, podule_header_end
podule_header:
        /* This is the header + chunk directory + loader in the first page: */
        .incbin "payload_podule_header.bin"
podule_header_end:

        .end
