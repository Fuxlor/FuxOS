#ifndef PIT_H
#define PIT_H

#include "types.h"

void pit_init(uint32_t freq_hz);
void pit_tick(void);
uint64_t pit_get_ticks(void);

#endif