cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_mc_image_convert_display LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_mc_image_convert_display/${core_id})

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_mc_image_convert_display/core1/test_mc_image_convert_display.c
            middleware/eiq/mpp/boards/${board}/tests/test_mc_image_convert_display/${core_id}/mpp_config.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_mc_image_convert_display/
             middleware/eiq/mpp/boards/${board}/tests/test_mc_image_convert_display/${core_id}/
)

# board and core-specific files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/board.cmake)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_mc_image_convert_display/reconfig.cmake OPTIONAL)
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_mc_image_convert_display/${core_id}/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/../test_mc_image_convert_display/${CONFIG_TOOLCHAIN}/core1_image.bin)
