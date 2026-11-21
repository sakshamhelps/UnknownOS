/*
 * Unknown OS - System Applications
 * Terminal, Settings, App Launcher, File Manager
 * Author: byrex <byrex.me@proton.me>
 */

#include <gui.h>
#include <kernel.h>

/* Forward declaration - implemented in compositor.c */
extern void terminal_execute_command(widget_t *w, const char *cmd);

/* ============================================================
 * Terminal App (PTY + Shell)
 * ============================================================ */

#define TERMINAL_MAX_ROWS 40
#define TERMINAL_MAX_COLS 80
#define TERMINAL_SCROLLBACK 1000

typedef struct {
    char *buffer;           /* Line buffer */
    size_t buffer_size;
    size_t buffer_pos;
    char *scrollback;       /* Scrollback buffer */
    size_t scrollback_size;
    size_t scrollback_pos;
    int cursor_row;
    int cursor_col;
    int scroll_offset;
    bool running;
    char prompt[32];
    char cwd[256];
} terminal_data_t;

static void terminal_draw(widget_t *w) {
    terminal_data_t *td = (terminal_data_t *)w->user_data;
    if (!td) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    /* Background */
    rect_t bg = { 0, 0, w->bounds.w, w->bounds.h };
    draw_rect_filled(buf, pitch, &bg, 0xFF1A1A2E);

    /* Draw scrollback lines */
    int char_w = 8;
    int char_h = 16;
    int cols = w->bounds.w / char_w;
    int rows = w->bounds.h / char_h;

    for (int row = 0; row < rows; row++) {
        int sb_idx = (td->scrollback_pos - td->scroll_offset - rows + row) % TERMINAL_SCROLLBACK;
        if (sb_idx < 0) sb_idx += TERMINAL_SCROLLBACK;

        char *line = &td->scrollback[sb_idx * TERMINAL_MAX_COLS];
        if (line[0] == 0) continue;

        int y = row * char_h;
        draw_text(buf, pitch, 8, y, line, GUI_COLOR_FG, 0xFF1A1A2E);
    }

    /* Draw cursor */
    if ((timer_get_ticks() / 500) % 2 == 0) {
        int cursor_x = 8 + td->cursor_col * char_w;
        int cursor_y = (rows - 1 - td->scroll_offset) * char_h;
        if (cursor_y >= 0 && cursor_y < (int)w->bounds.h) {
            rect_t cursor = { cursor_x, cursor_y, 2, char_h };
            draw_rect_filled(buf, pitch, &cursor, GUI_COLOR_ACCENT);
        }
    }
}

static void terminal_handle_event(widget_t *w, gui_event_t *e) {
    terminal_data_t *td = (terminal_data_t *)w->user_data;
    if (!td) return;

    switch (e->type) {
        case EVENT_MOUSE_DOWN:
            if (rect_contains(&w->bounds, e->mouse.x, e->mouse.y) && (e->mouse.buttons & MOUSE_BTN_LEFT)) {
                window_focus(w->window);
                /* Could implement click-to-position cursor */
            }
            break;
        case EVENT_KEY_CHAR:
            if (e->keyboard.character >= 32 && e->keyboard.character < 127 && td->buffer_pos < TERMINAL_MAX_COLS - 1) {
                td->buffer[td->buffer_pos++] = e->keyboard.character;
                td->buffer[td->buffer_pos] = '\0';
                td->cursor_col = td->buffer_pos;
                widget_invalidate(w);
            }
            break;
        case EVENT_KEY_DOWN:
            if (e->keyboard.keycode == 0x0E) {  /* Backspace */
                if (td->buffer_pos > 0) {
                    td->buffer[--td->buffer_pos] = '\0';
                    td->cursor_col = td->buffer_pos;
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x1C) {  /* Enter */
                /* Execute command */
                td->buffer[td->buffer_pos] = '\0';
                terminal_execute_command(w, td->buffer);
                td->buffer_pos = 0;
                td->buffer[0] = '\0';
                td->cursor_col = 0;
                widget_invalidate(w);
            } else if (e->keyboard.keycode == 0x48) {  /* Up arrow - scrollback */
                td->scroll_offset++;
                if (td->scroll_offset > TERMINAL_SCROLLBACK - rows) td->scroll_offset = TERMINAL_SCROLLBACK - rows;
                widget_invalidate(w);
            } else if (e->keyboard.keycode == 0x50) {  /* Down arrow */
                if (td->scroll_offset > 0) td->scroll_offset--;
                widget_invalidate(w);
            }
            break;
    }
}

static void terminal_destroy(widget_t *w) {
    terminal_data_t *td = (terminal_data_t *)w->user_data;
    if (td) {
        if (td->buffer) kfree(td->buffer);
        if (td->scrollback) kfree(td->scrollback);
        kfree(td);
    }
}

static const widget_vtable_t terminal_vtable = {
    .draw = terminal_draw,
    .handle_event = terminal_handle_event,
    .destroy = terminal_destroy,
};

void terminal_append_output(terminal_data_t *td, const char *text) {
    if (!td || !text) return;

    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        if (text[i] == '\n') {
            /* Move to next line in scrollback */
            td->scrollback_pos = (td->scrollback_pos + 1) % TERMINAL_SCROLLBACK;
            memset(&td->scrollback[td->scrollback_pos * TERMINAL_MAX_COLS], 0, TERMINAL_MAX_COLS);
            td->cursor_col = 0;
        } else if (text[i] == '\r') {
            td->cursor_col = 0;
        } else if (text[i] >= 32) {
            td->scrollback[td->scrollback_pos * TERMINAL_MAX_COLS + td->cursor_col] = text[i];
            td->cursor_col++;
            if (td->cursor_col >= TERMINAL_MAX_COLS - 1) {
                td->cursor_col = 0;
                td->scrollback_pos = (td->scrollback_pos + 1) % TERMINAL_SCROLLBACK;
                memset(&td->scrollback[td->scrollback_pos * TERMINAL_MAX_COLS], 0, TERMINAL_MAX_COLS);
            }
        }
    }
}

