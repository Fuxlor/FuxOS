#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "types.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

static void idt_set_entry(int n, void *handler)
{
    uint64_t addr = (uint64_t)handler;

    idt[n].offset_low  = addr & 0xFFFF;
    idt[n].selector    = 0x18;
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;
    idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

void idt_init(void)
{
    for (int i = 0; i < 32; i++)
        idt_set_entry(i, isr_get_handler(i));

    for (int i = 0; i < 16; i++)
        idt_set_entry(32 + i, irq_get_handler(i));

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}