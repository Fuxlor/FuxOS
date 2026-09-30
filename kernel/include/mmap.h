#ifndef MMAP_H
#define MMAP_H

#include "types.h"

#define MMAP_ADDR  0x9000
#define MMAP_COUNT_ADDR 0x8FFC

struct mmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attr;
} __attribute__((packed));

uint32_t mmap_get_count(void);
struct mmap_entry *mmap_get_entries(void);
void mmap_print(void);

#endif