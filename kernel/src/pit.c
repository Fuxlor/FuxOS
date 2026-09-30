#include "pit.h"
#include "io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193182

static volatile uint64_t timer_ticks = 0;

void pit_init(uint32_t freq_hz)
{
    uint16_t divisor = PIT_FREQ / freq_hz;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

void pit_tick(void)
{
    timer_ticks++;
}

uint64_t pit_get_ticks(void)
{
    return timer_ticks;
}