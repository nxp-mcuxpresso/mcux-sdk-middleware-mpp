# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

message(STATUS "MPP Module: Adding Zephyr-compatible sources")

# Add MPP library
zephyr_library_named(mpp)

# Add MPP source files
zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_api.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_heap.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_memory.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_debug.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_double_buffer.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_elements.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_camera.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_display.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_img_convert.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_img_decode.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_inference.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_labeled_rectangle.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_nullsink.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_static_img.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_img_compose.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/mpp_element_img_quality_check.c
)

# Add HAL source files
zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_static_image.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_utils.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_draw.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_graphics_cpu.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_jpegdec_cpu.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_graphics_pxp.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_graphics_vglite.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_display_lvgl.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/os/zephyr/hal_zephyr.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_vision_algo_tflite.c
    # Add Zephyr-specific camera and display HAL implementations
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_camera_zephyr.c
    ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_display_zephyr.c
)

# Add TensorFlow Lite sources conditionally
if(CONFIG_TENSORFLOW_LITE_MICRO)
    zephyr_library_sources(
        ${CMAKE_CURRENT_LIST_DIR}/../hal/tflite/model.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../hal/tflite/model_all_ops_micro.cpp
    )
endif()

# Add include directories
zephyr_library_include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/../inc
    ${CMAKE_CURRENT_LIST_DIR}/../hal
)

# Try to find mpp_config.h in common locations
set(CONFIG_SEARCH_PATHS
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/app"
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    "${CMAKE_CURRENT_LIST_DIR}/../../app"
)

foreach(CONFIG_PATH ${CONFIG_SEARCH_PATHS})
    if(EXISTS "${CONFIG_PATH}/mpp_config.h")
        zephyr_library_include_directories(${CONFIG_PATH})
        message(STATUS "MPP HAL: Found mpp_config.h in ${CONFIG_PATH}")
        break()
    endif()
endforeach()

# Make MPP's include directories available globally
zephyr_include_directories(
    ${ZEPHYR_CURRENT_MODULE_DIR}/include
    ${ZEPHYR_CURRENT_MODULE_DIR}/hal/include
    ${ZEPHYR_CURRENT_MODULE_DIR}/hal/os/include
)

# Add compile definitions
include(board_mapping.cmake)

if(DEFINED MPP_BOARD)
    target_include_directories(mpp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../boards/${MPP_BOARD}/inc
    )
    zephyr_library_sources(
        ${CMAKE_CURRENT_LIST_DIR}/../hal/hal_${MPP_BOARD}.c
    )
endif()

# Add required MCUX driver include paths
target_include_directories(mpp PRIVATE
    ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk-ng/drivers/csi
    ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk-ng/drivers/lcdifv2
    ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk-ng/drivers/mipi_csi2rx
    ${ZEPHYR_HAL_NXP_MODULE_DIR}/mcux/mcux-sdk-ng/drivers/pxp
    # Add other drivers as needed
)

# Add TensorFlow Lite Micro include paths if enabled
if(CONFIG_TENSORFLOW_LITE_MICRO)
    target_include_directories(mpp PRIVATE
        ${ZEPHYR_BASE}/../modules/lib/tflite-micro
    )
endif()
