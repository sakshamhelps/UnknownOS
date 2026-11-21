/*
 * Unknown OS - Virtual File System (VFS)
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * VFS State
 * ============================================================ */
static struct vfs_node *vfs_root_node = NULL;
static struct vfs_mount *vfs_mounts = NULL;
static struct fs_type *vfs_filesystems = NULL;
static uint64_t vfs_next_inode = 1;

/* ============================================================
 * VFS Initialization
 * ============================================================ */
void vfs_init(void) {
    serial_puts("[VFS] Initializing...\n");

    /* Create root node */
    vfs_root_node = kmalloc(sizeof(struct vfs_node));
    memset(vfs_root_node, 0, sizeof(struct vfs_node));
    strcpy(vfs_root_node->name, "/");
    vfs_root_node->type = VFS_TYPE_DIRECTORY;
    vfs_root_node->mode = 0755;
    vfs_root_node->inode = vfs_next_inode++;
    vfs_root_node->parent = vfs_root_node;
    vfs_root_node->ref_count = 1;

    serial_puts("[VFS] VFS initialized\n");
}

struct vfs_node *vfs_root(void) {
    return vfs_root_node;
}

/* ============================================================
 * Node Management
 * ============================================================ */
struct vfs_node *vfs_create_node(struct vfs_node *parent, const char *name,
                                  uint32_t type, uint32_t mode, struct vfs_ops *ops) {
    struct vfs_node *node = kmalloc(sizeof(struct vfs_node));
    if (!node) return NULL;

    memset(node, 0, sizeof(struct vfs_node));
    strncpy(node->name, name, VFS_NAME_MAX);
    node->type = type;
    node->mode = mode;
    node->inode = vfs_next_inode++;
    node->ops = ops;
    node->parent = parent;
    node->ref_count = 1;

    /* Add to parent's children list */
    if (parent) {
        node->next = parent->children;
        parent->children = node;
    }

    return node;
}

void vfs_destroy_node(struct vfs_node *node) {
    if (!node) return;

    if (--node->ref_count > 0) return;

    /* Free children */
    struct vfs_node *child = node->children;
    while (child) {
        struct vfs_node *next = child->next;
        vfs_destroy_node(child);
        child = next;
    }

    /* Remove from parent's children list */
    if (node->parent && node->parent != node) {
        struct vfs_node **pp = &node->parent->children;
        while (*pp && *pp != node) {
            pp = &(*pp)->next;
        }
        if (*pp) *pp = node->next;
    }

    kfree(node);
}

struct vfs_node *vfs_lookup(struct vfs_node *parent, const char *name) {
    if (!parent || !parent->ops || !parent->ops->lookup) {
        return NULL;
    }
    return parent->ops->lookup(parent, name);
}

/* ============================================================
 * Path Traversal
 * ============================================================ */
struct vfs_node *vfs_traverse(const char *path) {
    if (!path || *path != '/') return NULL;

    struct vfs_node *current = vfs_root_node;

    /* Skip leading slashes */
    while (*path == '/') path++;

    if (*path == '\0') return current;  /* Root */

    char name[VFS_NAME_MAX + 1];
    while (*path) {
        /* Extract path component */
        size_t i = 0;
        while (*path && *path != '/' && i < VFS_NAME_MAX) {
            name[i++] = *path++;
        }
        name[i] = '\0';

        if (i == 0) {
            while (*path == '/') path++;
            continue;
        }

        /* Look up component */
        struct vfs_node *next = vfs_lookup(current, name);
        if (!next) return NULL;

        vfs_destroy_node(current);  /* Decrease ref */
        current = next;

        /* Skip slashes */
        while (*path == '/') path++;
    }

    return current;
}

/* ============================================================
 * Mount Management
 * ============================================================ */
int vfs_mount(const char *path, struct vfs_ops *ops, void *private_data, const char *name) {
    struct vfs_node *mount_point = vfs_traverse(path);
    if (!mount_point) return -1;  /* ENOENT */

    if (mount_point->mounted) return -1;  /* EBUSY */

    struct vfs_mount *mount = kmalloc(sizeof(struct vfs_mount));
    if (!mount) return -1;

    memset(mount, 0, sizeof(struct vfs_mount));
    mount->ops = ops;
    mount->private_data = private_data;
    strncpy(mount->name, name ? name : "unknown", 31);

    /* Get root of filesystem */
    if (ops->lookup) {
        mount->root = ops->lookup(mount_point, ".");
    } else {
        mount->root = mount_point;
    }

    mount->root->mount = mount;
    mount->root->mounted = true;
    mount_point->mounted = true;

    /* Add to mount list */
    mount->next = vfs_mounts;
    vfs_mounts = mount;

    serial_printf("[VFS] Mounted '%s' at '%s'\n", name, path);
    return 0;
}

int vfs_unmount(const char *path) {
    struct vfs_node *mount_point = vfs_traverse(path);
    if (!mount_point) return -1;

    struct vfs_mount **pm = &vfs_mounts;
    while (*pm) {
        if ((*pm)->root == mount_point || (*pm)->root->parent == mount_point) {
            struct vfs_mount *m = *pm;
            *pm = m->next;

            mount_point->mounted = false;
            if (m->root) m->root->mounted = false;

            if (m->ops && m->ops->unmount) {
                m->ops->unmount(m);
            }
            kfree(m);
            return 0;
        }
        pm = &(*pm)->next;
    }

    return -1;
}

/* ============================================================
 * File Operations
 * ============================================================ */
