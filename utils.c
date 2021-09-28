#include <stdio.h>

void    pretty_hexdump(uint8_t *r, unsigned int len, int zeroidx)
{
        unsigned int left = len;
        uintptr_t idx = zeroidx ? 0 : (uintptr_t)r;

        for (int i = 0; i < len; i += 16) {
                unsigned int rowlen = left > 16 ? 16 : left;
                printf("%08x: ", idx + i);
                for (int j = 0; j < rowlen; j++) {
                        printf("%02x ", r[i+j]);
                }
                printf("  ");
                for (int j = 0; j < rowlen; j++) {
                        uint8_t c = r[i+j];
                        printf("%c", c >= 32 ? c : '.');
                }
                printf("\n");
                left -= 16;
        }
}
