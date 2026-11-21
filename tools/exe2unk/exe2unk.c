/*
 * exe2unk - ELF/PE to .unk Converter
 * Author: byrex <byrex.me@proton.me>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <byteswap.h>
#include <zlib.h>  // for CRC32

/* ============================================================
 * UNK Format Structures (matching spec)
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

/* ============================================================
 * ELF Structures (simplified)
 * ============================================================ */
#pragma pack(push, 1)
struct elf64_ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct elf64_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

struct elf64_sym {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

struct elf64_rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
};
#pragma pack(pop)

#define ELF_MAGIC 0x464C457F
#define EM_X86_64 62

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7

#define PF_R 4
#define PF_W 2
#define PF_X 1

#define SHT_NULL      0
#define SHT_PROGBITS  1
#define SHT_SYMTAB    2
#define SHT_STRTAB    3
#define SHT_RELA      4
#define SHT_HASH      5
#define SHT_DYNAMIC   6
#define SHT_NOTE      7
#define SHT_NOBITS    8
#define SHT_REL       9
#define SHT_DYNSYM   11

#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

#define R_X86_64_NONE        0
#define R_X86_64_64          1
#define R_X86_64_PC32        2
#define R_X86_64_GOT32       3
#define R_X86_64_PLT32       4
#define R_X86_64_COPY        5
#define R_X86_64_GLOB_DAT    6
#define R_X86_64_JUMP_SLOT   7
#define R_X86_64_RELATIVE    8
#define R_X86_64_GOTPCREL    9
#define R_X86_64_32          10
#define R_X86_64_32S         11
#define R_X86_64_16          12
#define R_X86_64_PC16        13
#define R_X86_64_8           14
#define R_X86_64_PC8         15

#define ELF64_R_SYM(info)  ((info) >> 32)
#define ELF64_R_TYPE(info) ((info) & 0xFFFFFFFF)

/* ============================================================
 * Context
 * ============================================================ */
struct context {
    uint8_t *input_data;
    size_t input_size;
    struct elf64_ehdr *ehdr;
    struct elf64_phdr *phdrs;
    struct elf64_shdr *shdrs;
    char *shstrtab;
    char *strtab;
    struct elf64_sym *symtab;
    size_t symtab_count;
    struct elf64_rela *rela;
    size_t rela_count;

    uint8_t *output_data;
    size_t output_size;
    size_t output_capacity;

    struct unk_header header;
    struct unk_phdr *unk_phdrs;
    struct unk_shdr *unk_shdrs;
    struct unk_sym *unk_syms;
    struct unk_rela *unk_relas;
    struct unk_capability *unk_caps;
    char *unk_strtab;
    size_t unk_strtab_size;
    size_t unk_strtab_capacity;
    char *metadata_json;
};

/* ============================================================
 * Helpers
 * ============================================================ */
