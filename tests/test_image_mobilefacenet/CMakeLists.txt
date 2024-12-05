cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_image_mobilefacenet LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_image_mobilefacenet)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_image_mobilefacenet/test_image_mobilefacenet.c
            middleware/eiq/mpp/boards/${board}/tests/test_image_mobilefacenet/mpp_config.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_image_mobilefacenet/
)

# board files
include(${SdkRootDirPath}/examples/_boards/${board}/eiq_examples/mpp/board.cmake)
# model and post-processing
include(${SdkRootDirPath}/middleware/eiq/mpp/internal/models/mobilefacenet/model.cmake)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_image_mobilefacenet/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

