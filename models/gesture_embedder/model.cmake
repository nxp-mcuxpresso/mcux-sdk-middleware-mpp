#list model specific source files

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp
    INCLUDES    models
                models/gesture_embedder
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES utils.cpp
            utils.h
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}/middleware/eiq/mpp/models
    SOURCES gesture_embedder/gesture_embedder1_ops_micro_tflite.cpp
            gesture_embedder/*.h
)
