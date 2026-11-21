/*
 * Unknown OS - Window Compositor
 * macOS-inspired compositing window manager
 * Adapted from PearOS/CatfishOS concepts for independent kernel
 * Author: byrex <byrex.me@proton.me>
 */

#include <gui.h>
#include <kernel.h>

/* Forward declarations for callback functions */
static void panel_launcher_callback(widget_t *w);
static void desktop_menu_new_window(void *data);
static void desktop_menu_new_terminal(void *data);
static void desktop_menu_lock_screen(void *data);
static void desktop_menu_shutdown(void *data);
static void desktop_window_on_close(window_t *win);

/* ============================================================
 * Global State
 * ============================================================ */
static desktop_t g_desktop;
static window_id_t g_next_window_id = 1;
static widget_id_t g_next_widget_id = 1;
static uint64_t g_frame_count = 0;

/* Double buffering */
static uint32_t *g_front_buffer = NULL;
static uint32_t *g_back_buffer = NULL;
static bool g_double_buffered = false;

/* ============================================================
 * Init / Shutdown
 * ============================================================ */
void gui_init(void) {
    serial_puts("[GUI] Initializing compositor...\n");

    memset(&g_desktop, 0, sizeof(desktop_t));
    g_desktop.mouse_pos = (point_t){ fb_width() / 2, fb_height() / 2 };
    g_desktop.last_frame = timer_get_ticks();
    g_desktop.needs_redraw = true;

    /* Allocate back buffer for double buffering */
    size_t fb_size = fb_width() * fb_height() * 4;
    g_back_buffer = (uint32_t *)kmalloc(fb_size);
    if (g_back_buffer) {
        g_double_buffered = true;
        serial_printf("[GUI] Double buffering enabled (%u KB)\n", fb_size / 1024);
    } else {
        serial_puts("[GUI] Warning: No double buffer, single-buffered mode\n");
    }

    /* Draw initial wallpaper */
    desktop_draw_wallpaper(&g_desktop);

    /* Create desktop window (root) - WITHOUT click-through so it receives right-clicks */
    window_t *desktop_win = window_create(WINDOW_TYPE_DESKTOP, "Desktop",
                                          0, 0, fb_width(), fb_height(),
                                          WINDOW_FLAG_NO_FOCUS);
    if (desktop_win) {
        desktop_win->on_close = desktop_window_on_close;
        g_desktop.windows = desktop_win;
        desktop_win->desktop = &g_desktop;
    }

    /* Create top panel */
    window_t *panel = window_create(WINDOW_TYPE_PANEL, "Panel",
                                    0, 0, fb_width(), 32,
                                    WINDOW_FLAG_NO_FOCUS | WINDOW_FLAG_ALWAYS_ON_TOP);
    if (panel) {
        g_desktop.panel_window = panel;
        panel->desktop = &g_desktop;
        panel->flags |= WINDOW_FLAG_ALWAYS_ON_TOP;

        /* Add panel widget with app launcher button and clock */
        widget_t *panel_widget = panel_create(panel, NULL, 0, 0, fb_width(), 32);
        if (panel_widget) {
            /* App launcher button (Apple menu equivalent) */
            button_create(panel, panel_widget, 4, 2, 40, 28, "🍎", panel_launcher_callback);

            /* Clock on right side - would need timer to update */
            label_create(panel, panel_widget, fb_width() - 100, 4, "00:00");
        }

        window_show(panel);
    }

    /* Create dock */
    int dock_w = 400;
    int dock_h = 64;
    int dock_x = (fb_width() - dock_w) / 2;
    int dock_y = fb_height() - dock_h - 8;
    window_t *dock = window_create(WINDOW_TYPE_DOCK, "Dock",
                                   dock_x, dock_y, dock_w, dock_h,
                                   WINDOW_FLAG_NO_FOCUS | WINDOW_FLAG_ALWAYS_ON_TOP | WINDOW_FLAG_TRANSPARENT);
    if (dock) {
        g_desktop.dock_window = dock;
        dock->desktop = &g_desktop;
        dock->flags |= WINDOW_FLAG_ALWAYS_ON_TOP | WINDOW_FLAG_TRANSPARENT;

        /* Add dock widget */
        widget_t *dock_widget = dock_create(dock, NULL, 0, 0, dock_w, dock_h);
        if (dock_widget) {
            /* Add default apps to dock */
            dock_add_app(dock_widget, "Terminal", "", launch_terminal);
            dock_add_app(dock_widget, "Settings", "", launch_settings);
            dock_add_app(dock_widget, "Files", "", launch_file_manager);
            dock_add_app(dock_widget, "Editor", "", launch_text_editor);
            dock_add_app(dock_widget, "Browser", "", launch_browser);
        }

        window_show(dock);
    }

    /* Create lock screen (hidden initially) */
    window_t *lock = window_create(WINDOW_TYPE_LOCKSCREEN, "Lock Screen",
                                   0, 0, fb_width(), fb_height(),
                                   WINDOW_FLAG_NO_FOCUS | WINDOW_FLAG_ALWAYS_ON_TOP);
    if (lock) {
        g_desktop.lock_window = lock;
        lock->desktop = &g_desktop;
        lock->flags |= WINDOW_FLAG_ALWAYS_ON_TOP;
        window_hide(lock);
    }

    /* Create app launcher widget (invisible overlay) */
    extern widget_t *launcher_create(window_t *win);
    launcher_create(lock);  /* Use lock window as parent since it covers full screen */

    serial_puts("[GUI] Compositor initialized\n");
}

void gui_shutdown(void) {
    if (g_back_buffer) {
        kfree(g_back_buffer);
        g_back_buffer = NULL;
    }
    serial_puts("[GUI] Compositor shutdown\n");
}

desktop_t *gui_get_desktop(void) {
    return &g_desktop;
}

/* ============================================================
 * Window Management
 * ============================================================ */
window_t *window_create(window_type_t type, const char *title, int x, int y, uint32_t w, uint32_t h, uint32_t flags) {
    window_t *win = (window_t *)kmalloc(sizeof(window_t));
    if (!win) return NULL;

    memset(win, 0, sizeof(window_t));
    win->id = g_next_window_id++;
    win->type = type;
    win->state = WINDOW_STATE_NORMAL;
    win->flags = flags;
    win->bounds = (rect_t){ x, y, w, h };

    /* Title bar for decorated windows */
    if (flags & WINDOW_FLAG_DECORATED) {
        win->title_bar = (rect_t){ 0, 0, w, 28 };
        win->close_btn = (rect_t){ w - 36, 4, 24, 20 };
        win->maximize_btn = (rect_t){ w - 64, 4, 24, 20 };
        win->minimize_btn = (rect_t){ w - 92, 4, 24, 20 };
        win->content_bounds = (rect_t){ 0, 28, w, h - 28 };
    } else {
        win->content_bounds = (rect_t){ 0, 0, w, h };
    }

    if (title) {
        win->title = (char *)kmalloc(strlen(title) + 1);
        if (win->title) strcpy(win->title, title);
    }

    /* Allocate window buffer */
    size_t buf_size = w * h * 4;
    win->buffer = (uint32_t *)kmalloc(buf_size);
    if (!win->buffer) {
        if (win->title) kfree(win->title);
        kfree(win);
        return NULL;
    }
    memset(win->buffer, 0, buf_size);

    win->dirty = true;
    return win;
}

void window_destroy(window_t *win) {
    if (!win) return;

    /* Remove from desktop window list */
    desktop_t *d = &g_desktop;
    if (d->windows == win) {
        d->windows = win->next;
    }
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;

    if (d->focused_window == win) d->focused_window = d->windows;
    if (d->captured_window == win) d->captured_window = NULL;

    /* Destroy widgets */
    widget_t *w = win->root_widget;
    while (w) {
        widget_t *next = w->next_sibling;
        widget_destroy(w);
        w = next;
    }

    if (win->buffer) kfree(win->buffer);
    if (win->title) kfree(win->title);
    kfree(win);
}

void window_show(window_t *win) {
    if (!win) return;

    desktop_t *d = &g_desktop;

    /* Add to front of window list (topmost) */
    win->next = d->windows;
    win->prev = NULL;
    if (d->windows) d->windows->prev = win;
    d->windows = win;
    win->desktop = d;

    win->dirty = true;
    d->needs_redraw = true;

    if (!(win->flags & WINDOW_FLAG_NO_FOCUS)) {
        window_focus(win);
    }
}

void window_hide(window_t *win) {
    if (!win) return;

    desktop_t *d = &g_desktop;

    if (d->windows == win) {
        d->windows = win->next;
    }
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;
    win->next = win->prev = NULL;

    if (d->focused_window == win) d->focused_window = d->windows;
    if (d->captured_window == win) d->captured_window = NULL;

    d->needs_redraw = true;
}

void window_set_title(window_t *win, const char *title) {
    if (!win || !title) return;
    if (win->title) kfree(win->title);
    win->title = (char *)kmalloc(strlen(title) + 1);
    if (win->title) strcpy(win->title, title);
    win->dirty = true;
}

void window_set_bounds(window_t *win, int x, int y, uint32_t w, uint32_t h) {
    if (!win) return;

    bool was_dirty = win->dirty;
    win->bounds = (rect_t){ x, y, w, h };

    if (win->flags & WINDOW_FLAG_DECORATED) {
        win->title_bar = (rect_t){ 0, 0, w, 28 };
        win->close_btn = (rect_t){ w - 36, 4, 24, 20 };
        win->maximize_btn = (rect_t){ w - 64, 4, 24, 20 };
        win->minimize_btn = (rect_t){ w - 92, 4, 24, 20 };
        win->content_bounds = (rect_t){ 0, 28, w, h > 28 ? h - 28 : 0 };
    } else {
        win->content_bounds = (rect_t){ 0, 0, w, h };
    }

    /* Reallocate buffer */
    if (win->buffer) kfree(win->buffer);
    win->buffer = (uint32_t *)kmalloc(w * h * 4);
    if (win->buffer) memset(win->buffer, 0, w * h * 4);

    win->dirty = true;
    if (win->on_resize) win->on_resize(win, w, h);
}

