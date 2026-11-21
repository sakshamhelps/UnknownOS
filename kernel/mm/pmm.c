/*
 * Unknown OS - Physical Memory Manager (PMM)
 * Bitmap-based page allocator
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * PMM State
 * ============================================================ */
static uint64_t *pmm_bitmap = NULL;
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_free_pages = 0;
static uint64_t pmm_bitmap_pages = 0;
static uint64_t kernel_physical_base = 0;
static uint64_t kernel_physical_end = 0;
static uint64_t hhdm_offset = 0;

/* ============================================================
 * Bitmap Helpers
 * ============================================================ */
static inline bool pmm_bitmap_test(uint64_t page) {
    return (pmm_bitmap[page / 64] >> (page % 64)) & 1;
}

static inline void pmm_bitmap_set(uint64_t page) {
    pmm_bitmap[page / 64] |= (1ULL << (page % 64));
}

static inline void pmm_bitmap_clear(uint64_t page) {
    pmm_bitmap[page / 64] &= ~(1ULL << (page % 64));
}

/* ============================================================
 * PMM Initialization
 * ============================================================ */
void pmm_init(struct boot_info *boot_info) {
    struct limine_memmap_response *mm = boot_info->memmap;
    struct limine_hhdm_response *hhdm = boot_info->hhdm;

    if (hhdm) {
        hhdm_offset = hhdm->offset;
    }

    kernel_physical_base = boot_info->kernel_physical_base;
    kernel_physical_end = boot_info->kernel_virtual_base - hhdm_offset;
    /* kernel size will be calculated from linker symbols */

    /* Find total usable memory */
    uint64_t max_addr = 0;
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = &mm->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            uint64_t end = e->base + e->length;
            if (end > max_addr) max_addr = end;
        }
    }

    pmm_total_pages = max_addr / PAGE_SIZE;
    pmm_bitmap_pages = (pmm_total_pages + 4095) / 4096;  // pages needed for bitmap

    /* Find a free region for the bitmap */
    uint64_t bitmap_phys = 0;
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = &mm->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= pmm_bitmap_pages * PAGE_SIZE) {
            bitmap_phys = e->base;
            break;
        }
    }

    if (!bitmap_phys) {
        PANIC("PMM: No space for bitmap");
    }

    /* Map bitmap into higher-half */
    pmm_bitmap = (uint64_t *)(bitmap_phys + hhdm_offset);

    /* Clear bitmap */
    memset(pmm_bitmap, 0xFF, pmm_bitmap_pages * PAGE_SIZE);  // Mark all as used initially

    /* Mark usable pages as free */
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = &mm->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_page = PAGE_ALIGN(e->base) / PAGE_SIZE;
            uint64_t end_page = (e->base + e->length) / PAGE_SIZE;
            for (uint64_t p = start_page; p < end_page; p++) {
                pmm_bitmap_clear(p);
                pmm_free_pages++;
            }
        }
    }

    /* Mark kernel pages as used */
    uint64_t kernel_start_page = kernel_physical_base / PAGE_SIZE;
    uint64_t kernel_end_page = (kernel_physical_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t p = kernel_start_page; p < kernel_end_page; p++) {
        if (!pmm_bitmap_test(p)) {
            pmm_bitmap_set(p);
            pmm_free_pages--;
        }
    }

    /* Mark bitmap pages as used */
    uint64_t bitmap_start_page = bitmap_phys / PAGE_SIZE;
    for (uint64_t p = bitmap_start_page; p < bitmap_start_page + pmm_bitmap_pages; p++) {
        if (!pmm_bitmap_test(p)) {
            pmm_bitmap_set(p);
            pmm_free_pages--;
        }
    }

    serial_printf("[PMM] Total pages: %lu, Free: %lu, Bitmap at 0x%lx\n",
                  pmm_total_pages, pmm_free_pages, bitmap_phys);
}

/* ============================================================
 * Page Allocation
 * ============================================================ */
void *pmm_alloc_page(void) {
    for (uint64_t i = 0; i < pmm_total_pages; i++) {
        if (!pmm_bitmap_test(i)) {
            pmm_bitmap_set(i);
            pmm_free_pages--;
            return (void *)(i * PAGE_SIZE + hhdm_offset);
        }
    }
    PANIC("PMM: Out of memory");
    return NULL;
}

void *pmm_alloc_pages(size_t count) {
    for (uint64_t i = 0; i <= pmm_total_pages - count; i++) {
        bool free = true;
        for (size_t j = 0; j < count; j++) {
            if (pmm_bitmap_test(i + j)) {
                free = false;
                i += j;
                break;
            }
        }
        if (free) {
            for (size_t j = 0; j < count; j++) {
                pmm_bitmap_set(i + j);
            }
            pmm_free_pages -= count;
            return (void *)(i * PAGE_SIZE + hhdm_offset);
        }
    }
    PANIC("PMM: Out of contiguous memory");
    return NULL;
}

/* ============================================================
 * Page Freeing
 * ============================================================ */
void pmm_free_page(void *page) {
    uint64_t phys = (uint64_t)page - hhdm_offset;
    uint64_t p = phys / PAGE_SIZE;

    if (p >= pmm_total_pages) return;
    if (!pmm_bitmap_test(p)) return;  // Double free

    pmm_bitmap_clear(p);
    pmm_free_pages++;
}

void pmm_free_pages(void *page, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_free_page((void *)((uint64_t)page + i * PAGE_SIZE));
    }
}

/* ============================================================
 * Queries
 * ============================================================ */
uint64_t pmm_get_total_pages(void) {
    return pmm_total_pages;
}

uint64_t pmm_get_free_pages(void) {
    return pmm_free_pages;
}