#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    INCLUDES    .
                persondetect
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES get_top_n.cpp
            utils.cpp
            persondetect/persondetect_ops_micro_tflite.cpp
            persondetect/persondetect_output_postprocess.c
            persondetect/*.h
            get_top_n.h
            utils.h
)

