#include "types.h"
#include "vga.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "mmap.h"
#include "pmm.h"
#include "heap.h"

extern char __bss_start[];
extern char __bss_end[];

__attribute__((section(".text.kmain")))
void kmain(void)
{
    for (volatile char *p = __bss_start; p < __bss_end; p++)
        *p = 0;

    console_clear();
    puts("Kernel C OK!\n");

    mmap_print();

    pmm_init();
    kprintf("PMM : %d frames total\n", (int64_t)pmm_get_total_frames());

    idt_init();
    kprintf("IDT chargee\n");

    keyboard_flush();
    pic_remap();
    pit_init(100);
    __asm__ volatile ("sti");
    kprintf("Interruptions activees\n");

    kprintf("Tape au clavier :\n");

    for (;;)
        __asm__ volatile ("hlt");
}