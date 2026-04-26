#ifndef VMM_H
#define VMM_H
#include "types.h"

#define VMM_PRESENT    (1ULL << 0)
#define VMM_WRITABLE   (1ULL << 1)
#define VMM_USER       (1ULL << 2)
#define VMM_PWT        (1ULL << 3)
#define VMM_PCD        (1ULL << 4)
#define VMM_ACCESSED   (1ULL << 5)
#define VMM_DIRTY      (1ULL << 6)
#define VMM_HUGE       (1ULL << 7)
#define VMM_NX         (1ULL << 63)

#define VMM_KERNEL_RW  (VMM_PRESENT | VMM_WRITABLE)
#define VMM_KERNEL_RO  (VMM_PRESENT)
#define VMM_USER_RW    (VMM_PRESENT | VMM_WRITABLE | VMM_USER)

#define PAGE_MASK      0xFFFFFFFFFFFFF000ULL

#define PML4_IDX(v)   (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v)   (((v) >> 30) & 0x1FF)
#define PD_IDX(v)     (((v) >> 21) & 0x1FF)
#define PT_IDX(v)     (((v) >> 12) & 0x1FF)

#define PTE_ADDR(e)   ((e) & 0x000FFFFFFFFFF000ULL)

void     vmm_init(void);
void     vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void     vmm_unmap(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);
void     vmm_switch(uint64_t pml4_phys);
void     vmm_flush(uint64_t virt);
uint64_t vmm_current_pml4(void);
void     vmm_print_range(uint64_t virt_start, uint64_t virt_end);

#endif