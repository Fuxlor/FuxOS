#include "isr.h"
#include "vga.h"
#include "types.h"

static void exception_handler(int vector, uint64_t error_code)
{
    kprintf("\n=== EXCEPTION %d ===\n", (int64_t)vector);
    if (error_code)
        kprintf("Code erreur : %x\n", error_code);
    kprintf("Systeme arrete.\n");

    for (;;)
        __asm__ volatile ("cli; hlt");
}

#define ISR_NOERR(n)                                                  \
    __attribute__((interrupt))                                        \
    static void isr##n(struct interrupt_frame *frame)                 \
    {                                                                  \
        (void)frame;                                                   \
        exception_handler(n, 0);                                       \
    }

#define ISR_ERR(n)                                                     \
    __attribute__((interrupt))                                         \
    static void isr##n(struct interrupt_frame *frame, uint64_t err)    \
    {                                                                   \
        (void)frame;                                                    \
        exception_handler(n, err);                                      \
    }

ISR_NOERR(0)   ISR_NOERR(1)   ISR_NOERR(2)   ISR_NOERR(3)
ISR_NOERR(4)   ISR_NOERR(5)   ISR_NOERR(6)   ISR_NOERR(7)
ISR_ERR(8)     ISR_NOERR(9)   ISR_ERR(10)    ISR_ERR(11)
ISR_ERR(12)    ISR_ERR(13)    ISR_ERR(14)    ISR_NOERR(15)
ISR_NOERR(16)  ISR_ERR(17)    ISR_NOERR(18)  ISR_NOERR(19)
ISR_NOERR(20)  ISR_NOERR(21)  ISR_NOERR(22)  ISR_NOERR(23)
ISR_NOERR(24)  ISR_NOERR(25)  ISR_NOERR(26)  ISR_NOERR(27)
ISR_NOERR(28)  ISR_NOERR(29)  ISR_ERR(30)    ISR_NOERR(31)

static void *isr_table[32] = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

void *isr_get_handler(int n)
{
    return isr_table[n];
}