void window_center(window_t *win) {
    if (!win) return;
    window_set_bounds(win,
                      (fb_width() - win->bounds.w) / 2,
                      (fb_height() - win->bounds.h) / 2,
                      win->bounds.w, win->bounds.h);
}

void window_bring_to_front(window_t *win) {
    if (!win || !win->desktop) return;

    desktop_t *d = win->desktop;

    if (d->windows == win) return;  /* Already at front */

    /* Unlink */
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;

    /* Link at front */
    win->next = d->windows;
    win->prev = NULL;
    if (d->windows) d->windows->prev = win;
    d->windows = win;

    d->needs_redraw = true;
}

void window_send_to_back(window_t *win) {
    if (!win || !win->desktop) return;

    desktop_t *d = win->desktop;
    window_t *last = d->windows;
    while (last && last->next) last = last->next;
    if (last == win) return;  /* Already at back */

    /* Unlink */
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;

    /* Link at back */
    win->next = NULL;
    win->prev = last;
    if (last) last->next = win;
    else d->windows = win;

    d->needs_redraw = true;
}

void window_focus(window_t *win) {
    if (!win || !win->desktop) return;

    desktop_t *d = win->desktop;
    if (d->focused_window == win) return;

    if (d->focused_window && d->focused_window->on_focus) {
        d->focused_window->on_focus(d->focused_window, false);
    }

    d->focused_window = win;
    window_bring_to_front(win);

    if (win->on_focus) win->on_focus(win, true);
    win->dirty = true;
}

void window_invalidate(window_t *win) {
    if (win) win->dirty = true;
}

void window_invalidate_rect(window_t *win, rect_t *r) {
    if (win) win->dirty = true;  /* Simple: full redraw for now */
}

/* ============================================================
 * Window Rendering
 * ============================================================ */
void draw_window_decorations(window_t *win) {
    if (!(win->flags & WINDOW_FLAG_DECORATED)) return;

    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;
    rect_t tb = win->title_bar;

    /* Title bar background */
    draw_rect_filled(buf, pitch, &tb, GUI_COLOR_TITLE_BG);

    /* Title text */
    if (win->title) {
        draw_text(buf, pitch, 12, 6, win->title, GUI_COLOR_TITLE_FG, GUI_COLOR_TITLE_BG);
    }

    /* Traffic light buttons (macOS style) */
    /* Close - red */
    draw_rounded_rect(buf, pitch, &win->close_btn, 10, GUI_COLOR_BTN_CLOSE);
    draw_text(buf, pitch, win->close_btn.x + 6, win->close_btn.y + 2, "×", GUI_COLOR_BLACK, GUI_COLOR_BTN_CLOSE);

    /* Maximize - yellow */
    draw_rounded_rect(buf, pitch, &win->maximize_btn, 10, GUI_COLOR_BTN_MAX);
    draw_text(buf, pitch, win->maximize_btn.x + 5, win->maximize_btn.y + 2, "□", GUI_COLOR_BLACK, GUI_COLOR_BTN_MAX);

    /* Minimize - green */
    draw_rounded_rect(buf, pitch, &win->minimize_btn, 10, GUI_COLOR_BTN_MIN);
    draw_text(buf, pitch, win->minimize_btn.x + 6, win->minimize_btn.y + 2, "−", GUI_COLOR_BLACK, GUI_COLOR_BTN_MIN);

    /* Window border */
    rect_t border = { 0, 0, win->bounds.w, win->bounds.h };
    draw_rect(buf, pitch, &border, GUI_COLOR_BORDER);

    /* Bottom resize handle */
    rect_t resize = { win->bounds.w - 16, win->bounds.h - 16, 16, 16 };
    draw_text(buf, pitch, resize.x + 2, resize.y + 2, "⌄", GUI_COLOR_BORDER, GUI_COLOR_TITLE_BG);
}

void window_render(window_t *win) {
    if (!win || !win->buffer) return;

    /* Clear window buffer */
    memset(win->buffer, 0, win->bounds.w * win->bounds.h * 4);

    /* Draw decorations */
    draw_window_decorations(win);

    /* Render root widget (which renders children) */
    if (win->root_widget) {
        widget_t *w = win->root_widget;
        while (w) {
            if (w->visible && w->vtable && w->vtable->draw) {
                w->vtable->draw(w);
            }
            w = w->next_sibling;
        }
    }

    win->dirty = false;
}

/* ============================================================
 * Desktop Compositing
 * ============================================================ */
void desktop_render(desktop_t *d) {
    if (!d) return;

    uint32_t *target = g_double_buffered ? g_back_buffer : g_fb_addr;
    uint32_t pitch = fb_pitch();

    /* Draw wallpaper first */
    if (d->wallpaper_buffer) {
        blit_buffer(target, pitch, d->wallpaper_buffer, fb_width(),
                    &(rect_t){0, 0, fb_width(), fb_height()}, 0, 0);
    } else {
        /* Draw gradient wallpaper */
        for (uint32_t y = 0; y < fb_height(); y++) {
            float t = (float)y / fb_height();
            uint32_t r = 30 + t * 20;
            uint32_t g = 30 + t * 10;
            uint32_t b = 46 + t * 30;
            uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
            uint32_t *line = &target[y * pitch];
            for (uint32_t x = 0; x < fb_width(); x++) line[x] = color;
        }
    }

    /* Collect windows in z-order (back to front) */
    window_t *windows[64];
    int count = 0;
    window_t *win = d->windows;
    while (win && count < 64) {
        windows[count++] = win;
        win = win->next;
    }

    /* Render from back to front */
    for (int i = count - 1; i >= 0; i--) {
        win = windows[i];
        if (!win->buffer) continue;

        /* Render window if dirty */
        if (win->dirty) {
            window_render(win);
        }

        /* Composite window onto screen */
        if (win->flags & WINDOW_FLAG_TRANSPARENT) {
            blit_buffer_alpha(target, pitch, win->buffer, win->bounds.w,
                              &(rect_t){0, 0, win->bounds.w, win->bounds.h},
                              win->bounds.x, win->bounds.y, 220);
        } else {
            blit_buffer(target, pitch, win->buffer, win->bounds.w,
                        &(rect_t){0, 0, win->bounds.w, win->bounds.h},
                        win->bounds.x, win->bounds.y);
        }
    }

    /* Draw mouse cursor */
    draw_mouse_cursor(target, pitch, d->mouse_pos.x, d->mouse_pos.y);

    d->needs_redraw = false;
}

void desktop_composite(desktop_t *d) {
    if (!d || !d->needs_redraw) return;
    desktop_render(d);

    /* Swap buffers if double buffered */
    if (g_double_buffered && g_back_buffer) {
        memcpy(g_fb_addr, g_back_buffer, fb_width() * fb_height() * 4);
    }

    g_frame_count++;
}

/* ============================================================
 * Mouse Cursor
 * ============================================================ */
static const uint8_t cursor_bits[16][16] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

void draw_mouse_cursor(uint32_t *buf, uint32_t pitch, int x, int y) {
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 16; cx++) {
            int px = x + cx;
            int py = y + cy;
            if (px >= 0 && px < (int)fb_width() && py >= 0 && py < (int)fb_height()) {
                if (cursor_bits[cy][cx]) {
                    buf[py * pitch + px] = GUI_COLOR_WHITE;
                } else if (cx < 15 && cy < 15 && cursor_bits[cy][cx+1] && cursor_bits[cy+1][cx]) {
                    buf[py * pitch + px] = GUI_COLOR_BLACK;
                }
            }
        }
    }
}

/* ============================================================
 * Window Drag/Resize State
 * ============================================================ */
typedef struct {
    bool active;
    bool resizing;
    int start_x, start_y;
    int win_start_x, win_start_y;
    uint32_t win_start_w, win_start_h;
    window_t *window;
} drag_state_t;

static drag_state_t g_drag_state = {0};

/* Context menu for desktop */
static widget_t *g_desktop_menu = NULL;

/* ============================================================
 * Input Handling
 * ============================================================ */
void gui_handle_mouse_move(int32_t x, int32_t y) {
    desktop_t *d = &g_desktop;
    d->mouse_pos.x = x;
    d->mouse_pos.y = y;

    /* Clamp to screen */
    if (x < 0) d->mouse_pos.x = 0;
    if (x >= (int)fb_width()) d->mouse_pos.x = fb_width() - 1;
    if (y < 0) d->mouse_pos.y = 0;
    if (y >= (int)fb_height()) d->mouse_pos.y = fb_height() - 1;

    /* Handle window drag/resize */
    if (g_drag_state.active && g_drag_state.window) {
        window_t *win = g_drag_state.window;
        if (g_drag_state.resizing) {
            int new_w = g_drag_state.win_start_w + (x - g_drag_state.start_x);
            int new_h = g_drag_state.win_start_h + (y - g_drag_state.start_y);
            if (new_w < 200) new_w = 200;
            if (new_h < 150) new_h = 150;
            if (new_w > fb_width()) new_w = fb_width();
            if (new_h > fb_height()) new_h = fb_height();
            window_set_bounds(win, win->bounds.x, win->bounds.y, new_w, new_h);
        } else {
            int new_x = g_drag_state.win_start_x + (x - g_drag_state.start_x);
            int new_y = g_drag_state.win_start_y + (y - g_drag_state.start_y);
            /* Clamp to screen */
            if (new_x < 0) new_x = 0;
            if (new_y < 0) new_y = 0;
            if (new_x + (int)win->bounds.w > fb_width()) new_x = fb_width() - win->bounds.w;
            if (new_y + (int)win->bounds.h > fb_height()) new_y = fb_height() - win->bounds.h;
            window_set_bounds(win, new_x, new_y, win->bounds.w, win->bounds.h);
        }
        d->needs_redraw = true;
        return;
    }

    /* Find window under cursor */
    window_t *win = d->windows;
    window_t *target = NULL;
    while (win) {
        if (x >= win->bounds.x && x < win->bounds.x + (int)win->bounds.w &&
            y >= win->bounds.y && y < win->bounds.y + (int)win->bounds.h) {
            if (!(win->flags & WINDOW_FLAG_CLICK_THROUGH)) {
                target = win;
                break;
            }
        }
        win = win->next;
    }

    /* Send mouse move event to target window */
    if (target) {
        gui_event_t ev = {0};
        ev.type = EVENT_MOUSE_MOVE;
        ev.window_id = target->id;
        ev.mouse.x = x - target->bounds.x;
        ev.mouse.y = y - target->bounds.y;
        ev.mouse.buttons = d->mouse_buttons;

        /* Dispatch to widgets */
        widget_t *w = target->root_widget;
        while (w) {
            if (w->visible && w->vtable && w->vtable->handle_event) {
                w->vtable->handle_event(w, &ev);
            }
            w = w->next_sibling;
        }
    }

    d->needs_redraw = true;
}

