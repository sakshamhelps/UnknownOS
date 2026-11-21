/*
 * Unknown OS - PS/2 Keyboard Driver
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * PS/2 Ports
 * ============================================================ */
#define PS2_DATA_PORT   0x60
#define PS2_CMD_PORT    0x64
#define PS2_STATUS_PORT 0x64

/* Controller Commands */
#define PS2_CMD_READ_CONFIG   0x20
#define PS2_CMD_WRITE_CONFIG  0x60
#define PS2_CMD_DISABLE_PORT2 0xA7
#define PS2_CMD_ENABLE_PORT2  0xA8
#define PS2_CMD_TEST_PORT2    0xA9
#define PS2_CMD_TEST_CONTROLLER 0xAA
#define PS2_CMD_TEST_PORT1    0xAB
#define PS2_CMD_DISABLE_PORT1 0xAD
#define PS2_CMD_ENABLE_PORT1  0xAE

/* Keyboard Commands */
#define PS2_KBD_SET_LEDS      0xED
#define PS2_KBD_ECHO          0xEE
#define PS2_KBD_SCAN_CODE_SET 0xF0
#define PS2_KBD_IDENTIFY      0xF2
#define PS2_KBD_SET_RATE      0xF3
#define PS2_KBD_ENABLE        0xF4
#define PS2_KBD_DISABLE       0xF5
#define PS2_KBD_RESET         0xFF

/* ============================================================
 * Keyboard State
 * ============================================================ */
static bool ps2_initialized = false;
static char ps2_buffer[256];
static size_t ps2_head = 0;
static size_t ps2_tail = 0;

/* Modifier keys */
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;
static bool num_lock = true;
static bool scroll_lock = false;

/* Scancode Set 1 (default) - Key mappings */
static const char scancode_to_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* F1-F10 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,       /* Num lock, Scroll lock, etc */
};

static const char scancode_to_ascii_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* Extended scancodes (E0 prefix) */
static const uint8_t extended_keys[] = {
    0x1C,  /* Enter (keypad) */
    0x1D,  /* Right Ctrl */
    0x35,  /* Keypad / */
    0x37,  /* Print Screen */
    0x38,  /* Right Alt */
    0x46,  /* Break (Ctrl+Pause) */
    0x47,  /* Home */
    0x48,  /* Up */
    0x49,  /* Page Up */
    0x4B,  /* Left */
    0x4D,  /* Right */
    0x4F,  /* End */
    0x50,  /* Down */
    0x51,  /* Page Down */
    0x52,  /* Insert */
    0x53,  /* Delete */
};

/* ============================================================
 * PS/2 Helper Functions
 * ============================================================ */
static inline void ps2_wait_input(void) {
    while (inb(PS2_STATUS_PORT) & 0x02) { asm volatile ("pause"); }
}

static inline void ps2_wait_output(void) {
    while (!(inb(PS2_STATUS_PORT) & 0x01)) { asm volatile ("pause"); }
}

static inline void ps2_write_cmd(uint8_t cmd) {
    ps2_wait_input();
    outb(PS2_CMD_PORT, cmd);
}

static inline void ps2_write_data(uint8_t data) {
    ps2_wait_input();
    outb(PS2_DATA_PORT, data);
}

static inline uint8_t ps2_read_data(void) {
    ps2_wait_output();
    return inb(PS2_DATA_PORT);
}

static void ps2_send_kbd_cmd(uint8_t cmd) {
    ps2_wait_input();
    outb(PS2_DATA_PORT, cmd);
    /* Wait for ACK (0xFA) */
    uint8_t resp = ps2_read_data();
    if (resp != 0xFA) {
        serial_printf("[PS2] Command 0x%02X failed, got 0x%02X\n", cmd, resp);
    }
}

/* ============================================================
 * Interrupt Handler
 * ============================================================ */
