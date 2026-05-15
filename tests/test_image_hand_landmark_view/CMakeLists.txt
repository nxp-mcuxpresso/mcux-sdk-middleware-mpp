cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_image_hand_landmark_view LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH middleware/eiq/mpp/boards/${board}/tests/test_image_hand_landmark_view)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/eiq/mpp/tests/test_image_hand_landmark_view/test_image_hand_landmark_view.c
            middleware/eiq/mpp/boards/${board}/tests/test_image_hand_landmark_view/mpp_config.h
            middleware/eiq/mpp/tests/test_image_hand_landmark_view/*.h
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/eiq/mpp/boards/${board}/tests/test_image_hand_landmark_view/
             middleware/eiq/mpp/tests/test_image_hand_landmark_view/
)

# board files
include(${SdkRootDirPath}/${board_root}/${board}/eiq_examples/mpp/board.cmake)
# model and post-processing
mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    INCLUDES    models
                models/hand_landmark
                models/gesture_embedder
                models/canned_gesture_classifier
)
mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES utils.cpp
            utils.h
            hand_landmark/hand_landmark_quant_output_postproc.c
            hand_landmark/*.h
            gesture_embedder/*.h
            canned_gesture_classifier/canned_gesture_classifier_output_postproc.c
            canned_gesture_classifier/*.h
)

# top level
include(${SdkRootDirPath}/CMakeLists.txt)
# board & app specific cmake
include(${SdkRootDirPath}/middleware/eiq/mpp/boards/${board}/tests/test_image_hand_landmark_view/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
