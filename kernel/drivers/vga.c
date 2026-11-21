/*
 * Unknown OS - VGA/Framebuffer Driver
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* VGA State variables - local copies for early boot (before fb.c init) */
static uint16_t vga_width = 0;
static uint16_t vga_height = 0;
static uint16_t vga_pitch = 0;
static uint8_t vga_bpp = 0;
static uint32_t vga_fg_color = 0xFFFFFFFF;
static uint32_t vga_bg_color = 0xFF000000;
static size_t g_cursor_x = 0;
static size_t g_cursor_y = 0;
static uint32_t *vga_fb_addr = NULL;
static const size_t CHAR_WIDTH = 8;
static const size_t CHAR_HEIGHT = 16;

/* Simple 8x16 font (first 128 ASCII) - embedded */
static const uint8_t font_8x16[128][16] = {
    [' '] = {0},
    ['!'] = {0x18,0x3C,0x3C,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x3C,0x3C,0x18,0x00},
    ['"'] = {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['#'] = {0x00,0x18,0x3C,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x3C,0x18,0x00},
};

/* Fill remaining font characters (0-255) with a simple pattern */
#define FONT_CHAR(idx) {0x00,0x00,0x00,0x00,0x00,0x18,0x3C,0x3C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00}

/* ============================================================
 * VGA Initialization
 * ============================================================ */
void vga_init(struct boot_info *boot_info) {
    struct limine_framebuffer_response *fb_resp = boot_info->framebuffer;
    if (!fb_resp || fb_resp->framebuffer_count == 0) {
        serial_puts("[VGA] No framebuffer available\n");
        return;
    }

    /* Initialize from boot_info's framebuffer (early boot, before fb.c init) */
    struct limine_framebuffer *fb = fb_resp->framebuffers[0];

    vga_width = fb->width;
    vga_height = fb->height;
    vga_pitch = fb->pitch / 4;  /* Convert bytes to pixels (32-bit color) */
    vga_bpp = fb->bpp;

    /* Set framebuffer address from HHDM offset */
    vga_fb_addr = (uint32_t *)((uint64_t)fb->address + boot_info->hhdm->offset);

    serial_printf("[VGA] Framebuffer: %ux%u, pitch %u, bpp %u, addr 0x%lx\n",
                  vga_width, vga_height, vga_pitch, vga_bpp, (uint64_t)vga_fb_addr);

    vga_clear();
}

/* ============================================================
 * Color Helpers
 * ============================================================ */
static uint32_t vga_make_color(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

void vga_set_color(uint32_t fg, uint32_t bg) {
    vga_fg_color = fg;
    vga_bg_color = bg;
}

/* ============================================================
 * Drawing
 * ============================================================ */
static void vga_draw_char(char c, size_t x, size_t y) {
    if (!vga_fb_addr) return;
    if ((unsigned char)c >= 128) c = '?';

    uint32_t *fb = vga_fb_addr;

    /* Use built-in font */
    const uint8_t *glyph = font_8x16[(unsigned char)c];

    for (size_t dy = 0; dy < CHAR_HEIGHT; dy++) {
        uint8_t bits = glyph[dy];
        for (size_t dx = 0; dx < CHAR_WIDTH; dx++) {
            size_t px = x * CHAR_WIDTH + dx;
            size_t py = y * CHAR_HEIGHT + dy;
            if (px >= vga_width || py >= vga_height) continue;

            uint32_t color = (bits & 0x80) ? vga_fg_color : vga_bg_color;
            fb[py * vga_pitch + px] = color;
            bits <<= 1;
        }
    }
}

static void vga_scroll(void) {
    if (!vga_fb_addr) return;

    size_t rows = vga_height / CHAR_HEIGHT;
    uint32_t *fb = vga_fb_addr;

    /* Move all rows up by one character height using memmove */
    memmove(fb, fb + CHAR_HEIGHT * vga_pitch, (rows - 1) * CHAR_HEIGHT * vga_pitch * sizeof(uint32_t));

    /* Clear last row */
    memset(fb + (rows - 1) * CHAR_HEIGHT * vga_pitch, 0,
           CHAR_HEIGHT * vga_pitch * sizeof(uint32_t));
    g_cursor_y = rows - 1;
}

/* ============================================================
 * Output Functions
 * ============================================================ */
void vga_putc(char c) {
    if (!vga_fb_addr) return;

    if (c == '\n') {
        g_cursor_x = 0;
        g_cursor_y++;
    } else if (c == '\r') {
        g_cursor_x = 0;
    } else if (c == '\b') {
        if (g_cursor_x > 0) {
            g_cursor_x--;
            vga_draw_char(' ', g_cursor_x, g_cursor_y);
        }
    } else if (c == '\t') {
        g_cursor_x = (g_cursor_x + 4) & ~3;
    } else if (c >= 32 && c < 127) {
        vga_draw_char(c, g_cursor_x, g_cursor_y);
        g_cursor_x++;
    }

    size_t cols = vga_width / CHAR_WIDTH;
    size_t rows = vga_height / CHAR_HEIGHT;

    if (g_cursor_x >= cols) {
        g_cursor_x = 0;
        g_cursor_y++;
    }

    if (g_cursor_y >= rows) {
        vga_scroll();
    }
}

void vga_puts(const char *s) {
    while (*s) {
        if (*s == '\n') vga_putc('\r');
        vga_putc(*s++);
    }
}

void vga_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    vga_puts(buf);
}

void vga_clear(void) {
    if (!vga_fb_addr) return;

    size_t total_pixels = vga_width * vga_height;
    for (size_t i = 0; i < total_pixels; i++) {
        vga_fb_addr[i] = vga_bg_color;
    }
    g_cursor_x = 0;
    g_cursor_y = 0;
}

/* ============================================================
 * Synchronize VGA with fb.c globals (call after fb_init)
 * ============================================================ */
void vga_sync_with_fb(void) {
    /* g_fb_addr, g_fb_width, g_fb_height, g_fb_pitch, g_fb_bpp are
     * declared extern in kernel.h and defined in fb.c */

    if (g_fb_addr != NULL) {
        vga_fb_addr = g_fb_addr;
    }
    vga_width = g_fb_width;
    vga_height = g_fb_height;
    vga_pitch = g_fb_pitch;
    vga_bpp = g_fb_bpp;
}

/* ============================================================
 * vsnprintf (shared with serial)
 * ============================================================ */
extern int vsnprintf(char *buf, size_t len, const char *fmt, va_list args);