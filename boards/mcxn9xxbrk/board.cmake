# lists board specific files for SDK
set(COMMON_SRC
    ${S}boards/${BOARD}/src/board.c
    ${S}boards/${BOARD}/src/board_init.c
    ${S}boards/${BOARD}/src/clock_config.c
    ${S}boards/${BOARD}/src/pin_mux.c
    ${S}boards/${BOARD}/src/board_i2c.c
    ${S}boards/${BOARD}/src/utick_config.c
    ${S}boards/${BOARD}/drivers/fsl_video_common.c
    ${S}boards/${BOARD}/drivers/fsl_sccb.c
    ${S}boards/${BOARD}/drivers/fsl_ov7670.c
    ${S}boards/${BOARD}/drivers/fsl_lpi2c.c
    ${S}boards/${BOARD}/drivers/fsl_inputmux.c
    ${S}boards/${BOARD}/drivers/fsl_smartdma.c
    ${S}boards/${BOARD}/drivers/fsl_smartdma_mcxn.c
)

