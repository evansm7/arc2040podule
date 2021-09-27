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
