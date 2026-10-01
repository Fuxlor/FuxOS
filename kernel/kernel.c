#include "types.h"
#include "vga.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "mmap.h"
#include "pmm.h"
#include "heap.h"
#include "serial.h"
#include "vmm.h"

extern char __bss_start[];
extern char __bss_end[];

__attribute__((section(".text.kmain")))
void kmain(void)
{
    for (volatile char *p = __bss_start; p < __bss_end; p++)
        *p = 0;

    serial_init();
    console_clear();
    puts("Kernel C OK!\n");

    mmap_print();

    pmm_init();
    kprintf("PMM : %d frames total\n", (int64_t)pmm_get_total_frames());

    vmm_init();

    uint32_t frame = pmm_alloc_frame();
    uint64_t phys = (uint64_t)frame * 4096;
    uint64_t virt = 0x40000000;      /* 1 Go : juste après le mapping 1:1 existant */

    if (vmm_map(virt, phys, PAGE_WRITABLE)) {
        kprintf("Mapping OK : virt %x -> phys %x\n", virt, phys);

        uint32_t *test = (uint32_t *)virt;
        *test = 0xDEADBEEF;
        kprintf("Lu : %x\n", (uint64_t)*test);
        kprintf("Verif physique : %x\n", vmm_get_physical(virt));
    } else {
        kprintf("Mapping echoue\n");
    }

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