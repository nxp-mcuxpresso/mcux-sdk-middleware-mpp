#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/
    INCLUDES    models
                models/mobilefacenet
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models/mobilefacenet
    SOURCES mobilefacenet_fully_quantized_ops_micro_tflite.cpp
            mobilefacenet_output_postproc_quantized.cpp
            ./*.h
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES utils.h
)
