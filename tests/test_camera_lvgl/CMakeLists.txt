cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_camera_lvgl LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_camera_lvgl)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_camera_lvgl/test_camera_lvgl.c
            middleware/eiq/mpp/tests/test_camera_lvgl/lvgl_camera_scene.c
            middleware/eiq/mpp/boards/${board}/tests/test_camera_lvgl/mpp_config.h
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/examples/lvgl_examples/
    SOURCES lvgl_freertos.c
            lvgl_freertos.h
            lvgl_demo_utils.c
            lvgl_demo_utils.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_camera_lvgl/
)

# board files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/board.cmake)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_camera_lvgl/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

