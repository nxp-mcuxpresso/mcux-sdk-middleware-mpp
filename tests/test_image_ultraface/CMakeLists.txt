cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_image_ultraface LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_image_ultraface)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_image_ultraface/test_image_ultraface.c
            middleware/eiq/mpp/boards/${board}/tests/test_image_ultraface/mpp_config.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_image_ultraface/
)

# board files
include(${SdkRootDirPath}/examples/_boards/${board}/eiq_examples/mpp/board.cmake)
# model and post-processing
include(${SdkRootDirPath}/middleware/eiq/mpp/models/ultraface_slim_quant_int8/model.cmake)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_image_ultraface/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

