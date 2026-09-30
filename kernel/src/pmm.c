#include "pmm.h"
#include "mmap.h"

#define FRAME_SIZE   4096
#define BITMAP_ADDR  0x100000

static uint8_t *pmm_bitmap;
static uint32_t pmm_total_frames;

static inline uint32_t addr_to_frame(uint64_t addr)
{
    return addr / FRAME_SIZE;
}

static void pmm_set(uint32_t frame)
{
    pmm_bitmap[frame / 8] |= (1 << (frame % 8));
}

static void pmm_clear(uint32_t frame)
{
    pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

int pmm_test(uint32_t frame)
{
    return (pmm_bitmap[frame / 8] >> (frame % 8)) & 1;
}

static void pmm_mark_region(uint64_t base, uint64_t length, int used)
{
    uint32_t start = addr_to_frame(base);
    uint32_t end   = addr_to_frame(base + length - 1);

    for (uint32_t f = start; f <= end && f < pmm_total_frames; f++) {
        if (used) pmm_set(f); else pmm_clear(f);
    }
}

uint32_t pmm_get_total_frames(void)
{
    return pmm_total_frames;
}

void pmm_init(void)
{
    uint32_t count = mmap_get_count();
    struct mmap_entry *entries = mmap_get_entries();

    uint64_t highest = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].type != 1) continue;
        uint64_t end = entries[i].base + entries[i].length;
        if (end > highest) highest = end;
    }
    pmm_total_frames = addr_to_frame(highest);

    pmm_bitmap = (uint8_t *)BITMAP_ADDR;
    uint32_t bitmap_size = (pmm_total_frames + 7) / 8;

    for (uint32_t i = 0; i < bitmap_size; i++)
        pmm_bitmap[i] = 0xFF;

    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].type == 1)
            pmm_mark_region(entries[i].base, entries[i].length, 0);
    }

    pmm_mark_region(0x0, 0x10000, 1);
    pmm_mark_region(0x10000, 0x90000 - 0x10000, 1);
    pmm_mark_region(0x90000 - 0x8000, 0x8000, 1);
    pmm_mark_region(BITMAP_ADDR, bitmap_size, 1);
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t f = 0; f < pmm_total_frames; f++) {
        if (!pmm_test(f)) {
            pmm_set(f);
            return f;
        }
    }
    return 0xFFFFFFFF;
}

void pmm_free_frame(uint32_t frame)
{
    pmm_clear(frame);
}