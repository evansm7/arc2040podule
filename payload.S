
        .section .rodata

        .global podule_header, podule_header_end
podule_header:
        /* This is the header + chunk directory + loader in the first page: */
        .incbin "payload_podule_header.bin"
podule_header_end:

        .align 4
        .global podule_rom, podule_rom_end
podule_rom:
        .incbin "payload_podule_rom.bin"
podule_rom_end:

        .end
