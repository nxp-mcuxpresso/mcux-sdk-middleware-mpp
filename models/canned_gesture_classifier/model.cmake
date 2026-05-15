#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    INCLUDES    models
                models/canned_gesture_classifier
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES utils.cpp
            utils.h
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES canned_gesture_classifier/canned_gesture_classifier_ops_micro_tflite.cpp
            canned_gesture_classifier/canned_gesture_classifier_output_postproc.c
            canned_gesture_classifier/*.h
)
