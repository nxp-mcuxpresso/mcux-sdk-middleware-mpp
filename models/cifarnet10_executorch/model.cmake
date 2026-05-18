 # list model specific source files
 
mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    INCLUDES    .
                cifarnet10_executorch
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES get_top_n.cpp
            cifarnet10_executorch/cifar10_output_postproc.cpp
            cifarnet10_executorch/*.h
            get_top_n.h
)


