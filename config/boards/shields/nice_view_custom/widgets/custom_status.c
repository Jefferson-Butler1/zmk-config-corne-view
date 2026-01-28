/*
 * Custom Nice!View Status Widget
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

// Central-only includes
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/hid.h>
#if IS_ENABLED(CONFIG_ZMK_WPM)
#include <zmk/wpm.h>
#include <zmk/events/wpm_state_changed.h>
#endif
#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/modifiers.h>
#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/ble.h>
#include <zmk/events/ble_active_profile_changed.h>
#endif
#endif

#include "custom_status.h"

#define CANVAS_SIZE 68
#define FOREGROUND lv_color_black()
#define BACKGROUND lv_color_white()

#define CHAR_BUFFER_SIZE 12

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static char char_buffer[CHAR_BUFFER_SIZE + 1] = {0};
static int char_buffer_pos = 0;
#if IS_ENABLED(CONFIG_ZMK_WPM)
static int current_wpm = 0;
#endif
#endif

// ============================================================================
// DRAWING HELPERS
// ============================================================================

static void init_label_dsc(lv_draw_label_dsc_t *dsc, const lv_font_t *font, lv_text_align_t align) {
    lv_draw_label_dsc_init(dsc);
    dsc->color = FOREGROUND;
    dsc->font = font;
    dsc->align = align;
}

static void init_rect_dsc(lv_draw_rect_dsc_t *dsc, lv_color_t color) {
    lv_draw_rect_dsc_init(dsc);
    dsc->bg_color = color;
}

static void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[]) {
    static lv_color_t temp[CANVAS_SIZE * CANVAS_SIZE];
    memcpy(temp, cbuf, sizeof(temp));
    lv_img_dsc_t img = {
        .header.w = CANVAS_SIZE,
        .header.h = CANVAS_SIZE,
        .data_size = CANVAS_SIZE * CANVAS_SIZE * sizeof(lv_color_t),
        .header.cf = LV_IMG_CF_TRUE_COLOR,
        .data = (void *)temp,
    };
    lv_canvas_fill_bg(canvas, BACKGROUND, LV_OPA_COVER);
    lv_canvas_transform(canvas, &img, 900, LV_IMG_ZOOM_NONE, -1, 0, CANVAS_SIZE / 2,
                        CANVAS_SIZE / 2, true);
}

// ============================================================================
// TOP SECTION: Battery + Status
// ============================================================================

static void draw_top(struct zmk_widget_custom_status *widget) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 0);

    lv_draw_rect_dsc_t bg_dsc;
    init_rect_dsc(&bg_dsc, BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &bg_dsc);

    // Battery percentage (top left)
    lv_draw_label_dsc_t bat_dsc;
    init_label_dsc(&bat_dsc, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT);

    char bat_text[8];
    snprintf(bat_text, sizeof(bat_text), "%d%%", widget->state.battery);
    lv_canvas_draw_text(canvas, 2, 2, 50, &bat_dsc, bat_text);

    // Status indicator (top right)
    lv_draw_label_dsc_t status_dsc;
    init_label_dsc(&status_dsc, &lv_font_montserrat_14, LV_TEXT_ALIGN_RIGHT);

    char status_text[12];
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#if IS_ENABLED(CONFIG_ZMK_BLE)
    if (widget->state.bt_connected) {
        snprintf(status_text, sizeof(status_text), LV_SYMBOL_BLUETOOTH "%d", widget->state.bt_profile + 1);
    } else {
        snprintf(status_text, sizeof(status_text), LV_SYMBOL_CLOSE "%d", widget->state.bt_profile + 1);
    }
#else
    snprintf(status_text, sizeof(status_text), "USB");
#endif
#else
    // Peripheral side - just show "R" for right
    snprintf(status_text, sizeof(status_text), "R");
#endif
    lv_canvas_draw_text(canvas, 0, 2, CANVAS_SIZE - 4, &status_dsc, status_text);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    // Modifier indicators (bottom of top section) - central only
    // Get actual modifier state from HID report
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();
    bool mod_ctrl = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    bool mod_alt = (mods & (MOD_LALT | MOD_RALT)) != 0;
    bool mod_gui = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
    bool mod_shift = (mods & (MOD_LSFT | MOD_RSFT)) != 0;

    lv_draw_rect_dsc_t mod_bg;
    init_rect_dsc(&mod_bg, FOREGROUND);

    lv_draw_label_dsc_t mod_dsc;
    init_label_dsc(&mod_dsc, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

    lv_draw_label_dsc_t mod_dsc_inv;
    lv_draw_label_dsc_init(&mod_dsc_inv);
    mod_dsc_inv.color = BACKGROUND;
    mod_dsc_inv.font = &lv_font_montserrat_14;
    mod_dsc_inv.align = LV_TEXT_ALIGN_CENTER;

    int box_w = 14;
    int gap = 2;
    int total_w = 4 * box_w + 3 * gap;
    int start_x = (CANVAS_SIZE - total_w) / 2;
    int y = 44;

    const char *mod_labels[] = {"C", "A", "G", "S"};
    bool mod_states[] = {mod_ctrl, mod_alt, mod_gui, mod_shift};

    for (int i = 0; i < 4; i++) {
        int x = start_x + i * (box_w + gap);

        if (mod_states[i]) {
            lv_canvas_draw_rect(canvas, x, y, box_w, 18, &mod_bg);
            lv_canvas_draw_text(canvas, x, y + 1, box_w, &mod_dsc_inv, mod_labels[i]);
        } else {
            lv_canvas_draw_text(canvas, x, y + 1, box_w, &mod_dsc, mod_labels[i]);
        }
    }
#endif

    rotate_canvas(canvas, widget->cbuf);
}

// ============================================================================
// MIDDLE SECTION
// ============================================================================

static void draw_middle(struct zmk_widget_custom_status *widget) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 1);

    lv_draw_rect_dsc_t bg_dsc;
    init_rect_dsc(&bg_dsc, BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &bg_dsc);

    lv_draw_rect_dsc_t border_dsc;
    init_rect_dsc(&border_dsc, FOREGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, 1, &border_dsc);
    lv_canvas_draw_rect(canvas, 0, CANVAS_SIZE - 1, CANVAS_SIZE, 1, &border_dsc);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    // Central: show recent keystrokes + WPM
    int len = strlen(char_buffer);

    // Keystrokes at top
    if (len == 0) {
        lv_canvas_draw_text(canvas, 0, 12, CANVAS_SIZE, &label_dsc, "...");
    } else {
        lv_canvas_draw_text(canvas, 0, 12, CANVAS_SIZE, &label_dsc, char_buffer);
    }

#if IS_ENABLED(CONFIG_ZMK_WPM)
    // WPM at bottom
    char wpm_text[16];
    snprintf(wpm_text, sizeof(wpm_text), "%d WPM", current_wpm);
    lv_draw_label_dsc_t wpm_dsc;
    init_label_dsc(&wpm_dsc, &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 0, 38, CANVAS_SIZE, &wpm_dsc, wpm_text);
#endif
#else
    // Peripheral: show uptime
    int64_t uptime_ms = k_uptime_get();
    int hours = (int)(uptime_ms / 3600000);
    int mins = (int)((uptime_ms % 3600000) / 60000);
    int secs = (int)((uptime_ms % 60000) / 1000);

    char uptime_text[16];
    if (hours > 0) {
        snprintf(uptime_text, sizeof(uptime_text), "%dh %dm", hours, mins);
    } else {
        snprintf(uptime_text, sizeof(uptime_text), "%dm %ds", mins, secs);
    }

    lv_draw_label_dsc_t big_dsc;
    init_label_dsc(&big_dsc, &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 0, 24, CANVAS_SIZE, &big_dsc, uptime_text);
#endif

    rotate_canvas(canvas, widget->cbuf2);
}

// ============================================================================
// BOTTOM SECTION: Layer indicator
// ============================================================================

static void draw_bottom(struct zmk_widget_custom_status *widget) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 2);

    lv_draw_rect_dsc_t bg_dsc;
    init_rect_dsc(&bg_dsc, BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &bg_dsc);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    const char *layer_names[] = {"BASE", "LOWER", "RAISE"};
    const char *name;

    if (widget->state.layer_label && strlen(widget->state.layer_label) > 0) {
        name = widget->state.layer_label;
    } else if (widget->state.layer_index < 3) {
        name = layer_names[widget->state.layer_index];
    } else {
        static char buf[12];
        snprintf(buf, sizeof(buf), "L%d", widget->state.layer_index);
        name = buf;
    }

    lv_canvas_draw_text(canvas, 0, 24, CANVAS_SIZE, &label_dsc, name);
#else
    // Peripheral: show "RIGHT" identifier
    lv_canvas_draw_text(canvas, 0, 24, CANVAS_SIZE, &label_dsc, "RIGHT");
#endif

    rotate_canvas(canvas, widget->cbuf3);
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

struct battery_state {
    uint8_t level;
};

static void battery_update_cb(struct battery_state state) {
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.battery = state.level;
        draw_top(widget);
    }
}

static struct battery_state battery_get_state(const zmk_event_t *eh) {
    return (struct battery_state){.level = zmk_battery_state_of_charge()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery, struct battery_state, battery_update_cb, battery_get_state)
ZMK_SUBSCRIPTION(widget_battery, zmk_battery_state_changed);

// Central-only event handlers
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#if IS_ENABLED(CONFIG_ZMK_BLE)
struct bt_state {
    int profile;
    bool connected;
};

static void bt_update_cb(struct bt_state state) {
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.bt_profile = state.profile;
        widget->state.bt_connected = state.connected;
        draw_top(widget);
    }
}

static struct bt_state bt_get_state(const zmk_event_t *eh) {
    return (struct bt_state){
        .profile = zmk_ble_active_profile_index(),
        .connected = zmk_ble_active_profile_is_connected(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bt, struct bt_state, bt_update_cb, bt_get_state)
ZMK_SUBSCRIPTION(widget_bt, zmk_ble_active_profile_changed);
#endif

struct layer_state {
    uint8_t index;
    const char *label;
};

static void layer_update_cb(struct layer_state state) {
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.layer_index = state.index;
        widget->state.layer_label = state.label;
        draw_bottom(widget);
    }
}

static struct layer_state layer_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_state){
        .index = index,
        .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index)),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer, struct layer_state, layer_update_cb, layer_get_state)
ZMK_SUBSCRIPTION(widget_layer, zmk_layer_state_changed);

#if IS_ENABLED(CONFIG_ZMK_WPM)
// WPM handling
struct wpm_state {
    int wpm;
};

static void wpm_update_cb(struct wpm_state state) {
    current_wpm = state.wpm;
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_middle(widget);
    }
}

static struct wpm_state wpm_get_state(const zmk_event_t *eh) {
    return (struct wpm_state){.wpm = zmk_wpm_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm, struct wpm_state, wpm_update_cb, wpm_get_state)
ZMK_SUBSCRIPTION(widget_wpm, zmk_wpm_state_changed);
#endif

// Keycode handling
static char keycode_to_char(uint32_t keycode, bool shifted) {
    if (keycode >= HID_USAGE_KEY_KEYBOARD_A && keycode <= HID_USAGE_KEY_KEYBOARD_Z) {
        char base = 'a' + (keycode - HID_USAGE_KEY_KEYBOARD_A);
        return shifted ? (base - 32) : base;
    }

    if (keycode >= HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION &&
        keycode <= HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS) {
        if (shifted) {
            const char shifted_nums[] = "!@#$%^&*()";
            if (keycode == HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS) {
                return ')';
            }
            return shifted_nums[keycode - HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION];
        }
        if (keycode == HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS) {
            return '0';
        }
        return '1' + (keycode - HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION);
    }

    switch (keycode) {
        case HID_USAGE_KEY_KEYBOARD_SPACEBAR: return ' ';
        case HID_USAGE_KEY_KEYBOARD_MINUS_AND_UNDERSCORE: return shifted ? '_' : '-';
        case HID_USAGE_KEY_KEYBOARD_EQUAL_AND_PLUS: return shifted ? '+' : '=';
        case HID_USAGE_KEY_KEYBOARD_LEFT_BRACKET_AND_LEFT_BRACE: return shifted ? '{' : '[';
        case HID_USAGE_KEY_KEYBOARD_RIGHT_BRACKET_AND_RIGHT_BRACE: return shifted ? '}' : ']';
        case HID_USAGE_KEY_KEYBOARD_BACKSLASH_AND_PIPE: return shifted ? '|' : '\\';
        case HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON: return shifted ? ':' : ';';
        case HID_USAGE_KEY_KEYBOARD_APOSTROPHE_AND_QUOTE: return shifted ? '"' : '\'';
        case HID_USAGE_KEY_KEYBOARD_GRAVE_ACCENT_AND_TILDE: return shifted ? '~' : '`';
        case HID_USAGE_KEY_KEYBOARD_COMMA_AND_LESS_THAN: return shifted ? '<' : ',';
        case HID_USAGE_KEY_KEYBOARD_PERIOD_AND_GREATER_THAN: return shifted ? '>' : '.';
        case HID_USAGE_KEY_KEYBOARD_SLASH_AND_QUESTION_MARK: return shifted ? '?' : '/';
        default: return 0;
    }
}

static void add_char_to_buffer(char c) {
    if (c == 0) return;

    if (char_buffer_pos >= CHAR_BUFFER_SIZE) {
        memmove(char_buffer, char_buffer + 1, CHAR_BUFFER_SIZE - 1);
        char_buffer_pos = CHAR_BUFFER_SIZE - 1;
    }

    char_buffer[char_buffer_pos++] = c;
    char_buffer[char_buffer_pos] = '\0';
}

struct keycode_state {
    uint32_t keycode;
    bool pressed;
};

static void keycode_update_cb(struct keycode_state state) {
    // Redraw top section (modifiers are read from HID report in draw_top)
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_top(widget);
    }

    if (!state.pressed) return;

    // Get current shift state from HID report
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();
    bool shifted = (mods & (MOD_LSFT | MOD_RSFT)) != 0;

    char c = keycode_to_char(state.keycode, shifted);
    if (c != 0) {
        add_char_to_buffer(c);

        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            draw_middle(widget);
        }
    }
}

static struct keycode_state keycode_get_state(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    return (struct keycode_state){
        .keycode = ev->keycode,
        .pressed = ev->state,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_keycode, struct keycode_state, keycode_update_cb, keycode_get_state)
ZMK_SUBSCRIPTION(widget_keycode, zmk_keycode_state_changed);

#endif // Central-only

// ============================================================================
// WIDGET INITIALIZATION
// ============================================================================

int zmk_widget_custom_status_init(struct zmk_widget_custom_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);

    lv_obj_t *middle = lv_canvas_create(widget->obj);
    lv_obj_align(middle, LV_ALIGN_TOP_LEFT, 24, 0);
    lv_canvas_set_buffer(middle, widget->cbuf2, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);

    lv_obj_t *bottom = lv_canvas_create(widget->obj);
    lv_obj_align(bottom, LV_ALIGN_TOP_LEFT, -44, 0);
    lv_canvas_set_buffer(bottom, widget->cbuf3, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);

    widget->state.battery = zmk_battery_state_of_charge();

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#if IS_ENABLED(CONFIG_ZMK_BLE)
    widget->state.bt_profile = zmk_ble_active_profile_index();
    widget->state.bt_connected = zmk_ble_active_profile_is_connected();
#endif
    widget->state.layer_index = zmk_keymap_highest_layer_active();
    widget->state.layer_label = NULL;
#if IS_ENABLED(CONFIG_ZMK_WPM)
    current_wpm = zmk_wpm_get_state();
#endif
#else
    widget->state.layer_index = 0;
    widget->state.layer_label = NULL;
#endif

    sys_slist_append(&widgets, &widget->node);

    widget_battery_init();

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#if IS_ENABLED(CONFIG_ZMK_BLE)
    widget_bt_init();
#endif
    widget_layer_init();
#if IS_ENABLED(CONFIG_ZMK_WPM)
    widget_wpm_init();
#endif
    widget_keycode_init();
#endif

    draw_top(widget);
    draw_middle(widget);
    draw_bottom(widget);

    return 0;
}

lv_obj_t *zmk_widget_custom_status_obj(struct zmk_widget_custom_status *widget) {
    return widget->obj;
}
