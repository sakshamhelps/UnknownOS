/*
 * Unknown OS - RAM Filesystem
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * RAMFS State
 * ============================================================ */
static struct ramfs_node *ramfs_root_node = NULL;
static struct vfs_node *ramfs_vfs_root = NULL;
static struct vfs_ops ramfs_ops;
static uint64_t ramfs_next_inode = 1;

/* ============================================================
 * RAMFS Node Operations
 * ============================================================ */
static int ramfs_open(struct vfs_node *node, struct vfs_file *file, int flags) {
    struct ramfs_node *rn = (struct ramfs_node *)node->private_data;
    if (!rn) return -1;
    file->private_data = rn;
    return 0;
}

static int ramfs_close(struct vfs_file *file) {
    return 0;
}

static int64_t ramfs_read(struct vfs_file *file, void *buf, size_t count, off_t offset) {
    struct ramfs_node *rn = (struct ramfs_node *)file->private_data;
    if (!rn || rn->type != VFS_TYPE_REGULAR || !rn->data) return 0;
    if (offset >= rn->size) return 0;

    size_t to_read = count;
    if (offset + to_read > rn->size) to_read = rn->size - offset;

    memcpy(buf, (uint8_t *)rn->data + offset, to_read);
    return to_read;
}

static int64_t ramfs_write(struct vfs_file *file, const void *buf, size_t count, off_t offset) {
    struct ramfs_node *rn = (struct ramfs_node *)file->private_data;
    if (!rn || rn->type != VFS_TYPE_REGULAR) return -1;

    size_t new_size = offset + count;
    if (new_size > rn->size) {
        void *new_data = krealloc(rn->data, new_size);
        if (!new_data) return -1;
        rn->data = new_data;
        rn->size = new_size;
        node->size = new_size;  /* Update VFS node too */
    }

    memcpy((uint8_t *)rn->data + offset, buf, count);
    rn->mtime = timer_get_ticks();
    return count;
}

static int64_t ramfs_lseek(struct vfs_file *file, off_t offset, int whence) {
    struct ramfs_node *rn = (struct ramfs_node *)file->private_data;
    if (!rn) return -1;

    off_t new_offset;
    switch (whence) {
        case VFS_SEEK_SET: new_offset = offset; break;
        case VFS_SEEK_CUR: new_offset = file->offset + offset; break;
        case VFS_SEEK_END: new_offset = rn->size + offset; break;
        default: return -1;
    }
    if (new_offset < 0) new_offset = 0;
    if (new_offset > rn->size) new_offset = rn->size;
    return new_offset;
}

static int ramfs_getdents(struct vfs_file *file, struct vfs_dirent *dirent, size_t count) {
    struct ramfs_node *rn = (struct ramfs_node *)file->private_data;
    if (!rn || rn->type != VFS_TYPE_DIRECTORY) return -1;

    static struct ramfs_node *next_child = NULL;

    if (file->offset == 0) {
        /* First call - return "." */
        dirent->d_ino = rn->inode;
        dirent->d_type = VFS_TYPE_DIRECTORY;
        strcpy(dirent->d_name, ".");
        dirent->d_reclen = sizeof(struct vfs_dirent);
        next_child = rn->children;
        file->offset++;
        return sizeof(struct vfs_dirent);
    } else if (file->offset == 1) {
        /* Second call - return ".." */
        dirent->d_ino = rn->parent ? rn->parent->inode : rn->inode;
        dirent->d_type = VFS_TYPE_DIRECTORY;
        strcpy(dirent->d_name, "..");
        dirent->d_reclen = sizeof(struct vfs_dirent);
        file->offset++;
        return sizeof(struct vfs_dirent);
    }

    /* Return children */
    while (next_child) {
        dirent->d_ino = next_child->inode;
        dirent->d_type = next_child->type;
        strcpy(dirent->d_name, next_child->name);
        dirent->d_reclen = sizeof(struct vfs_dirent);
        next_child = next_child->next;
        file->offset++;
        return sizeof(struct vfs_dirent);
    }

    return 0;  /* End of directory */
}

static int ramfs_getattr(struct vfs_node *node, struct vfs_stat *stat) {
    struct ramfs_node *rn = (struct ramfs_node *)node->private_data;
    memset(stat, 0, sizeof(struct vfs_stat));
    stat->st_ino = node->inode;
    stat->st_mode = (rn->type == VFS_TYPE_DIRECTORY ? 0040000 : 0100000) | rn->mode;
    stat->st_nlink = 1;
    stat->st_size = rn->size;
    stat->st_atime = rn->atime;
    stat->st_mtime = rn->mtime;
    stat->st_ctime = rn->ctime;
    return 0;
}

static int ramfs_mkdir(struct vfs_node *parent, const char *name, int mode) {
    struct ramfs_node *parent_rn = (struct ramfs_node *)parent->private_data;
    if (!parent_rn || parent_rn->type != VFS_TYPE_DIRECTORY) return -1;

    struct ramfs_node *new_rn = ramfs_create_node(parent_rn, name, VFS_TYPE_DIRECTORY, mode);
    if (!new_rn) return -1;

    struct vfs_node *node = vfs_create_node(parent, name, VFS_TYPE_DIRECTORY, mode, &ramfs_ops);
    node->private_data = new_rn;
    new_rn->inode = node->inode;
    return 0;
}

