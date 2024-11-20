#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    INCLUDES    .
                ultraface_slim_quant_int8
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES get_top_n.cpp
            utils.cpp
            ultraface_slim_quant_int8/ultraface_output_postproc.c
            ultraface_slim_quant_int8/ultraface_ops_micro_tflite.cpp
            ultraface_slim_quant_int8/*.h
            get_top_n.h
            utils.h
)

