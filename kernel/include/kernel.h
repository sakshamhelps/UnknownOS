/*
 * Unknown OS - Kernel Header
 * India's First Independent Operating System
 * Author: byrex <byrex.me@proton.me>
 */

#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

/* ============================================================
 * Basic Types
 * ============================================================ */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uintptr_t usize;
typedef intptr_t  isize;

#define NULL ((void*)0)
#define KERNEL_BASE 0xFFFF800000000000UL  /* Higher-half kernel base */

/* ============================================================
 * Limine Boot Protocol Structures
 * ============================================================ */
struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry *entries;
};

struct limine_framebuffer {
    void *address;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift;
    uint8_t green_mask_size, green_mask_shift;
    uint8_t blue_mask_size, blue_mask_shift;
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_rsdp_response {
    uint64_t revision;
    void *address;
};

struct limine_smp_response {
    uint64_t revision;
    uint64_t flags;
    uint64_t cpu_count;
    struct limine_smp_info **cpus;
};

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

/* Limine Module Structure */
struct limine_module {
    uint64_t base;
    uint64_t size;
    char cmdline[256];
};

struct limine_module_response {
    uint64_t revision;
    uint64_t module_count;
    struct limine_module **modules;
};

/* Boot info passed to kernel_main */
struct boot_info {
    struct limine_memmap_response *memmap;
    struct limine_framebuffer_response *framebuffer;
    struct limine_rsdp_response *rsdp;
    struct limine_smp_response *smp;
    struct limine_hhdm_response *hhdm;
    struct limine_module_response *modules;
    uint64_t kernel_physical_base;
    uint64_t kernel_virtual_base;
};

/* ============================================================
 * GDT / TSS
 * ============================================================ */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

/* ============================================================
 * IDT
 * ============================================================ */
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define IDT_MAX 256

/* ============================================================
 * Physical Memory Manager (PMM)
 * ============================================================ */
#define PAGE_SIZE 4096
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* Memory map types (Limine) */
#define LIMINE_MEMMAP_USABLE        0
#define LIMINE_MEMMAP_RESERVED      1
#define LIMINE_MEMMAP_ACPI_RECLAIM  2
#define LIMINE_MEMMAP_ACPI_NVS      3
#define LIMINE_MEMMAP_BAD_MEMORY    4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIM 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES 6
#define LIMINE_MEMMAP_FRAMEBUFFER   7

void pmm_init(struct boot_info *boot_info);
void *pmm_alloc_page(void);
void pmm_free_page(void *page);
uint64_t pmm_get_total_pages(void);
uint64_t pmm_get_free_pages(void);

/* ============================================================
 * Virtual Memory Manager (VMM)
 * ============================================================ */
#define VMM_FLAGS_PRESENT    (1ULL << 0)
#define VMM_FLAGS_WRITE      (1ULL << 1)
#define VMM_FLAGS_USER       (1ULL << 2)
#define VMM_FLAGS_WRITETHROUGH (1ULL << 3)
#define VMM_FLAGS_NOCACHE    (1ULL << 4)
#define VMM_FLAGS_ACCESSED   (1ULL << 5)
#define VMM_FLAGS_DIRTY      (1ULL << 6)
#define VMM_FLAGS_HUGE       (1ULL << 7)
#define VMM_FLAGS_GLOBAL     (1ULL << 8)
#define VMM_FLAGS_NX         (1ULL << 63)

void vmm_init(struct boot_info *boot_info);
void vmm_map_page(void *virt, void *phys, uint64_t flags);
void vmm_unmap_page(void *virt);
void *vmm_alloc_pages(size_t count, uint64_t flags);
void vmm_free_pages(void *virt, size_t count);
void *vmm_phys_to_virt(void *phys);
void *vmm_virt_to_phys(void *virt);

/* ============================================================
 * Kernel Heap
 * ============================================================ */
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t size);

/* ============================================================
 * Serial / Console
 * ============================================================ */
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_printf(const char *fmt, ...);

void vga_init(struct boot_info *boot_info);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_printf(const char *fmt, ...);
void vga_clear(void);
void vga_set_color(uint32_t fg, uint32_t bg);
void vga_sync_with_fb(void);

/* ============================================================
 * PS/2 Keyboard
 * ============================================================ */
void ps2_init(void);
char ps2_getchar(void);
bool ps2_has_char(void);

