Files origin from sdk v2.16.0:

├── config.cmake
│   /* boards/evkbimxrt1050/rtos_examples/freertos_hello/armgcc/config.cmake combined with boards/evkmimxrt1050/eiq_examples/
|   tflm_label_image/armgcc/config.cmake */
├── flags.cmake
|   /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/armgcc/flags.cmake */
├── inc
│   ├── board.h
│   │   /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/board.h, not modified */
│   ├── board_init.h
│   │   /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/board_init.h, not modified */
│   ├── camera_support.h
│   │   /* boards/evkbimxrt1050/driver_examples/csi/rgb565/camera_support.h, not modified */
│   ├── clock_config.h
│   │   /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/clock_config.h, not modified */
│   ├── dcd.h
│   │   /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/dcd.h, not modified */
│   ├── display_support.h
│   │   /* boards/evkbimxrt1050/driver_examples/csi/rgb565/display_support.h, not modified */
│   ├── FreeRTOSConfig.h
│   │   /* boards/evkbimxrt1050/freertos_examples/freertos_hello/FreeRTOSConfig.h and modified to fit mpp demo - configTICK_RATE_HZ is changed to 1000 */
│   └── pin_mux.h
│       /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/pin_mux.h, not modified */
├─ src
│   ├── board.c
│   │   /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/board.c, not modified */
│   ├── board_init.c
│   │   /* boards/evkbimxrt1050/eiq_examples/glow_cifar10_camera/source/glow_cifar10_camera.c, not modified */
│   ├── camera_support.c
│   │   /* boards/evkbimxrt1050/driver_examples/csi/rgb565/camera_support.c, not modified */
│   ├── clock_config.c
│   │   /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/clock_config.c, not modified */
│   ├── dcd.c
│   │   /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/dcd.c, not modified */
│   ├── display_support.c
│   │   /* boards/evkbimxrt1050/driver_examples/csi/rgb565/display_support.c, not modified */
│   └── pin_mux.c
│       /* boards/evkbimxrt1050/eiq_examples/tflm_label_image/pin_mux.c, not modified */
├── MIMXRT1052xxxxx_flexspi_nor_sdram.ld
│   /* devices/MIMXRT1052/gcc/MIMXRT1052xxxxx_flexspi_nor_sdram.ld, modified heap and stack size.
└── 
