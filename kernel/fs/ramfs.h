/*
 * Unknown OS - RAM Filesystem
 * Author: byrex <byrex.me@proton.me>
 */

#ifndef _RAMFS_H
#define _RAMFS_H

#include <kernel.h>

/* ============================================================
 * RAMFS API
 * ============================================================ */
void ramfs_init(void);
struct vfs_node *ramfs_get_root(void);
struct vfs_ops *ramfs_get_ops(void);

/* Internal */
struct ramfs_node {
    char name[VFS_NAME_MAX + 1];
    uint32_t type;
    uint32_t mode;
    uint64_t size;
    uint64_t inode;
    void *data;           /* For files: content. For dirs: children list */
    struct ramfs_node *parent;
    struct ramfs_node *children;
    struct ramfs_node *next;
    uint64_t atime, mtime, ctime;
};

struct ramfs_node *ramfs_create_node(struct ramfs_node *parent, const char *name,
                                      uint32_t type, uint32_t mode);
void ramfs_destroy_node(struct ramfs_node *node);

#endif /* _RAMFS_H */