# Custom Nice!View widget build
# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_DISPLAY AND CONFIG_NICE_VIEW_CUSTOM_WIDGET)
    zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
    zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/widgets)

    zephyr_library_sources(custom_screen.c)
    zephyr_library_sources(widgets/util.c)
    zephyr_library_sources(widgets/art.c)

    if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        zephyr_library_sources(widgets/custom_status.c)
    else()
        zephyr_library_sources(widgets/peripheral_status.c)
    endif()
endif()
