/*
 * Custom Nice!View Status Widget
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_custom_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_color_t cbuf[68 * 68];      // Top section (battery/bt)
    lv_color_t cbuf2[68 * 68];     // Middle section (custom art)
    lv_color_t cbuf3[68 * 68];     // Bottom section (layer)
    struct {
        uint8_t battery;
        bool charging;
        bool bt_connected;
        int bt_profile;
        uint8_t layer_index;
        const char *layer_label;
    } state;
};

int zmk_widget_custom_status_init(struct zmk_widget_custom_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_custom_status_obj(struct zmk_widget_custom_status *widget);