void ps2_interrupt_handler(void) {
    uint8_t scancode = inb(PS2_DATA_PORT);

    /* Handle extended scancodes (E0 prefix) */
    static bool extended = false;
    static bool released = false;

    if (scancode == 0xE0) {
        extended = true;
        return;
    }
    if (scancode == 0xE1) {
        /* Pause key - ignore for now */
        return;
    }

    if (scancode & 0x80) {
        released = true;
        scancode &= 0x7F;
    }

    /* Handle modifier keys */
    switch (scancode) {
        case 0x2A: case 0x36:  /* Left/Right Shift */
            shift_pressed = !released;
            break;
        case 0x1D:  /* Left Ctrl */
            ctrl_pressed = !released;
            break;
        case 0x38:  /* Left Alt */
            alt_pressed = !released;
            break;
        case 0x3A:  /* Caps Lock */
            if (!released) caps_lock = !caps_lock;
            break;
        case 0x45:  /* Num Lock */
            if (!released) num_lock = !num_lock;
            break;
        case 0x46:  /* Scroll Lock */
            if (!released) scroll_lock = !scroll_lock;
            break;
    }

    /* Only process key press (not release) for character input */
    if (!released && scancode < 128) {
        char c = 0;
        if (shift_pressed ^ caps_lock) {
            c = scancode_to_ascii_shift[scancode];
        } else {
            c = scancode_to_ascii[scancode];
        }

        if (c) {
            /* Add to buffer */
            size_t next = (ps2_head + 1) % 256;
            if (next != ps2_tail) {
                ps2_buffer[ps2_head] = c;
                ps2_head = next;
            }
        }
    }

    extended = false;
    released = false;
}

/* ============================================================
 * Public API
 * ============================================================ */
void ps2_init(void) {
    serial_puts("[PS2] Initializing keyboard...\n");

    /* Disable devices */
    ps2_write_cmd(PS2_CMD_DISABLE_PORT1);
    ps2_write_cmd(PS2_CMD_DISABLE_PORT2);

    /* Flush output buffer */
    while (inb(PS2_STATUS_PORT) & 0x01) {
        inb(PS2_DATA_PORT);
    }

    /* Test controller */
    ps2_write_cmd(PS2_CMD_TEST_CONTROLLER);
    uint8_t resp = ps2_read_data();
    if (resp != 0x55) {
        serial_printf("[PS2] Controller test failed: 0x%02X\n", resp);
        return;
    }
    serial_puts("[PS2] Controller test passed\n");

    /* Test port 1 */
    ps2_write_cmd(PS2_CMD_TEST_PORT1);
    resp = ps2_read_data();
    if (resp != 0x00) {
        serial_printf("[PS2] Port 1 test failed: 0x%02X\n", resp);
        return;
    }
    serial_puts("[PS2] Port 1 test passed\n");

    /* Enable port 1 */
    ps2_write_cmd(PS2_CMD_ENABLE_PORT1);

    /* Read config */
    ps2_write_cmd(PS2_CMD_READ_CONFIG);
    uint8_t config = ps2_read_data();

    /* Enable port 1 interrupt */
    config |= 0x01;
    /* Disable port 2 interrupt */
    config &= ~0x02;
    /* Disable translation */
    config |= 0x40;

    ps2_write_cmd(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config);

    /* Reset keyboard */
    ps2_send_kbd_cmd(PS2_KBD_RESET);
    resp = ps2_read_data();  /* Should be 0xAA (self-test passed) */
    resp = ps2_read_data();  /* Should be 0xAA */

    /* Set scan code set 1 */
    ps2_send_kbd_cmd(PS2_KBD_SCAN_CODE_SET);
    ps2_send_kbd_cmd(0x01);

    /* Enable keyboard */
    ps2_send_kbd_cmd(PS2_KBD_ENABLE);

    /* Set LEDs */
    ps2_send_kbd_cmd(PS2_KBD_SET_LEDS);
    uint8_t leds = (scroll_lock ? 1 : 0) | (num_lock ? 2 : 0) | (caps_lock ? 4 : 0);
    ps2_send_kbd_cmd(leds);

    ps2_initialized = true;
    serial_puts("[PS2] Keyboard initialized\n");
}

bool ps2_has_char(void) {
    return ps2_head != ps2_tail;
}

char ps2_getchar(void) {
    if (!ps2_has_char()) return 0;
    char c = ps2_buffer[ps2_tail];
    ps2_tail = (ps2_tail + 1) % 256;
    return c;
}