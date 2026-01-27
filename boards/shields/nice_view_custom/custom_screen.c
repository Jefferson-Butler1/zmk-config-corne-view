/*
 * Custom Nice!View screen - main entry point
 * SPDX-License-Identifier: MIT
 */

#include "widgets/custom_status.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_custom_status status_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    zmk_widget_custom_status_init(&status_widget, screen);
    lv_obj_align(zmk_widget_custom_status_obj(&status_widget), LV_ALIGN_TOP_LEFT, 0, 0);

    return screen;
}
