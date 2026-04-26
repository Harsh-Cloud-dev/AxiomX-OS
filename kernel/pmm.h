#ifndef PMM_H
#define PMM_H
#include "types.h"

#define PAGE_SIZE       4096
#define PAGE_SHIFT      12
#define PMM_BITMAP_ADDR 0x300000
#define PMM_MAX_PAGES   (1024 * 1024)
#define PMM_ALLOC_FAIL  0xFFFFFFFFFFFFFFFFULL

void     pmm_init(void);
uint64_t pmm_alloc(void);
void     pmm_free(uint64_t addr);
void     pmm_print_stats(void);
uint64_t pmm_free_count(void);
uint64_t pmm_total_count(void);
#endif