static int ramfs_rmdir(struct vfs_node *parent, const char *name) {
    /* TODO: Implement */
    return -1;
}

static int ramfs_unlink(struct vfs_node *parent, const char *name) {
    /* TODO: Implement */
    return -1;
}

static struct vfs_node *ramfs_lookup(struct vfs_node *parent, const char *name) {
    struct ramfs_node *parent_rn = (struct ramfs_node *)parent->private_data;
    if (!parent_rn) return NULL;

    for (struct ramfs_node *child = parent_rn->children; child; child = child->next) {
        if (strcmp(child->name, name) == 0) {
            struct vfs_node *node = vfs_create_node(parent, name, child->type, child->mode, &ramfs_ops);
            node->private_data = child;
            child->inode = node->inode;
            node->size = child->size;
            return node;
        }
    }
    return NULL;
}

static int ramfs_create(struct vfs_node *parent, const char *name, int mode) {
    struct ramfs_node *parent_rn = (struct ramfs_node *)parent->private_data;
    if (!parent_rn || parent_rn->type != VFS_TYPE_DIRECTORY) return -1;

    struct ramfs_node *new_rn = ramfs_create_node(parent_rn, name, VFS_TYPE_REGULAR, mode);
    if (!new_rn) return -1;

    struct vfs_node *node = vfs_create_node(parent, name, VFS_TYPE_REGULAR, mode, &ramfs_ops);
    node->private_data = new_rn;
    new_rn->inode = node->inode;
    return 0;
}

/* ============================================================
 * RAMFS Node Management
 * ============================================================ */
struct ramfs_node *ramfs_create_node(struct ramfs_node *parent, const char *name,
                                      uint32_t type, uint32_t mode) {
    struct ramfs_node *node = kmalloc(sizeof(struct ramfs_node));
    if (!node) return NULL;

    memset(node, 0, sizeof(struct ramfs_node));
    strncpy(node->name, name, VFS_NAME_MAX);
    node->type = type;
    node->mode = mode;
    node->inode = ramfs_next_inode++;
    node->parent = parent;

    if (type == VFS_TYPE_DIRECTORY) {
        node->data = NULL;  /* Children list */
    } else {
        node->data = NULL;  /* File content */
    }

    /* Add to parent's children */
    if (parent) {
        node->next = parent->children;
        parent->children = node;
    }

    node->atime = node->mtime = node->ctime = timer_get_ticks();
    return node;
}

void ramfs_destroy_node(struct ramfs_node *node) {
    if (!node) return;

    /* Free children */
    struct ramfs_node *child = node->children;
    while (child) {
        struct ramfs_node *next = child->next;
        ramfs_destroy_node(child);
        child = next;
    }

    /* Free file data */
    if (node->data) {
        kfree(node->data);
    }

    /* Remove from parent */
    if (node->parent) {
        struct ramfs_node **pp = &node->parent->children;
        while (*pp && *pp != node) {
            pp = &(*pp)->next;
        }
        if (*pp) *pp = node->next;
    }

    kfree(node);
}

/* ============================================================
 * RAMFS Initialization
 * ============================================================ */
void ramfs_init(void) {
    serial_puts("[RAMFS] Initializing...\n");

    /* Set up operations */
    ramfs_ops.open = ramfs_open;
    ramfs_ops.close = ramfs_close;
    ramfs_ops.read = ramfs_read;
    ramfs_ops.write = ramfs_write;
    ramfs_ops.lseek = ramfs_lseek;
    ramfs_ops.getdents = ramfs_getdents;
    ramfs_ops.getattr = ramfs_getattr;
    ramfs_ops.mkdir = ramfs_mkdir;
    ramfs_ops.rmdir = ramfs_rmdir;
    ramfs_ops.unlink = ramfs_unlink;
    ramfs_ops.lookup = ramfs_lookup;
    ramfs_ops.create = ramfs_create;

    /* Create root */
    ramfs_root_node = ramfs_create_node(NULL, "/", VFS_TYPE_DIRECTORY, 0755);

    ramfs_vfs_root = vfs_create_node(NULL, "/", VFS_TYPE_DIRECTORY, 0755, &ramfs_ops);
    ramfs_vfs_root->private_data = ramfs_root_node;
    ramfs_root_node->inode = ramfs_vfs_root->inode;

    /* Create some default directories */
    ramfs_create_node(ramfs_root_node, "bin", VFS_TYPE_DIRECTORY, 0755);
    ramfs_create_node(ramfs_root_node, "etc", VFS_TYPE_DIRECTORY, 0755);
    ramfs_create_node(ramfs_root_node, "dev", VFS_TYPE_DIRECTORY, 0755);
    ramfs_create_node(ramfs_root_node, "tmp", VFS_TYPE_DIRECTORY, 0777);
    ramfs_create_node(ramfs_root_node, "home", VFS_TYPE_DIRECTORY, 0755);
    ramfs_create_node(ramfs_root_node, "root", VFS_TYPE_DIRECTORY, 0700);
    ramfs_create_node(ramfs_root_node, "proc", VFS_TYPE_DIRECTORY, 0555);
    ramfs_create_node(ramfs_root_node, "sys", VFS_TYPE_DIRECTORY, 0555);

    serial_puts("[RAMFS] Initialized\n");
}

struct vfs_node *ramfs_get_root(void) {
    return ramfs_vfs_root;
}

struct vfs_ops *ramfs_get_ops(void) {
    return &ramfs_ops;
}