/* ============================================================
 * PS/2 Mouse
 * ============================================================ */
void ps2_mouse_init(void);
int32_t mouse_get_x(void);
int32_t mouse_get_y(void);
uint8_t mouse_get_buttons(void);
void mouse_set_position(int32_t x, int32_t y);
void mouse_set_sensitivity(float sens);

/* ============================================================
 * PCI
 * ============================================================ */
struct pci_device {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint32_t bar[6];
};

void pci_init(void);
struct pci_device *pci_find_device(uint16_t vendor, uint16_t device);
uint32_t pci_read_config(struct pci_device *dev, uint8_t offset, uint8_t size);
void pci_write_config(struct pci_device *dev, uint8_t offset, uint8_t size, uint32_t value);

/* ============================================================
 * APIC
 * ============================================================ */
void apic_init(void);
void lapic_init(void);
void ioapic_init(void);
void lapic_send_ipi(uint8_t vector, uint32_t destination);
void lapic_send_eoi(void);
void lapic_timer_init(uint32_t frequency_hz);
void lapic_timer_set_oneshot(uint32_t ticks);
void apic_register_irq_handler(uint8_t vector, void (*handler)(struct isr_regs *));
void apic_unregister_irq_handler(uint8_t vector);

/* ============================================================
 * Scheduler / Tasks
 * ============================================================ */
#define TASK_STACK_SIZE 0x4000  /* 16 KiB */

struct task_context {
    uint64_t rip, rflags;
    uint64_t rsp;
    uint64_t rbx, rbp, r12, r13, r14, r15;
};

/* Forward declaration for fd_table */
struct fd_table;

struct task {
    uint64_t pid;
    char name[32];
    struct task_context ctx;
    void *stack;
    uint64_t page_table;
    enum { TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_ZOMBIE } state;
    struct task *next;
    struct fd_table *fd_table;
};

void sched_init(void);
void sched_yield(void);
struct task *sched_create_task(void (*entry)(void), const char *name);
void sched_exit(int code);
struct task *sched_get_current(void);

/* ============================================================
 * Syscalls
 * ============================================================ */
#define SYSCALL_WRITE   0
#define SYSCALL_READ    1
#define SYSCALL_OPEN    2
#define SYSCALL_CLOSE   3
#define SYSCALL_EXIT    60
#define SYSCALL_FORK    57
#define SYSCALL_EXECVE  59

void syscall_init(void);
int64_t syscall_handler(int64_t num, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6);

/* ============================================================
 * VFS
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

struct vfs_node;
struct vfs_mount;
struct vfs_file;
struct vfs_dirent;
struct vfs_stat;
struct vfs_ops;
struct fs_type;

struct vfs_ops {
    int (*open)(struct vfs_node *node, struct vfs_file *file, int flags);
    int (*close)(struct vfs_file *file);
    int64_t (*read)(struct vfs_file *file, void *buf, size_t count, off_t offset);
    int64_t (*write)(struct vfs_file *file, const void *buf, size_t count, off_t offset);
    int64_t (*lseek)(struct vfs_file *file, off_t offset, int whence);
    int (*getdents)(struct vfs_file *file, struct vfs_dirent *dirent, size_t count);
    int (*ioctl)(struct vfs_file *file, unsigned long request, void *arg);
    int (*mmap)(struct vfs_file *file, void *addr, size_t len, int prot, int flags, off_t offset);

    int (*mkdir)(struct vfs_node *parent, const char *name, int mode);
    int (*rmdir)(struct vfs_node *parent, const char *name);
    int (*unlink)(struct vfs_node *parent, const char *name);
    int (*rename)(struct vfs_node *old_parent, const char *old_name,
                  struct vfs_node *new_parent, const char *new_name);
    struct vfs_node *(*lookup)(struct vfs_node *parent, const char *name);
    int (*create)(struct vfs_node *parent, const char *name, int mode);
    int (*symlink)(struct vfs_node *parent, const char *name, const char *target);
    int (*readlink)(struct vfs_node *node, char *buf, size_t buflen);

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

struct fs_type {
    const char *name;
    int (*mount)(struct vfs_mount *mount, const char *source, void *data);
    void (*unmount)(struct vfs_mount *mount);
    struct fs_type *next;
};

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

int vfs_register_fs(struct fs_type *fs);
int vfs_unregister_fs(struct fs_type *fs);
struct fs_type *vfs_find_fs(const char *name);

/* ============================================================
 * TARFS (Initrd)
 * ============================================================ */