void gui_handle_mouse_down(int32_t x, int32_t y, uint32_t btn) {
    desktop_t *d = &g_desktop;
    d->mouse_buttons |= btn;

    /* Right click on desktop - show context menu */
    if (btn & MOUSE_BTN_RIGHT) {
        window_t *win = d->windows;
        bool on_desktop = true;
        while (win) {
            if (x >= win->bounds.x && x < win->bounds.x + (int)win->bounds.w &&
                y >= win->bounds.y && y < win->bounds.y + (int)win->bounds.h) {
                if (!(win->flags & WINDOW_FLAG_CLICK_THROUGH)) {
                    on_desktop = false;
                    break;
                }
            }
            win = win->next;
        }
        if (on_desktop) {
            if (!g_desktop_menu) {
                g_desktop_menu = menu_create(d->windows, NULL, 0, 0, 180);
                menu_add_item(g_desktop_menu, "New Window", desktop_menu_new_window, NULL);
                menu_add_item(g_desktop_menu, "New Terminal", desktop_menu_new_terminal, NULL);
                menu_add_separator(g_desktop_menu);
                menu_add_item(g_desktop_menu, "Lock Screen", desktop_menu_lock_screen, NULL);
                menu_add_item(g_desktop_menu, "Shutdown", desktop_menu_shutdown, NULL);
            }
            menu_show_at(g_desktop_menu, x, y);
            d->needs_redraw = true;
            return;
        }
    }

    /* Find window under cursor */
    win = d->windows;
    while (win) {
        if (x >= win->bounds.x && x < win->bounds.x + (int)win->bounds.w &&
            y >= win->bounds.y && y < win->bounds.y + (int)win->bounds.h) {
            if (!(win->flags & WINDOW_FLAG_CLICK_THROUGH)) {
                window_focus(win);
                d->captured_window = win;

                /* Check for window decoration interactions */
                if (win->flags & WINDOW_FLAG_DECORATED) {
                    int rel_x = x - win->bounds.x;
                    int rel_y = y - win->bounds.y;

                    /* Close button */
                    if (rect_contains(&win->close_btn, rel_x, rel_y)) {
                        if (win->on_close) win->on_close(win);
                        else window_destroy(win);
                        d->needs_redraw = true;
                        return;
                    }

                    /* Maximize button */
                    if (rect_contains(&win->maximize_btn, rel_x, rel_y)) {
                        if (win->state == WINDOW_STATE_MAXIMIZED) {
                            win->state = WINDOW_STATE_NORMAL;
                            /* Restore previous bounds - simplified */
                        } else {
                            win->state = WINDOW_STATE_MAXIMIZED;
                            window_set_bounds(win, 0, 32, fb_width(), fb_height() - 32);
                        }
                        d->needs_redraw = true;
                        return;
                    }

                    /* Minimize button */
                    if (rect_contains(&win->minimize_btn, rel_x, rel_y)) {
                        win->state = WINDOW_STATE_MINIMIZED;
                        window_hide(win);
                        d->needs_redraw = true;
                        return;
                    }

                    /* Title bar drag */
                    if (rect_contains(&win->title_bar, rel_x, rel_y) && (win->flags & WINDOW_FLAG_MOVABLE)) {
                        g_drag_state.active = true;
                        g_drag_state.resizing = false;
                        g_drag_state.start_x = x;
                        g_drag_state.start_y = y;
                        g_drag_state.win_start_x = win->bounds.x;
                        g_drag_state.win_start_y = win->bounds.y;
                        g_drag_state.window = win;
                        d->needs_redraw = true;
                        return;
                    }

                    /* Resize handle (bottom-right 16x16) */
                    rect_t resize_handle = { (int)win->bounds.w - 16, (int)win->bounds.h - 16, 16, 16 };
                    if (rect_contains(&resize_handle, rel_x, rel_y) && (win->flags & WINDOW_FLAG_RESIZABLE)) {
                        g_drag_state.active = true;
                        g_drag_state.resizing = true;
                        g_drag_state.start_x = x;
                        g_drag_state.start_y = y;
                        g_drag_state.win_start_w = win->bounds.w;
                        g_drag_state.win_start_h = win->bounds.h;
                        g_drag_state.window = win;
                        d->needs_redraw = true;
                        return;
                    }
                }

                gui_event_t ev = {0};
                ev.type = EVENT_MOUSE_DOWN;
                ev.window_id = win->id;
                ev.mouse.x = x - win->bounds.x;
                ev.mouse.y = y - win->bounds.y;
                ev.mouse.buttons = btn;

                widget_t *w = win->root_widget;
                while (w) {
                    if (w->visible && w->vtable && w->vtable->handle_event) {
                        w->vtable->handle_event(w, &ev);
                    }
                    w = w->next_sibling;
                }
                break;
            }
        }
        win = win->next;
    }

    /* Hide desktop menu if clicking elsewhere */
    if (g_desktop_menu) {
        menu_hide(g_desktop_menu);
    }

    d->needs_redraw = true;
}

void gui_handle_mouse_up(int32_t x, int32_t y, uint32_t btn) {
    desktop_t *d = &g_desktop;
    d->mouse_buttons &= ~btn;

    /* End drag/resize */
    if (g_drag_state.active) {
        g_drag_state.active = false;
        g_drag_state.window = NULL;
    }

    window_t *win = d->captured_window ? d->captured_window : d->focused_window;
    if (win) {
        gui_event_t ev = {0};
        ev.type = EVENT_MOUSE_UP;
        ev.window_id = win->id;
        ev.mouse.x = x - win->bounds.x;
        ev.mouse.y = y - win->bounds.y;
        ev.mouse.buttons = btn;

        widget_t *w = win->root_widget;
        while (w) {
            if (w->visible && w->vtable && w->vtable->handle_event) {
                w->vtable->handle_event(w, &ev);
            }
            w = w->next_sibling;
        }
    }

    d->captured_window = NULL;
    d->needs_redraw = true;
}

void gui_handle_mouse_scroll(int32_t x, int32_t y, int32_t delta) {
    desktop_t *d = &g_desktop;
    window_t *win = d->windows;
    while (win) {
        if (x >= win->bounds.x && x < win->bounds.x + (int)win->bounds.w &&
            y >= win->bounds.y && y < win->bounds.y + (int)win->bounds.h) {
            if (!(win->flags & WINDOW_FLAG_CLICK_THROUGH)) {
                gui_event_t ev = {0};
                ev.type = EVENT_MOUSE_SCROLL;
                ev.window_id = win->id;
                ev.mouse.x = x - win->bounds.x;
                ev.mouse.y = y - win->bounds.y;
                ev.mouse.buttons = delta;

                widget_t *w = win->root_widget;
                while (w) {
                    if (w->visible && w->vtable && w->vtable->handle_event) {
                        w->vtable->handle_event(w, &ev);
                    }
                    w = w->next_sibling;
                }
                break;
            }
        }
        win = win->next;
    }
}

void gui_handle_key_down(uint32_t keycode, uint32_t modifiers) {
    desktop_t *d = &g_desktop;
    d->modifiers = modifiers;

    if (d->focused_window) {
        gui_event_t ev = {0};
        ev.type = EVENT_KEY_DOWN;
        ev.window_id = d->focused_window->id;
        ev.keyboard.keycode = keycode;
        ev.keyboard.modifiers = modifiers;

        widget_t *w = d->focused_window->root_widget;
        while (w) {
            if (w->visible && w->vtable && w->vtable->handle_event) {
                w->vtable->handle_event(w, &ev);
            }
            w = w->next_sibling;
        }
    }

    /* Global shortcuts */
    if (modifiers & 0x04) {  /* Super/Command key */
        if (keycode == 0x2C) {  /* Space - app launcher */
            extern void launcher_show(void);
            launcher_show();
        }
    }
}

void gui_handle_key_up(uint32_t keycode, uint32_t modifiers) {
    desktop_t *d = &g_desktop;
    d->modifiers = modifiers;

    if (d->focused_window) {
        gui_event_t ev = {0};
        ev.type = EVENT_KEY_UP;
        ev.window_id = d->focused_window->id;
        ev.keyboard.keycode = keycode;
        ev.keyboard.modifiers = modifiers;

        widget_t *w = d->focused_window->root_widget;
        while (w) {
            if (w->visible && w->vtable && w->vtable->handle_event) {
                w->vtable->handle_event(w, &ev);
            }
            w = w->next_sibling;
        }
    }
}

void gui_handle_char(char c) {
    desktop_t *d = &g_desktop;

    if (d->focused_window) {
        gui_event_t ev = {0};
        ev.type = EVENT_KEY_CHAR;
        ev.window_id = d->focused_window->id;
        ev.keyboard.character = c;
        ev.keyboard.modifiers = d->modifiers;

        widget_t *w = d->focused_window->root_widget;
        while (w) {
            if (w->visible && w->vtable && w->vtable->handle_event) {
                w->vtable->handle_event(w, &ev);
            }
            w = w->next_sibling;
        }
    }
}

/* ============================================================
 * Event Loop
 * ============================================================ */
void gui_process_events(void) {
    desktop_composite(&g_desktop);
}

void gui_main_loop(void) {
    serial_puts("[GUI] Entering main loop\n");

    while (true) {
        gui_process_events();

        /* Yield to scheduler */
        asm volatile("pause");
        if (g_desktop.needs_redraw) {
            sched_yield();
        }
    }
}

/* ============================================================
 * Widget System
 * ============================================================ */
