#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    INCLUDES    models
                models/scrfd_kps_500m_full_integer_quant
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES get_top_n.cpp
            utils.cpp
            get_top_n.h
            utils.h
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES scrfd_kps_500m_full_integer_quant/scrfd_kps_output_postproc.c
            scrfd_kps_500m_full_integer_quant/scrfd_kps_ops_micro_tflite.cpp
            scrfd_kps_500m_full_integer_quant/*.h
)
