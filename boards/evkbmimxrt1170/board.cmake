# lists board specific files for SDK
set(COMMON_SRC
    ${S}boards/${BOARD}/src/board.c
    ${S}boards/${BOARD}/src/board_init.c
    ${S}boards/${BOARD}/src/camera_support.c
    ${S}boards/${BOARD}/src/clock_config.c
    ${S}boards/${BOARD}/src/dcd.c
    ${S}boards/${BOARD}/src/display_support.c
    ${S}boards/${BOARD}/src/pin_mux.c
    ${S}boards/${BOARD}/src/gpt_config.c
    ${S}boards/${BOARD}/src/vglite_support.c
)

