/*
 * Unknown OS - UNK Binary Loader
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * UNK Structures (from spec)
 * ============================================================ */
#define UNK_MAGIC "UNKEXEC"
#define UNK_VERSION 1

#define UNK_ARCH_X86_64   1
#define UNK_ARCH_AARCH64  2
#define UNK_ARCH_RISCV64  3

#define UNK_FLAG_PIC           (1 << 0)
#define UNK_FLAG_STATIC        (1 << 1)
#define UNK_FLAG_SIGNED        (1 << 2)
#define UNK_FLAG_DEBUG         (1 << 3)
#define UNK_FLAG_COMPRESSED    (1 << 4)
#define UNK_FLAG_CAPABILITIES  (1 << 5)

#define UNK_PT_NULL      0
#define UNK_PT_LOAD      1
#define UNK_PT_DYNAMIC   2
#define UNK_PT_INTERP    3
#define UNK_PT_NOTE      4
#define UNK_PT_SHLIB     5
#define UNK_PT_PHDR      6
#define UNK_PT_TLS       7
#define UNK_PT_CAPS      8
#define UNK_PT_METADATA  9

#define UNK_PF_R    (1 << 0)
#define UNK_PF_W    (1 << 1)
#define UNK_PF_X    (1 << 2)

#pragma pack(push, 1)
struct unk_header {
    uint8_t  magic[8];
    uint8_t  version;
    uint8_t  arch;
    uint16_t flags;
    uint64_t entry_point;
    uint64_t ph_offset;
    uint64_t sh_offset;
    uint64_t str_offset;
    uint64_t sym_offset;
    uint64_t rel_offset;
    uint64_t sig_offset;
    uint32_t ph_count;
    uint32_t sh_count;
    uint32_t str_size;
    uint32_t sym_count;
    uint32_t rel_count;
    uint32_t sig_size;
    uint32_t checksum;
    uint8_t  reserved[24];
};

struct unk_phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

struct unk_shdr {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t vaddr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t align;
    uint64_t entsize;
};

struct unk_sym {
    uint32_t name;
    uint8_t  info;
    uint8_t  other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
};

struct unk_rela {
    uint64_t offset;
    uint64_t info;
    int64_t  addend;
};

struct unk_capability {
    uint64_t type;
    uint64_t value;
};

struct unk_signature {
    uint8_t  algorithm;
    uint8_t  reserved[3];
    uint32_t key_id;
    uint64_t signed_offset;
    uint64_t signed_size;
    uint8_t  signature[64];
};
#pragma pack(pop)

/* Relocation types */
#define UNK_R_X86_64_NONE       0
#define UNK_R_X86_64_64         1
#define UNK_R_X86_64_PC32       2
#define UNK_R_X86_64_GOT32      3
#define UNK_R_X86_64_PLT32      4
#define UNK_R_X86_64_COPY       5
#define UNK_R_X86_64_GLOB_DAT   6
#define UNK_R_X86_64_JUMP_SLOT  7
#define UNK_R_X86_64_RELATIVE   8
#define UNK_R_X86_64_GOTPCREL   9
#define UNK_R_X86_64_32         10
#define UNK_R_X86_64_32S        11
#define UNK_R_X86_64_16         12
#define UNK_R_X86_64_PC16       13
#define UNK_R_X86_64_8          14
#define UNK_R_X86_64_PC8        15

#define UNK_R_SYM(info)  ((info) >> 32)
#define UNK_R_TYPE(info) ((info) & 0xFFFFFFFF)

/* ============================================================
 * Helper Functions
 * ============================================================ */
