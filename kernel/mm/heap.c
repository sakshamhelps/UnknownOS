/*
 * Unknown OS - Kernel Heap (Simple Buddy Allocator)
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * Heap State
 * ============================================================ */
#define HEAP_START (KERNEL_BASE + 0x20000000)  /* 512 MiB into higher half */
#define HEAP_SIZE  (16 * 1024 * 1024)          /* 16 MiB initial heap */
#define HEAP_BLOCK_MIN 16
#define HEAP_BLOCK_MAX (1 << 20)  /* 1 MiB max block */

struct heap_block {
    size_t size;
    bool free;
    struct heap_block *next;
    struct heap_block *prev;
};

static struct heap_block *heap_start = NULL;
static struct heap_block *heap_end = NULL;
static bool heap_initialized = false;

/* ============================================================
 * Heap Initialization
 * ============================================================ */
void heap_init(void) {
    if (heap_initialized) return;

    void *heap_virt = vmm_alloc_pages(HEAP_SIZE / PAGE_SIZE, VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE);
    if (!heap_virt) {
        PANIC("Heap: Failed to allocate virtual memory");
    }

    heap_start = (struct heap_block *)heap_virt;
    heap_start->size = HEAP_SIZE - sizeof(struct heap_block);
    heap_start->free = true;
    heap_start->next = NULL;
    heap_start->prev = NULL;

    heap_end = (struct heap_block *)((uint8_t *)heap_virt + HEAP_SIZE);
    heap_initialized = true;

    serial_printf("[HEAP] Initialized at 0x%lx, size %lu KB\n",
                  (uint64_t)heap_virt, HEAP_SIZE / 1024);
}

/* ============================================================
 * Block Splitting
 * ============================================================ */
static void heap_split_block(struct heap_block *block, size_t size) {
    if (block->size <= size + sizeof(struct heap_block) + HEAP_BLOCK_MIN) {
        return;  /* Not worth splitting */
    }

    struct heap_block *new_block = (struct heap_block *)((uint8_t *)block + sizeof(struct heap_block) + size);
    new_block->size = block->size - size - sizeof(struct heap_block);
    new_block->free = true;
    new_block->next = block->next;
    new_block->prev = block;

    if (block->next) {
        block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = size;
}

/* ============================================================
 * Block Merging (Coalescing)
 * ============================================================ */
static void heap_merge_block(struct heap_block *block) {
    /* Merge with next */
    if (block->next && block->next->free) {
        block->size += sizeof(struct heap_block) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    /* Merge with prev */
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(struct heap_block) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        block = block->prev;
    }
}

/* ============================================================
 * Allocation
 * ============================================================ */
void *kmalloc(size_t size) {
    if (!heap_initialized) heap_init();

    if (size == 0) return NULL;

    /* Align to 16 bytes */
    size = (size + 15) & ~15;

    struct heap_block *block = heap_start;
    while (block) {
        if (block->free && block->size >= size) {
            /* Found suitable block */
            block->free = false;
            heap_split_block(block, size);
            return (void *)((uint8_t *)block + sizeof(struct heap_block));
        }
        block = block->next;
    }

    /* Out of memory - expand heap */
    size_t pages = (size + sizeof(struct heap_block) + PAGE_SIZE - 1) / PAGE_SIZE;
    void *new_virt = vmm_alloc_pages(pages, VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE);
    if (!new_virt) {
        return NULL;
    }

    struct heap_block *new_block = (struct heap_block *)new_virt;
    new_block->size = pages * PAGE_SIZE - sizeof(struct heap_block);
    new_block->free = false;
    new_block->next = NULL;
    new_block->prev = heap_end;
    if (heap_end) {
        heap_end->next = new_block;
    }
    heap_end = new_block;

    heap_split_block(new_block, size);
    return (void *)((uint8_t *)new_block + sizeof(struct heap_block));
}

void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/* ============================================================
 * Free
 * ============================================================ */
void kfree(void *ptr) {
    if (!ptr) return;

    struct heap_block *block = (struct heap_block *)((uint8_t *)ptr - sizeof(struct heap_block));
    block->free = true;
    heap_merge_block(block);
}

/* ============================================================
 * Realloc
 * ============================================================ */
void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    struct heap_block *block = (struct heap_block *)((uint8_t *)ptr - sizeof(struct heap_block));
    if (block->size >= size) {
        /* Can fit in current block */
        heap_split_block(block, size);
        return ptr;
    }

    /* Need larger block */
    void *new_ptr = kmalloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}