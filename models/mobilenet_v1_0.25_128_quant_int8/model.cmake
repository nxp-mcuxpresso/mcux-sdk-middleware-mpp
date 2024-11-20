#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    INCLUDES    .
                mobilenet_v1_0.25_128_quant_int8
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES get_top_n.cpp
            utils.cpp
            mobilenet_v1_0.25_128_quant_int8/mobilenetv1_ops_micro_tflite.cpp
            mobilenet_v1_0.25_128_quant_int8/mobilenetv1_output_postproc.cpp
            mobilenet_v1_0.25_128_quant_int8/*.h
            get_top_n.h
            utils.h
)
