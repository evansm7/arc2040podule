/* Board description/config for ArcPipePodule
 * (V1 of PCB)
 */

#ifndef _2040PODULE_H
#define _2040PODULE_H

#define MATT_2040PODULE

#define PICO_DEFAULT_UART 0
#define PICO_DEFAULT_UART_TX_PIN 28
#define PICO_DEFAULT_UART_BAUD_RATE 230400
/* #define PICO_DEFAULT_UART_RX_PIN 1 */
#define PICO_DEFAULT_LED_PIN 29

//#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1     // Doesn't work w/ V1 board's 25Q16JVN
#define PICO_BOOT_STAGE2_CHOOSE_W25X10CL 1      // Does work w/ V1, but it's 2-bit

// Hmmm, V1 board doesn't like /2, or unfortunately even /4 :(
#define PICO_FLASH_SPI_CLKDIV 8

// NB: Some boards might have flash larger than 2MB:
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)

#define PICO_FLOAT_SUPPORT_ROM_V1 0
#define PICO_DOUBLE_SUPPORT_ROM_V1 0

#endif