void terminal_execute_command(widget_t *w, const char *cmd) {
    terminal_data_t *td = (terminal_data_t *)w->user_data;
    if (!td || !cmd) return;

    /* Print command with prompt */
    char output[512];
    snprintf(output, sizeof(output), "%s%s\n", td->prompt, cmd);
    terminal_append_output(td, output);

    /* Built-in commands */
    if (strcmp(cmd, "clear") == 0) {
        td->scrollback_pos = 0;
        memset(td->scrollback, 0, TERMINAL_SCROLLBACK * TERMINAL_MAX_COLS);
    } else if (strcmp(cmd, "help") == 0) {
        terminal_append_output(td, "Unknown OS Terminal 1.0\n");
        terminal_append_output(td, "Commands:\n");
        terminal_append_output(td, "  help       - Show this help\n");
        terminal_append_output(td, "  clear      - Clear screen\n");
        terminal_append_output(td, "  ls         - List directory\n");
        terminal_append_output(td, "  pwd        - Print working directory\n");
        terminal_append_output(td, "  cd <dir>   - Change directory\n");
        terminal_append_output(td, "  echo <txt> - Print text\n");
        terminal_append_output(td, "  version    - Show OS version\n");
        terminal_append_output(td, "  shutdown   - Power off\n");
        terminal_append_output(td, "  reboot     - Reboot system\n");
    } else if (strcmp(cmd, "ls") == 0) {
        /* List root directory via VFS */
        struct vfs_file *file;
        if (vfs_open("/", VFS_O_RDONLY, &file) == 0) {
            struct vfs_dirent dirent;
            while (vfs_getdents(file, &dirent, sizeof(dirent)) > 0) {
                char line[256];
                snprintf(line, sizeof(line), "%s  ", dirent.d_name);
                terminal_append_output(td, line);
            }
            terminal_append_output(td, "\n");
            vfs_close(file);
        } else {
            terminal_append_output(td, "Error: Cannot open directory\n");
        }
    } else if (strcmp(cmd, "pwd") == 0) {
        terminal_append_output(td, td->cwd);
        terminal_append_output(td, "\n");
    } else if (strncmp(cmd, "cd ", 3) == 0) {
        const char *path = cmd + 3;
        struct vfs_stat st;
        if (vfs_stat(path, &st) == 0 && (st.st_mode & VFS_TYPE_DIRECTORY)) {
            strncpy(td->cwd, path, sizeof(td->cwd) - 1);
            terminal_append_output(td, "\n");
        } else {
            terminal_append_output(td, "Error: No such directory\n");
        }
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        terminal_append_output(td, cmd + 5);
        terminal_append_output(td, "\n");
    } else if (strcmp(cmd, "version") == 0) {
        terminal_append_output(td, "Unknown OS 1.0.0-IND\n");
        terminal_append_output(td, "India's First Independent OS\n");
        terminal_append_output(td, "byrex.me@proton.me\n");
    } else if (strcmp(cmd, "shutdown") == 0) {
        terminal_append_output(td, "Shutting down...\n");
        extern void kernel_shutdown(void);
        kernel_shutdown();
    } else if (strcmp(cmd, "reboot") == 0) {
        terminal_append_output(td, "Rebooting...\n");
        outb(0xCF9, 0x06);
    } else if (strlen(cmd) > 0) {
        char err[256];
        snprintf(err, sizeof(err), "Unknown command: %s\n", cmd);
        terminal_append_output(td, err);
    }

    /* Print new prompt */
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "%s", td->prompt);
    terminal_append_output(td, prompt);
}

