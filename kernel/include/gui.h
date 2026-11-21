/*
 * Unknown OS - GUI/Window System Header
 * Window manager, compositor, and desktop environment
 * Author: byrex <byrex.me@proton.me>
 */

#ifndef _GUI_H
#define _GUI_H

#include <kernel.h>

/* ============================================================
 * Types
 * ============================================================ */
typedef uint32_t gui_color_t;
typedef uint32_t window_id_t;
typedef uint32_t widget_id_t;

/* Window types */
typedef enum {
    WINDOW_TYPE_NORMAL = 0,
    WINDOW_TYPE_DOCK,
    WINDOW_TYPE_PANEL,
    WINDOW_TYPE_MENU,
    WINDOW_TYPE_DIALOG,
    WINDOW_TYPE_DESKTOP,
    WINDOW_TYPE_LOCKSCREEN,
} window_type_t;

/* Window states */
typedef enum {
    WINDOW_STATE_NORMAL = 0,
    WINDOW_STATE_MINIMIZED,
    WINDOW_STATE_MAXIMIZED,
    WINDOW_STATE_FULLSCREEN,
    WINDOW_STATE_CLOSED,
} window_state_t;

/* Window flags */
#define WINDOW_FLAG_DECORATED     (1 << 0)  /* Has title bar, borders */
#define WINDOW_FLAG_RESIZABLE     (1 << 1)
#define WINDOW_FLAG_MOVABLE       (1 << 2)
#define WINDOW_FLAG_ALWAYS_ON_TOP (1 << 3)
#define WINDOW_FLAG_NO_FOCUS      (1 << 4)
#define WINDOW_FLAG_TRANSPARENT   (1 << 5)
#define WINDOW_FLAG_CLICK_THROUGH (1 << 6)

/* Text Input widget data */
#define TEXT_INPUT_MAX_LEN 255

typedef struct text_input_data text_input_data_t;
struct text_input_data {
    char *text;
    size_t len;
    size_t capacity;
    char *placeholder;
    bool focused;
    size_t cursor_pos;
    uint64_t blink_time;
};

/* Event types */
typedef enum {
    EVENT_NONE = 0,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_MOUSE_SCROLL,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_KEY_CHAR,
    EVENT_WINDOW_CLOSE,
    EVENT_WINDOW_FOCUS,
    EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_MOVE,
    EVENT_TIMER,
    EVENT_REDRAW,
} event_type_t;

/* Mouse buttons */
#define MOUSE_BTN_LEFT    (1 << 0)
#define MOUSE_BTN_RIGHT   (1 << 1)
#define MOUSE_BTN_MIDDLE  (1 << 2)

/* ============================================================
 * Structures
 * ============================================================ */
typedef struct rect {
    int32_t x, y;
    uint32_t w, h;
} rect_t;

typedef struct point {
    int32_t x, y;
} point_t;

typedef struct gui_event {
    event_type_t type;
    window_id_t window_id;
    union {
        struct {
            int32_t x, y;
            uint32_t buttons;
        } mouse;
        struct {
            uint32_t keycode;
            uint32_t modifiers;
            char character;
        } keyboard;
        struct {
            uint32_t width, height;
        } resize;
        struct {
            int32_t x, y;
        } move;
        struct {
            uint64_t ticks;
        } timer;
    };
} gui_event_t;

/* Forward declarations */
struct window;
struct widget;
struct desktop;

/* Widget vtable */
typedef struct widget_vtable {
    void (*draw)(struct widget *w);
    void (*handle_event)(struct widget *w, gui_event_t *e);
    void (*destroy)(struct widget *w);
} widget_vtable_t;

/* Widget base */
typedef struct widget {
    widget_id_t id;
    struct window *window;
    struct widget *parent;
    struct widget *children;
    struct widget *next_sibling;
    rect_t bounds;
    bool visible;
    bool focused;
    bool hovered;
    void *user_data;
    const widget_vtable_t *vtable;
} widget_t;

/* Window */
typedef struct window {
    window_id_t id;
    struct desktop *desktop;
    window_type_t type;
    window_state_t state;
    uint32_t flags;
    char *title;
    rect_t bounds;
    rect_t content_bounds;  /* Inside decorations */
    uint32_t *buffer;       /* Backing buffer */
    bool dirty;
    bool focused;
    struct window *next;
    struct window *prev;
    /* Decoration */
    rect_t title_bar;
    rect_t close_btn;
    rect_t maximize_btn;
    rect_t minimize_btn;
    /* Widgets */
    widget_t *root_widget;
    /* Callbacks */
    void (*on_close)(struct window *w);
    void (*on_focus)(struct window *w, bool focus);
    void (*on_resize)(struct window *w, uint32_t w_new, uint32_t h_new);
    void *user_data;
} window_t;