static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) { fprintf(stderr, "OOM\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t size) {
    p = realloc(p, size);
    if (!p) { fprintf(stderr, "OOM\n"); exit(1); }
    return p;
}

static uint32_t crc32_file(uint8_t *data, size_t size) {
    return crc32(0L, Z_NULL, 0);
}

static int strtab_add(struct context *ctx, const char *str) {
    int offset = ctx->unk_strtab_size;
    size_t len = strlen(str) + 1;
    if (ctx->unk_strtab_size + len > ctx->unk_strtab_capacity) {
        ctx->unk_strtab_capacity = (ctx->unk_strtab_capacity + len) * 2;
        ctx->unk_strtab = xrealloc(ctx->unk_strtab, ctx->unk_strtab_capacity);
    }
    strcpy(ctx->unk_strtab + ctx->unk_strtab_size, str);
    ctx->unk_strtab_size += len;
    return offset;
}

/* ============================================================
 * ELF Parsing
 * ============================================================ */
static bool parse_elf(struct context *ctx) {
    if (ctx->input_size < sizeof(struct elf64_ehdr)) return false;
    ctx->ehdr = (struct elf64_ehdr *)ctx->input_data;

    if (*(uint32_t *)ctx->ehdr->e_ident != ELF_MAGIC) {
        fprintf(stderr, "Not an ELF file\n");
        return false;
    }

    if (ctx->ehdr->e_ident[4] != 2) {  // 64-bit
        fprintf(stderr, "Only 64-bit ELF supported\n");
        return false;
    }

    if (ctx->ehdr->e_machine != EM_X86_64) {
        fprintf(stderr, "Only x86_64 supported\n");
        return false;
    }

    ctx->phdrs = (struct elf64_phdr *)(ctx->input_data + ctx->ehdr->e_phoff);
    ctx->shdrs = (struct elf64_shdr *)(ctx->input_data + ctx->ehdr->e_shoff);

    if (ctx->ehdr->e_shstrndx < ctx->ehdr->e_shnum) {
        ctx->shstrtab = (char *)(ctx->input_data + ctx->shdrs[ctx->ehdr->e_shstrndx].sh_offset);
    }

    /* Find string tables */
    for (int i = 0; i < ctx->ehdr->e_shnum; i++) {
        if (ctx->shdrs[i].sh_type == SHT_STRTAB) {
            const char *name = ctx->shstrtab + ctx->shdrs[i].sh_name;
            if (strcmp(name, ".strtab") == 0) {
                ctx->strtab = (char *)(ctx->input_data + ctx->shdrs[i].sh_offset);
            } else if (strcmp(name, ".dynstr") == 0) {
                ctx->strtab = (char *)(ctx->input_data + ctx->shdrs[i].sh_offset);
            }
        }
        if (ctx->shdrs[i].sh_type == SHT_SYMTAB || ctx->shdrs[i].sh_type == SHT_DYNSYM) {
            ctx->symtab = (struct elf64_sym *)(ctx->input_data + ctx->shdrs[i].sh_offset);
            ctx->symtab_count = ctx->shdrs[i].sh_size / sizeof(struct elf64_sym);
        }
        if (ctx->shdrs[i].sh_type == SHT_RELA) {
            ctx->rela = (struct elf64_rela *)(ctx->input_data + ctx->shdrs[i].sh_offset);
            ctx->rela_count = ctx->shdrs[i].sh_size / sizeof(struct elf64_rela);
        }
    }

    return true;
}

/* ============================================================
 * Conversion
 * ============================================================ */
static void convert_header(struct context *ctx) {
    memset(&ctx->header, 0, sizeof(ctx->header));
    memcpy(ctx->header.magic, UNK_MAGIC, 8);
    ctx->header.version = UNK_VERSION;
    ctx->header.arch = UNK_ARCH_X86_64;
    ctx->header.flags = UNK_FLAG_PIC;  // Assume PIC for now
    ctx->header.entry_point = ctx->ehdr->e_entry;
    ctx->header.checksum = 0;  // Calculate later
}

static void convert_program_headers(struct context *ctx) {
    int load_count = 0;
    for (int i = 0; i < ctx->ehdr->e_phnum; i++) {
        if (ctx->phdrs[i].p_type == PT_LOAD) load_count++;
    }

    ctx->header.ph_count = load_count;
    ctx->unk_phdrs = xmalloc(load_count * sizeof(struct unk_phdr));

    int idx = 0;
    for (int i = 0; i < ctx->ehdr->e_phnum; i++) {
        struct elf64_phdr *eph = &ctx->phdrs[i];
        if (eph->p_type != PT_LOAD) continue;

        struct unk_phdr *uph = &ctx->unk_phdrs[idx++];
        uph->type = UNK_PT_LOAD;
        uph->flags = 0;
        if (eph->p_flags & PF_R) uph->flags |= UNK_PF_R;
        if (eph->p_flags & PF_W) uph->flags |= UNK_PF_W;
        if (eph->p_flags & PF_X) uph->flags |= UNK_PF_X;
        uph->offset = eph->p_offset;
        uph->vaddr = eph->p_vaddr;
        uph->paddr = eph->p_paddr;
        uph->filesz = eph->p_filesz;
        uph->memsz = eph->p_memsz;
        uph->align = eph->p_align;
    }
}

static void convert_section_headers(struct context *ctx) {
    int count = 0;
    for (int i = 0; i < ctx->ehdr->e_shnum; i++) {
        if (ctx->shdrs[i].sh_type != SHT_NULL) count++;
    }

    ctx->header.sh_count = count;
    ctx->unk_shdrs = xmalloc(count * sizeof(struct unk_shdr));

    int idx = 0;
    for (int i = 0; i < ctx->ehdr->e_shnum; i++) {
        struct elf64_shdr *esh = &ctx->shdrs[i];
        if (esh->sh_type == SHT_NULL) continue;

        struct unk_shdr *ush = &ctx->unk_shdrs[idx++];
        const char *name = ctx->shstrtab + esh->sh_name;
        ush->name = strtab_add(ctx, name);

        switch (esh->sh_type) {
            case SHT_PROGBITS: ush->type = UNK_SHT_PROGBITS; break;
            case SHT_SYMTAB:   ush->type = UNK_SHT_SYMTAB; break;
            case SHT_STRTAB:   ush->type = UNK_SHT_STRTAB; break;
            case SHT_RELA:     ush->type = UNK_SHT_RELA; break;
            case SHT_HASH:     ush->type = UNK_SHT_HASH; break;
            case SHT_DYNAMIC:  ush->type = UNK_SHT_DYNAMIC; break;
            case SHT_NOTE:     ush->type = UNK_SHT_NOTE; break;
            case SHT_NOBITS:   ush->type = UNK_SHT_NOBITS; break;
            case SHT_REL:      ush->type = UNK_SHT_REL; break;
            case SHT_DYNSYM:   ush->type = UNK_SHT_DYNSYM; break;
            default:           ush->type = UNK_SHT_PROGBITS; break;
        }

        ush->flags = esh->sh_flags;
        ush->vaddr = esh->sh_addr;
        ush->offset = esh->sh_offset;
        ush->size = esh->sh_size;
        ush->link = esh->sh_link;
        ush->info = esh->sh_info;
        ush->align = esh->sh_addralign;
        ush->entsize = esh->sh_entsize;
    }
}

static void convert_symbols(struct context *ctx) {
    if (!ctx->symtab || ctx->symtab_count == 0) return;

    ctx->header.sym_count = ctx->symtab_count;
    ctx->unk_syms = xmalloc(ctx->symtab_count * sizeof(struct unk_sym));

    for (size_t i = 0; i < ctx->symtab_count; i++) {
        struct elf64_sym *esym = &ctx->symtab[i];
        struct unk_sym *usym = &ctx->unk_syms[i];

        if (ctx->strtab) {
            usym->name = strtab_add(ctx, ctx->strtab + esym->st_name);
        } else {
            usym->name = 0;
        }

        int bind = esym->st_info >> 4;
        int type = esym->st_info & 0xF;
        usym->info = (bind << 4) | type;
        usym->other = esym->st_other;
        usym->shndx = esym->st_shndx;
        usym->value = esym->st_value;
        usym->size = esym->st_size;
    }
}

static void convert_relocations(struct context *ctx) {
    if (!ctx->rela || ctx->rela_count == 0) return;

    ctx->header.rel_count = ctx->rela_count;
    ctx->unk_relas = xmalloc(ctx->rela_count * sizeof(struct unk_rela));

    for (size_t i = 0; i < ctx->rela_count; i++) {
        struct elf64_rela *erel = &ctx->rela[i];
        struct unk_rela *urel = &ctx->unk_relas[i];

        urel->offset = erel->r_offset;
        urel->info = ((uint64_t)ELF64_R_SYM(erel->r_info) << 32) | ELF64_R_TYPE(erel->r_info);
        urel->addend = erel->r_addend;
    }
}

static void convert_capabilities(struct context *ctx, int argc, char **argv) {
    /* Parse --cap arguments */
    int cap_count = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--cap") == 0 && i + 1 < argc) {
            cap_count++;
        }
    }

    if (cap_count > 0) {
        ctx->header.flags |= UNK_FLAG_CAPABILITIES;
        ctx->unk_caps = xmalloc(cap_count * sizeof(struct unk_capability));

        int idx = 0;
        for (int i = 0; i < argc; i++) {
            if (strcmp(argv[i], "--cap") == 0 && i + 1 < argc) {
                const char *cap = argv[i + 1];
                uint64_t type = 0;

                if (strcmp(cap, "fs_read") == 0) type = 1;
                else if (strcmp(cap, "fs_write") == 0) type = 2;
                else if (strcmp(cap, "fs_execute") == 0) type = 3;
                else if (strcmp(cap, "net_tcp_client") == 0) type = 4;
                else if (strcmp(cap, "net_tcp_server") == 0) type = 5;
                else if (strcmp(cap, "net_udp") == 0) type = 6;
                else if (strcmp(cap, "net_raw") == 0) type = 7;
                else if (strcmp(cap, "hw_gpu") == 0) type = 8;
                else if (strcmp(cap, "hw_audio") == 0) type = 9;
                else if (strcmp(cap, "hw_camera") == 0) type = 10;
                else if (strcmp(cap, "hw_usb") == 0) type = 11;
                else if (strcmp(cap, "sys_proc_spawn") == 0) type = 12;
                else if (strcmp(cap, "sys_proc_kill") == 0) type = 13;
                else if (strcmp(cap, "sys_debug") == 0) type = 14;
                else if (strcmp(cap, "sys_admin") == 0) type = 15;
                else if (strcmp(cap, "ipc_send") == 0) type = 16;
                else if (strcmp(cap, "ipc_recv") == 0) type = 17;
                else if (strcmp(cap, "crypto_random") == 0) type = 18;
                else if (strcmp(cap, "crypto_keys") == 0) type = 19;
                else {
                    fprintf(stderr, "Unknown capability: %s\n", cap);
                    continue;
                }

                ctx->unk_caps[idx].type = type;
                ctx->unk_caps[idx].value = 0;
                idx++;
            }
        }
    }
}

