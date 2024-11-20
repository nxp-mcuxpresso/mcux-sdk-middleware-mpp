#list app specific source files
list(APPEND APP_MODEL_SRC
    ${S}examples/models/get_top_n.cpp
    ${S}examples/models/utils.cpp
    ${S}examples/models/ultraface_slim_quant_int8/ultraface_output_postproc.c
    ${S}examples/models/persondetect/persondetect_output_postprocess.c
)

