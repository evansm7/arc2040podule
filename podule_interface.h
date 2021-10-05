/*
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

#ifndef PODULE_INTERFACE_H
#define PODULE_INTERFACE_H

#include "hw.h"


#define PODULE_MEM_LOADER       0
#define PODULE_MEM_ROM_WINDOW   1024
#define PODULE_REGS             2048

void	podule_if_init(void);
void	podule_if_debug(void);
void    podule_if_reset_host(void);

extern volatile uint8_t podule_space[];

static volatile uint8_t *podule_if_get_memspace(void)
{
        return &podule_space[0];
}

static volatile uint8_t *podule_if_get_loader(void)
{
        return &podule_space[PODULE_MEM_LOADER];
}

static volatile uint8_t *podule_if_get_rom_window(void)
{
        return &podule_space[PODULE_MEM_ROM_WINDOW];
}

static volatile uint8_t *podule_if_get_regs(void)
{
        return &podule_space[PODULE_REGS];
}

#endif
