
if (CONFIG_MCUX_COMPONENT_middleware.eiq.mpp)
    # MPP sources
    mcux_add_source(
        BASE_PATH ${CMAKE_CURRENT_LIST_DIR}
        SOURCES src/mpp_api.c
        SOURCES src/mpp_debug.c
        SOURCES src/mpp_elements.c
        SOURCES src/mpp_element_camera.c
        SOURCES src/mpp_element_display.c
        SOURCES src/mpp_element_img_convert.c
        SOURCES src/mpp_element_img_decode.c
        SOURCES src/mpp_element_labeled_rectangle.c
        SOURCES src/mpp_element_static_img.c
        SOURCES src/mpp_element_inference.c
        SOURCES src/mpp_element_nullsink.c
        SOURCES src/mpp_heap.c
        SOURCES src/mpp_memory.c
        SOURCES src/*.h
        include/mpp_api_types.h
        include/mpp_api.h
    )
    # include path    
    mcux_add_include(
        BASE_PATH ${CMAKE_CURRENT_LIST_DIR}
        INCLUDES include
            hal/include
            src
            .
    )
    mcux_add_include(
        BASE_PATH ${SdkRootDirPath}
        INCLUDES middleware/libjpeg/template
    )

    # macros
    mcux_add_macro(
        CC "-DSDK_OS_FREE_RTOS"
    )
    # set log level
    # supported log levels (from mpp_debug.h file):
        # LOG_LVL_ERR       0 (default)
        # LOG_LVL_INFO      1
        # LOG_LVL_DEBUG     2
    if (NOT ${HAL_LOG_LEVEL} STREQUAL "")
        mcux_add_macro (
            CC "-DHAL_LOG_LEVEL=${HAL_LOG_LEVEL}"
        )
    endif ()
    # set MPP commit
    if (NOT ${MPP_COMMIT} STREQUAL "")
        mcux_add_macro (
            CC "-DMPP_COMMIT=${MPP_COMMIT}"
        )
    endif ()
    # set extra build flags
    if (NOT ${EXTRA_BUILD_FLAGS} STREQUAL "")
        separate_arguments (LIST_FLAG UNIX_COMMAND ${EXTRA_BUILD_FLAGS})
        foreach(flag ${LIST_FLAG})
            message("EXTRA_BUILD_FLAGS: ${flag}")
            mcux_add_macro (
                CC "${flag}"
            )
        endforeach()
    endif () #(NOT ${EXTRA_BUILD_FLAGS} STREQUAL "")
    # HAL sources
    mcux_add_source(
        BASE_PATH ${CMAKE_CURRENT_LIST_DIR}
        SOURCES hal/hal_camera_mipi_ov5640.c
        SOURCES hal/hal_draw.c
        SOURCES hal/hal_display_lcdifv2_rk055.c
        SOURCES hal/hal_graphics_pxp.c
        SOURCES hal/hal_graphics_cpu.c
        SOURCES hal/hal_static_image.c
        SOURCES hal/hal_utils.c
        SOURCES hal/hal_freertos.c
        SOURCES hal/hal_vision_algo_tflite.c
        SOURCES hal/tflite/model.cpp
        SOURCES hal/tflite/model_all_ops_micro.cpp
        SOURCES hal/hal_display_lcdif_rk043fn.c
        SOURCES hal/hal_camera_csi_mt9m114.c
        SOURCES hal/hal_display_spi_ili9341.c
        SOURCES hal/hal_camera_ezh_ov7670.c
        SOURCES hal/hal_display_mculcd_ssd1963.c
        SOURCES hal/hal_display_mculcd_st7796s.c
        SOURCES hal/hal_display_lvgl.c
        SOURCES hal/hal_graphics_vglite.c
        SOURCES hal/hal_camera_ezhv_ov7670.c
        SOURCES hal/hal_jpegdec_cpu.c
        SOURCES hal/hal_camera_usb.c
        SOURCES hal/include/*.h
        SOURCES hal/tflite/*.h
    )

endif() #(CONFIG_MCUX_COMPONENT_middleware.eiq.mpp)