/* ============================================================
 * Output Writing
 * ============================================================ */
static void write_output(struct context *ctx, const char *output_file) {
    /* Calculate offsets */
    size_t offset = sizeof(struct unk_header);

    /* Program headers */
    ctx->header.ph_offset = offset;
    offset += ctx->header.ph_count * sizeof(struct unk_phdr);

    /* Section headers */
    ctx->header.sh_offset = offset;
    offset += ctx->header.sh_count * sizeof(struct unk_shdr);

    /* String table */
    ctx->header.str_offset = offset;
    ctx->header.str_size = ctx->unk_strtab_size;
    offset += ctx->unk_strtab_size;

    /* Symbol table */
    ctx->header.sym_offset = offset;
    offset += ctx->header.sym_count * sizeof(struct unk_sym);

    /* Relocation table */
    ctx->header.rel_offset = offset;
    offset += ctx->header.rel_count * sizeof(struct unk_rela);

    /* Capability table */
    if (ctx->header.flags & UNK_FLAG_CAPABILITIES) {
        /* Add capability section */
    }

    /* Program data - copy from ELF loadable segments */
    for (int i = 0; i < ctx->header.ph_count; i++) {
        struct unk_phdr *uph = &ctx->unk_phdrs[i];
        uph->offset = offset;
        offset += uph->filesz;
    }

    /* Align */
    offset = (offset + 7) & ~7;

    ctx->output_size = offset;
    ctx->output_data = xmalloc(ctx->output_size);
    ctx->output_capacity = ctx->output_size;

    /* Write header */
    memcpy(ctx->output_data, &ctx->header, sizeof(ctx->header));

    /* Write program headers */
    memcpy(ctx->output_data + ctx->header.ph_offset,
           ctx->unk_phdrs, ctx->header.ph_count * sizeof(struct unk_phdr));

    /* Write section headers */
    memcpy(ctx->output_data + ctx->header.sh_offset,
           ctx->unk_shdrs, ctx->header.sh_count * sizeof(struct unk_shdr));

    /* Write string table */
    memcpy(ctx->output_data + ctx->header.str_offset,
           ctx->unk_strtab, ctx->unk_strtab_size);

    /* Write symbol table */
    memcpy(ctx->output_data + ctx->header.sym_offset,
           ctx->unk_syms, ctx->header.sym_count * sizeof(struct unk_sym));

    /* Write relocation table */
    memcpy(ctx->output_data + ctx->header.rel_offset,
           ctx->unk_relas, ctx->header.rel_count * sizeof(struct unk_rela));

    /* Write program data */
    for (int i = 0; i < ctx->header.ph_count; i++) {
        struct unk_phdr *uph = &ctx->unk_phdrs[i];
        /* Find corresponding ELF program header */
        for (int j = 0; j < ctx->ehdr->e_phnum; j++) {
            if (ctx->phdrs[j].p_type == PT_LOAD &&
                ctx->phdrs[j].p_vaddr == uph->vaddr &&
                ctx->phdrs[j].p_filesz == uph->filesz) {
                memcpy(ctx->output_data + uph->offset,
                       ctx->input_data + ctx->phdrs[j].p_offset,
                       uph->filesz);
                break;
            }
        }
    }

    /* Calculate checksum */
    ctx->header.checksum = crc32(0L, ctx->output_data + 4, ctx->output_size - 4);
    memcpy(ctx->output_data, &ctx->header, sizeof(ctx->header));

    /* Write to file */
    FILE *f = fopen(output_file, "wb");
    if (!f) { perror("fopen"); exit(1); }
    fwrite(ctx->output_data, 1, ctx->output_size, f);
    fclose(f);

    printf("Written %zu bytes to %s\n", ctx->output_size, output_file);
}

