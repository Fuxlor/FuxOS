#include "vmm.h"
#include "pmm.h"

#define PML4_ADDR 0x1000

#define ENTRIES_PER_TABLE 512
#define PAGE_SIZE_4K       4096

typedef uint64_t pte_t;

static inline uint64_t pml4_index(uint64_t virt) { return (virt >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t virt) { return (virt >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t virt)   { return (virt >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t virt)   { return (virt >> 12) & 0x1FF; }

static pte_t *pml4 = (pte_t *)PML4_ADDR;

void vmm_init(void)
{
    /* Les tables PML4/PDPT/PD existent déjà, construites par stage2.asm. */
}

static pte_t *get_or_create_table(pte_t *table, uint64_t index, uint64_t flags)
{
    if (!(table[index] & PAGE_PRESENT)) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0xFFFFFFFF)
            return 0;

        uint64_t new_table_addr = (uint64_t)frame * PAGE_SIZE_4K;

        pte_t *new_table = (pte_t *)new_table_addr;
        for (int i = 0; i < ENTRIES_PER_TABLE; i++)
            new_table[i] = 0;

        table[index] = new_table_addr | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    }

    return (pte_t *)(table[index] & ~0xFFFULL);
}

int vmm_map(uint64_t virt, uint64_t phys, uint64_t flags)
{
    pte_t *pdpt = get_or_create_table(pml4, pml4_index(virt), flags);
    if (!pdpt) return 0;

    pte_t *pd = get_or_create_table(pdpt, pdpt_index(virt), flags);
    if (!pd) return 0;

    if ((pd[pd_index(virt)] & PAGE_PRESENT) && (pd[pd_index(virt)] & 0x80))
        return 0;

    pte_t *pt = get_or_create_table(pd, pd_index(virt), flags);
    if (!pt) return 0;

    pt[pt_index(virt)] = (phys & ~0xFFFULL) | PAGE_PRESENT | (flags & (PAGE_WRITABLE | PAGE_USER));

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");

    return 1;
}

void vmm_unmap(uint64_t virt)
{
    if (!(pml4[pml4_index(virt)] & PAGE_PRESENT)) return;
    pte_t *pdpt = (pte_t *)(pml4[pml4_index(virt)] & ~0xFFFULL);

    if (!(pdpt[pdpt_index(virt)] & PAGE_PRESENT)) return;
    pte_t *pd = (pte_t *)(pdpt[pdpt_index(virt)] & ~0xFFFULL);

    if (!(pd[pd_index(virt)] & PAGE_PRESENT)) return;
    if (pd[pd_index(virt)] & 0x80) return;

    pte_t *pt = (pte_t *)(pd[pd_index(virt)] & ~0xFFFULL);
    pt[pt_index(virt)] = 0;

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t vmm_get_physical(uint64_t virt)
{
    if (!(pml4[pml4_index(virt)] & PAGE_PRESENT)) return 0;
    pte_t *pdpt = (pte_t *)(pml4[pml4_index(virt)] & ~0xFFFULL);

    if (!(pdpt[pdpt_index(virt)] & PAGE_PRESENT)) return 0;
    pte_t *pd = (pte_t *)(pdpt[pdpt_index(virt)] & ~0xFFFULL);

    if (!(pd[pd_index(virt)] & PAGE_PRESENT)) return 0;

    if (pd[pd_index(virt)] & 0x80) {
        uint64_t base = pd[pd_index(virt)] & ~0x1FFFFFULL;
        return base | (virt & 0x1FFFFF);
    }

    pte_t *pt = (pte_t *)(pd[pd_index(virt)] & ~0xFFFULL);
    if (!(pt[pt_index(virt)] & PAGE_PRESENT)) return 0;

    uint64_t base = pt[pt_index(virt)] & ~0xFFFULL;
    return base | (virt & 0xFFF);
}