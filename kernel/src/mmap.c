#include "mmap.h"
#include "vga.h"

uint32_t mmap_get_count(void)
{
    return *(uint32_t *)MMAP_COUNT_ADDR;
}

struct mmap_entry *mmap_get_entries(void)
{
    return (struct mmap_entry *)MMAP_ADDR;
}

static const char *mmap_type_str(uint32_t type)
{
    switch (type) {
        case 1: return "Usable";
        case 2: return "Reserved";
        case 3: return "ACPI reclaimable";
        case 4: return "ACPI NVS";
        case 5: return "Bad memory";
        default: return "Unknown";
    }
}

void mmap_print(void)
{
    uint32_t count = mmap_get_count();
    struct mmap_entry *entries = mmap_get_entries();

    kprintf("Carte memoire : %d entrees\n", (int64_t)count);

    uint64_t total_usable = 0;
    for (uint32_t i = 0; i < count; i++) {
        struct mmap_entry *e = &entries[i];
        kprintf("  base=%x taille=%x type=%s\n", e->base, e->length, mmap_type_str(e->type));
        if (e->type == 1)
            total_usable += e->length;
    }

    kprintf("RAM utilisable : %d Ko\n", (int64_t)(total_usable / 1024));
}