widget_t *widget_create(window_t *win, widget_t *parent, const widget_vtable_t *vtable, void *user_data) {
    widget_t *w = (widget_t *)kmalloc(sizeof(widget_t));
    if (!w) return NULL;

    memset(w, 0, sizeof(widget_t));
    w->id = g_next_widget_id++;
    w->window = win;
    w->parent = parent;
    w->vtable = vtable;
    w->user_data = user_data;
    w->visible = true;

    /* Add to parent's children or window's root */
    if (parent) {
        w->next_sibling = parent->children;
        parent->children = w;
    } else if (win) {
        w->next_sibling = win->root_widget;
        win->root_widget = w;
    }

    return w;
}

void widget_destroy(widget_t *w) {
    if (!w) return;

    /* Destroy children first */
    widget_t *child = w->children;
    while (child) {
        widget_t *next = child->next_sibling;
        widget_destroy(child);
        child = next;
    }

    /* Unlink from parent/sibling list */
    if (w->parent) {
        widget_t **pp = &w->parent->children;
        while (*pp && *pp != w) pp = &(*pp)->next_sibling;
        if (*pp) *pp = w->next_sibling;
    } else if (w->window) {
        widget_t **pp = &w->window->root_widget;
        while (*pp && *pp != w) pp = &(*pp)->next_sibling;
        if (*pp) *pp = w->next_sibling;
    }

    if (w->vtable && w->vtable->destroy) {
        w->vtable->destroy(w);
    }

    kfree(w);
}

void widget_set_bounds(widget_t *w, rect_t *b) {
    if (w && b) w->bounds = *b;
}

void widget_show(widget_t *w) {
    if (w) w->visible = true;
}

void widget_hide(widget_t *w) {
    if (w) w->visible = false;
}

void widget_invalidate(widget_t *w) {
    if (w && w->window) w->window->dirty = true;
}

/* ============================================================
 * Built-in Widgets: Label
 * ============================================================ */
static void label_draw(widget_t *w) {
    if (!w->user_data) return;
    window_t *win = w->window;
    draw_text(win->buffer, win->bounds.w,
              w->bounds.x, w->bounds.y,
              (const char *)w->user_data,
              GUI_COLOR_FG, GUI_COLOR_SURFACE);
}

static const widget_vtable_t label_vtable = {
    .draw = label_draw,
    .handle_event = NULL,
    .destroy = NULL,
};

widget_t *label_create(window_t *win, widget_t *parent, int x, int y, const char *text) {
    widget_t *w = widget_create(win, parent, &label_vtable, (void *)text);
    if (w) {
        w->bounds = (rect_t){ x, y, strlen(text) * 8, 16 };
    }
    return w;
}

/* ============================================================
 * Built-in Widgets: Text Input
 * ============================================================ */
#define TEXT_INPUT_MAX_LEN 255

typedef struct {
    char *text;
    size_t len;
    size_t capacity;
    char *placeholder;
    bool focused;
    size_t cursor_pos;
    uint64_t blink_time;
} text_input_data_t;

static void text_input_draw(widget_t *w) {
    text_input_data_t *tid = (text_input_data_t *)w->user_data;
    if (!tid) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;
    rect_t *r = &w->bounds;

    gui_color_t bg = tid->focused ? GUI_COLOR_SURFACE_HL : GUI_COLOR_SURFACE;
    gui_color_t border = tid->focused ? GUI_COLOR_ACCENT : GUI_COLOR_BORDER;
    gui_color_t fg = GUI_COLOR_FG;
    gui_color_t placeholder_fg = GUI_COLOR_SURFACE_HL;

    draw_rounded_rect(buf, pitch, r, 6, bg);
    draw_rounded_rect_outline(buf, pitch, r, 6, border);

    int text_x = r->x + 8;
    int text_y = r->y + (r->h - 16) / 2;

    if (tid->len > 0) {
        draw_text(buf, pitch, text_x, text_y, tid->text, fg, bg);
    } else if (tid->placeholder) {
        draw_text(buf, pitch, text_x, text_y, tid->placeholder, placeholder_fg, bg);
    }

    /* Draw cursor */
    if (tid->focused && ((timer_get_ticks() - tid->blink_time) / 500) % 2 == 0) {
        int cursor_x = text_x + tid->cursor_pos * 8;
        rect_t cursor = { cursor_x, text_y, 2, 16 };
        draw_rect_filled(buf, pitch, &cursor, GUI_COLOR_ACCENT);
    }
}

static void text_input_handle_event(widget_t *w, gui_event_t *e) {
    text_input_data_t *tid = (text_input_data_t *)w->user_data;
    if (!tid) return;

    switch (e->type) {
        case EVENT_MOUSE_DOWN:
            if (rect_contains(&w->bounds, e->mouse.x, e->mouse.y) && (e->mouse.buttons & MOUSE_BTN_LEFT)) {
                tid->focused = true;
                tid->blink_time = timer_get_ticks();
                widget_invalidate(w);
            } else {
                tid->focused = false;
                widget_invalidate(w);
            }
            break;
        case EVENT_KEY_CHAR:
            if (tid->focused && e->keyboard.character >= 32 && e->keyboard.character < 127 && tid->len < TEXT_INPUT_MAX_LEN - 1) {
                memmove(&tid->text[tid->cursor_pos + 1], &tid->text[tid->cursor_pos], tid->len - tid->cursor_pos + 1);
                tid->text[tid->cursor_pos++] = e->keyboard.character;
                tid->len++;
                widget_invalidate(w);
            }
            break;
        case EVENT_KEY_DOWN:
            if (!tid->focused) break;
            if (e->keyboard.keycode == 0x0E) {  /* Backspace */
                if (tid->cursor_pos > 0) {
                    memmove(&tid->text[tid->cursor_pos - 1], &tid->text[tid->cursor_pos], tid->len - tid->cursor_pos + 1);
                    tid->cursor_pos--;
                    tid->len--;
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x4B) {  /* Left arrow */
                if (tid->cursor_pos > 0) {
                    tid->cursor_pos--;
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x4D) {  /* Right arrow */
                if (tid->cursor_pos < tid->len) {
                    tid->cursor_pos++;
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x1C) {  /* Enter */
                /* Trigger callback if set */
            }
            break;
    }
}

static void text_input_destroy(widget_t *w) {
    text_input_data_t *tid = (text_input_data_t *)w->user_data;
    if (tid) {
        if (tid->text) kfree(tid->text);
        if (tid->placeholder) kfree(tid->placeholder);
        kfree(tid);
    }
}

static const widget_vtable_t text_input_vtable = {
    .draw = text_input_draw,
    .handle_event = text_input_handle_event,
    .destroy = text_input_destroy,
};

widget_t *text_input_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h, const char *placeholder) {
    text_input_data_t *tid = (text_input_data_t *)kmalloc(sizeof(text_input_data_t));
    if (!tid) return NULL;
    memset(tid, 0, sizeof(text_input_data_t));
    tid->capacity = TEXT_INPUT_MAX_LEN;
    tid->text = (char *)kzalloc(TEXT_INPUT_MAX_LEN);
    if (placeholder) {
        tid->placeholder = (char *)kmalloc(strlen(placeholder) + 1);
        if (tid->placeholder) strcpy(tid->placeholder, placeholder);
    }
    tid->cursor_pos = 0;

    widget_t *widget = widget_create(win, parent, &text_input_vtable, tid);
    if (widget) {
        widget->bounds = (rect_t){ x, y, w, h };
    }
    return widget;
}

/* ============================================================
 * Built-in Widgets: Button
 * ============================================================ */
typedef struct {
    char *text;
    void (*callback)(widget_t *);
    bool pressed;
    bool hovered;
} button_data_t;

static void button_draw(widget_t *w) {
    button_data_t *bd = (button_data_t *)w->user_data;
    if (!bd) return;

    window_t *win = w->window;
    gui_color_t bg = bd->pressed ? GUI_COLOR_ACCENT :
                     bd->hovered ? GUI_COLOR_SURFACE_HL : GUI_COLOR_SURFACE;
    gui_color_t fg = GUI_COLOR_FG;
    gui_color_t border = bd->hovered ? GUI_COLOR_ACCENT : GUI_COLOR_BORDER;

    draw_rounded_rect(win->buffer, win->bounds.w, &w->bounds, 6, bg);
    draw_rounded_rect_outline(win->buffer, win->bounds.w, &w->bounds, 6, border);

    int text_x = w->bounds.x + (w->bounds.w - strlen(bd->text) * 8) / 2;
    int text_y = w->bounds.y + (w->bounds.h - 16) / 2;
    draw_text(win->buffer, win->bounds.w, text_x, text_y, bd->text, fg, bg);
}

static void button_handle_event(widget_t *w, gui_event_t *e) {
    button_data_t *bd = (button_data_t *)w->user_data;
    if (!bd) return;

    switch (e->type) {
        case EVENT_MOUSE_MOVE: {
            bool was_hovered = bd->hovered;
            bd->hovered = rect_contains(&w->bounds, e->mouse.x, e->mouse.y);
            if (was_hovered != bd->hovered) widget_invalidate(w);
            break;
        }
        case EVENT_MOUSE_DOWN:
            if (rect_contains(&w->bounds, e->mouse.x, e->mouse.y) &&
                (e->mouse.buttons & MOUSE_BTN_LEFT)) {
                bd->pressed = true;
                widget_invalidate(w);
            }
            break;
        case EVENT_MOUSE_UP:
            if (bd->pressed && rect_contains(&w->bounds, e->mouse.x, e->mouse.y) &&
                (e->mouse.buttons & MOUSE_BTN_LEFT)) {
                if (bd->callback) bd->callback(w);
            }
            bd->pressed = false;
            widget_invalidate(w);
            break;
    }
}

static void button_destroy(widget_t *w) {
    button_data_t *bd = (button_data_t *)w->user_data;
    if (bd) {
        if (bd->text) kfree(bd->text);
        kfree(bd);
    }
}

static const widget_vtable_t button_vtable = {
    .draw = button_draw,
    .handle_event = button_handle_event,
    .destroy = button_destroy,
};

widget_t *button_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h, const char *text, void (*callback)(widget_t *)) {
    button_data_t *bd = (button_data_t *)kmalloc(sizeof(button_data_t));
    if (!bd) return NULL;

    bd->text = (char *)kmalloc(strlen(text) + 1);
    if (!bd->text) { kfree(bd); return NULL; }
    strcpy(bd->text, text);
    bd->callback = callback;
    bd->pressed = false;
    bd->hovered = false;

    widget_t *widget = widget_create(win, parent, &button_vtable, bd);
    if (widget) {
        widget->bounds = (rect_t){ x, y, w, h };
    }
    return widget;
}

/* ============================================================
 * Built-in Widgets: Panel (container)
 * ============================================================ */
static void panel_draw(widget_t *w) {
    window_t *win = w->window;
    draw_rect_filled(win->buffer, win->bounds.w, &w->bounds, GUI_COLOR_PANEL_BG);
}

static const widget_vtable_t panel_vtable = {
    .draw = panel_draw,
    .handle_event = NULL,
    .destroy = NULL,
};

widget_t *panel_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h) {
    widget_t *widget = widget_create(win, parent, &panel_vtable, NULL);
    if (widget) {
        widget->bounds = (rect_t){ x, y, w, h };
    }
    return widget;
}

