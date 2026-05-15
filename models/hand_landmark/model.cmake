#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    INCLUDES    models
                models/hand_landmark
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES utils.cpp
            utils.h
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES hand_landmark/hand_landmark_quant_output_postproc.c
            hand_landmark/hand_landmark_quant_ops_micro_tflite.cpp
            hand_landmark/*.h
)
