Files origin from sdk v2.15.0:

├── config.cmake
│   /* boards/evkbmimxrt1170/rtos_examples/freertos_hello/cm7/armgcc/config.cmake combined with boards/evkbmimxrt1170/eiq_examples/
|   tflm_label_image/armgcc/config.cmake and modified to add deepviewrt module */
├── flags.cmake
│   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/armgcc/flags.cmake, removed XMCD, enabled DCD */
├── inc
│   ├── board.h
│   │   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/board.h, not modified */
│   ├── board_init.h
│   │   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/board_init.h, not modified */
│   ├── camera_support.h
│   │   /* boards/evkbmimxrt1170/driver_examples/csi/mipi_yuv/cm7/camera_support.h, not modified */
│   ├── clock_config.h
│   │   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/clock_config.h, not modified */
│   ├── dcd.h
│   │   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/dcd.h, not modified */
│   ├── display_support.h
│   │   /* boards/evkbmimxrt1170/display_examples/mipi_dsi_compliant_test/cm7/display_support.h, not modified */
│   ├── FreeRTOSConfig.h
│   │   /* boards/evkbmimxrt1170/freertos_examples/freertos_hello/cm7/FreeRTOSConfig.h and modified to fit mpp demo - configTICK_RATE_HZ is changed to 1000 */
│   ├── pin_mux.h
│   │   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/pin_mux.h, not modified */
│   └── vglite_support.h
│       /* boards/evkbmimxrt1170/vglite_examples/vglite_support.h, not modified */
├─ src
│   ├── board.c
│   │   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/board.c, not modified */
│   ├── board_init.c
│   │   /* created to redefine BOARD_Init() for mpp demo */
│   ├── camera_support.c
│   │   /* boards/evkbmimxrt1170/driver_examples/csi/mipi_yuv/cm7/camera_support.c, not modified */
│   ├── clock_config.c
│   │   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/clock_config.c, not modified */
│   ├── dcd.c
│   │   /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/dcd.c, not modified */
│   ├── display_support.c
│   │   /* boards/evkbmimxrt1170/display_examples/mipi_dsi_compliant_test/cm7/display_support.c, not modified */
│   ├── pin_mux.c
│   │    /* boards/evkbmimxrt1170/eiq_examples/tflm_label_image/pin_mux.c, not modified */
│   └── vglite_support.c
│       /* boards/evkbmimxrt1170/vglite_examples/vglite_support.c, not modified */
├── MIMXRT1176xxxxx_cm7_flexspi_nor_sdram.ld
│   /* boards/evkbmimxrt1170/devices/MIMXRT1176/gcc/MIMXRT1176xxxxx_cm7_flexspi_nor_sdram.ld, modified
|       This file has been modified to support deepviewRT library(a new section .got has been added to m_data region */
└── 
