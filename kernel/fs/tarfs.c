/*
 * Unknown OS - TAR Filesystem (for initrd)
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * TARFS State
 * ============================================================ */
static uint8_t *tarfs_archive = NULL;
static size_t tarfs_size = 0;
static struct vfs_node *tarfs_root = NULL;
static struct vfs_ops tarfs_ops;

/* ============================================================
 * TAR Helpers
 * ============================================================ */
static uint32_t tar_oct_to_uint(const char *str, size_t len) {
    uint32_t val = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] >= '0' && str[i] <= '7') {
            val = (val << 3) | (str[i] - '0');
        }
    }
    return val;
}

static size_t tar_str_to_size(const char *str, size_t len) {
    return tar_oct_to_uint(str, len);
}

static bool tar_checksum_valid(struct tar_header *hdr) {
    unsigned sum = 0;
    unsigned char *p = (unsigned char *)hdr;
    for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
        if (i >= 148 && i < 156) {
            sum += ' ';  /* Treat checksum field as spaces */
        } else {
            sum += p[i];
        }
    }
    uint32_t chksum = tar_oct_to_uint(hdr->chksum, 8);
    return sum == chksum;
}

static struct tar_header *tar_next_header(struct tar_header *hdr) {
    size_t size = tar_str_to_size(hdr->size, 12);
    size_t blocks = (size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;
    return (struct tar_header *)((uint8_t *)hdr + TAR_BLOCK_SIZE + blocks * TAR_BLOCK_SIZE);
}

static char *tar_get_name(struct tar_header *hdr) {
    static char name[TAR_NAME_SIZE + TAR_PREFIX_SIZE + 2];
    if (hdr->prefix[0]) {
        strncpy(name, hdr->prefix, TAR_PREFIX_SIZE);
        name[TAR_PREFIX_SIZE] = '/';
        strncpy(name + TAR_PREFIX_SIZE + 1, hdr->name, TAR_NAME_SIZE);
    } else {
        strncpy(name, hdr->name, TAR_NAME_SIZE);
        name[TAR_NAME_SIZE] = '\0';
    }
    return name;
}

/* ============================================================
 * TARFS Node Operations
 * ============================================================ */
static int tarfs_open(struct vfs_node *node, struct vfs_file *file, int flags) {
    struct tar_file *tf = (struct tar_file *)node->private_data;
    if (!tf) return -1;
    file->private_data = tf;
    return 0;
}

static int tarfs_close(struct vfs_file *file) {
    return 0;
}

static int64_t tarfs_read(struct vfs_file *file, void *buf, size_t count, off_t offset) {
    struct tar_file *tf = (struct tar_file *)file->private_data;
    if (!tf || offset >= tf->size) return 0;

    size_t to_read = count;
    if (offset + to_read > tf->size) to_read = tf->size - offset;

    memcpy(buf, tf->data + offset, to_read);
    return to_read;
}

static int64_t tarfs_write(struct vfs_file *file, const void *buf, size_t count, off_t offset) {
    return -1;  /* Read-only */
}

static int64_t tarfs_lseek(struct vfs_file *file, off_t offset, int whence) {
    struct tar_file *tf = (struct tar_file *)file->private_data;
    if (!tf) return -1;

    off_t new_offset;
    switch (whence) {
        case VFS_SEEK_SET: new_offset = offset; break;
        case VFS_SEEK_CUR: new_offset = file->offset + offset; break;
        case VFS_SEEK_END: new_offset = tf->size + offset; break;
        default: return -1;
    }
    if (new_offset < 0) new_offset = 0;
    if (new_offset > tf->size) new_offset = tf->size;
    return new_offset;
}

static int tarfs_getdents(struct vfs_file *file, struct vfs_dirent *dirent, size_t count) {
    /* Simplified - only return "." and ".." for now */
    static int index = 0;
    if (index == 0) {
        dirent->d_ino = 1;
        dirent->d_type = VFS_TYPE_DIRECTORY;
        strcpy(dirent->d_name, ".");
        dirent->d_reclen = sizeof(struct vfs_dirent);
        index++;
        return sizeof(struct vfs_dirent);
    } else if (index == 1) {
        dirent->d_ino = 1;
        dirent->d_type = VFS_TYPE_DIRECTORY;
        strcpy(dirent->d_name, "..");
        dirent->d_reclen = sizeof(struct vfs_dirent);
        index++;
        return sizeof(struct vfs_dirent);
    }
    return 0;
}

static int tarfs_getattr(struct vfs_node *node, struct vfs_stat *stat) {
    struct tar_file *tf = (struct tar_file *)node->private_data;
    memset(stat, 0, sizeof(struct vfs_stat));
    stat->st_ino = node->inode;
    stat->st_mode = (node->type == VFS_TYPE_DIRECTORY ? 0040000 : 0100000) | node->mode;
    stat->st_nlink = 1;
    if (tf) stat->st_size = tf->size;
    return 0;
}

static struct vfs_node *tarfs_lookup(struct vfs_node *parent, const char *name) {
    /* Linear search through archive - not efficient but works for initrd */
    struct tar_header *hdr = (struct tar_header *)tarfs_archive;
    struct vfs_node *parent_node = (struct vfs_node *)parent->private_data;