static uint32_t crc32_checksum(uint8_t *data, size_t size) {
    /* Simple CRC32 implementation */
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/* ============================================================
 * Validation
 * ============================================================ */
int unk_validate(uint8_t *data, size_t size) {
    if (size < sizeof(struct unk_header)) {
        serial_puts("[UNK] File too small\n");
        return -1;
    }

    struct unk_header *hdr = (struct unk_header *)data;

    /* Check magic */
    if (memcmp(hdr->magic, UNK_MAGIC, 8) != 0) {
        serial_puts("[UNK] Invalid magic\n");
        return -1;
    }

    /* Check version */
    if (hdr->version != UNK_VERSION) {
        serial_printf("[UNK] Unsupported version: %d\n", hdr->version);
        return -1;
    }

    /* Check architecture */
    if (hdr->arch != UNK_ARCH_X86_64) {
        serial_printf("[UNK] Unsupported architecture: %d\n", hdr->arch);
        return -1;
    }

    /* Verify checksum (skip checksum field) */
    uint32_t stored_checksum = hdr->checksum;
    hdr->checksum = 0;
    uint32_t calc_checksum = crc32_checksum(data, size);
    hdr->checksum = stored_checksum;

    if (stored_checksum != calc_checksum) {
        serial_printf("[UNK] Checksum mismatch: stored=0x%x, calc=0x%x\n",
                      stored_checksum, calc_checksum);
        return -1;
    }

    /* Validate offsets */
    if (hdr->ph_offset + hdr->ph_count * sizeof(struct unk_phdr) > size) {
        serial_puts("[UNK] Program header table out of bounds\n");
        return -1;
    }

    return 0;
}

struct unk_header *unk_parse_header(uint8_t *data, size_t size) {
    if (unk_validate(data, size) != 0) {
        return NULL;
    }
    return (struct unk_header *)data;
}

/* ============================================================
 * Relocation Application
 * ============================================================ */
int unk_apply_relocations(uint8_t *base, struct unk_header *hdr) {
    if (!hdr->rel_count || !hdr->rel_offset) return 0;

    struct unk_rela *relas = (struct unk_rela *)(base + hdr->rel_offset);
    struct unk_sym *syms = (struct unk_sym *)(base + hdr->sym_offset);
    char *strtab = (char *)(base + hdr->str_offset);

    for (uint32_t i = 0; i < hdr->rel_count; i++) {
        struct unk_rela *rel = &relas[i];
        uint32_t sym_idx = UNK_R_SYM(rel->info);
        uint32_t type = UNK_R_TYPE(rel->info);

        uint64_t *addr = (uint64_t *)(base + rel->offset);
        uint64_t sym_val = 0;

        if (sym_idx < hdr->sym_count) {
            struct unk_sym *sym = &syms[sym_idx];
            sym_val = sym->value;
        }

        switch (type) {
            case UNK_R_X86_64_NONE:
                break;
            case UNK_R_X86_64_64:
                *addr = sym_val + rel->addend;
                break;
            case UNK_R_X86_64_PC32:
                *(uint32_t *)addr = (sym_val + rel->addend) - (uint64_t)addr;
                break;
            case UNK_R_X86_64_RELATIVE:
                *addr = (uint64_t)base + rel->addend;
                break;
            case UNK_R_X86_64_COPY:
                /* Copy from shared library - not implemented */
                break;
            case UNK_R_X86_64_GLOB_DAT:
                *addr = sym_val + rel->addend;
                break;
            case UNK_R_X86_64_JUMP_SLOT:
                *addr = sym_val + rel->addend;
                break;
            default:
                serial_printf("[UNK] Unknown relocation type: %d\n", type);
                return -1;
        }
    }

    return 0;
}

/* ============================================================
 * Capability Checking
 * ============================================================ */
bool unk_check_capability(struct unk_header *hdr, uint64_t cap_type) {
    if (!(hdr->flags & UNK_FLAG_CAPABILITIES)) return false;

    /* Find capability table section */
    struct unk_shdr *shdrs = (struct unk_shdr *)((uint8_t *)hdr + hdr->sh_offset);
    for (uint32_t i = 0; i < hdr->sh_count; i++) {
        if (shdrs[i].type == 12) {  // UNK_SHT_CAPS
            struct unk_capability *caps = (struct unk_capability *)((uint8_t *)hdr + shdrs[i].offset);
            uint32_t count = shdrs[i].size / sizeof(struct unk_capability);
            for (uint32_t j = 0; j < count; j++) {
                if (caps[j].type == cap_type) return true;
            }
        }
    }
    return false;
}

/* ============================================================
 * Load .unk Binary
 * ============================================================ */
void *unk_load(const char *path, uint64_t *entry_point) {
    serial_printf("[UNK] Loading %s\n", path);

    /* Open file via VFS */
    struct vfs_file *file;
    if (vfs_open(path, VFS_O_RDONLY, &file) != 0) {
        serial_printf("[UNK] Failed to open %s\n", path);
        return NULL;
    }

    /* Get file size */
    struct vfs_stat stat;
    vfs_stat(path, &stat);
    size_t size = stat.st_size;

    /* Read entire file */
    uint8_t *data = kmalloc(size);
    if (!data) {
        vfs_close(file);
        return NULL;
    }

    if (vfs_read(file, data, size) != (int64_t)size) {
        serial_puts("[UNK] Failed to read file\n");
        kfree(data);
        vfs_close(file);
        return NULL;
    }
    vfs_close(file);

    /* Validate */
    if (unk_validate(data, size) != 0) {
        kfree(data);
        return NULL;
    }

    struct unk_header *hdr = (struct unk_header *)data;

    /* Calculate total memory needed */
    uint64_t min_addr = ~0UL, max_addr = 0;
    struct unk_phdr *phdrs = (struct unk_phdr *)(data + hdr->ph_offset);
    for (uint32_t i = 0; i < hdr->ph_count; i++) {
        if (phdrs[i].type == UNK_PT_LOAD) {
            if (phdrs[i].vaddr < min_addr) min_addr = phdrs[i].vaddr;
            if (phdrs[i].vaddr + phdrs[i].memsz > max_addr)
                max_addr = phdrs[i].vaddr + phdrs[i].memsz;
        }
    }

    size_t total_size = max_addr - min_addr;
    if (total_size == 0 || total_size > 1024 * 1024 * 1024) {  // Max 1GB
        serial_puts("[UNK] Invalid memory layout\n");
        kfree(data);
        return NULL;
    }

    /* Allocate memory at preferred address (or let VMM choose) */
    void *base = vmm_alloc_pages((total_size + PAGE_SIZE - 1) / PAGE_SIZE,
                                  VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE | VMM_FLAGS_USER);
    if (!base) {
        serial_puts("[UNK] Failed to allocate memory\n");
        kfree(data);
        return NULL;
    }

    /* Load segments */
    for (uint32_t i = 0; i < hdr->ph_count; i++) {
        struct unk_phdr *ph = &phdrs[i];
        if (ph->type != UNK_PT_LOAD) continue;

        uint8_t *dst = (uint8_t *)base + (ph->vaddr - min_addr);
        uint8_t *src = data + ph->offset;

        memcpy(dst, src, ph->filesz);
        if (ph->memsz > ph->filesz) {
            memset(dst + ph->filesz, 0, ph->memsz - ph->filesz);
        }

        /* Set page permissions */
        uint64_t flags = VMM_FLAGS_PRESENT | VMM_FLAGS_USER;
        if (ph->flags & UNK_PF_W) flags |= VMM_FLAGS_WRITE;
        if (!(ph->flags & UNK_PF_X)) flags |= VMM_FLAGS_NX;

        for (size_t off = 0; off < ph->memsz; off += PAGE_SIZE) {
            vmm_map_page(dst + off, vmm_virt_to_phys(dst + off), flags);
        }
    }

    /* Apply relocations */
    if (unk_apply_relocations((uint8_t *)base, hdr) != 0) {
        serial_puts("[UNK] Relocation failed\n");
        kfree(data);
        return NULL;
    }

    *entry_point = (uint64_t)base + (hdr->entry_point - min_addr);
    kfree(data);

    serial_printf("[UNK] Loaded at %p, entry=%lx\n", base, *entry_point);
    return base;
}

/* ============================================================
 * Load and Execute
 * ============================================================ */
int unk_load_and_exec(const char *path, char *const argv[], char *const envp[]) {
    uint64_t entry_point;
    void *base = unk_load(path, &entry_point);
    if (!base) return -1;

    /* Create new task */
    struct task *task = sched_create_task((void (*)(void))entry_point, path);
    if (!task) return -1;

    /* Set up argument passing - simplified */
    /* In a real OS, you'd set up stack with argc, argv, envp */

    serial_printf("[UNK] Executing %s (pid=%lu)\n", path, task->pid);
    return task->pid;
}