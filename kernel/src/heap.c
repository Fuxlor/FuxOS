#include "heap.h"
#include "pmm.h"

#define FRAME_SIZE 4096
#define HEAP_START 0x200000

struct block_header {
    uint64_t size;
    int      free;
    struct block_header *next;
};

static struct block_header *heap_head = 0;
static uint64_t heap_end = HEAP_START;

static int heap_extend(uint64_t needed)
{
    uint64_t frames_needed = (needed + FRAME_SIZE - 1) / FRAME_SIZE;

    for (uint64_t i = 0; i < frames_needed; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0xFFFFFFFF)
            return 0;
        (void)frame;
        heap_end += FRAME_SIZE;
    }
    return 1;
}

void *kmalloc(uint64_t size)
{
    if (size == 0) return 0;
    size = (size + 7) & ~7ULL;

    struct block_header *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            b->free = 0;
            return (void *)(b + 1);
        }
        b = b->next;
    }

    uint64_t needed = sizeof(struct block_header) + size;

    if (heap_head == 0) {
        if (heap_end < HEAP_START + needed) {
            if (!heap_extend(needed)) return 0;
        }
        struct block_header *nb = (struct block_header *)HEAP_START;
        nb->size = size;
        nb->free = 0;
        nb->next = 0;
        heap_head = nb;
        return (void *)(nb + 1);
    }

    b = heap_head;
    while (b->next) b = b->next;

    uint64_t new_block_addr = (uint64_t)(b + 1) + b->size;

    if (new_block_addr + needed > heap_end) {
        if (!heap_extend(new_block_addr + needed - heap_end)) return 0;
    }

    struct block_header *nb = (struct block_header *)new_block_addr;
    nb->size = size;
    nb->free = 0;
    nb->next = 0;
    b->next = nb;

    return (void *)(nb + 1);
}

void kfree(void *ptr)
{
    if (!ptr) return;
    struct block_header *b = (struct block_header *)ptr - 1;
    b->free = 1;
}