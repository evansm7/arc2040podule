#ifndef PODULE_INTERFACE_H
#define PODULE_INTERFACE_H

#include "hw.h"


#define PODULE_MEM_LOADER       0
#define PODULE_MEM_ROM_WINDOW   1024
#define PODULE_REGS             2048

void	podule_if_init(void);
void	podule_if_debug(void);

extern uint8_t podule_space[];

static uint8_t *podule_if_get_memspace(void)
{
        return &podule_space[0];
}

#endif
