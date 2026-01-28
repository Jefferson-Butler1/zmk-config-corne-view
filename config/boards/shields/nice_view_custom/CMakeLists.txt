# Custom Nice!View widget build
# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_DISPLAY AND CONFIG_NICE_VIEW_CUSTOM_WIDGET)
    zephyr_library_include_directories(${ZMK_APP_DIR}/include)
    zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    zephyr_library_sources(custom_screen.c)
    zephyr_library_sources(widgets/custom_status.c)
endif()
