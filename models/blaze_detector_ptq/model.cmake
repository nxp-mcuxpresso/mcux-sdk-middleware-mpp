#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    INCLUDES    models
                models/blaze_detector_ptq
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES utils.cpp
            utils.h
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES blaze_detector_ptq/blaze_detector_ptq_output_postproc.c
            blaze_detector_ptq/blaze_detector_ptq_ops_micro_tflite.cpp
            blaze_detector_ptq/*.h
)
