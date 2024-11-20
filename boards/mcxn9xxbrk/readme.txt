Files origin from mcx-n10 sdk v2.14.0:

├── config.cmake
│   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/cm33_core0/armgcc/config.cmake
|      and freertos-kernel modules */
├── flags.cmake
│   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/cm33_core0/armgcc/flags.cmake concatenated with */
|   /* boards/mcxn9xxbrk/rtos_examples/freertos_hello/cm33_core0/armgcc/flags.cmake */
├── inc
│   ├── board.h
│   │   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/board.h, modified to use uart6 */
│   ├── board_config.h
│   │   /* MPP specific file */
│   ├── board_init.h
│   │   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/board_init.h, not modified */
│   ├── clock_config.h
│   │   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/clock_config.h, not modified */
│   ├── dcd.h
│   │   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/dcd.h, not modified */
│   ├── FreeRTOSConfig.h
│   │   /* boards/mcxn9xxbrk/rtos_examples/freertos_hello/FreeRTOSConfig.h and modified to fit mpp demo - configTICK_RATE_HZ is changed to 1000, configurable heap size */
│   └── pin_mux.h
│   |    /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/pin_mux.h, modified for camera pins and to add flexio LCD pins config from  
|   |       boards/mcxn9xxbrk/driver_examples/flexio/mculcd/edma_transfer/cm33_core0/pin_mux.h */
│   └── board_i2c.h
│       /* new i2c wrapper file for SCCB */
├─ src
│   ├── board.c
│   │   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/board.c, not modified */
│   ├── board_init.c
│   │   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/board_init.c, modified to use uart6 */
│   ├── clock_config.c
│   │   /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/clock_config.c, nmodified to set CLKOUT to 10MHz for camera */
│   └── pin_mux.c
│   |    /* boards/mcxn9xxbrk/eiq_examples/tflm_label_image/pin_mux.c, modified for camera pins and to add flexio LCD pins config from
|   |       boards/mcxn9xxbrk/driver_examples/flexio/mculcd/edma_transfer/cm33_core0/pin_mux.c */
│   └── board_i2c.c
│       /* new i2c wrapper file for SCCB */
├── MCXN947_cm33_core0_flash.ld
│   /* devices/MCXN947/gcc/MCXN947_cm33_core0_flash.ld, modified
       to remove core1 memory section and rpmsg-shared-mem, in order to enlarge core0 sram */
└── sdk_modules.cmake
    /* created to fit mpp demo requirements based on boards/mcxn9xxbrk/eiq_examples/tflm_label_image/cm33_core0/armgcc/CMakeLists.txt */
└── drivers
    │   /* cherry-picked from branch develop/sdk_api_2.x in folder components/video/ */
    ├── fsl_video_common.c
    ├── fsl_video_common.h
    ├── fsl_camera_device.h
    ├── fsl_camera.h
    │   /* cherry-picked from branch develop/sdk_api_2.x in folder components/video/camera/device/sccb and modified */
    ├── fsl_sccb.c
    ├── fsl_sccb.h
    │   /* from /devices/MCXN947/drivers not modified */
    ├── fsl_inputmux_connections.h
    │   /* from /platform/drivers/inputmux/ not modified */
    ├── fsl_inputmux.c
    ├── fsl_inputmux.h
    │   /* from /platform/drivers/lpflexcomm/lpi2c/ not modified */
    ├── fsl_lpi2c.c
    ├── fsl_lpi2c.h
    │   /* cherry-picked from branch develop/sdk_api_2.x in folder components/video/ and modified */
    ├── fsl_ov7670.c
    ├── fsl_ov7670.h
    │   /* provided by SE team, see https://bitbucket.sw.nxp.com/projects/VTEC/repos/mcxn9xxbrk_flexio_lcd_smartdma_camera/pull-requests/1/overview */
    ├── fsl_smartdma.c
    └── fsl_smartdma.h
