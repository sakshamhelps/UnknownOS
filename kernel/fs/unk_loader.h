/*
 * Unknown OS - UNK Binary Loader
 * Author: byrex <byrex.me@proton.me>
 */

#ifndef _UNK_LOADER_H
#define _UNK_LOADER_H

#include <kernel.h>

/* ============================================================
 * UNK Loader API
 * ============================================================ */

/* Load and execute a .unk binary */
int unk_load_and_exec(const char *path, char *const argv[], char *const envp[]);

/* Load a .unk binary into memory (for dlopen-style) */
void *unk_load(const char *path, uint64_t *entry_point);

/* Validate .unk file */
int unk_validate(uint8_t *data, size_t size);

/* Parse UNK header */
struct unk_header *unk_parse_header(uint8_t *data, size_t size);

/* Apply relocations */
int unk_apply_relocations(uint8_t *base, struct unk_header *hdr);

/* Capability checking */
bool unk_check_capability(struct unk_header *hdr, uint64_t cap_type);

#endif /* _UNK_LOADER_H */