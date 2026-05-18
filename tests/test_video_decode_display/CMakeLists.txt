cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_video_decode_display LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_video_decode_display)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_video_decode_display/test_video_decode_display.c
            middleware/eiq/mpp/boards/${board}/tests/test_video_decode_display/mpp_config.h
            examples/_boards/${board}/sdmmc_config.c
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_video_decode_display/
             middleware/eiq/mpp/tests/test_video_decode_display/${board}/
             middleware/eiq/mpp/tests/test_video_decode_display/
)

# board files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/board.cmake)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_video_decode_display/reconfig.cmake OPTIONAL)

mcux_add_armgcc_configuration(
    LD "-Xlinker --defsym=__heap_size__=0x600000"
)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

