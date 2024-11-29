# config to select component, the format is CONFIG_USE_${component}
# Please refer to cmake files below to get available components:
#  ${SdkRootDirPath}/devices/MCXN947/all_lib_device.cmake

set(CONFIG_COMPILER gcc)
set(CONFIG_TOOLCHAIN armgcc)
set(CONFIG_USE_COMPONENT_CONFIGURATION false)
set(CONFIG_USE_utility_debug_console_lite true)
set(CONFIG_USE_utility_assert_lite true)
set(CONFIG_USE_utility_str true)
set(CONFIG_USE_component_lists true)
set(CONFIG_USE_component_lpuart_adapter true)
set(CONFIG_USE_driver_lpuart true)
set(CONFIG_USE_device_MCXN947_CMSIS true)
set(CONFIG_USE_device_MCXN947_startup true)
set(CONFIG_USE_driver_port true)
set(CONFIG_USE_driver_clock true)
set(CONFIG_USE_driver_common true)
set(CONFIG_USE_driver_gpio true)
set(CONFIG_USE_driver_reset true)
set(CONFIG_USE_utilities_misc_utilities true)
set(CONFIG_USE_driver_lpflexcomm true)
set(CONFIG_USE_CMSIS_Include_core_cm true)
set(CONFIG_USE_device_MCXN947_system true)
set(CONFIG_USE_driver_mcx_spc true)

set(CONFIG_CORE cm33)
set(CONFIG_DEVICE MCXN947)
set(CONFIG_BOARD mcxn9xxbrk)
set(CONFIG_KIT mcxn9xxbrk)
set(CONFIG_DEVICE_ID MCXN947)
set(CONFIG_FPU SP_FPU)
set(CONFIG_DSP DSP)
set(CONFIG_CORE_ID cm33_core0)

set(CONFIG_USE_middleware_freertos-kernel_cm33_non_trustzone true)
set(CONFIG_USE_middleware_freertos-kernel true)
set(CONFIG_USE_middleware_freertos-kernel_heap_4 true)
set(CONFIG_USE_middleware_freertos-kernel_extension true)
set(CONFIG_USE_middleware_freertos-kernel_template true)

set(CONFIG_USE_driver_ssd1963 true)
set(CONFIG_USE_driver_dbi true)
set(CONFIG_USE_driver_flexio true)
set(CONFIG_USE_driver_flexio_mculcd true)
set(CONFIG_USE_driver_flexio_mculcd_edma true)
set(CONFIG_USE_driver_edma4 true)
set(CONFIG_USE_driver_edma_soc true)
set(CONFIG_USE_driver_utick true)

set(DEP_LIBS
	${SDK_DIR}/middleware/eiq/tensorflow-lite/third_party/neutron/libnd.a
	${SDK_DIR}/middleware/eiq/tensorflow-lite/third_party/neutron/libnfw.a
)
