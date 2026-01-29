/*
 * Custom Nice!View Status Widget - Central
 * Battery %, modifiers, WPM graph, layer
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#include <zmk/events/layer_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/hid.h>
#include <zmk/wpm.h>
#include <zmk/events/wpm_state_changed.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>

#include "util.h"
#include "custom_status.h"

LV_IMG_DECLARE(bolt);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// TOP: Battery with % inside | Connection status
static void draw_top(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t label_dsc_right;
    init_label_dsc(&label_dsc_right, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);

    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Connection status (top right, draw first so battery can overlap if needed)
    char conn_text[10] = {};
    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        strcat(conn_text, LV_SYMBOL_USB);
        break;
    case ZMK_TRANSPORT_BLE:
        if (state->active_profile_bonded) {
            if (state->active_profile_connected) {
                snprintf(conn_text, sizeof(conn_text), LV_SYMBOL_WIFI " %d",
                         state->active_profile_index + 1);
            } else {
                snprintf(conn_text, sizeof(conn_text), LV_SYMBOL_CLOSE " %d",
                         state->active_profile_index + 1);
            }
        } else {
            strcat(conn_text, LV_SYMBOL_SETTINGS);
        }
        break;
    }
    lv_canvas_draw_text(canvas, 40, 0, CANVAS_SIZE - 42, &label_dsc_right, conn_text);

    // Battery outline with fill level
    lv_canvas_draw_rect(canvas, 0, 2, 29, 12, &rect_white_dsc);
    lv_canvas_draw_rect(canvas, 1, 3, 27, 10, &rect_black_dsc);
    lv_canvas_draw_rect(canvas, 2, 4, (state->battery * 25) / 100, 8, &rect_white_dsc);
    lv_canvas_draw_rect(canvas, 29, 4, 3, 6, &rect_white_dsc);
    lv_canvas_draw_rect(canvas, 30, 5, 1, 4, &rect_black_dsc);

    // Battery percentage text inside
    char bat_text[5];
    snprintf(bat_text, sizeof(bat_text), "%d", state->battery);
    lv_canvas_draw_text(canvas, 0, 0, 29, &label_dsc, bat_text);

    // Charging bolt
    if (state->charging) {
        lv_draw_img_dsc_t img_dsc;
        lv_draw_img_dsc_init(&img_dsc);
        lv_canvas_draw_img(canvas, 9, -1, &bolt, &img_dsc);
    }

    rotate_canvas(canvas, cbuf);
}

// MIDDLE: Modifiers + WPM graph
static void draw_middle(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 1);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t label_dsc_inv;
    init_label_dsc(&label_dsc_inv, LVGL_BACKGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t label_dsc_wpm;
    init_label_dsc(&label_dsc_wpm, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_RIGHT);
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 2);

    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Modifier indicators at top
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();
    bool mod_ctrl = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    bool mod_alt = (mods & (MOD_LALT | MOD_RALT)) != 0;
    bool mod_gui = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
    bool mod_shift = (mods & (MOD_LSFT | MOD_RSFT)) != 0;

    int box_w = 15;
    int gap = 2;
    int total_w = 4 * box_w + 3 * gap;
    int start_x = (CANVAS_SIZE - total_w) / 2;
    int y = 2;

    const char *mod_labels[] = {"C", "A", "G", "S"};
    bool mod_states[] = {mod_ctrl, mod_alt, mod_gui, mod_shift};

    for (int i = 0; i < 4; i++) {
        int x = start_x + i * (box_w + gap);
        if (mod_states[i]) {
            lv_canvas_draw_rect(canvas, x, y, box_w, 18, &rect_white_dsc);
            lv_canvas_draw_text(canvas, x, y + 1, box_w, &label_dsc_inv, mod_labels[i]);
        } else {
            lv_canvas_draw_text(canvas, x, y + 1, box_w, &label_dsc, mod_labels[i]);
        }
    }

    // WPM graph box
    lv_canvas_draw_rect(canvas, 0, 24, 68, 42, &rect_white_dsc);
    lv_canvas_draw_rect(canvas, 1, 25, 66, 40, &rect_black_dsc);

    // Current WPM number
    char wpm_text[6];
    snprintf(wpm_text, sizeof(wpm_text), "%d", state->wpm[9]);
    lv_canvas_draw_text(canvas, 42, 54, 24, &label_dsc_wpm, wpm_text);

    // WPM graph line
    int max = 0;
    int min = 256;
    for (int i = 0; i < 10; i++) {
        if (state->wpm[i] > max) max = state->wpm[i];
        if (state->wpm[i] < min) min = state->wpm[i];
    }
    int range = max - min;
    if (range == 0) range = 1;

    lv_point_t points[10];
    for (int i = 0; i < 10; i++) {
        points[i].x = 2 + i * 7;
        points[i].y = 63 - (state->wpm[i] - min) * 36 / range;
    }

    lv_canvas_draw_line(canvas, points, 10, &line_dsc);

    rotate_canvas(canvas, cbuf);
}

// BOTTOM: Layer name
static void draw_bottom(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 2);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);

    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    if (state->layer_label == NULL || strlen(state->layer_label) == 0) {
        char text[12];
        snprintf(text, sizeof(text), "LAYER %i", state->layer_index);
        lv_canvas_draw_text(canvas, 0, 24, CANVAS_SIZE, &label_dsc, text);
    } else {
        lv_canvas_draw_text(canvas, 0, 24, CANVAS_SIZE, &label_dsc, state->layer_label);
    }

    rotate_canvas(canvas, cbuf);
}

// Event handlers
static void set_battery_status(struct zmk_widget_custom_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_status(widget, state);
    }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    return (struct battery_status_state){
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
};

static void set_output_status(struct zmk_widget_custom_status *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_output_status(widget, &state);
    }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);

struct layer_status_state {
    uint8_t index;
    const char *label;
};

static void set_layer_status(struct zmk_widget_custom_status *widget,
                             struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;
    draw_bottom(widget->obj, widget->cbuf3, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_status(widget, state);
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){
        .index = index,
        .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index)),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state,
                            layer_status_update_cb, layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

struct wpm_status_state {
    uint8_t wpm;
};

static void set_wpm_status(struct zmk_widget_custom_status *widget,
                           struct wpm_status_state state) {
    for (int i = 0; i < 9; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[9] = state.wpm;
    draw_middle(widget->obj, widget->cbuf2, &widget->state);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_wpm_status(widget, state);
    }
}

static struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state,
                            wpm_status_update_cb, wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

// Keycode handler for modifier updates
struct keycode_state {
    bool pressed;
};

static void keycode_update_cb(struct keycode_state state) {
    struct zmk_widget_custom_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_middle(widget->obj, widget->cbuf2, &widget->state);
    }
}

static struct keycode_state keycode_get_state(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    return (struct keycode_state){.pressed = ev->state};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_keycode, struct keycode_state,
                            keycode_update_cb, keycode_get_state)
ZMK_SUBSCRIPTION(widget_keycode, zmk_keycode_state_changed);

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

    sys_slist_append(&widgets, &widget->node);

    widget->state.battery = zmk_battery_state_of_charge();
    widget->state.selected_endpoint = zmk_endpoints_selected();
    widget->state.active_profile_index = zmk_ble_active_profile_index();
    widget->state.active_profile_connected = zmk_ble_active_profile_is_connected();
    widget->state.active_profile_bonded = !zmk_ble_active_profile_is_open();
    widget->state.layer_index = zmk_keymap_highest_layer_active();
    widget->state.layer_label = NULL;

    widget_battery_status_init();
    widget_output_status_init();
    widget_layer_status_init();
    widget_wpm_status_init();
    widget_keycode_init();

    draw_top(widget->obj, widget->cbuf, &widget->state);
    draw_middle(widget->obj, widget->cbuf2, &widget->state);
    draw_bottom(widget->obj, widget->cbuf3, &widget->state);

    return 0;
}

lv_obj_t *zmk_widget_custom_status_obj(struct zmk_widget_custom_status *widget) {
    return widget->obj;
}