widget_t *terminal_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h) {
    terminal_data_t *td = (terminal_data_t *)kmalloc(sizeof(terminal_data_t));
    if (!td) return NULL;
    memset(td, 0, sizeof(terminal_data_t));

    td->buffer = (char *)kzalloc(TERMINAL_MAX_COLS);
    td->buffer_size = TERMINAL_MAX_COLS;
    td->scrollback = (char *)kzalloc(TERMINAL_SCROLLBACK * TERMINAL_MAX_COLS);
    td->scrollback_size = TERMINAL_SCROLLBACK * TERMINAL_MAX_COLS;
    td->scrollback_pos = 0;
    td->cursor_row = 0;
    td->cursor_col = 0;
    td->scroll_offset = 0;
    td->running = true;
    strcpy(td->prompt, "unknownos> ");
    strcpy(td->cwd, "/");

    /* Print welcome message */
    terminal_append_output(td, "Unknown OS Terminal 1.0\n");
    terminal_append_output(td, "Type 'help' for commands\n");
    terminal_append_output(td, td->prompt);

    widget_t *widget = widget_create(win, parent, &terminal_vtable, td);
    if (widget) {
        widget->bounds = (rect_t){ x, y, w, h };
    }
    return widget;
}

/* Global terminal window for launching from dock/menu */
static window_t *g_terminal_window = NULL;

static void terminal_on_close(window_t *win) {
    window_destroy(win);
    g_terminal_window = NULL;
}

void launch_terminal(void) {
    if (g_terminal_window) {
        window_show(g_terminal_window);
        window_focus(g_terminal_window);
        return;
    }

    g_terminal_window = window_create(WINDOW_TYPE_NORMAL, "Terminal",
                                      100, 100, 800, 500,
                                      WINDOW_FLAG_DECORATED | WINDOW_FLAG_RESIZABLE | WINDOW_FLAG_MOVABLE);
    if (!g_terminal_window) return;

    g_terminal_window->on_close = terminal_on_close;

    terminal_create(g_terminal_window, NULL, 0, 28, 800, 472);
    window_show(g_terminal_window);
    window_focus(g_terminal_window);
}

/* ============================================================
 * Settings App
 * ============================================================ */

/* Use settings_data_t from gui.h which already includes content_area field */

/* Helper to sync active_tab from tabbar */
static int get_tabbar_active_tab(widget_t *tabbar_widget) {
    if (!tabbar_widget || !tabbar_widget->user_data) return 0;
    tabbar_data_t *td = (tabbar_data_t *)tabbar_widget->user_data;
    return td->active_tab;
}

/* Store reference to tabbar for sync */
static widget_t *g_settings_tabbar = NULL;