/* ============================================================
 * Dock Widget (macOS-style)
 * ============================================================ */
#define DOCK_MAX_ITEMS 16
#define DOCK_ITEM_SIZE 48
#define DOCK_ITEM_PADDING 8

typedef struct {
    char *name;
    char *icon_path;
    void (*launch)(void *);
    void *launch_data;
    bool active;
    float scale;  /* For magnification animation */
} dock_item_t;

typedef struct {
    dock_item_t items[DOCK_MAX_ITEMS];
    int item_count;
    int active_index;
    int hovered_index;
} dock_data_t;

static void dock_draw(widget_t *w) {
    dock_data_t *dd = (dock_data_t *)w->user_data;
    if (!dd) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    /* Dock background - rounded rectangle with transparency */
    rect_t bg = { 0, 0, w->bounds.w, w->bounds.h };
    draw_rounded_rect(buf, pitch, &bg, 16, GUI_COLOR_DOCK_BG);

    /* Draw items */
    int total_w = dd->item_count * (DOCK_ITEM_SIZE + DOCK_ITEM_PADDING) - DOCK_ITEM_PADDING;
    int start_x = (w->bounds.w - total_w) / 2;
    int item_y = (w->bounds.h - DOCK_ITEM_SIZE) / 2;

    for (int i = 0; i < dd->item_count; i++) {
        dock_item_t *item = &dd->items[i];
        float scale = item->scale;
        if (i == dd->hovered_index) scale = 1.3f;
        else if (scale < 1.0f) scale = 1.0f;

        int size = (int)(DOCK_ITEM_SIZE * scale);
        int x = start_x + i * (DOCK_ITEM_SIZE + DOCK_ITEM_PADDING) + (DOCK_ITEM_SIZE - size) / 2;
        int y = item_y + (DOCK_ITEM_SIZE - size) / 2;

        rect_t item_rect = { x, y, size, size };
        gui_color_t bg_color = item->active ? GUI_COLOR_DOCK_ACTIVE : GUI_COLOR_DOCK_ITEM;
        if (i == dd->hovered_index) bg_color = GUI_COLOR_DOCK_HOVER;

        draw_rounded_rect(buf, pitch, &item_rect, 10, bg_color);

        /* Draw item indicator (dot) for active apps */
        if (item->active) {
            rect_t dot = { x + size/2 - 3, y + size - 6, 6, 6 };
            draw_rounded_rect(buf, pitch, &dot, 3, GUI_COLOR_ACCENT);
        }

        /* Draw app name on hover */
        if (i == dd->hovered_index && item->name) {
            int text_w = strlen(item->name) * 8;
            int text_x = x + (size - text_w) / 2;
            int text_y = y - 20;
            if (text_x < 0) text_x = 0;
            if (text_x + text_w > (int)w->bounds.w) text_x = w->bounds.w - text_w;
            draw_text(buf, pitch, text_x, text_y, item->name, GUI_COLOR_FG, GUI_COLOR_DOCK_BG);
        }
    }
}

static void dock_handle_event(widget_t *w, gui_event_t *e) {
    dock_data_t *dd = (dock_data_t *)w->user_data;
    if (!dd) return;

    switch (e->type) {
        case EVENT_MOUSE_MOVE: {
            int old_hover = dd->hovered_index;
            dd->hovered_index = -1;

            int total_w = dd->item_count * (DOCK_ITEM_SIZE + DOCK_ITEM_PADDING) - DOCK_ITEM_PADDING;
            int start_x = (w->bounds.w - total_w) / 2;
            int item_y = (w->bounds.h - DOCK_ITEM_SIZE) / 2;

            for (int i = 0; i < dd->item_count; i++) {
                int x = start_x + i * (DOCK_ITEM_SIZE + DOCK_ITEM_PADDING);
                int size = DOCK_ITEM_SIZE;
                if (e->mouse.x >= x && e->mouse.x < x + size &&
                    e->mouse.y >= item_y && e->mouse.y < item_y + size) {
                    dd->hovered_index = i;
                    break;
                }
            }

            if (old_hover != dd->hovered_index) widget_invalidate(w);
            break;
        }
        case EVENT_MOUSE_DOWN:
            if (dd->hovered_index >= 0 && (e->mouse.buttons & MOUSE_BTN_LEFT)) {
                dock_item_t *item = &dd->items[dd->hovered_index];
                if (item->launch) item->launch(item->launch_data);
            }
            break;
    }
}

static void dock_destroy(widget_t *w) {
    dock_data_t *dd = (dock_data_t *)w->user_data;
    if (dd) {
        for (int i = 0; i < dd->item_count; i++) {
            if (dd->items[i].name) kfree(dd->items[i].name);
            if (dd->items[i].icon_path) kfree(dd->items[i].icon_path);
        }
        kfree(dd);
    }
}

static const widget_vtable_t dock_vtable = {
    .draw = dock_draw,
    .handle_event = dock_handle_event,
    .destroy = dock_destroy,
};

widget_t *dock_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h) {
    dock_data_t *dd = (dock_data_t *)kmalloc(sizeof(dock_data_t));
    if (!dd) return NULL;
    memset(dd, 0, sizeof(dock_data_t));

    widget_t *widget = widget_create(win, parent, &dock_vtable, dd);
    if (widget) {
        widget->bounds = (rect_t){ x, y, w, h };
    }
    return widget;
}

void dock_add_app(widget_t *dock, const char *name, const char *icon_path, void (*launch)(void *)) {
    dock_data_t *dd = (dock_data_t *)dock->user_data;
    if (!dd || dd->item_count >= DOCK_MAX_ITEMS) return;

    dock_item_t *item = &dd->items[dd->item_count];
    item->name = (char *)kmalloc(strlen(name) + 1);
    if (item->name) strcpy(item->name, name);
    item->icon_path = icon_path ? (char *)kmalloc(strlen(icon_path) + 1) : NULL;
    if (item->icon_path) strcpy(item->icon_path, icon_path);
    item->launch = launch;
    item->launch_data = NULL;
    item->active = false;
    item->scale = 1.0f;
    dd->item_count++;
    widget_invalidate(dock);
}

void dock_set_active(widget_t *dock, int index) {
    dock_data_t *dd = (dock_data_t *)dock->user_data;
    if (!dd || index < 0 || index >= dd->item_count) return;
    dd->active_index = index;
    for (int i = 0; i < dd->item_count; i++) {
        dd->items[i].active = (i == index);
    }
    widget_invalidate(dock);
}

int dock_get_active(widget_t *dock) {
    dock_data_t *dd = (dock_data_t *)dock->user_data;
    return dd ? dd->active_index : -1;
}

/* ============================================================
 * Lock Screen
 * ============================================================ */
typedef struct {
    char password[64];
    int password_len;
    char input[64];
    int input_len;
    bool wrong_password;
    uint64_t shake_time;
} lock_data_t;

static void lock_draw(widget_t *w) {
    lock_data_t *ld = (lock_data_t *)w->user_data;
    if (!ld) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    /* Dark overlay */
    rect_t full = { 0, 0, w->bounds.w, w->bounds.h };
    draw_rect_filled(buf, pitch, &full, GUI_COLOR_LOCK_BG);

    /* Center box */
    int box_w = 320;
    int box_h = 200;
    int box_x = (w->bounds.w - box_w) / 2 + (ld->wrong_password ? (int)(sin_approx(ld->shake_time / 100) / 100) : 0);
    int box_y = (w->bounds.h - box_h) / 2;

    rect_t box = { box_x, box_y, box_w, box_h };
    draw_rounded_rect(buf, pitch, &box, 16, GUI_COLOR_SURFACE);
    draw_rounded_rect_outline(buf, pitch, &box, 16, GUI_COLOR_BORDER);

    /* Title */
    draw_text(buf, pitch, box_x + (box_w - 8*8)/2, box_y + 24, "Unknown OS", GUI_COLOR_LOCK_FG, GUI_COLOR_SURFACE);
    draw_text(buf, pitch, box_x + (box_w - 10*8)/2, box_y + 48, "Lock Screen", GUI_COLOR_LOCK_FG, GUI_COLOR_SURFACE);

    /* Password input */
    int input_x = box_x + 24;
    int input_y = box_y + 90;
    int input_w = box_w - 48;
    rect_t input_rect = { input_x, input_y, input_w, 36 };
    draw_rounded_rect(buf, pitch, &input_rect, 8, GUI_COLOR_LOCK_INPUT);
    draw_rounded_rect_outline(buf, pitch, &input_rect, 8,
                              ld->wrong_password ? GUI_COLOR_RED : GUI_COLOR_BORDER);

    /* Show dots for password */
    for (int i = 0; i < ld->input_len; i++) {
        int dot_x = input_x + 12 + i * 20;
        rect_t dot = { dot_x, input_y + 13, 8, 8 };
        draw_rounded_rect(buf, pitch, &dot, 4, GUI_COLOR_ACCENT);
    }

    /* Hint */
    draw_text(buf, pitch, box_x + (box_w - 20*8)/2, box_y + 150, "Press Enter to unlock", GUI_COLOR_SURFACE_HL, GUI_COLOR_SURFACE);
}

