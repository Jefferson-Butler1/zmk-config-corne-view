/*
 * Custom Nice!View screen - main entry point
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include "widgets/custom_status.h"
static struct zmk_widget_custom_status status_widget;
#else
#include "widgets/peripheral_status.h"
static struct zmk_widget_peripheral_status status_widget;
#endif

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zmk_widget_custom_status_init(&status_widget, screen);
    lv_obj_align(zmk_widget_custom_status_obj(&status_widget), LV_ALIGN_TOP_LEFT, 0, 0);
#else
    zmk_widget_peripheral_status_init(&status_widget, screen);
    lv_obj_align(zmk_widget_peripheral_status_obj(&status_widget), LV_ALIGN_TOP_LEFT, 0, 0);
#endif

    return screen;
}
