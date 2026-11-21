/*
 * Unknown OS - Virtual Memory Manager (VMM)
 * Higher-half kernel, 4-level page tables
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * VMM State
 * ============================================================ */
static uint64_t *kernel_pml4 = NULL;
static uint64_t hhdm_offset = 0;

/* ============================================================
 * VMM Initialization
 * ============================================================ */
void vmm_init(struct boot_info *boot_info) {
    struct limine_hhdm_response *hhdm = boot_info->hhdm;
    if (hhdm) {
        hhdm_offset = hhdm->offset;
    }

    /* Kernel PML4 is already set up by paging.asm */
    /* We just need to get its address from CR3 */
    asm volatile ("mov %%cr3, %0" : "=r"(kernel_pml4));
    kernel_pml4 = (uint64_t *)((uint64_t)kernel_pml4 + hhdm_offset);

    serial_printf("[VMM] Kernel PML4 at 0x%lx, HHDM offset 0x%lx\n",
                  (uint64_t)kernel_pml4, hhdm_offset);
}

/* ============================================================
 * Page Table Walking Helpers
 * ============================================================ */
static uint64_t *vmm_get_pml4(void) {
    return kernel_pml4;
}

static uint64_t *vmm_walk(uint64_t *pml4, uint64_t virt, bool create, uint64_t flags) {
    /* PML4 index (bits 47-39) */
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t *pdp;

    if (!(pml4[pml4_idx] & VMM_FLAGS_PRESENT)) {
        if (!create) return NULL;
        void *page = pmm_alloc_page();
        if (!page) return NULL;
        memset(page, 0, PAGE_SIZE);
        pml4[pml4_idx] = ((uint64_t)page - hhdm_offset) | flags | VMM_FLAGS_PRESENT;
    }
    pdp = (uint64_t *)((pml4[pml4_idx] & ~0xFFF) + hhdm_offset);

    /* PDP index (bits 38-30) */
    uint64_t pdp_idx = (virt >> 30) & 0x1FF;
    uint64_t *pd;

    if (!(pdp[pdp_idx] & VMM_FLAGS_PRESENT)) {
        if (!create) return NULL;
        void *page = pmm_alloc_page();
        if (!page) return NULL;
        memset(page, 0, PAGE_SIZE);
        pdp[pdp_idx] = ((uint64_t)page - hhdm_offset) | flags | VMM_FLAGS_PRESENT;
    }
    pd = (uint64_t *)((pdp[pdp_idx] & ~0xFFF) + hhdm_offset);

    /* PD index (bits 29-21) */
    uint64_t pd_idx = (virt >> 21) & 0x1FF;
    uint64_t *pt;

    if (!(pd[pd_idx] & VMM_FLAGS_PRESENT)) {
        if (!create) return NULL;
        void *page = pmm_alloc_page();
        if (!page) return NULL;
        memset(page, 0, PAGE_SIZE);
        pd[pd_idx] = ((uint64_t)page - hhdm_offset) | flags | VMM_FLAGS_PRESENT;
    }
    pt = (uint64_t *)((pd[pd_idx] & ~0xFFF) + hhdm_offset);

    /* PT index (bits 20-12) */
    uint64_t pt_idx = (virt >> 12) & 0x1FF;

    return &pt[pt_idx];
}

/* ============================================================
 * Map/Unmap Pages
 * ============================================================ */
void vmm_map_page(void *virt, void *phys, uint64_t flags) {
    uint64_t *entry = vmm_walk(kernel_pml4, (uint64_t)virt, true, flags);
    if (!entry) {
        PANIC("VMM: Failed to allocate page table");
    }
    *entry = ((uint64_t)phys - hhdm_offset) | flags | VMM_FLAGS_PRESENT;
    invlpg(virt);
}

void vmm_unmap_page(void *virt) {
    uint64_t *entry = vmm_walk(kernel_pml4, (uint64_t)virt, false, 0);
    if (entry) {
        *entry = 0;
        invlpg(virt);
    }
}

/* ============================================================
 * Allocate/Free Virtual Pages
 * ============================================================ */
void *vmm_alloc_pages(size_t count, uint64_t flags) {
    /* Simple first-fit in kernel virtual space */
    static uint64_t next_virt = KERNEL_BASE + 0x10000000;  /* Start after kernel */

    void *virt = (void *)next_virt;
    next_virt += count * PAGE_SIZE;

    for (size_t i = 0; i < count; i++) {
        void *phys = pmm_alloc_page();
        if (!phys) {
            /* Rollback */
            for (size_t j = 0; j < i; j++) {
                vmm_unmap_page((void *)((uint64_t)virt + j * PAGE_SIZE));
            }
            return NULL;
        }
        vmm_map_page((void *)((uint64_t)virt + i * PAGE_SIZE), phys, flags);
    }

    return virt;
}

void vmm_free_pages(void *virt, size_t count) {
    for (size_t i = 0; i < count; i++) {
        void *page = (void *)((uint64_t)virt + i * PAGE_SIZE);
        uint64_t *entry = vmm_walk(kernel_pml4, (uint64_t)page, false, 0);
        if (entry && (*entry & VMM_FLAGS_PRESENT)) {
            uint64_t phys = (*entry & ~0xFFF) + hhdm_offset;
            pmm_free_page((void *)phys);
            *entry = 0;
            invlpg(page);
        }
    }
}

/* ============================================================
 * Address Translation
 * ============================================================ */
void *vmm_phys_to_virt(void *phys) {
    return (void *)((uint64_t)phys + hhdm_offset);
}

void *vmm_virt_to_phys(void *virt) {
    uint64_t *entry = vmm_walk(kernel_pml4, (uint64_t)virt, false, 0);
    if (!entry || !(*entry & VMM_FLAGS_PRESENT)) {
        return NULL;
    }
    return (void *)((*entry & ~0xFFF) + hhdm_offset);
}

/* ============================================================
 * Invalidate TLB
 * ============================================================ */
static inline void invlpg(void *addr) {
    asm volatile ("invlpg [%0]" :: "r"(addr) : "memory");
}