#define TAR_BLOCK_SIZE    512
#define TAR_NAME_SIZE     100
#define TAR_PREFIX_SIZE   155

int tarfs_init(uint8_t *archive, size_t size);
struct vfs_node *tarfs_get_root(void);
struct vfs_ops *tarfs_get_ops(void);

/* ============================================================
 * ISR Registers
 * ============================================================ */
struct isr_regs {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t error_code, vector;
};

/* ============================================================
 * Timer
 * ============================================================ */
uint64_t timer_get_ticks(void);
void timer_sleep(uint64_t ms);

/* ============================================================
 * IRQ Handlers
 * ============================================================ */
void timer_irq_handler(struct isr_regs *regs);
void keyboard_irq_handler(struct isr_regs *regs);

/* ============================================================
 * Framebuffer Graphics
 * ============================================================ */
void fb_init(struct boot_info *boot_info);
bool fb_available(void);
uint32_t fb_width(void);
uint32_t fb_height(void);
uint32_t fb_pitch(void);
void fb_clear(uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);
void fb_draw_string_centered(uint32_t y, const char *str, uint32_t fg, uint32_t bg);
void fb_draw_char_to_buffer(uint32_t *buf, uint32_t pitch, int x, int y, char c, uint32_t fg, uint32_t bg);
void fb_fill_rect_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, uint8_t alpha);
void fb_draw_wallpaper(void);

/* ============================================================
 * GUI / Compositor
 * ============================================================ */
void gui_init(void);
void gui_shutdown(void);
void gui_main_loop(void);
void gui_process_events(void);
void gui_handle_mouse_move(int32_t x, int32_t y);
void gui_handle_mouse_down(int32_t x, int32_t y, uint32_t btn);
void gui_handle_mouse_up(int32_t x, int32_t y, uint32_t btn);
void gui_handle_mouse_scroll(int32_t x, int32_t y, int32_t delta);
void gui_handle_key_down(uint32_t keycode, uint32_t modifiers);
void gui_handle_key_up(uint32_t keycode, uint32_t modifiers);
void gui_handle_char(char c);
void lock_screen_show(void);
void lock_screen_hide(void);

/* ============================================================
 * RAMFS
 * ============================================================ */
void ramfs_init(void);
struct vfs_node *ramfs_get_root(void);
struct vfs_ops *ramfs_get_ops(void);

/* ============================================================
 * UNK Loader
 * ============================================================ */
int unk_load_and_exec(const char *path, char *const argv[], char *const envp[]);
void *unk_load(const char *path, uint64_t *entry_point);
int unk_validate(uint8_t *data, size_t size);
struct unk_header *unk_parse_header(uint8_t *data, size_t size);
int unk_apply_relocations(uint8_t *base, struct unk_header *hdr);
bool unk_check_capability(struct unk_header *hdr, uint64_t cap_type);

/* ============================================================
 * Kernel Entry
 * ============================================================ */
void kernel_main(struct boot_info *boot_info);
void kernel_panic(const char *msg, const char *file, int line);

#define PANIC(msg) kernel_panic(msg, __FILE__, __LINE__)

/* ============================================================
 * Utility
 * ============================================================ */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);

void *memmove(void *dest, const void *src, size_t n);
char *itoa(int value, char *str, int base);
char *utoa(unsigned int value, char *str, int base);
char *ultoa(unsigned long value, char *str, int base);

/* Simple printf-style formatting */
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list args);

/* Kernel shutdown/reboot */
void kernel_shutdown(void);

/* ============================================================
 * Math functions
 * ============================================================ */
/* Simple integer sine approximation using lookup table */
extern const int16_t sin_table[256];
int32_t sin_approx(int32_t angle);  /* Angle in degrees, returns value in range [-1000, 1000] */

/* ============================================================
 * Framebuffer external access
 * ============================================================ */
extern struct limine_framebuffer *g_fb;
extern uint32_t *g_fb_addr;
extern uint32_t g_fb_width;
extern uint32_t g_fb_height;
extern uint32_t g_fb_pitch;
extern uint32_t g_fb_bpp;

/* I/O port helpers - inline assembly definitions
 * These macros/inline functions provide direct hardware I/O access */
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t value) {
    asm volatile ("outw %0, %1" :: "a"(value), "Nd"(port));
}

#endif /* KERNEL_H */