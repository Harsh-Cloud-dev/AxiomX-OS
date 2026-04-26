#include "vmm.h"
#include "pmm.h"
#include "serial.h"

static void zero_page(uint64_t phys) {
    uint64_t *p = (uint64_t *)phys;
    for (int i = 0; i < 512; i++)
        p[i] = 0;
}

static uint64_t *get_or_create_table(uint64_t *entry, uint64_t flags) {
    if (*entry & VMM_PRESENT)
        return (uint64_t *)PTE_ADDR(*entry);

    uint64_t page = pmm_alloc();
    if (page == PMM_ALLOC_FAIL) {
        serial_puts("[vmm] FATAL: pmm_alloc failed\n");
        __asm__ volatile("cli; hlt");
        return (uint64_t *)0;
    }

    zero_page(page);
    *entry = page | flags | VMM_PRESENT | VMM_WRITABLE;
    return (uint64_t *)page;
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(cr3 & PAGE_MASK);

    uint64_t *pdpt = get_or_create_table(&pml4[PML4_IDX(virt)], VMM_PRESENT | VMM_WRITABLE);
    uint64_t *pd   = get_or_create_table(&pdpt[PDPT_IDX(virt)], VMM_PRESENT | VMM_WRITABLE);
    uint64_t *pt   = get_or_create_table(&pd[PD_IDX(virt)],     VMM_PRESENT | VMM_WRITABLE);

    pt[PT_IDX(virt)] = (phys & PAGE_MASK) | (flags | VMM_PRESENT);
    vmm_flush(virt);
}

void vmm_unmap(uint64_t virt) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(cr3 & PAGE_MASK);

    uint64_t pml4e = pml4[PML4_IDX(virt)];
    if (!(pml4e & VMM_PRESENT)) return;
    uint64_t *pdpt = (uint64_t *)PTE_ADDR(pml4e);

    uint64_t pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & VMM_PRESENT)) return;
    uint64_t *pd = (uint64_t *)PTE_ADDR(pdpte);

    uint64_t pde = pd[PD_IDX(virt)];
    if (!(pde & VMM_PRESENT)) return;
    uint64_t *pt = (uint64_t *)PTE_ADDR(pde);

    pt[PT_IDX(virt)] = 0;
    vmm_flush(virt);
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(cr3 & PAGE_MASK);

    uint64_t pml4e = pml4[PML4_IDX(virt)];
    if (!(pml4e & VMM_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t *)PTE_ADDR(pml4e);

    uint64_t pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & VMM_PRESENT)) return 0;
    uint64_t *pd = (uint64_t *)PTE_ADDR(pdpte);

    uint64_t pde = pd[PD_IDX(virt)];
    if (!(pde & VMM_PRESENT)) return 0;

    if (pde & VMM_HUGE)
        return PTE_ADDR(pde) + (virt & 0x1FFFFF);

    uint64_t *pt = (uint64_t *)PTE_ADDR(pde);
    uint64_t pte = pt[PT_IDX(virt)];
    if (!(pte & VMM_PRESENT)) return 0;

    return PTE_ADDR(pte) + (virt & 0xFFF);
}

void vmm_switch(uint64_t pml4_phys) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

void vmm_flush(uint64_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t vmm_current_pml4(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & PAGE_MASK;
}

void vmm_init(void) {
    zero_page(0x4000);
    zero_page(0x5000);
    zero_page(0x6000);

    uint64_t *pml4 = (uint64_t *)0x4000;
    uint64_t *pdpt = (uint64_t *)0x5000;
    uint64_t *pd   = (uint64_t *)0x6000;

    pml4[0] = 0x5000 | VMM_PRESENT | VMM_WRITABLE;
    pdpt[0] = 0x6000 | VMM_PRESENT | VMM_WRITABLE;

    for (int i = 0; i < 64; i++)
        pd[i] = ((uint64_t)i * 0x200000) | VMM_PRESENT | VMM_WRITABLE | VMM_HUGE;

    vmm_switch(0x4000);

    serial_puts("[vmm] PML4 at 0x4000, 128MB identity-mapped\n");

    uint64_t test_addrs[] = {
        0x0, 0x7C00, 0x100000, 0x200000,
        0x300000, 0x400000, 0x5FF000, 0x7FF0000
    };
    serial_puts("[vmm] Verification:\n");
    for (int i = 0; i < 8; i++) {
        uint64_t phys = vmm_get_phys(test_addrs[i]);
        serial_printf("  virt ");
        serial_puthex(test_addrs[i]);
        serial_puts(" -> phys ");
        serial_puthex(phys);
        serial_puts(phys == test_addrs[i] ? "  OK\n" : "  FAIL\n");
    }

    serial_printf("[vmm] Active PML4: ");
    serial_puthex(vmm_current_pml4());
    serial_puts("\n");
}

void vmm_print_range(uint64_t virt_start, uint64_t virt_end) {
    if (virt_end - virt_start > 0x400000) {
        serial_puts("[vmm] Range too large to print (max 4MB)\n");
        return;
    }
    serial_puts("[vmm] Mappings:\n");
    for (uint64_t v = virt_start; v < virt_end; v += PAGE_SIZE) {
        uint64_t phys = vmm_get_phys(v);
        if (phys) {
            serial_puts("  ");
            serial_puthex(v);
            serial_puts(" -> ");
            serial_puthex(phys);
            serial_puts("\n");
        }
    }
}