cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_virtual_dual_camera_view LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_virtual_dual_camera_view/cm33_core1)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_virtual_dual_camera_view/core1/test_virtual_dual_camera_view_core1.c
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_virtual_dual_camera_view/cm33_core1/
             middleware/eiq/mpp/images/
             middleware/eiq/mpp/hal/include/
)

# core-specific files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/${core_id}/reconfig.cmake)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_virtual_dual_camera_view/cm33_core1/reconfig.cmake OPTIONAL)

mcux_convert_binary(
    BINARY ${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/drivers/core1_image.bin
)
