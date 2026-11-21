/*
 * Unknown OS - TAR Filesystem (for initrd)
 * Author: byrex <byrex.me@proton.me>
 */

#ifndef _TARFS_H
#define _TARFS_H

#include <kernel.h>

/* ============================================================
 * TAR Header (POSIX ustar format)
 * ============================================================ */
#define TAR_BLOCK_SIZE    512
#define TAR_NAME_SIZE     100
#define TAR_PREFIX_SIZE   155

struct tar_header {
    char name[TAR_NAME_SIZE];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[TAR_NAME_SIZE];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[TAR_PREFIX_SIZE];
    char padding[12];
};

/* Type flags */
#define TAR_TYPE_REGULAR      '0'
#define TAR_TYPE_REGULAR_ALT  '\0'
#define TAR_TYPE_HARDLINK     '1'
#define TAR_TYPE_SYMLINK      '2'
#define TAR_TYPE_CHARDEV      '3'
#define TAR_TYPE_BLOCKDEV     '4'
#define TAR_TYPE_DIRECTORY    '5'
#define TAR_TYPE_FIFO         '6'
#define TAR_TYPE_CONTIG       '7'
#define TAR_TYPE_GLOBAL_HDR   'g'
#define TAR_TYPE_EXT_HDR      'x'

/* ============================================================
 * TARFS API
 * ============================================================ */
int tarfs_init(uint8_t *archive, size_t size);
struct vfs_node *tarfs_get_root(void);
struct vfs_ops *tarfs_get_ops(void);

/* Internal */
struct tar_file {
    struct tar_header *header;
    uint8_t *data;
    size_t size;
};

struct tar_file *tarfs_find_file(const char *path);

#endif /* _TARFS_H */