static void settings_draw(widget_t *w) {
    settings_data_t *sd = (settings_data_t *)w->user_data;
    if (!sd) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    /* Sync active_tab from tabbar if available */
    int active_tab = 0;
    if (g_settings_tabbar) {
        active_tab = get_tabbar_active_tab(g_settings_tabbar);
    }

    /* Background */
    rect_t bg = { 0, 0, w->bounds.w, w->bounds.h };
    draw_rect_filled(buf, pitch, &bg, GUI_COLOR_SURFACE);

    /* Title */
    draw_text(buf, pitch, 24, 24, "Settings", GUI_COLOR_FG, GUI_COLOR_SURFACE);
    draw_text(buf, pitch, 24, 50, "Unknown OS Configuration", GUI_COLOR_SURFACE_HL, GUI_COLOR_SURFACE);

    /* Draw based on active tab */
    switch (active_tab) {
        case 0: /* General */
            draw_text(buf, pitch, 24, 100, "General Settings", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 140, "OS Name: Unknown OS", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 160, "Version: 1.0.0-IND", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 180, "Author: byrex <byrex.me@proton.me>", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 200, "Architecture: x86_64", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 220, "Kernel: Independent (Custom)", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            break;
        case 1: /* Appearance */
            draw_text(buf, pitch, 24, 100, "Appearance", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 140, "Theme: Dark (macOS-inspired)", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 160, "Accent Color: Blue", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 180, "Wallpaper: Gradient", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 200, "Dock Magnification: Enabled", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            break;
        case 2: /* System */
            draw_text(buf, pitch, 24, 100, "System Information", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            {
                extern uint64_t pmm_get_total_pages(void);
                extern uint64_t pmm_get_free_pages(void);
                char mem[128];
                snprintf(mem, sizeof(mem), "Total Memory: %lu MB", pmm_get_total_pages() * 4 / 1024);
                draw_text(buf, pitch, 24, 140, mem, GUI_COLOR_FG, GUI_COLOR_SURFACE);
                snprintf(mem, sizeof(mem), "Free Memory: %lu MB", pmm_get_free_pages() * 4 / 1024);
                draw_text(buf, pitch, 24, 160, mem, GUI_COLOR_FG, GUI_COLOR_SURFACE);
            }
            draw_text(buf, pitch, 24, 200, "CPU: x86_64 (APC enabled)", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 220, "Bootloader: Limine", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 240, "Filesystems: RAMFS, TARFS", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            break;
        case 3: /* About */
            draw_text(buf, pitch, 24, 100, "About Unknown OS", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 140, "India's First Independent Operating System", GUI_COLOR_ACCENT, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 180, "Built from scratch with:", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 200, "  - Custom Kernel (Higher-half, 4-level paging)", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 220, "  - Custom GUI/Compositor (Double-buffered)", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 240, "  - Custom VFS (RAMFS, TARFS, UNK loader)", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 260, "  - Capability-based Security (.unk format)", GUI_COLOR_FG, GUI_COLOR_SURFACE);
            draw_text(buf, pitch, 24, 300, "Support: byrex.me@proton.me", GUI_COLOR_ACCENT, GUI_COLOR_SURFACE);
            break;
    }
}

static void settings_handle_event(widget_t *w, gui_event_t *e) {
    settings_data_t *sd = (settings_data_t *)w->user_data;
    if (!sd) return;

    /* Handled by tabbar widget */
}

static void settings_destroy(widget_t *w) {
    settings_data_t *sd = (settings_data_t *)w->user_data;
    if (sd) kfree(sd);
}

static const widget_vtable_t settings_vtable = {
    .draw = settings_draw,
    .handle_event = settings_handle_event,
    .destroy = settings_destroy,
};

static void settings_on_close(window_t *w) { window_destroy(w); }

void launch_settings(void) {
    window_t *win = window_create(WINDOW_TYPE_NORMAL, "Settings",
                                  200, 100, 700, 500,
                                  WINDOW_FLAG_DECORATED | WINDOW_FLAG_RESIZABLE | WINDOW_FLAG_MOVABLE);
    if (!win) return;

    win->on_close = settings_on_close;

    /* Create settings data for content widget */
    settings_data_t *sd = (settings_data_t *)kzalloc(sizeof(settings_data_t));
    if (!sd) {
        window_destroy(win);
        return;
    }

    /* Create content area widget */
    sd->content_area = widget_create(win, NULL, &settings_vtable, sd);
    if (!sd->content_area) {
        kfree(sd);
        window_destroy(win);
        return;
    }
    sd->content_area->bounds = (rect_t){ 0, 28 + 36, 700, 500 - 28 - 36 };

    /* Create tab bar at top */
    widget_t *tabbar = tabbar_create(win, NULL, 0, 28, 700, 36);
    g_settings_tabbar = tabbar;  /* Store for tab sync */

    /* Create individual content widgets for each tab */
    tabbar_add_tab(tabbar, "General", sd->content_area);
    tabbar_add_tab(tabbar, "Appearance", sd->content_area);
    tabbar_add_tab(tabbar, "System", sd->content_area);
    tabbar_add_tab(tabbar, "About", sd->content_area);

    window_show(win);
    window_focus(win);
}

/* ============================================================
 * App Launcher (Super+Space)
 * ============================================================ */

static void launcher_draw(widget_t *w) {
    launcher_data_t *ld = (launcher_data_t *)w->user_data;
    if (!ld || !ld->visible) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    /* Dark overlay */
    rect_t full = { 0, 0, w->bounds.w, w->bounds.h };
    draw_rect_filled(buf, pitch, &full, 0xCC11111B);

    /* Center panel */
    int panel_w = 500;
    int panel_h = 400;
    int panel_x = (w->bounds.w - panel_w) / 2;
    int panel_y = (w->bounds.h - panel_h) / 2;

    rect_t panel = { panel_x, panel_y, panel_w, panel_h };
    draw_rounded_rect(buf, pitch, &panel, 16, GUI_COLOR_SURFACE);
    draw_rounded_rect_outline(buf, pitch, &panel, 16, GUI_COLOR_BORDER);

    /* Title */
    draw_text(buf, pitch, panel_x + 24, panel_y + 24, "App Launcher", GUI_COLOR_FG, GUI_COLOR_SURFACE);
    draw_text(buf, pitch, panel_x + 24, panel_y + 50, "Press Super+Space to close", GUI_COLOR_SURFACE_HL, GUI_COLOR_SURFACE);

    /* Search box */
    rect_t search_box = { panel_x + 24, panel_y + 90, panel_w - 48, 36 };
    draw_rounded_rect(buf, pitch, &search_box, 8, GUI_COLOR_BG);
    draw_rounded_rect_outline(buf, pitch, &search_box, 8, GUI_COLOR_BORDER);
    draw_text(buf, pitch, panel_x + 36, panel_y + 96, ld->search, GUI_COLOR_FG, GUI_COLOR_BG);

    /* App grid */
    int app_size = 80;
    int padding = 16;
    int cols = 5;
    int start_x = panel_x + (panel_w - (cols * app_size + (cols - 1) * padding)) / 2;
    int start_y = panel_y + 150;

    int visible_count = 0;
    for (int i = 0; i < ld->app_count; i++) {
        if (ld->search_len > 0) {
            if (strstr(ld->apps[i].name, ld->search) == NULL) continue;
        }
        if (visible_count >= 10) break; /* Limit visible */

        int col = visible_count % cols;
        int row = visible_count / cols;
        int x = start_x + col * (app_size + padding);
        int y = start_y + row * (app_size + padding);

        rect_t app_rect = { x, y, app_size, app_size };
        gui_color_t bg = (visible_count == ld->hovered_index) ? GUI_COLOR_SURFACE_HL : GUI_COLOR_BG;
        draw_rounded_rect(buf, pitch, &app_rect, 12, bg);

        /* App icon placeholder */
        rect_t icon = { x + 20, y + 12, 40, 40 };
        draw_rounded_rect(buf, pitch, &icon, 8, GUI_COLOR_ACCENT);
        draw_text(buf, pitch, x + 28, y + 22, ld->apps[i].icon, GUI_COLOR_WHITE, GUI_COLOR_ACCENT);

        /* App name */
        int text_w = strlen(ld->apps[i].name) * 8;
        int text_x = x + (app_size - text_w) / 2;
        int text_y = y + 60;
        draw_text(buf, pitch, text_x, text_y, ld->apps[i].name, GUI_COLOR_FG, bg);

        visible_count++;
    }
}

static void launcher_handle_event(widget_t *w, gui_event_t *e) {
    launcher_data_t *ld = (launcher_data_t *)w->user_data;
    if (!ld || !ld->visible) return;

    switch (e->type) {
        case EVENT_MOUSE_MOVE: {
            int old_hover = ld->hovered_index;
            ld->hovered_index = -1;

            int panel_w = 500;
            int panel_h = 400;
            int panel_x = (w->bounds.w - panel_w) / 2;
            int panel_y = (w->bounds.h - panel_h) / 2;
            int app_size = 80;
            int padding = 16;
            int cols = 5;
            int start_x = panel_x + (panel_w - (cols * app_size + (cols - 1) * padding)) / 2;
            int start_y = panel_y + 150;

            int visible_idx = 0;
            for (int i = 0; i < ld->app_count; i++) {
                if (ld->search_len > 0 && strstr(ld->apps[i].name, ld->search) == NULL) continue;
                if (visible_idx >= 10) break;

                int col = visible_idx % cols;
                int row = visible_idx / cols;
                int x = start_x + col * (app_size + padding);
                int y = start_y + row * (app_size + padding);

                if (e->mouse.x >= x && e->mouse.x < x + app_size &&
                    e->mouse.y >= y && e->mouse.y < y + app_size) {
                    ld->hovered_index = visible_idx;
                    break;
                }
                visible_idx++;
            }

            if (old_hover != ld->hovered_index) widget_invalidate(w);
            break;
        }
        case EVENT_MOUSE_DOWN:
            if (ld->hovered_index >= 0 && (e->mouse.buttons & MOUSE_BTN_LEFT)) {
                /* Find actual app index */
                int visible_idx = 0;
                for (int i = 0; i < ld->app_count; i++) {
                    if (ld->search_len > 0 && strstr(ld->apps[i].name, ld->search) == NULL) continue;
                    if (visible_idx == ld->hovered_index) {
                        if (ld->apps[i].launch) ld->apps[i].launch();
                        launcher_hide(w);
                        break;
                    }
                    visible_idx++;
                }
            }
            break;
        case EVENT_KEY_CHAR:
            if (e->keyboard.character >= 32 && ld->search_len < 63) {
                ld->search[ld->search_len++] = e->keyboard.character;
                ld->search[ld->search_len] = '\0';
                widget_invalidate(w);
            }
            break;
        case EVENT_KEY_DOWN:
            if (e->keyboard.keycode == 0x0E) {  /* Backspace */
                if (ld->search_len > 0) {
                    ld->search[--ld->search_len] = '\0';
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x01) {  /* Escape */
                launcher_hide(w);
            } else if (e->keyboard.keycode == 0x1C) {  /* Enter - launch first match */
                for (int i = 0; i < ld->app_count; i++) {
                    if (ld->search_len > 0 && strstr(ld->apps[i].name, ld->search) == NULL) continue;
                    if (ld->apps[i].launch) ld->apps[i].launch();
                    launcher_hide(w);
                    break;
                }
            }
            break;
    }
}

static void launcher_destroy(widget_t *w) {
    launcher_data_t *ld = (launcher_data_t *)w->user_data;
    if (ld) {
        for (int i = 0; i < ld->app_count; i++) {
            if (ld->apps[i].name) kfree(ld->apps[i].name);
            if (ld->apps[i].icon) kfree(ld->apps[i].icon);
        }
        kfree(ld);
    }
}

static const widget_vtable_t launcher_vtable = {
    .draw = launcher_draw,
    .handle_event = launcher_handle_event,
    .destroy = launcher_destroy,
};

static widget_t *g_launcher_widget = NULL;

/* Forward declarations for app launch functions */
void launch_shell_window(void);

widget_t *launcher_create(window_t *win) {
    launcher_data_t *ld = (launcher_data_t *)kzalloc(sizeof(launcher_data_t));
    if (!ld) return NULL;

    /* Add default apps */
    #define ADD_APP(name, icon, func) \
        do { \
            ld->apps[ld->app_count].name = (char *)kmalloc(strlen(name) + 1); \
            strcpy(ld->apps[ld->app_count].name, name); \
            ld->apps[ld->app_count].icon = (char *)kmalloc(strlen(icon) + 1); \
            strcpy(ld->apps[ld->app_count].icon, icon); \
            ld->apps[ld->app_count].launch = func; \
            ld->app_count++; \
        } while(0)

    ADD_APP("Terminal", ">", launch_terminal);
    ADD_APP("Settings", "⚙", launch_settings);
    ADD_APP("Files", "📁", launch_file_manager);
    ADD_APP("Editor", "📝", launch_text_editor);
    ADD_APP("Browser", "🌐", launch_browser);
    ADD_APP("Shell", "$", launch_shell_window);

    widget_t *widget = widget_create(win, NULL, &launcher_vtable, ld);
    if (widget) {
        widget->bounds = (rect_t){ 0, 0, fb_width(), fb_height() };
        ld->visible = false;
    }
    g_launcher_widget = widget;
    return widget;
}

void launcher_show(void) {
    if (g_launcher_widget) {
        launcher_data_t *ld = (launcher_data_t *)g_launcher_widget->user_data;
        ld->visible = true;
        ld->search_len = 0;
        ld->search[0] = '\0';
        ld->hovered_index = -1;
        widget_invalidate(g_launcher_widget);
    }
}

void launcher_hide(widget_t *w) {
    launcher_data_t *ld = (launcher_data_t *)w->user_data;
    if (ld) {
        ld->visible = false;
        widget_invalidate(w);
    }
}

/* ============================================================
 * File Manager App
 * ============================================================ */

typedef struct {
    widget_t *list_view;
    char current_path[256];
    struct vfs_file *dir_file;
} file_manager_data_t;

static file_manager_data_t *g_file_manager_data = NULL;

static void file_manager_draw(widget_t *w) {
    file_manager_data_t *fd = (file_manager_data_t *)w->user_data;
    if (!fd) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    /* Background */
    rect_t bg = { 0, 0, w->bounds.w, w->bounds.h };
    draw_rect_filled(buf, pitch, &bg, GUI_COLOR_SURFACE);

    /* Title bar */
    draw_text(buf, pitch, 8, 12, "Files", GUI_COLOR_FG, GUI_COLOR_TITLE_BG);
}

static void file_manager_handle_event(widget_t *w, gui_event_t *e) {
    /* Handle events */
}

static void file_manager_destroy(widget_t *w) {
    file_manager_data_t *fd = (file_manager_data_t *)w->user_data;
    if (fd) {
        if (fd->dir_file) vfs_close(fd->dir_file);
        kfree(fd);
        g_file_manager_data = NULL;
    }
}

static const widget_vtable_t file_manager_vtable = {
    .draw = file_manager_draw,
    .handle_event = file_manager_handle_event,
    .destroy = file_manager_destroy,
};

static void file_manager_on_close(window_t *w) {
    if (g_file_manager_data) {
        if (g_file_manager_data->dir_file) vfs_close(g_file_manager_data->dir_file);
        kfree(g_file_manager_data);
        g_file_manager_data = NULL;
    }
    window_destroy(w);
}

void launch_file_manager(void) {
    window_t *win = window_create(WINDOW_TYPE_NORMAL, "Files",
                                  150, 100, 800, 600,
                                  WINDOW_FLAG_DECORATED | WINDOW_FLAG_RESIZABLE | WINDOW_FLAG_MOVABLE);
    if (!win) return;
    win->on_close = file_manager_on_close;

    g_file_manager_data = (file_manager_data_t *)kzalloc(sizeof(file_manager_data_t));
    if (!g_file_manager_data) {
        window_destroy(win);
        return;
    }

    strcpy(g_file_manager_data->current_path, "/");

    /* Create content widget */
    widget_t *content = widget_create(win, NULL, &file_manager_vtable, g_file_manager_data);
    if (!content) {
        kfree(g_file_manager_data);
        window_destroy(win);
        return;
    }
    content->bounds = (rect_t){ 0, 28, 800, 600 - 28 };

    /* Create sidebar (tabbar) */
    widget_t *tabbar = tabbar_create(win, NULL, 0, 28, 200, 36);
    widget_t *main_content = panel_create(win, NULL, 200, 28, 600, 600 - 28);
    tabbar_add_tab(tabbar, "Home", content);
    tabbar_add_tab(tabbar, "Documents", content);
    tabbar_add_tab(tabbar, "Downloads", content);

    window_show(win);
    window_focus(win);
}

/* ============================================================
 * Text Editor App
 * ============================================================ */

typedef struct {
    widget_t *text_area;
    char filename[256];
} text_editor_data_t;

static text_editor_data_t *g_text_editor_data = NULL;

static void text_editor_on_close(window_t *w) {
    if (g_text_editor_data) {
        kfree(g_text_editor_data);
        g_text_editor_data = NULL;
    }
    window_destroy(w);
}

void launch_text_editor(void) {
    window_t *win = window_create(WINDOW_TYPE_NORMAL, "Editor",
                                  200, 150, 700, 500,
                                  WINDOW_FLAG_DECORATED | WINDOW_FLAG_RESIZABLE | WINDOW_FLAG_MOVABLE);
    if (!win) return;
    win->on_close = text_editor_on_close;

    g_text_editor_data = (text_editor_data_t *)kzalloc(sizeof(text_editor_data_t));
    if (!g_text_editor_data) {
        window_destroy(win);
        return;
    }

    /* Copy default filename */
    strcpy(g_text_editor_data->filename, "untitled.txt");

    /* Toolbar */
    widget_t *toolbar = panel_create(win, NULL, 0, 28, 700, 40);
    button_create(win, toolbar, 8, 4, 80, 32, "New", NULL);
    button_create(win, toolbar, 96, 4, 80, 32, "Open", NULL);
    button_create(win, toolbar, 184, 4, 80, 32, "Save", NULL);

    /* Text area */
    g_text_editor_data->text_area = text_input_create(win, NULL, 0, 28 + 40, 700, 500 - 28 - 40, "Start typing...");

    window_show(win);
    window_focus(win);
}

/* ============================================================
 * Shell Window App
 * ============================================================ */

typedef struct {
    char *output_buffer;
    size_t output_size;
    size_t output_pos;
    char input_buffer[256];
    size_t input_pos;
    int cursor_row;
    int scroll_offset;
} shell_window_data_t;

static shell_window_data_t *g_shell_data = NULL;

static void shell_draw(widget_t *w) {
    shell_window_data_t *sd = (shell_window_data_t *)w->user_data;
    if (!sd) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    /* Background */
    rect_t bg = { 0, 0, w->bounds.w, w->bounds.h };
    draw_rect_filled(buf, pitch, &bg, 0xFF1A1A2E);

    /* Draw output buffer */
    int char_w = 8;
    int char_h = 16;
    int cols = w->bounds.w / char_w;
    int rows = w->bounds.h / char_h;

    /* Draw output lines */
    int output_lines = sd->output_pos / cols;
    for (int row = 0; row < rows - 2; row++) {
        int line_idx = output_lines - (rows - 2) + row;
        if (line_idx < 0) continue;

        int start = line_idx * cols;
        int line_len = cols;
        if (start + line_len > sd->output_pos) line_len = sd->output_pos - start;

        int y = row * char_h;
        for (int i = 0; i < line_len && start + i < sd->output_pos; i++) {
            fb_draw_char_to_buffer(buf, pitch, 8 + i * char_w, y, sd->output_buffer[start + i], GUI_COLOR_FG, 0xFF1A1A2E);
        }
    }

    /* Draw input line */
    int input_row = rows - 2;
    draw_text(buf, pitch, 8, input_row * char_h, sd->input_buffer, GUI_COLOR_ACCENT, 0xFF1A1A2E);

    /* Draw cursor */
    if ((timer_get_ticks() / 500) % 2 == 0) {
        int cursor_x = 8 + sd->input_pos * char_w;
        int cursor_y = input_row * char_h;
        rect_t cursor = { cursor_x, cursor_y, 2, char_h };
        draw_rect_filled(buf, pitch, &cursor, GUI_COLOR_ACCENT);
    }
}

static void shell_handle_event(widget_t *w, gui_event_t *e) {
    shell_window_data_t *sd = (shell_window_data_t *)w->user_data;
    if (!sd) return;

    switch (e->type) {
        case EVENT_KEY_CHAR:
            if (e->keyboard.character >= 32 && e->keyboard.character < 127 && sd->input_pos < 255) {
                sd->input_buffer[sd->input_pos++] = e->keyboard.character;
                sd->input_buffer[sd->input_pos] = '\0';
                widget_invalidate(w);
            }
            break;
        case EVENT_KEY_DOWN:
            if (e->keyboard.keycode == 0x0E) {  /* Backspace */
                if (sd->input_pos > 0) {
                    sd->input_pos--;
                    sd->input_buffer[sd->input_pos] = '\0';
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x1C) {  /* Enter */
                /* Add input to output */
                for (int i = 0; i < sd->input_pos && sd->output_pos < 4096 - 256; i++) {
                    sd->output_buffer[sd->output_pos++] = sd->input_buffer[i];
                }
                sd->output_buffer[sd->output_pos++] = '\n';
                sd->output_buffer[sd->output_pos] = '\0';

                /* Reset input */
                sd->input_pos = 0;
                sd->input_buffer[0] = '\0';

                widget_invalidate(w);
            }
            break;
    }
}

static void shell_destroy(widget_t *w) {
    shell_window_data_t *sd = (shell_window_data_t *)w->user_data;
    if (sd) {
        if (sd->output_buffer) kfree(sd->output_buffer);
        kfree(sd);
        g_shell_data = NULL;
    }
}

static const widget_vtable_t shell_vtable = {
    .draw = shell_draw,
    .handle_event = shell_handle_event,
    .destroy = shell_destroy,
};

static void shell_on_close(window_t *w) {
    if (g_shell_data) {
        kfree(g_shell_data->output_buffer);
        kfree(g_shell_data);
        g_shell_data = NULL;
    }
    window_destroy(w);
}

void launch_shell_window(void) {
    window_t *win = window_create(WINDOW_TYPE_NORMAL, "Shell",
                                  200, 100, 600, 400,
                                  WINDOW_FLAG_DECORATED | WINDOW_FLAG_RESIZABLE | WINDOW_FLAG_MOVABLE);
    if (!win) return;
    win->on_close = shell_on_close;

    g_shell_data = (shell_window_data_t *)kzalloc(sizeof(shell_window_data_t));
    if (!g_shell_data) {
        window_destroy(win);
        return;
    }

    g_shell_data->output_buffer = (char *)kzalloc(4096);
    if (!g_shell_data->output_buffer) {
        kfree(g_shell_data);
        window_destroy(win);
        return;
    }

    g_shell_data->output_pos = 0;
    strcpy(g_shell_data->input_buffer, "");
    g_shell_data->input_pos = 0;

    widget_t *shell_widget = widget_create(win, NULL, &shell_vtable, g_shell_data);
    if (!shell_widget) {
        kfree(g_shell_data->output_buffer);
        kfree(g_shell_data);
        window_destroy(win);
        return;
    }

    shell_widget->bounds = (rect_t){ 0, 28, 600, 400 - 28 };

    /* Add welcome message */
    const char *welcome = "Unknown OS Shell 1.0\nType 'help' for commands\n\n";
    for (int i = 0; welcome[i]; i++) {
        g_shell_data->output_buffer[g_shell_data->output_pos++] = welcome[i];
    }

    window_show(win);
    window_focus(win);
}

/* ============================================================
 * Browser App
 * ============================================================ */

typedef struct {
    widget_t *content_view;
    char url[256];
} browser_data_t;

static browser_data_t *g_browser_data = NULL;

static void browser_draw(widget_t *w) {
    browser_data_t *bd = (browser_data_t *)w->user_data;
    if (!bd) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    /* Background */
    rect_t bg = { 0, 0, w->bounds.w, w->bounds.h };
    draw_rect_filled(buf, pitch, &bg, GUI_COLOR_BG);

    /* Title */
    draw_text(buf, pitch, 8, 12, "Browser", GUI_COLOR_FG, GUI_COLOR_BG);

    /* Placeholder content */
    draw_text(buf, pitch, 100, 180, "Unknown OS Browser", GUI_COLOR_ACCENT, GUI_COLOR_BG);
    draw_text(buf, pitch, 100, 220, "Web engine not yet implemented.", GUI_COLOR_FG, GUI_COLOR_BG);
    draw_text(buf, pitch, 100, 250, "This would use a custom WebView or", GUI_COLOR_FG, GUI_COLOR_BG);
    draw_text(buf, pitch, 100, 280, "port of Servo/WebKit.", GUI_COLOR_FG, GUI_COLOR_BG);
}

static void browser_handle_event(widget_t *w, gui_event_t *e) {
    /* Handle events */
}

static void browser_destroy(widget_t *w) {
    browser_data_t *bd = (browser_data_t *)w->user_data;
    if (bd) {
        kfree(bd);
        g_browser_data = NULL;
    }
}

static const widget_vtable_t browser_vtable = {
    .draw = browser_draw,
    .handle_event = browser_handle_event,
    .destroy = browser_destroy,
};

static void browser_on_close(window_t *w) {
    if (g_browser_data) {
        kfree(g_browser_data);
        g_browser_data = NULL;
    }
    window_destroy(w);
}

void launch_browser(void) {
    window_t *win = window_create(WINDOW_TYPE_NORMAL, "Browser",
                                  100, 50, 1000, 700,
                                  WINDOW_FLAG_DECORATED | WINDOW_FLAG_RESIZABLE | WINDOW_FLAG_MOVABLE);
    if (!win) return;
    win->on_close = browser_on_close;

    g_browser_data = (browser_data_t *)kzalloc(sizeof(browser_data_t));
    if (!g_browser_data) {
        window_destroy(win);
        return;
    }

    strcpy(g_browser_data->url, "https://");

    /* Address bar */
    widget_t *toolbar = panel_create(win, NULL, 0, 28, 1000, 40);
    text_input_create(win, toolbar, 8, 4, 800, 32, "Enter URL...");

    /* Content view */
    g_browser_data->content_view = widget_create(win, NULL, &browser_vtable, g_browser_data);
    if (!g_browser_data->content_view) {
        kfree(g_browser_data);
        window_destroy(win);
        return;
    }
    g_browser_data->content_view->bounds = (rect_t){ 0, 28 + 40, 1000, 700 - 28 - 40 };

    window_show(win);
    window_focus(win);
}