cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_camera_mobilefacenet_shell_console_view LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_camera_mobilefacenet_shell_console_view)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_camera_mobilefacenet_shell_console_view/test_camera_mobilefacenet_shell_console_view.c
            middleware/eiq/mpp/boards/${board}/tests/test_camera_mobilefacenet_shell_console_view/mpp_config.h
            middleware/eiq/mpp/internal/images/thispersondoesnotexist_11_bgra.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_camera_mobilefacenet_shell_console_view/
)

# board files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/board.cmake)
# model and post-processing
mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    INCLUDES    models
                models/mobilefacenet
                internal/images
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    SOURCES models/get_top_n.cpp
            models/utils.cpp
            models/get_top_n.h
            models/utils.h
            models/shell_database_utils.h
            models/shell_database_utils.c
            models/mobilefacenet/mobilefacenet_output_postproc_quantized.cpp
            models/mobilefacenet/*.h
)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_camera_mobilefacenet_shell_console_view/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)

