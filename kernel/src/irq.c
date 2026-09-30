#include "irq.h"
#include "isr.h"      /* pour struct interrupt_frame */
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "types.h"

static void irq_dispatch(int vector)
{
    if (vector == 32)
        pit_tick();
    else if (vector == 33)
        keyboard_handle();

    pic_send_eoi(vector);
}

#define IRQ_HANDLER(n)                                                \
    __attribute__((interrupt))                                        \
    static void irq##n(struct interrupt_frame *frame)                 \
    {                                                                  \
        (void)frame;                                                   \
        irq_dispatch(32 + n);                                          \
    }

IRQ_HANDLER(0)  IRQ_HANDLER(1)  IRQ_HANDLER(2)  IRQ_HANDLER(3)
IRQ_HANDLER(4)  IRQ_HANDLER(5)  IRQ_HANDLER(6)  IRQ_HANDLER(7)
IRQ_HANDLER(8)  IRQ_HANDLER(9)  IRQ_HANDLER(10) IRQ_HANDLER(11)
IRQ_HANDLER(12) IRQ_HANDLER(13) IRQ_HANDLER(14) IRQ_HANDLER(15)

static void *irq_table[16] = {
    irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
    irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15
};

void *irq_get_handler(int n)
{
    return irq_table[n];
}