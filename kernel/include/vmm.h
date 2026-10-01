#ifndef VMM_H
#define VMM_H

#include "types.h"

#define PAGE_PRESENT  0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER     0x4

void vmm_init(void);
int  vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(uint64_t virt);
uint64_t vmm_get_physical(uint64_t virt);

#endif