static void lock_handle_event(widget_t *w, gui_event_t *e) {
    lock_data_t *ld = (lock_data_t *)w->user_data;
    if (!ld) return;

    switch (e->type) {
        case EVENT_KEY_CHAR:
            if (e->keyboard.character >= 32 && e->keyboard.character < 127 && ld->input_len < 63) {
                ld->input[ld->input_len++] = e->keyboard.character;
                ld->input[ld->input_len] = 0;
                widget_invalidate(w);
            }
            break;
        case EVENT_KEY_DOWN:
            if (e->keyboard.keycode == 0x0E) {  /* Backspace */
                if (ld->input_len > 0) {
                    ld->input[--ld->input_len] = 0;
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x1C) {  /* Enter */
                if (lock_screen_check_password(ld->input)) {
                    lock_screen_hide();
                } else {
                    ld->wrong_password = true;
                    ld->shake_time = timer_get_ticks();
                    ld->input_len = 0;
                    ld->input[0] = 0;
                    widget_invalidate(w);
                }
            }
            break;
    }
}

static void lock_destroy(widget_t *w) {
    lock_data_t *ld = (lock_data_t *)w->user_data;
    if (ld) kfree(ld);
}

static const widget_vtable_t lock_vtable = {
    .draw = lock_draw,
    .handle_event = lock_handle_event,
    .destroy = lock_destroy,
};

void lock_screen_show(void) {
    desktop_t *d = &g_desktop;
    if (!d->lock_window) return;

    /* Create lock widget if not exists */
    if (!d->lock_window->root_widget) {
        lock_data_t *ld = (lock_data_t *)kmalloc(sizeof(lock_data_t));
        if (ld) {
            memset(ld, 0, sizeof(lock_data_t));
            strcpy(ld->password, "unknownos");  /* Default password */
            ld->password_len = strlen(ld->password);

            widget_t *lock = widget_create(d->lock_window, NULL, &lock_vtable, ld);
            if (lock) {
                lock->bounds = (rect_t){ 0, 0, fb_width(), fb_height() };
            }
        }
    }

    d->locked = true;
    d->lock_time = timer_get_ticks();
    window_show(d->lock_window);
    window_bring_to_front(d->lock_window);
    window_focus(d->lock_window);
}

void lock_screen_hide(void) {
    desktop_t *d = &g_desktop;
    d->locked = false;
    window_hide(d->lock_window);
    if (d->focused_window) window_focus(d->focused_window);
}

bool lock_screen_check_password(const char *password) {
    return password && strcmp(password, "unknownos") == 0;
}

/* ============================================================
 * Wallpaper
 * ============================================================ */
void desktop_set_wallpaper(desktop_t *d, const char *path) {
    if (!d || !path) return;
    if (d->wallpaper_path) kfree(d->wallpaper_path);
    d->wallpaper_path = (char *)kmalloc(strlen(path) + 1);
    if (d->wallpaper_path) strcpy(d->wallpaper_path, path);

    /* Load wallpaper (simplified - would use image decoder) */
    desktop_draw_wallpaper(d);
}

void desktop_draw_wallpaper(desktop_t *d) {
    if (!d) return;

    size_t buf_size = fb_width() * fb_height() * 4;
    if (d->wallpaper_buffer) kfree(d->wallpaper_buffer);
    d->wallpaper_buffer = (uint32_t *)kmalloc(buf_size);
    if (!d->wallpaper_buffer) return;

    /* macOS-style gradient wallpaper */
    for (uint32_t y = 0; y < fb_height(); y++) {
        float t = (float)y / fb_height();
        /* Dark blue to purple gradient */
        uint32_t r = 25 + (uint32_t)(t * 35);
        uint32_t g = 25 + (uint32_t)(t * 20);
        uint32_t b = 50 + (uint32_t)(t * 40);
        uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;

        uint32_t *line = &d->wallpaper_buffer[y * fb_width()];
        for (uint32_t x = 0; x < fb_width(); x++) {
            /* Add subtle noise/texture */
            uint32_t noise = ((x * 17 + y * 23) & 0xF) - 8;
            uint32_t nr = (r + noise) & 0xFF;
            uint32_t ng = (g + noise) & 0xFF;
            uint32_t nb = (b + noise) & 0xFF;
            line[x] = (0xFF << 24) | (nr << 16) | (ng << 8) | nb;
        }
    }
}

/* ============================================================
 * Drawing Helpers
 * ============================================================ */
void draw_rect_filled(uint32_t *buf, uint32_t pitch, rect_t *r, gui_color_t color) {
    if (!buf || !r) return;
    int x1 = r->x < 0 ? 0 : r->x;
    int y1 = r->y < 0 ? 0 : r->y;
    int x2 = x1 + r->w;
    int y2 = y1 + r->h;
    if (x2 > (int)pitch) x2 = pitch;
    if (y2 > (int)fb_height()) y2 = fb_height();

    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            buf[y * pitch + x] = color;
        }
    }
}

void draw_rect(uint32_t *buf, uint32_t pitch, rect_t *r, gui_color_t color) {
    if (!buf || !r) return;
    draw_rect_filled(buf, pitch, &(rect_t){r->x, r->y, r->w, 1}, color);
    draw_rect_filled(buf, pitch, &(rect_t){r->x, r->y + r->h - 1, r->w, 1}, color);
    draw_rect_filled(buf, pitch, &(rect_t){r->x, r->y, 1, r->h}, color);
    draw_rect_filled(buf, pitch, &(rect_t){r->x + r->w - 1, r->y, 1, r->h}, color);
}

void draw_rounded_rect(uint32_t *buf, uint32_t pitch, rect_t *r, uint32_t radius, gui_color_t color) {
    if (!buf || !r || radius == 0) {
        draw_rect_filled(buf, pitch, r, color);
        return;
    }

    int x1 = r->x, y1 = r->y;
    int x2 = r->x + r->w, y2 = r->y + r->h;
    int r_int = radius;

    /* Corners */
    for (int y = y1; y < y1 + r_int; y++) {
        for (int x = x1; x < x1 + r_int; x++) {
            int dx = x - (x1 + r_int);
            int dy = y - (y1 + r_int);
            if (dx*dx + dy*dy <= r_int*r_int) {
                if (x >= 0 && x < (int)pitch && y >= 0 && y < (int)fb_height())
                    buf[y * pitch + x] = color;
            }
        }
    }
    for (int y = y1; y < y1 + r_int; y++) {
        for (int x = x2 - r_int; x < x2; x++) {
            int dx = x - (x2 - r_int - 1);
            int dy = y - (y1 + r_int);
            if (dx*dx + dy*dy <= r_int*r_int) {
                if (x >= 0 && x < (int)pitch && y >= 0 && y < (int)fb_height())
                    buf[y * pitch + x] = color;
            }
        }
    }
    for (int y = y2 - r_int; y < y2; y++) {
        for (int x = x1; x < x1 + r_int; x++) {
            int dx = x - (x1 + r_int);
            int dy = y - (y2 - r_int - 1);
            if (dx*dx + dy*dy <= r_int*r_int) {
                if (x >= 0 && x < (int)pitch && y >= 0 && y < (int)fb_height())
                    buf[y * pitch + x] = color;
            }
        }
    }
    for (int y = y2 - r_int; y < y2; y++) {
        for (int x = x2 - r_int; x < x2; x++) {
            int dx = x - (x2 - r_int - 1);
            int dy = y - (y2 - r_int - 1);
            if (dx*dx + dy*dy <= r_int*r_int) {
                if (x >= 0 && x < (int)pitch && y >= 0 && y < (int)fb_height())
                    buf[y * pitch + x] = color;
            }
        }
    }

    /* Edges and center */
    draw_rect_filled(buf, pitch, &(rect_t){x1 + r_int, y1, r->w - 2*r_int, r_int}, color);
    draw_rect_filled(buf, pitch, &(rect_t){x1 + r_int, y2 - r_int, r->w - 2*r_int, r_int}, color);
    draw_rect_filled(buf, pitch, &(rect_t){x1, y1 + r_int, r_int, r->h - 2*r_int}, color);
    draw_rect_filled(buf, pitch, &(rect_t){x2 - r_int, y1 + r_int, r_int, r->h - 2*r_int}, color);
    draw_rect_filled(buf, pitch, &(rect_t){x1 + r_int, y1 + r_int, r->w - 2*r_int, r->h - 2*r_int}, color);
}

void draw_rounded_rect_outline(uint32_t *buf, uint32_t pitch, rect_t *r, uint32_t radius, gui_color_t color) {
    /* Simplified outline */
    draw_rect(buf, pitch, r, color);
}

void draw_text(uint32_t *buf, uint32_t pitch, int x, int y, const char *text, gui_color_t fg, gui_color_t bg) {
    if (!buf || !text) return;
    while (*text) {
        fb_draw_char_to_buffer(buf, pitch, x, y, *text, fg, bg);
        x += 8;
        text++;
    }
}

void fb_draw_char_to_buffer(uint32_t *buf, uint32_t pitch, int x, int y, char c, uint32_t fg, uint32_t bg) {
    extern const uint8_t font_8x16[256][16];
    if ((unsigned char)c >= 256) c = '?';
    const uint8_t *glyph = font_8x16[(unsigned char)c];

    for (uint32_t row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < 8; col++) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < (int)pitch && py >= 0 && py < (int)fb_height()) {
                buf[py * pitch + px] = (bits & 0x80) ? fg : bg;
            }
            bits <<= 1;
        }
    }
}

void draw_text_shadow(uint32_t *buf, uint32_t pitch, int x, int y, const char *text, gui_color_t fg, gui_color_t shadow) {
    draw_text(buf, pitch, x + 1, y + 1, text, shadow, 0);
    draw_text(buf, pitch, x, y, text, fg, 0);
}