/* Desktop */
typedef struct desktop {
    window_t *windows;           /* List of all windows */
    window_t *focused_window;    /* Currently focused */
    window_t *dock_window;       /* Dock window */
    window_t *panel_window;      /* Top panel */
    window_t *lock_window;       /* Lock screen */
    window_t *menu_window;       /* App menu */
    bool locked;
    uint64_t lock_time;
    char *wallpaper_path;
    uint32_t *wallpaper_buffer;  /* Cached wallpaper */
    /* Input */
    point_t mouse_pos;
    uint32_t mouse_buttons;
    uint32_t modifiers;
    window_t *captured_window;   /* Mouse capture */
    widget_t *hovered_widget;
    widget_t *focused_widget;
    /* Animation */
    uint64_t last_frame;
    bool needs_redraw;
} desktop_t;

/* ============================================================
 * Colors (macOS-inspired)
 * ============================================================ */
#define GUI_COLOR_BG           0xFF1E1E2E
#define GUI_COLOR_FG           0xFFCDD6F4
#define GUI_COLOR_ACCENT       0xFF89B4FA
#define GUI_COLOR_ACCENT_HOVER 0xFF74C7EC
#define GUI_COLOR_SURFACE      0xFF313244
#define GUI_COLOR_SURFACE_HL   0xFF45475A
#define GUI_COLOR_BORDER       0xFF585B70
#define GUI_COLOR_TITLE_BG     0xFF1E1E2E
#define GUI_COLOR_TITLE_FG     0xFFCDD6F4
#define GUI_COLOR_BTN_CLOSE    0xFFF38BA8
#define GUI_COLOR_BTN_MAX      0xFFF9E2AF
#define GUI_COLOR_BTN_MIN      0xFFA6E3A1
#define GUI_COLOR_DOCK_BG      0xCC1E1E2E
#define GUI_COLOR_DOCK_ITEM    0xFF313244
#define GUI_COLOR_DOCK_HOVER   0xFF45475A
#define GUI_COLOR_DOCK_ACTIVE  0xFF89B4FA
#define GUI_COLOR_PANEL_BG     0xCC1E1E2E
#define GUI_COLOR_PANEL_FG     0xFFCDD6F4
#define GUI_COLOR_MENU_BG      0xFF1E1E2E
#define GUI_COLOR_MENU_HOVER   0xFF313244
#define GUI_COLOR_MENU_SEL     0xFF45475A
#define GUI_COLOR_LOCK_BG      0xCC11111B
#define GUI_COLOR_LOCK_FG      0xFFCDD6F4
#define GUI_COLOR_LOCK_INPUT   0xFF313244
#define GUI_COLOR_TEXT_SEL     0xFF89B4FA
#define GUI_COLOR_BLACK        0xFF11111B
#define GUI_COLOR_RED          0xFFF38BA8
#define GUI_COLOR_GREEN        0xFFA6E3A1
#define GUI_COLOR_YELLOW       0xFFF9E2AF
#define GUI_COLOR_BLUE         0xFF89B4FA

/* ============================================================
 * Window Manager API
 * ============================================================ */
void gui_init(void);
void gui_shutdown(void);
desktop_t *gui_get_desktop(void);

/* Window management */
window_t *window_create(window_type_t type, const char *title, int x, int y, uint32_t w, uint32_t h, uint32_t flags);
void window_destroy(window_t *win);
void window_show(window_t *win);
void window_hide(window_t *win);
void window_set_title(window_t *win, const char *title);
void window_set_bounds(window_t *win, int x, int y, uint32_t w, uint32_t h);
void window_center(window_t *win);
void window_bring_to_front(window_t *win);
void window_send_to_back(window_t *win);
void window_focus(window_t *win);
void window_invalidate(window_t *win);
void window_invalidate_rect(window_t *win, rect_t *r);

/* Rendering */
void window_render(window_t *win);
void desktop_render(desktop_t *d);
void desktop_composite(desktop_t *d);

/* Input */
void gui_handle_mouse_move(int32_t x, int32_t y);
void gui_handle_mouse_down(int32_t x, int32_t y, uint32_t btn);
void gui_handle_mouse_up(int32_t x, int32_t y, uint32_t btn);
void gui_handle_mouse_scroll(int32_t x, int32_t y, int32_t delta);
void gui_handle_key_down(uint32_t keycode, uint32_t modifiers);
void gui_handle_key_up(uint32_t keycode, uint32_t modifiers);
void gui_handle_char(char c);

/* Event loop */
void gui_process_events(void);
void gui_main_loop(void);

/* Widget API */
widget_t *widget_create(window_t *win, widget_t *parent, const widget_vtable_t *vtable, void *user_data);
void widget_destroy(widget_t *w);
void widget_set_bounds(widget_t *w, rect_t *b);
void widget_show(widget_t *w);
void widget_hide(widget_t *w);
void widget_invalidate(widget_t *w);

