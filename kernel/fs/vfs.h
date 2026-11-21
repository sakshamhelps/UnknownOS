/*
 * Unknown OS - Virtual File System (VFS)
 * Author: byrex <byrex.me@proton.me>
 */

#ifndef _VFS_H
#define _VFS_H

#include <kernel.h>

/* ============================================================
 * VFS Types
 * ============================================================ */
#define VFS_NAME_MAX    255
#define VFS_PATH_MAX    4096

/* File types */
#define VFS_TYPE_UNKNOWN    0
#define VFS_TYPE_REGULAR    1
#define VFS_TYPE_DIRECTORY  2
#define VFS_TYPE_SYMLINK    3
#define VFS_TYPE_CHARDEV    4
#define VFS_TYPE_BLOCKDEV   5
#define VFS_TYPE_FIFO       6
#define VFS_TYPE_SOCKET     7

/* Open flags */
#define VFS_O_RDONLY    0x0000
#define VFS_O_WRONLY    0x0001
#define VFS_O_RDWR      0x0002
#define VFS_O_CREAT     0x0040
#define VFS_O_TRUNC     0x0200
#define VFS_O_APPEND    0x0400

/* Seek whence */
#define VFS_SEEK_SET    0
#define VFS_SEEK_CUR    1
#define VFS_SEEK_END    2

/* File permissions */
#define VFS_IRUSR   0400
#define VFS_IWUSR   0200
#define VFS_IXUSR   0100
#define VFS_IRGRP   0040
#define VFS_IWGRP   0020
#define VFS_IXGRP   0010
#define VFS_IROTH   0004
#define VFS_IWOTH   0002
#define VFS_IXOTH   0001

/* ============================================================
 * VFS Structures
 * ============================================================ */
struct vfs_node;
struct vfs_mount;
struct vfs_file;
struct vfs_dirent;

struct vfs_ops {
    /* Node operations */
    int (*open)(struct vfs_node *node, struct vfs_file *file, int flags);
    int (*close)(struct vfs_file *file);
    int64_t (*read)(struct vfs_file *file, void *buf, size_t count, off_t offset);
    int64_t (*write)(struct vfs_file *file, const void *buf, size_t count, off_t offset);
    int64_t (*lseek)(struct vfs_file *file, off_t offset, int whence);
    int (*getdents)(struct vfs_file *file, struct vfs_dirent *dirent, size_t count);
    int (*ioctl)(struct vfs_file *file, unsigned long request, void *arg);
    int (*mmap)(struct vfs_file *file, void *addr, size_t len, int prot, int flags, off_t offset);

    /* Directory operations */
    int (*mkdir)(struct vfs_node *parent, const char *name, int mode);
    int (*rmdir)(struct vfs_node *parent, const char *name);
    int (*unlink)(struct vfs_node *parent, const char *name);
    int (*rename)(struct vfs_node *old_parent, const char *old_name,
                  struct vfs_node *new_parent, const char *new_name);
    struct vfs_node *(*lookup)(struct vfs_node *parent, const char *name);
    int (*create)(struct vfs_node *parent, const char *name, int mode);
    int (*symlink)(struct vfs_node *parent, const char *name, const char *target);
    int (*readlink)(struct vfs_node *node, char *buf, size_t buflen);

    /* Inode operations */
    int (*getattr)(struct vfs_node *node, struct vfs_stat *stat);
    int (*setattr)(struct vfs_node *node, struct vfs_stat *stat);
};

struct vfs_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

struct vfs_node {
    char name[VFS_NAME_MAX + 1];
    uint32_t type;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t inode;
    struct vfs_mount *mount;
    struct vfs_ops *ops;
    void *private_data;
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
    uint32_t ref_count;
    bool mounted;
};

struct vfs_file {
    struct vfs_node *node;
    off_t offset;
    int flags;
    void *private_data;
};

struct vfs_dirent {
    uint64_t d_ino;
    uint64_t d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[VFS_NAME_MAX + 1];
};

struct vfs_mount {
    struct vfs_node *root;
    struct vfs_ops *ops;
    void *private_data;
    struct vfs_mount *next;
    char name[32];
};

/* ============================================================
 * VFS API
 * ============================================================ */
void vfs_init(void);
struct vfs_node *vfs_root(void);
int vfs_mount(const char *path, struct vfs_ops *ops, void *private_data, const char *name);
int vfs_unmount(const char *path);

struct vfs_node *vfs_create_node(struct vfs_node *parent, const char *name,
                                  uint32_t type, uint32_t mode, struct vfs_ops *ops);
void vfs_destroy_node(struct vfs_node *node);
struct vfs_node *vfs_lookup(struct vfs_node *parent, const char *name);
struct vfs_node *vfs_traverse(const char *path);

int vfs_open(const char *path, int flags, struct vfs_file **file);
int vfs_close(struct vfs_file *file);
int64_t vfs_read(struct vfs_file *file, void *buf, size_t count);
int64_t vfs_write(struct vfs_file *file, const void *buf, size_t count);
int64_t vfs_lseek(struct vfs_file *file, off_t offset, int whence);
int vfs_getdents(struct vfs_file *file, struct vfs_dirent *dirent, size_t count);
int vfs_ioctl(struct vfs_file *file, unsigned long request, void *arg);

int vfs_mkdir(const char *path, int mode);
int vfs_rmdir(const char *path);
int vfs_unlink(const char *path);
int vfs_rename(const char *old_path, const char *new_path);
int vfs_stat(const char *path, struct vfs_stat *stat);
int vfs_symlink(const char *target, const char *linkpath);
int vfs_readlink(const char *path, char *buf, size_t buflen);

/* ============================================================
 * Filesystem Registration
 * ============================================================ */
struct fs_type {
    const char *name;
    int (*mount)(struct vfs_mount *mount, const char *source, void *data);
    void (*unmount)(struct vfs_mount *mount);
    struct fs_type *next;
};

int vfs_register_fs(struct fs_type *fs);
int vfs_unregister_fs(struct fs_type *fs);
struct fs_type *vfs_find_fs(const char *name);

#endif /* _VFS_H */