void draw_line(uint32_t *buf, uint32_t pitch, int x1, int y1, int x2, int y2, gui_color_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        if (x1 >= 0 && x1 < (int)pitch && y1 >= 0 && y1 < (int)fb_height())
            buf[y1 * pitch + x1] = color;
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void blit_buffer(uint32_t *dst, uint32_t dst_pitch, uint32_t *src, uint32_t src_pitch, rect_t *src_rect, int dst_x, int dst_y) {
    if (!dst || !src || !src_rect) return;

    int src_x = src_rect->x;
    int src_y = src_rect->y;
    int w = src_rect->w;
    int h = src_rect->h;

    for (int y = 0; y < h; y++) {
        int dy = dst_y + y;
        if (dy < 0 || dy >= (int)fb_height()) continue;
        for (int x = 0; x < w; x++) {
            int dx = dst_x + x;
            if (dx < 0 || dx >= (int)dst_pitch) continue;
            dst[dy * dst_pitch + dx] = src[(src_y + y) * src_pitch + (src_x + x)];
        }
    }
}

void blit_buffer_alpha(uint32_t *dst, uint32_t dst_pitch, uint32_t *src, uint32_t src_pitch, rect_t *src_rect, int dst_x, int dst_y, uint8_t alpha) {
    if (!dst || !src || !src_rect) return;

    int src_x = src_rect->x;
    int src_y = src_rect->y;
    int w = src_rect->w;
    int h = src_rect->h;
    uint8_t inv_alpha = 255 - alpha;

    for (int y = 0; y < h; y++) {
        int dy = dst_y + y;
        if (dy < 0 || dy >= (int)fb_height()) continue;
        for (int x = 0; x < w; x++) {
            int dx = dst_x + x;
            if (dx < 0 || dx >= (int)dst_pitch) continue;
            uint32_t s = src[(src_y + y) * src_pitch + (src_x + x)];
            uint32_t d = dst[dy * dst_pitch + dx];

            uint32_t r = ((s >> 16) & 0xFF) * alpha + ((d >> 16) & 0xFF) * inv_alpha;
            uint32_t g = ((s >> 8) & 0xFF) * alpha + ((d >> 8) & 0xFF) * inv_alpha;
            uint32_t b = (s & 0xFF) * alpha + (d & 0xFF) * inv_alpha;

            dst[dy * dst_pitch + dx] = 0xFF000000 | ((r >> 8) << 16) | ((g >> 8) << 8) | (b >> 8);
        }
    }
}

rect_t rect_intersect(rect_t *a, rect_t *b) {
    rect_t r = {0};
    int ax1 = a->x, ay1 = a->y, ax2 = a->x + a->w, ay2 = a->y + a->h;
    int bx1 = b->x, by1 = b->y, bx2 = b->x + b->w, by2 = b->y + b->h;
    r.x = ax1 > bx1 ? ax1 : bx1;
    r.y = ay1 > by1 ? ay1 : by1;
    r.w = (ax2 < bx2 ? ax2 : bx2) - r.x;
    r.h = (ay2 < by2 ? ay2 : by2) - r.y;
    if ((int)r.w < 0) r.w = 0;
    if ((int)r.h < 0) r.h = 0;
    return r;
}

bool rect_contains(rect_t *r, int x, int y) {
    return x >= r->x && x < r->x + (int)r->w && y >= r->y && y < r->y + (int)r->h;
}

uint32_t *window_get_buffer(window_t *win) {
    return win ? win->buffer : NULL;
}

uint32_t window_get_pitch(window_t *win) {
    return win ? win->bounds.w : 0;
}

/* ============================================================
 * Animation
 * ============================================================ */
void animate_window_open(window_t *win) {
    /* Placeholder for zoom/fade animation */
}

void animate_window_close(window_t *win) {
    /* Placeholder for zoom/fade animation */
}

void animate_dock_item(widget_t *item, bool expand) {
    /* Handled in dock_draw via scale factor */
}

/* ============================================================
 * Built-in Widgets: Menu (Context Menu / Dropdown)
 * ============================================================ */
#define MENU_MAX_ITEMS 32

typedef struct {
    char *label;
    void (*callback)(void *);
    void *user_data;
    bool separator;
} menu_item_t;

typedef struct {
    menu_item_t items[MENU_MAX_ITEMS];
    int item_count;
    int hovered_index;
    bool visible;
} menu_data_t;

static void menu_draw(widget_t *w) {
    menu_data_t *md = (menu_data_t *)w->user_data;
    if (!md || !md->visible) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    int item_h = 28;
    int total_h = md->item_count * item_h;
    rect_t bg = { 0, 0, w->bounds.w, total_h };
    draw_rounded_rect(buf, pitch, &bg, 8, GUI_COLOR_MENU_BG);
    draw_rounded_rect_outline(buf, pitch, &bg, 8, GUI_COLOR_BORDER);

    for (int i = 0; i < md->item_count; i++) {
        menu_item_t *item = &md->items[i];
        int y = i * item_h;
        rect_t item_rect = { 0, y, w->bounds.w, item_h };

        if (item->separator) {
            draw_line(buf, pitch, 8, y + item_h/2, w->bounds.w - 8, y + item_h/2, GUI_COLOR_BORDER);
            continue;
        }

        if (i == md->hovered_index) {
            draw_rect_filled(buf, pitch, &item_rect, GUI_COLOR_MENU_HOVER);
        }

        int text_x = 12;
        int text_y = y + (item_h - 16) / 2;
        draw_text(buf, pitch, text_x, text_y, item->label, GUI_COLOR_FG, GUI_COLOR_MENU_BG);
    }
}

static void menu_handle_event(widget_t *w, gui_event_t *e) {
    menu_data_t *md = (menu_data_t *)w->user_data;
    if (!md || !md->visible) return;

    switch (e->type) {
        case EVENT_MOUSE_MOVE: {
            int old_hover = md->hovered_index;
            md->hovered_index = -1;
            int item_h = 28;
            for (int i = 0; i < md->item_count; i++) {
                if (e->mouse.y >= i * item_h && e->mouse.y < (i + 1) * item_h) {
                    if (!md->items[i].separator) {
                        md->hovered_index = i;
                    }
                    break;
                }
            }
            if (old_hover != md->hovered_index) widget_invalidate(w);
            break;
        }
        case EVENT_MOUSE_UP:
            if (md->hovered_index >= 0 && (e->mouse.buttons & MOUSE_BTN_LEFT)) {
                menu_item_t *item = &md->items[md->hovered_index];
                if (item->callback) item->callback(item->user_data);
                menu_hide(w);
            }
            break;
        case EVENT_MOUSE_DOWN:
            if (e->mouse.buttons & MOUSE_BTN_RIGHT) {
                menu_hide(w);
            }
            break;
        case EVENT_KEY_DOWN:
            if (e->keyboard.keycode == 0x01) {  /* Escape */
                menu_hide(w);
            }
            break;
    }
}

static void menu_destroy(widget_t *w) {
    menu_data_t *md = (menu_data_t *)w->user_data;
    if (md) {
        for (int i = 0; i < md->item_count; i++) {
            if (md->items[i].label) kfree(md->items[i].label);
        }
        kfree(md);
    }
}

static const widget_vtable_t menu_vtable = {
    .draw = menu_draw,
    .handle_event = menu_handle_event,
    .destroy = menu_destroy,
};

widget_t *menu_create(window_t *win, widget_t *parent, int x, int y, uint32_t w) {
    menu_data_t *md = (menu_data_t *)kzalloc(sizeof(menu_data_t));
    if (!md) return NULL;

    widget_t *widget = widget_create(win, parent, &menu_vtable, md);
    if (widget) {
        widget->bounds = (rect_t){ x, y, w, 0 };  /* Height calculated in draw */
    }
    return widget;
}

void menu_add_item(widget_t *menu, const char *label, void (*callback)(void *), void *user_data) {
    menu_data_t *md = (menu_data_t *)menu->user_data;
    if (!md || md->item_count >= MENU_MAX_ITEMS) return;

    menu_item_t *item = &md->items[md->item_count];
    item->label = (char *)kmalloc(strlen(label) + 1);
    if (item->label) strcpy(item->label, label);
    item->callback = callback;
    item->user_data = user_data;
    item->separator = false;
    md->item_count++;
    /* Update height */
    menu->bounds.h = md->item_count * 28;
}

void menu_add_separator(widget_t *menu) {
    menu_data_t *md = (menu_data_t *)menu->user_data;
    if (!md || md->item_count >= MENU_MAX_ITEMS) return;
    md->items[md->item_count].separator = true;
    md->item_count++;
    menu->bounds.h = md->item_count * 28;
}

void menu_show_at(widget_t *menu, int x, int y) {
    menu_data_t *md = (menu_data_t *)menu->user_data;
    if (!md) return;
    menu->bounds.x = x;
    menu->bounds.y = y;
    md->visible = true;
    md->hovered_index = -1;
    widget_invalidate(menu);
}

void menu_hide(widget_t *menu) {
    menu_data_t *md = (menu_data_t *)menu->user_data;
    if (md) {
        md->visible = false;
        md->hovered_index = -1;
        widget_invalidate(menu);
    }
}

/* ============================================================
 * Built-in Widgets: List
 * ============================================================ */
#define LIST_MAX_ITEMS 128

typedef struct {
    char *text;
    void *user_data;
} list_item_t;

typedef struct {
    list_item_t items[LIST_MAX_ITEMS];
    int item_count;
    int selected_index;
    int hovered_index;
    int scroll_offset;
    void (*callback)(void *user_data);
} list_data_t;

static void list_draw(widget_t *w) {
    list_data_t *ld = (list_data_t *)w->user_data;
    if (!ld) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    draw_rounded_rect(buf, pitch, &w->bounds, 6, GUI_COLOR_SURFACE);
    draw_rounded_rect_outline(buf, pitch, &w->bounds, 6, GUI_COLOR_BORDER);

    int item_h = 24;
    int visible_items = w->bounds.h / item_h;

    for (int i = 0; i < visible_items; i++) {
        int idx = ld->scroll_offset + i;
        if (idx >= ld->item_count) break;

        list_item_t *item = &ld->items[idx];
        int y = w->bounds.y + i * item_h;
        rect_t item_rect = { w->bounds.x, y, w->bounds.w, item_h };

        gui_color_t bg;
        if (idx == ld->selected_index) bg = GUI_COLOR_ACCENT;
        else if (idx == ld->hovered_index) bg = GUI_COLOR_SURFACE_HL;
        else bg = (i % 2 == 0) ? GUI_COLOR_SURFACE : GUI_COLOR_BG;

        draw_rect_filled(buf, pitch, &item_rect, bg);

        int text_x = w->bounds.x + 8;
        int text_y = y + (item_h - 16) / 2;
        draw_text(buf, pitch, text_x, text_y, item->text, GUI_COLOR_FG, bg);
    }
}

static void list_handle_event(widget_t *w, gui_event_t *e) {
    list_data_t *ld = (list_data_t *)w->user_data;
    if (!ld) return;

    int item_h = 24;

    switch (e->type) {
        case EVENT_MOUSE_MOVE: {
            int old_hover = ld->hovered_index;
            ld->hovered_index = -1;
            if (rect_contains(&w->bounds, e->mouse.x, e->mouse.y)) {
                int rel_y = e->mouse.y - w->bounds.y;
                int idx = ld->scroll_offset + rel_y / item_h;
                if (idx >= 0 && idx < ld->item_count) {
                    ld->hovered_index = idx;
                }
            }
            if (old_hover != ld->hovered_index) widget_invalidate(w);
            break;
        }
        case EVENT_MOUSE_DOWN:
            if (rect_contains(&w->bounds, e->mouse.x, e->mouse.y) && (e->mouse.buttons & MOUSE_BTN_LEFT)) {
                int rel_y = e->mouse.y - w->bounds.y;
                int idx = ld->scroll_offset + rel_y / item_h;
                if (idx >= 0 && idx < ld->item_count) {
                    ld->selected_index = idx;
                    widget_invalidate(w);
                }
            }
            break;
        case EVENT_MOUSE_UP:
            if (ld->selected_index >= 0 && ld->callback) {
                ld->callback(ld->items[ld->selected_index].user_data);
            }
            break;
        case EVENT_MOUSE_SCROLL:
            ld->scroll_offset -= e->mouse.buttons;
            if (ld->scroll_offset < 0) ld->scroll_offset = 0;
            int max_scroll = ld->item_count - w->bounds.h / item_h;
            if (ld->scroll_offset > max_scroll) ld->scroll_offset = max_scroll;
            widget_invalidate(w);
            break;
        case EVENT_KEY_DOWN:
            if (e->keyboard.keycode == 0x48) {  /* Up arrow */
                if (ld->selected_index > 0) {
                    ld->selected_index--;
                    if (ld->selected_index < ld->scroll_offset) ld->scroll_offset = ld->selected_index;
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x50) {  /* Down arrow */
                if (ld->selected_index < ld->item_count - 1) {
                    ld->selected_index++;
                    if (ld->selected_index >= ld->scroll_offset + w->bounds.h / item_h)
                        ld->scroll_offset = ld->selected_index - w->bounds.h / item_h + 1;
                    widget_invalidate(w);
                }
            } else if (e->keyboard.keycode == 0x1C) {  /* Enter */
                if (ld->selected_index >= 0 && ld->callback) {
                    ld->callback(ld->items[ld->selected_index].user_data);
                }
            }
            break;
    }
}

static void list_destroy(widget_t *w) {
    list_data_t *ld = (list_data_t *)w->user_data;
    if (ld) {
        for (int i = 0; i < ld->item_count; i++) {
            if (ld->items[i].text) kfree(ld->items[i].text);
        }
        kfree(ld);
    }
}

static const widget_vtable_t list_vtable = {
    .draw = list_draw,
    .handle_event = list_handle_event,
    .destroy = list_destroy,
};

widget_t *list_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h) {
    list_data_t *ld = (list_data_t *)kzalloc(sizeof(list_data_t));
    if (!ld) return NULL;

    widget_t *widget = widget_create(win, parent, &list_vtable, ld);
    if (widget) {
        widget->bounds = (rect_t){ x, y, w, h };
    }
    return widget;
}

void list_add_item(widget_t *list, const char *text, void *user_data) {
    list_data_t *ld = (list_data_t *)list->user_data;
    if (!ld || ld->item_count >= LIST_MAX_ITEMS) return;

    list_item_t *item = &ld->items[ld->item_count];
    item->text = (char *)kmalloc(strlen(text) + 1);
    if (item->text) strcpy(item->text, text);
    item->user_data = user_data;
    ld->item_count++;
    if (ld->selected_index < 0) ld->selected_index = 0;
    widget_invalidate(list);
}

void list_set_callback(widget_t *list, void (*callback)(void *user_data)) {
    list_data_t *ld = (list_data_t *)list->user_data;
    if (ld) ld->callback = callback;
}

/* ============================================================
 * Built-in Widgets: Tab Bar
 * ============================================================ */
#define TABBAR_MAX_TABS 16

typedef struct {
    char *label;
    widget_t *content;
    bool active;
} tab_item_t;

typedef struct {
    tab_item_t tabs[TABBAR_MAX_TABS];
    int tab_count;
    int active_tab;
    int hovered_tab;
} tabbar_data_t;

static void tabbar_draw(widget_t *w) {
    tabbar_data_t *td = (tabbar_data_t *)w->user_data;
    if (!td) return;

    window_t *win = w->window;
    uint32_t *buf = win->buffer;
    uint32_t pitch = win->bounds.w;

    draw_rect_filled(buf, pitch, &w->bounds, GUI_COLOR_SURFACE);
    draw_line(buf, pitch, w->bounds.x, w->bounds.y + w->bounds.h - 1,
              w->bounds.x + w->bounds.w, w->bounds.y + w->bounds.h - 1, GUI_COLOR_BORDER);

    int tab_w = 120;
    int tab_h = 28;
    int start_x = w->bounds.x + 4;

    for (int i = 0; i < td->tab_count; i++) {
        tab_item_t *tab = &td->tabs[i];
        int x = start_x + i * (tab_w + 4);
        int y = w->bounds.y + 2;
        rect_t tab_rect = { x, y, tab_w, tab_h };

        gui_color_t bg = tab->active ? GUI_COLOR_ACCENT : GUI_COLOR_SURFACE_HL;
        if (i == td->hovered_tab) bg = GUI_COLOR_ACCENT_HOVER;

        draw_rounded_rect(buf, pitch, &tab_rect, 6, bg);

        int text_x = x + (tab_w - strlen(tab->label) * 8) / 2;
        int text_y = y + (tab_h - 16) / 2;
        draw_text(buf, pitch, text_x, text_y, tab->label, tab->active ? GUI_COLOR_BLACK : GUI_COLOR_FG, bg);

        if (tab->active && tab->content) {
            tab->content->visible = true;
        } else if (tab->content) {
            tab->content->visible = false;
        }
    }
}

static void tabbar_handle_event(widget_t *w, gui_event_t *e) {
    tabbar_data_t *td = (tabbar_data_t *)w->user_data;
    if (!td) return;

    int tab_w = 120;
    int start_x = w->bounds.x + 4;
    int tab_h = 28;

    switch (e->type) {
        case EVENT_MOUSE_MOVE: {
            int old_hover = td->hovered_tab;
            td->hovered_tab = -1;
            for (int i = 0; i < td->tab_count; i++) {
                int x = start_x + i * (tab_w + 4);
                if (e->mouse.x >= x && e->mouse.x < x + tab_w &&
                    e->mouse.y >= w->bounds.y && e->mouse.y < w->bounds.y + tab_h) {
                    td->hovered_tab = i;
                    break;
                }
            }
            if (old_hover != td->hovered_tab) widget_invalidate(w);
            break;
        }
        case EVENT_MOUSE_DOWN:
            if (e->mouse.buttons & MOUSE_BTN_LEFT) {
                for (int i = 0; i < td->tab_count; i++) {
                    int x = start_x + i * (tab_w + 4);
                    if (e->mouse.x >= x && e->mouse.x < x + tab_w &&
                        e->mouse.y >= w->bounds.y && e->mouse.y < w->bounds.y + tab_h) {
                        td->active_tab = i;
                        for (int j = 0; j < td->tab_count; j++) {
                            td->tabs[j].active = (j == i);
                        }
                        widget_invalidate(w);
                        break;
                    }
                }
            }
            break;
    }
}

static void tabbar_destroy(widget_t *w) {
    tabbar_data_t *td = (tabbar_data_t *)w->user_data;
    if (td) {
        for (int i = 0; i < td->tab_count; i++) {
            if (td->tabs[i].label) kfree(td->tabs[i].label);
        }
        kfree(td);
    }
}

static const widget_vtable_t tabbar_vtable = {
    .draw = tabbar_draw,
    .handle_event = tabbar_handle_event,
    .destroy = tabbar_destroy,
};

widget_t *tabbar_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h) {
    tabbar_data_t *td = (tabbar_data_t *)kzalloc(sizeof(tabbar_data_t));
    if (!td) return NULL;

    widget_t *widget = widget_create(win, parent, &tabbar_vtable, td);
    if (widget) {
        widget->bounds = (rect_t){ x, y, w, h };
    }
    return widget;
}

