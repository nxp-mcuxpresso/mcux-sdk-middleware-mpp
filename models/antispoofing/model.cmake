#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    INCLUDES    .
                antispoofing
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models/antispoofing
    SOURCES antispoofing_fully_quantized_ops_micro_tflite.cpp
            antispoofing_output_postproc_quantized.cpp
            ./*.h
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES utils.h
)
