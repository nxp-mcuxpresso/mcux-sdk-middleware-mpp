cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_multi_image_mobilefacenet_view LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_multi_image_mobilefacenet_view)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_multi_image_mobilefacenet_view/test_multi_image_mobilefacenet_view.c
            middleware/eiq/mpp/boards/${board}/tests/test_multi_image_mobilefacenet_view/mpp_config.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_multi_image_mobilefacenet_view/
)

# board files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/board.cmake)
# model and post-processing
mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    INCLUDES    internal/models
                internal/models/mobilefacenet
                internal/images
                internal/face_recognition
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    SOURCES models/get_top_n.cpp
            models/utils.cpp
            models/get_top_n.h
            models/utils.h
            models/database_utils.c
            internal/models/mobilefacenet/mobilefacenet_output_postproc_quantized.cpp
            internal/models/mobilefacenet/*.h
)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_multi_image_mobilefacenet_view/reconfig.cmake OPTIONAL)

mcux_add_armgcc_configuration(
    LD "-Xlinker --defsym=__stack_size__=0x1000 -Xlinker --defsym=__heap_size__=0x20000"
)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

