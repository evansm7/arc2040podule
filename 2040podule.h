/* Board description/config for ArcPipePodule
 * (V1 of PCB)
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

#ifndef _2040PODULE_H
#define _2040PODULE_H

#define MATT_2040PODULE

#define PICO_DEFAULT_UART                       0
#define PICO_DEFAULT_UART_TX_PIN                28
#define PICO_DEFAULT_UART_BAUD_RATE             230400
/* #define PICO_DEFAULT_UART_RX_PIN 1 */

#if BOARD_HW == 1
#define PICO_DEFAULT_LED_PIN                    29

// This part/driver works w/ V1, but it's 2-bit:
#define PICO_BOOT_STAGE2_CHOOSE_W25X10CL        1

// Hmmm, V1 board doesn't like /2, or unfortunately even /4 :(
#define PICO_FLASH_SPI_CLKDIV                   8

#elif BOARD_HW == 2
#define PICO_DEFAULT_LED_PIN                    25

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080         1
#define PICO_FLASH_SPI_CLKDIV                   4       // Conservative

#endif

// NB: Some boards might have flash larger than 2MB:
#define PICO_FLASH_SIZE_BYTES                   (2 * 1024 * 1024)

#define PICO_FLOAT_SUPPORT_ROM_V1               0
#define PICO_DOUBLE_SUPPORT_ROM_V1              0

#endif