void tabbar_add_tab(widget_t *tabbar, const char *label, widget_t *content) {
    tabbar_data_t *td = (tabbar_data_t *)tabbar->user_data;
    if (!td || td->tab_count >= TABBAR_MAX_TABS) return;

    tab_item_t *tab = &td->tabs[td->tab_count];
    tab->label = (char *)kmalloc(strlen(label) + 1);
    if (tab->label) strcpy(tab->label, label);
    tab->content = content;
    tab->active = (td->tab_count == 0);
    if (content) content->visible = tab->active;
    td->tab_count++;
    widget_invalidate(tabbar);
}

/* ============================================================
 * Callback Function Implementations
 * ============================================================ */

static void panel_launcher_callback(widget_t *w) {
    extern void launcher_show(void);
    launcher_show();
}

static void desktop_menu_new_window(void *data) {
    (void)data;
    window_t *w = window_create(WINDOW_TYPE_NORMAL, "New Window",
                                100, 100, 600, 400,
                                WINDOW_FLAG_DECORATED | WINDOW_FLAG_RESIZABLE | WINDOW_FLAG_MOVABLE);
    if (w) {
        w->on_close = desktop_window_on_close;
        window_show(w);
    }
}

static void desktop_menu_new_terminal(void *data) {
    (void)data;
    extern void launch_terminal(void);
    launch_terminal();
}

static void desktop_menu_lock_screen(void *data) {
    (void)data;
    lock_screen_show();
}

static void desktop_menu_shutdown(void *data) {
    (void)data;
    extern void kernel_shutdown(void);
    kernel_shutdown();
}

static void desktop_window_on_close(window_t *win) {
    /* Don't allow closing the desktop window */
    (void)win;
}