/* ============================================================
 * Main
 * ============================================================ */
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.elf> <output.unk> [options]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --cap <capability>  Add capability (fs_read, net_tcp_client, etc.)\n");
        fprintf(stderr, "  --sign              Sign the output (requires --key-id)\n");
        fprintf(stderr, "  --key-id <id>       Key ID for signing\n");
        fprintf(stderr, "  --metadata <file>   Add metadata JSON\n");
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];

    /* Read input file */
    int fd = open(input_file, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st;
    fstat(fd, &st);

    struct context ctx = {0};
    ctx.input_size = st.st_size;
    ctx.input_data = mmap(NULL, ctx.input_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (ctx.input_data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    ctx.unk_strtab_capacity = 4096;
    ctx.unk_strtab = xmalloc(ctx.unk_strtab_capacity);
    ctx.unk_strtab[0] = '\0';
    ctx.unk_strtab_size = 1;

    /* Parse and convert */
    if (!parse_elf(&ctx)) {
        fprintf(stderr, "Failed to parse ELF\n");
        return 1;
    }

    convert_header(&ctx);
    convert_program_headers(&ctx);
    convert_section_headers(&ctx);
    convert_symbols(&ctx);
    convert_relocations(&ctx);
    convert_capabilities(&ctx, argc, argv);
    write_output(&ctx, output_file);

    munmap(ctx.input_data, ctx.input_size);
    return 0;
}