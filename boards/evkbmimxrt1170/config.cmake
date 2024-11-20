# config to select component, the format is CONFIG_USE_${component}
# Please refer to cmake files below to get available components:
#  ${SdkRootDirPath}/devices/MIMXRT1176/all_lib_device.cmake

set(CONFIG_COMPILER gcc)
set(CONFIG_TOOLCHAIN armgcc)
set(CONFIG_USE_COMPONENT_CONFIGURATION true)
set(CONFIG_USE_driver_clock true)
set(CONFIG_USE_middleware_freertos-kernel_heap_4 true)
set(CONFIG_USE_driver_common true)
set(CONFIG_USE_device_MIMXRT1176_CMSIS true)
set(CONFIG_USE_utility_debug_console true)
set(CONFIG_USE_component_lpuart_adapter true)
set(CONFIG_USE_component_serial_manager_uart true)
set(CONFIG_USE_component_serial_manager true)
set(CONFIG_USE_driver_lpuart true)
set(CONFIG_USE_component_lists true)
set(CONFIG_USE_device_MIMXRT1176_startup true)
set(CONFIG_USE_driver_iomuxc true)
set(CONFIG_USE_utility_assert true)
set(CONFIG_USE_driver_igpio true)
set(CONFIG_USE_driver_xip_device true)
set(CONFIG_USE_driver_xip_board_evkbmimxrt1170 true)
set(CONFIG_USE_driver_pmu_1 true)
set(CONFIG_USE_driver_dcdc_soc true)
set(CONFIG_USE_driver_cache_armv7_m7 true)
set(CONFIG_USE_utilities_misc_utilities true)
set(CONFIG_USE_driver_anatop_ai true)
set(CONFIG_USE_middleware_freertos-kernel true)
set(CONFIG_USE_middleware_freertos-kernel_extension true)
set(CONFIG_USE_middleware_freertos-kernel_template true)
set(CONFIG_USE_CMSIS_Include_core_cm true)
set(CONFIG_USE_device_MIMXRT1176_system true)
set(CONFIG_USE_driver_camera-common true)
set(CONFIG_USE_driver_camera-device-common true)
set(CONFIG_USE_driver_camera-device-ov5640 true)
set(CONFIG_USE_driver_camera-device-sccb true)
set(CONFIG_USE_driver_camera-receiver-common true)
set(CONFIG_USE_driver_camera-receiver-csi true)
set(CONFIG_USE_driver_csi true)
set(CONFIG_USE_driver_dc-fb-common true)
set(CONFIG_USE_driver_dc-fb-elcdif true)
set(CONFIG_USE_driver_dc-fb-lcdifv2 true)
set(CONFIG_USE_driver_display-common true)
set(CONFIG_USE_driver_display-hx8394 true)
set(CONFIG_USE_driver_display-mipi-dsi-cmd true)
set(CONFIG_USE_driver_display-rm68191 true)
set(CONFIG_USE_driver_display-rm68200 true)
set(CONFIG_USE_driver_elcdif true)
set(CONFIG_USE_driver_lcdifv2 true)
set(CONFIG_USE_driver_lpi2c true)
set(CONFIG_USE_driver_memory true)
set(CONFIG_USE_driver_mipi_csi2rx true)
set(CONFIG_USE_driver_mipi_dsi_split true)
set(CONFIG_USE_driver_pxp true)
set(CONFIG_USE_driver_soc_mipi_csi2rx true)
set(CONFIG_USE_driver_soc_src true)
set(CONFIG_USE_driver_video-common true)
set(CONFIG_USE_driver_video-i2c true)
set(CONFIG_USE_utility_assert_lite true)
set(CONFIG_USE_utility_debug_console_lite true)
set(CONFIG_USE_utility_str true)
set(CONFIG_USE_driver_dcic true)
set(CONFIG_USE_driver_gpt true)
set(CONFIG_USE_driver_soc_mipi_dsi true)
set(CONFIG_USE_middleware_vglite true)
set(CONFIG_CORE cm7f)
set(CONFIG_DEVICE MIMXRT1176)
set(CONFIG_BOARD evkbmimxrt1170)
set(CONFIG_KIT evkbmimxrt1170)
set(CONFIG_DEVICE_ID MIMXRT1176xxxxx)
set(CONFIG_FPU DP_FPU)
set(CONFIG_DSP NO_DSP)
set(CONFIG_CORE_ID cm7)
