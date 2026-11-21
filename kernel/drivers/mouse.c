/*
 * Unknown OS - PS/2 Mouse Driver
 * Basic mouse support for GUI
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * PS/2 Mouse State
 * ============================================================ */
static bool mouse_initialized = false;
static int32_t mouse_x = 0;
static int32_t mouse_y = 0;
static uint8_t mouse_buttons = 0;
static int mouse_packet_byte = 0;
static uint8_t mouse_packet[3];

/* Mouse sensitivity */
static float mouse_sensitivity = 1.0f;

/* ============================================================
 * Mouse Helper Functions
 * ============================================================ */
static inline void ps2_wait_input(void) {
    while (inb(0x64) & 0x02) { asm volatile ("pause"); }
}

static inline void ps2_wait_output(void) {
    while (!(inb(0x64) & 0x01)) { asm volatile ("pause"); }
}

static inline void ps2_write_cmd(uint8_t cmd) {
    ps2_wait_input();
    outb(0x64, cmd);
}

static inline void ps2_write_data(uint8_t data) {
    ps2_wait_input();
    outb(0x60, data);
}

static inline uint8_t ps2_read_data(void) {
    ps2_wait_output();
    return inb(0x60);
}

static void ps2_send_mouse_cmd(uint8_t cmd) {
    ps2_write_cmd(0xD4);  /* Send command to mouse */
    ps2_write_data(cmd);
    uint8_t resp = ps2_read_data();
    if (resp != 0xFA) {
        serial_printf("[MOUSE] Command 0x%02X failed, got 0x%02X\n", cmd, resp);
    }
}

/* ============================================================
 * Interrupt Handler (IRQ12)
 * ============================================================ */
void ps2_mouse_interrupt_handler(void) {
    uint8_t data = inb(0x60);

    switch (mouse_packet_byte) {
        case 0:
            /* First byte - status */
            if (data & 0x08) {  /* Bit 3 should always be 1 */
                mouse_packet[0] = data;
                mouse_packet_byte = 1;
            }
            break;
        case 1:
            /* Second byte - X movement */
            mouse_packet[1] = data;
            mouse_packet_byte = 2;
            break;
        case 2:
            /* Third byte - Y movement */
            mouse_packet[2] = data;
            mouse_packet_byte = 0;

            /* Process complete packet */
            int8_t dx = (int8_t)mouse_packet[1];
            int8_t dy = (int8_t)mouse_packet[2];

            /* Update buttons */
            mouse_buttons = mouse_packet[0] & 0x07;  /* Bits 0-2: left, right, middle */

            /* Update position with sensitivity */
            mouse_x += (int32_t)(dx * mouse_sensitivity);
            mouse_y -= (int32_t)(dy * mouse_sensitivity);  /* Y is inverted */

            /* Clamp to screen */
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= (int)fb_width()) mouse_x = fb_width() - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= (int)fb_height()) mouse_y = fb_height() - 1;

            /* Notify GUI */
            extern void gui_handle_mouse_move(int32_t x, int32_t y);
            gui_handle_mouse_move(mouse_x, mouse_y);

            /* Check for button changes */
            static uint8_t prev_buttons = 0;
            uint8_t changed = mouse_buttons ^ prev_buttons;
            if (changed) {
                if (changed & 0x01) {  /* Left button */
                    if (mouse_buttons & 0x01) {
                        extern void gui_handle_mouse_down(int32_t, int32_t, uint32_t);
                        gui_handle_mouse_down(mouse_x, mouse_y, 1);
                    } else {
                        extern void gui_handle_mouse_up(int32_t, int32_t, uint32_t);
                        gui_handle_mouse_up(mouse_x, mouse_y, 1);
                    }
                }
                if (changed & 0x02) {  /* Right button */
                    if (mouse_buttons & 0x02) {
                        extern void gui_handle_mouse_down(int32_t, int32_t, uint32_t);
                        gui_handle_mouse_down(mouse_x, mouse_y, 2);
                    } else {
                        extern void gui_handle_mouse_up(int32_t, int32_t, uint32_t);
                        gui_handle_mouse_up(mouse_x, mouse_y, 2);
                    }
                }
                if (changed & 0x04) {  /* Middle button */
                    if (mouse_buttons & 0x04) {
                        extern void gui_handle_mouse_down(int32_t, int32_t, uint32_t);
                        gui_handle_mouse_down(mouse_x, mouse_y, 4);
                    } else {
                        extern void gui_handle_mouse_up(int32_t, int32_t, uint32_t);
                        gui_handle_mouse_up(mouse_x, mouse_y, 4);
                    }
                }
                prev_buttons = mouse_buttons;
            }
            break;
    }
}

/* ============================================================
 * Public API
 * ============================================================ */
void ps2_mouse_init(void) {
    serial_puts("[MOUSE] Initializing PS/2 mouse...\n");

    /* Enable aux port (mouse) */
    ps2_write_cmd(0xA8);

    /* Enable mouse interrupt */
    ps2_write_cmd(0x20);
    uint8_t config = ps2_read_data();
    config |= 0x02;  /* Enable IRQ12 */
    ps2_write_cmd(0x60);
    ps2_write_data(config);

    /* Reset mouse */
    ps2_send_mouse_cmd(0xFF);
    uint8_t resp = ps2_read_data();
    if (resp != 0xAA) {
        serial_printf("[MOUSE] Reset failed: 0x%02X\n", resp);
        return;
    }
    resp = ps2_read_data();  /* Should be 0x00 */
    serial_puts("[MOUSE] Reset passed\n");

    /* Set default settings */
    ps2_send_mouse_cmd(0xF6);  /* Set defaults */
    ps2_read_data();

    /* Enable mouse */
    ps2_send_mouse_cmd(0xF4);
    ps2_read_data();

    /* Set sample rate */
    ps2_send_mouse_cmd(0xF3);
    ps2_send_mouse_cmd(100);  /* 100 Hz */

    mouse_initialized = true;
    mouse_x = fb_width() / 2;
    mouse_y = fb_height() / 2;

    serial_puts("[MOUSE] Mouse initialized\n");
}

int32_t mouse_get_x(void) { return mouse_x; }
int32_t mouse_get_y(void) { return mouse_y; }
uint8_t mouse_get_buttons(void) { return mouse_buttons; }

void mouse_set_position(int32_t x, int32_t y) {
    mouse_x = x;
    mouse_y = y;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= (int)fb_width()) mouse_x = fb_width() - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= (int)fb_height()) mouse_y = fb_height() - 1;
    extern void gui_handle_mouse_move(int32_t x, int32_t y);
    gui_handle_mouse_move(mouse_x, mouse_y);
}

void mouse_set_sensitivity(float sens) {
    if (sens > 0.0f) mouse_sensitivity = sens;
}