int vfs_open(const char *path, int flags, struct vfs_file **file) {
    struct vfs_node *node = vfs_traverse(path);
    if (!node) {
        if (flags & VFS_O_CREAT) {
            /* TODO: Create file */
            return -1;
        }
        return -1;  /* ENOENT */
    }

    struct vfs_file *f = kmalloc(sizeof(struct vfs_file));
    if (!f) return -1;

    memset(f, 0, sizeof(struct vfs_file));
    f->node = node;
    f->flags = flags;
    f->offset = 0;
    node->ref_count++;

    if (node->ops && node->ops->open) {
        int ret = node->ops->open(node, f, flags);
        if (ret < 0) {
            kfree(f);
            return ret;
        }
    }

    *file = f;
    return 0;
}

int vfs_close(struct vfs_file *file) {
    if (!file) return -1;

    if (file->node->ops && file->node->ops->close) {
        file->node->ops->close(file);
    }

    vfs_destroy_node(file->node);
    kfree(file);
    return 0;
}

int64_t vfs_read(struct vfs_file *file, void *buf, size_t count) {
    if (!file || !file->node->ops || !file->node->ops->read) return -1;
    int64_t ret = file->node->ops->read(file, buf, count, file->offset);
    if (ret > 0) file->offset += ret;
    return ret;
}

int64_t vfs_write(struct vfs_file *file, const void *buf, size_t count) {
    if (!file || !file->node->ops || !file->node->ops->write) return -1;
    int64_t ret = file->node->ops->write(file, buf, count, file->offset);
    if (ret > 0) file->offset += ret;
    return ret;
}

int64_t vfs_lseek(struct vfs_file *file, off_t offset, int whence) {
    if (!file || !file->node->ops || !file->node->ops->lseek) return -1;
    int64_t ret = file->node->ops->lseek(file, offset, whence);
    if (ret >= 0) file->offset = ret;
    return ret;
}

int vfs_getdents(struct vfs_file *file, struct vfs_dirent *dirent, size_t count) {
    if (!file || !file->node->ops || !file->node->ops->getdents) return -1;
    return file->node->ops->getdents(file, dirent, count);
}

int vfs_ioctl(struct vfs_file *file, unsigned long request, void *arg) {
    if (!file || !file->node->ops || !file->node->ops->ioctl) return -1;
    return file->node->ops->ioctl(file, request, arg);
}

/* ============================================================
 * Directory Operations
 * ============================================================ */
int vfs_mkdir(const char *path, int mode) {
    /* Find parent directory */
    char *path_copy = kmalloc(strlen(path) + 1);
    strcpy(path_copy, path);

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash || last_slash == path_copy) {
        kfree(path_copy);
        return -1;
    }

    *last_slash = '\0';
    struct vfs_node *parent = vfs_traverse(path_copy);
    char *name = last_slash + 1;
    kfree(path_copy);

    if (!parent || !parent->ops || !parent->ops->mkdir) return -1;
    return parent->ops->mkdir(parent, name, mode);
}

int vfs_rmdir(const char *path) {
    char *path_copy = kmalloc(strlen(path) + 1);
    strcpy(path_copy, path);

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash || last_slash == path_copy) {
        kfree(path_copy);
        return -1;
    }

    *last_slash = '\0';
    struct vfs_node *parent = vfs_traverse(path_copy);
    char *name = last_slash + 1;
    kfree(path_copy);

    if (!parent || !parent->ops || !parent->ops->rmdir) return -1;
    return parent->ops->rmdir(parent, name);
}

int vfs_unlink(const char *path) {
    char *path_copy = kmalloc(strlen(path) + 1);
    strcpy(path_copy, path);

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash || last_slash == path_copy) {
        kfree(path_copy);
        return -1;
    }

    *last_slash = '\0';
    struct vfs_node *parent = vfs_traverse(path_copy);
    char *name = last_slash + 1;
    kfree(path_copy);

    if (!parent || !parent->ops || !parent->ops->unlink) return -1;
    return parent->ops->unlink(parent, name);
}

int vfs_rename(const char *old_path, const char *new_path) {
    /* TODO: Implement rename */
    return -1;
}

/* ============================================================
 * Stat Operations
 * ============================================================ */
int vfs_stat(const char *path, struct vfs_stat *stat) {
    struct vfs_node *node = vfs_traverse(path);
    if (!node) return -1;

    if (node->ops && node->ops->getattr) {
        return node->ops->getattr(node, stat);
    }

    /* Default stat */
    memset(stat, 0, sizeof(struct vfs_stat));
    stat->st_ino = node->inode;
    stat->st_mode = (node->type == VFS_TYPE_DIRECTORY ? 0040000 : 0100000) | node->mode;
    stat->st_nlink = 1;
    stat->st_size = node->size;
    return 0;
}

int vfs_symlink(const char *target, const char *linkpath) {
    /* TODO: Implement symlink */
    return -1;
}

int vfs_readlink(const char *path, char *buf, size_t buflen) {
    struct vfs_node *node = vfs_traverse(path);
    if (!node || node->type != VFS_TYPE_SYMLINK) return -1;

    if (node->ops && node->ops->readlink) {
        return node->ops->readlink(node, buf, buflen);
    }
    return -1;
}

/* ============================================================
 * Filesystem Registration
 * ============================================================ */
int vfs_register_fs(struct fs_type *fs) {
    fs->next = vfs_filesystems;
    vfs_filesystems = fs;
    serial_printf("[VFS] Registered filesystem: %s\n", fs->name);
    return 0;
}

int vfs_unregister_fs(struct fs_type *fs) {
    struct fs_type **p = &vfs_filesystems;
    while (*p) {
        if (*p == fs) {
            *p = fs->next;
            return 0;
        }
        p = &(*p)->next;
    }
    return -1;
}

struct fs_type *vfs_find_fs(const char *name) {
    for (struct fs_type *fs = vfs_filesystems; fs; fs = fs->next) {
        if (strcmp(fs->name, name) == 0) return fs;
    }
    return NULL;
}