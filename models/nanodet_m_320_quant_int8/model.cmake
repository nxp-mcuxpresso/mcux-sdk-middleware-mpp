#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    INCLUDES    .
                nanodet_m_320_quant_int8
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES get_top_n.cpp
            utils.cpp
            nanodet_m_320_quant_int8/nanodet_m_ops_micro_tflite.cpp
            nanodet_m_320_quant_int8/nanodet_m_output_postproc.cpp
            nanodet_m_320_quant_int8/*.h
            get_top_n.h
            utils.h
)

