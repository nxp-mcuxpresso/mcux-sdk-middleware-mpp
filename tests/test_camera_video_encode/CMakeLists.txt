cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_camera_video_encode LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_camera_video_encode)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_camera_video_encode/test_camera_video_encode.c
            middleware/eiq/mpp/boards/${board}/tests/test_camera_video_encode/mpp_config.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_camera_video_encode/
             middleware/eiq/mpp/tests/test_camera_video_encode/${board}/
             middleware/eiq/mpp/tests/test_camera_video_encode/
)

# board files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/board.cmake)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_camera_video_encode/reconfig.cmake OPTIONAL)

mcux_add_armgcc_configuration(
    LD "-Xlinker --defsym=__heap_size__=0x200000"
)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

