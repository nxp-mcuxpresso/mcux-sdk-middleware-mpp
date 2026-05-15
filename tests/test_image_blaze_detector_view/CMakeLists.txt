cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_image_blaze_detector_view LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_image_blaze_detector_view)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_image_blaze_detector_view/test_image_blaze_detector_view.c
            middleware/eiq/mpp/boards/${board}/tests/test_image_blaze_detector_view/mpp_config.h
            middleware/eiq/mpp/tests/test_image_blaze_detector_view/*.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_image_blaze_detector_view/
             middleware/eiq/mpp/tests/test_image_blaze_detector_view/
)

# board files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/board.cmake)
# model and post-processing
include(${SdkRootDirPath}/middleware/eiq/mpp/models/blaze_detector_ptq/model.cmake)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_image_blaze_detector_view/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
