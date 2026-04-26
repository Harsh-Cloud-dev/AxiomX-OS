#include "pmm.h"
#include "e820.h"
#include "serial.h"

static uint8_t  *bitmap      = (uint8_t *)PMM_BITMAP_ADDR;
static uint64_t  total_pages = 0;
static uint64_t  free_pages  = 0;

static inline void bitmap_set(uint64_t page) {
    bitmap[page / 8] |= (uint8_t)(1 << (page % 8));
}

static inline void bitmap_clear(uint64_t page) {
    bitmap[page / 8] &= (uint8_t)~(1 << (page % 8));
}

static inline int bitmap_test(uint64_t page) {
    return (bitmap[page / 8] >> (page % 8)) & 1;
}

static void pmm_mark_range(uint64_t base, uint64_t length, int used) {
    uint64_t start_page = base / PAGE_SIZE;
    uint64_t end_page   = (base + length + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t p = start_page; p < end_page && p < total_pages; p++) {
        if (used) {
            if (!bitmap_test(p)) {
                bitmap_set(p);
                if (free_pages > 0) free_pages--;
            }
        } else {
            if (bitmap_test(p)) {
                bitmap_clear(p);
                free_pages++;
            }
        }
    }
}

void pmm_init(void) {
    uint16_t count = e820_count();
    if (count > 64) count = 64;

    volatile e820_entry_t *entries = (volatile e820_entry_t *)0x504;

    uint64_t max_addr = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (entries[i].type == E820_USABLE) {
            uint64_t end = entries[i].base + entries[i].length;
            if (end > max_addr) max_addr = end;
        }
    }

    if (max_addr > 0x100000000ULL) max_addr = 0x100000000ULL;

    total_pages = max_addr / PAGE_SIZE;
    free_pages  = 0;

    uint64_t bitmap_bytes = (total_pages + 7) / 8;
    for (uint64_t i = 0; i < bitmap_bytes; i++)
        bitmap[i] = 0xFF;

    for (uint16_t i = 0; i < count; i++) {
        if (entries[i].type == E820_USABLE)
            pmm_mark_range(entries[i].base, entries[i].length, 0);
    }

    pmm_mark_range(0x000000, 0x100000, 1);
    pmm_mark_range(0x100000, 0x200000, 1);
    pmm_mark_range(PMM_BITMAP_ADDR, bitmap_bytes, 1);

    serial_printf("[pmm] Initialised: %u total pages, %u free pages\n",
                  (unsigned int)total_pages, (unsigned int)free_pages);
    serial_printf("[pmm] Bitmap at 0x%x, size %u bytes\n",
                  (unsigned int)PMM_BITMAP_ADDR, (unsigned int)bitmap_bytes);
}

uint64_t pmm_alloc(void) {
    if (free_pages == 0) {
        serial_puts("[pmm] OUT OF MEMORY\n");
        return PMM_ALLOC_FAIL;
    }

    uint64_t bitmap_bytes = (total_pages + 7) / 8;
    for (uint64_t byte = 0; byte < bitmap_bytes; byte++) {
        if (bitmap[byte] == 0xFF) continue;

        for (int bit = 0; bit < 8; bit++) {
            uint64_t page = byte * 8 + (uint64_t)bit;
            if (page >= total_pages) break;

            if (!bitmap_test(page)) {
                bitmap_set(page);
                free_pages--;
                return page * PAGE_SIZE;
            }
        }
    }

    serial_puts("[pmm] pmm_alloc: no free page found (bitmap corrupt?)\n");
    return PMM_ALLOC_FAIL;
}

void pmm_free(uint64_t addr) {
    if (addr == 0 || addr == PMM_ALLOC_FAIL) return;
    if (addr % PAGE_SIZE != 0) {
        serial_printf("[pmm] pmm_free: unaligned address 0x%x\n",
                      (unsigned int)addr);
        return;
    }

    uint64_t page = addr / PAGE_SIZE;
    if (page >= total_pages) {
        serial_printf("[pmm] pmm_free: address 0x%x out of range\n",
                      (unsigned int)addr);
        return;
    }

    if (!bitmap_test(page)) {
        serial_printf("[pmm] pmm_free: double free at 0x%x\n",
                      (unsigned int)addr);
        return;
    }

    bitmap_clear(page);
    free_pages++;
}

uint64_t pmm_free_count(void)  { return free_pages;  }
uint64_t pmm_total_count(void) { return total_pages; }

void pmm_print_stats(void) {
    uint64_t used     = total_pages - free_pages;
    uint64_t free_mb  = (free_pages  * PAGE_SIZE) / (1024 * 1024);
    uint64_t used_mb  = (used        * PAGE_SIZE) / (1024 * 1024);
    uint64_t total_mb = (total_pages * PAGE_SIZE) / (1024 * 1024);

    serial_puts("\n[pmm] Memory Stats:\n");
    serial_printf("  Total : %u pages (%u MB)\n",
                  (unsigned int)total_pages, (unsigned int)total_mb);
    serial_printf("  Used  : %u pages (%u MB)\n",
                  (unsigned int)used,        (unsigned int)used_mb);
    serial_printf("  Free  : %u pages (%u MB)\n",
                  (unsigned int)free_pages,  (unsigned int)free_mb);
}