    while (hdr->name[0] && (uint8_t *)hdr < tarfs_archive + tarfs_size) {
        if (!tar_checksum_valid(hdr)) break;

        char *full_name = tar_get_name(hdr);
        size_t fn_len = strlen(full_name);

        /* Check if this file is under parent */
        if (parent_node) {
            struct tar_file *parent_tf = (struct tar_file *)parent_node->private_data;
            if (parent_tf) {
                char *parent_name = tar_get_name(parent_tf->header);
                size_t pn_len = strlen(parent_name);
                if (pn_len > 0 && parent_name[pn_len - 1] != '/') pn_len++;
                if (strncmp(full_name, parent_name, pn_len) == 0) {
                    const char *base = full_name + pn_len;
                    if (strcmp(base, name) == 0) {
                        /* Found it - create node */
                        struct tar_file *tf = kmalloc(sizeof(struct tar_file));
                        tf->header = hdr;
                        tf->data = (uint8_t *)hdr + TAR_BLOCK_SIZE;
                        tf->size = tar_str_to_size(hdr->size, 12);

                        struct vfs_node *node = vfs_create_node(parent, name,
                            (hdr->typeflag == TAR_TYPE_DIRECTORY) ? VFS_TYPE_DIRECTORY :
                            (hdr->typeflag == TAR_TYPE_SYMLINK) ? VFS_TYPE_SYMLINK : VFS_TYPE_REGULAR,
                            tar_oct_to_uint(hdr->mode, 8), &tarfs_ops);
                        node->private_data = tf;
                        node->size = tf->size;
                        return node;
                    }
                }
            }
        } else {
            /* Looking in root */
            if (strchr(full_name, '/') == NULL || (strchr(full_name, '/') == full_name + strlen(full_name) - 1)) {
                /* Top-level entry */
                if (strcmp(full_name, name) == 0 || (strcmp(full_name, name) == 0 && full_name[strlen(name)] == '/')) {
                    struct tar_file *tf = kmalloc(sizeof(struct tar_file));
                    tf->header = hdr;
                    tf->data = (uint8_t *)hdr + TAR_BLOCK_SIZE;
                    tf->size = tar_str_to_size(hdr->size, 12);

                    struct vfs_node *node = vfs_create_node(parent, name,
                        (hdr->typeflag == TAR_TYPE_DIRECTORY) ? VFS_TYPE_DIRECTORY :
                        (hdr->typeflag == TAR_TYPE_SYMLINK) ? VFS_TYPE_SYMLINK : VFS_TYPE_REGULAR,
                        tar_oct_to_uint(hdr->mode, 8), &tarfs_ops);
                    node->private_data = tf;
                    node->size = tf->size;
                    return node;
                }
            }
        }

        hdr = tar_next_header(hdr);
    }

    return NULL;
}

static int tarfs_readlink(struct vfs_node *node, char *buf, size_t buflen) {
    struct tar_file *tf = (struct tar_file *)node->private_data;
    if (!tf || tf->header->typeflag != TAR_TYPE_SYMLINK) return -1;
    strncpy(buf, tf->header->linkname, buflen - 1);
    buf[buflen - 1] = '\0';
    return 0;
}

/* ============================================================
 * TARFS Initialization
 * ============================================================ */
int tarfs_init(uint8_t *archive, size_t size) {
    serial_puts("[TARFS] Initializing...\n");

    tarfs_archive = archive;
    tarfs_size = size;

    /* Set up operations */
    tarfs_ops.open = tarfs_open;
    tarfs_ops.close = tarfs_close;
    tarfs_ops.read = tarfs_read;
    tarfs_ops.write = tarfs_write;
    tarfs_ops.lseek = tarfs_lseek;
    tarfs_ops.getdents = tarfs_getdents;
    tarfs_ops.getattr = tarfs_getattr;
    tarfs_ops.lookup = tarfs_lookup;
    tarfs_ops.readlink = tarfs_readlink;

    /* Create root node */
    struct tar_file *root_tf = kmalloc(sizeof(struct tar_file));
    root_tf->header = NULL;
    root_tf->data = NULL;
    root_tf->size = 0;

    tarfs_root = vfs_create_node(NULL, "/", VFS_TYPE_DIRECTORY, 0755, &tarfs_ops);
    tarfs_root->private_data = root_tf;

    /* Scan archive to count files */
    struct tar_header *hdr = (struct tar_header *)tarfs_archive;
    int file_count = 0;
    while (hdr->name[0] && (uint8_t *)hdr < tarfs_archive + tarfs_size) {
        if (!tar_checksum_valid(hdr)) break;
        file_count++;
        hdr = tar_next_header(hdr);
    }

    serial_printf("[TARFS] Found %d files in initrd\n", file_count);
    return 0;
}

struct vfs_node *tarfs_get_root(void) {
    return tarfs_root;
}

struct vfs_ops *tarfs_get_ops(void) {
    return &tarfs_ops;
}

struct tar_file *tarfs_find_file(const char *path) {
    /* Linear search */
    struct tar_header *hdr = (struct tar_header *)tarfs_archive;
    while (hdr->name[0] && (uint8_t *)hdr < tarfs_archive + tarfs_size) {
        if (!tar_checksum_valid(hdr)) break;
        char *name = tar_get_name(hdr);
        if (strcmp(name, path) == 0) {
            struct tar_file *tf = kmalloc(sizeof(struct tar_file));
            tf->header = hdr;
            tf->data = (uint8_t *)hdr + TAR_BLOCK_SIZE;
            tf->size = tar_str_to_size(hdr->size, 12);
            return tf;
        }
        hdr = tar_next_header(hdr);
    }
    return NULL;
}