/* Built-in widgets */
widget_t *label_create(window_t *win, widget_t *parent, int x, int y, const char *text);
widget_t *button_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h, const char *text, void (*callback)(widget_t *));
widget_t *text_input_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h, const char *placeholder);
widget_t *panel_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h);
widget_t *dock_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h);
widget_t *menu_create(window_t *win, widget_t *parent, int x, int y, uint32_t w);
void menu_add_item(widget_t *menu, const char *label, void (*callback)(void *), void *user_data);
void menu_add_separator(widget_t *menu);
void menu_show_at(widget_t *menu, int x, int y);
void menu_hide(widget_t *menu);
widget_t *list_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h);
void list_add_item(widget_t *list, const char *text, void *user_data);
void list_set_callback(widget_t *list, void (*callback)(void *user_data));
widget_t *tabbar_create(window_t *win, widget_t *parent, int x, int y, uint32_t w, uint32_t h);
void tabbar_add_tab(widget_t *tabbar, const char *label, widget_t *content);

/* Dock */
void dock_add_app(widget_t *dock, const char *name, const char *icon_path, void (*launch)(void *));
void dock_set_active(widget_t *dock, int index);
int dock_get_active(widget_t *dock);

/* Lock Screen */
void lock_screen_show(void);
void lock_screen_hide(void);
bool lock_screen_check_password(const char *password);

/* Wallpaper */
void desktop_set_wallpaper(desktop_t *d, const char *path);
void desktop_draw_wallpaper(desktop_t *d);

/* Drawing helpers */
void draw_rect_filled(uint32_t *buf, uint32_t pitch, rect_t *r, gui_color_t color);
void draw_rect(uint32_t *buf, uint32_t pitch, rect_t *r, gui_color_t color);
void draw_rounded_rect(uint32_t *buf, uint32_t pitch, rect_t *r, uint32_t radius, gui_color_t color);
void draw_text(uint32_t *buf, uint32_t pitch, int x, int y, const char *text, gui_color_t fg, gui_color_t bg);
void draw_text_shadow(uint32_t *buf, uint32_t pitch, int x, int y, const char *text, gui_color_t fg, gui_color_t shadow);
void draw_line(uint32_t *buf, uint32_t pitch, int x1, int y1, int x2, int y2, gui_color_t color);
void draw_rounded_rect_outline(uint32_t *buf, uint32_t pitch, rect_t *r, uint32_t radius, gui_color_t color);
void draw_icon(uint32_t *buf, uint32_t pitch, int x, int y, uint32_t *icon, uint32_t iw, uint32_t ih);
void blit_buffer(uint32_t *dst, uint32_t dst_pitch, uint32_t *src, uint32_t src_pitch, rect_t *src_rect, int dst_x, int dst_y);
void blit_buffer_alpha(uint32_t *dst, uint32_t dst_pitch, uint32_t *src, uint32_t src_pitch, rect_t *src_rect, int dst_x, int dst_y, uint8_t alpha);

/* Animation */
void animate_window_open(window_t *win);
void animate_window_close(window_t *win);
void animate_dock_item(widget_t *item, bool expand);

/* Utility */
rect_t rect_intersect(rect_t *a, rect_t *b);
bool rect_contains(rect_t *r, int x, int y);
uint32_t *window_get_buffer(window_t *win);
uint32_t window_get_pitch(window_t *win);

/* Widget data types - need full definitions for apps.c usage */
#define TABBAR_MAX_TABS 16
#define LAUNCHER_MAX_APPS 32

typedef struct tab_item {
    char *label;
    widget_t *content;
    bool active;
} tab_item_t;

typedef struct tabbar_data {
    tab_item_t tabs[TABBAR_MAX_TABS];
    int tab_count;
    int active_tab;
    int hovered_tab;
} tabbar_data_t;

typedef struct launcher_app {
    char *name;
    char *icon;
    void (*launch)(void);
} launcher_app_t;

typedef struct launcher_data {
    launcher_app_t apps[LAUNCHER_MAX_APPS];
    int app_count;
    int selected_index;
    int hovered_index;
    char search[64];
    int search_len;
    bool visible;
} launcher_data_t;

typedef struct settings_data {
    int active_tab;
    widget_t *content_area;  /* Content area widget for drawing */
} settings_data_t;

/* App Launchers */
void launch_terminal(void);
void launch_settings(void);
void launch_file_manager(void);
void launch_text_editor(void);
void launch_browser(void);

/* App Launcher */
widget_t *launcher_create(window_t *win);
void launcher_show(void);
void launcher_hide(widget_t *w);

#endif /* _GUI_H */