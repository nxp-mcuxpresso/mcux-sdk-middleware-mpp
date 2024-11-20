Files origin from mcx-n10 sdk v2.14.0:

├── config.cmake
│   /* boards/mcxn9xxbevk/eiq_examples/tflm_label_image/cm33_core0/armgcc/config.cmake
|      and freertos-kernel modules */
├── flags.cmake
│   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/cm33_core0/armgcc/flags.cmake modified */
|   /* added FreeRTOS, removed stack size override */
├── inc
│   ├── board.h
│   │   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/board.h, not modified */
│   ├── board_init.h
│   │   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/board_init.h, not modified */
│   ├── clock_config.h
│   │   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/clock_config.h, not modified */
│   ├── dcd.h
│   │   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/dcd.h, not modified */
│   ├── FreeRTOSConfig.h
│   |   /* boards/mcxn9xxevk/rtos_examples/freertos_hello/FreeRTOSConfig.h and modified to fit mpp demo - configTICK_RATE_HZ is changed to 1000,
|          and added new RTOS_HEAP_SIZE flag that should be defined depending on the application(62K by default). */
│   |── pin_mux.h
│   │   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/pin_mux.h, modified to add spi pins config from 
|   |      boards/mcxn9xxevk/lvgl_examples/lvgl_demo_widgets_bm/cm33_core0/pin_mux.h */
│   └── RTE_Device.h
│       /* boards/mcxn9xxevk/lvgl_examples//RTE_Device.h, modified to remove I2C configuration*/
├─ src
│   ├── board.c
│   │   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/board.c, not modified */
│   ├── board_init.c
│   │   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/source/glow_cifar10_camera.c, not modified */
│   ├── clock_config.c
│   │   /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/clock_config.c, not modified */
│   └── pin_mux.c
│       /* boards/mcxn9xxevk/eiq_examples/tflm_label_image/pin_mux.c, modified to add spi pins config from 
|          boards/mcxn9xxevk/lvgl_examples/lvgl_demo_widgets_bm/cm33_core0/pin_mux.h */
├── MCXN947_cm33_core0_flash.ld
│   /* devices/MIMXRT1052/gcc/MCXN947_cm33_core0_flash.ld, modified
|      to remove core1 memory section and rpmsg-shared-mem, in order to enlarge core0 sram. Increase TEXT_SIZE in qspi flash */
└── sdk_modules.cmake
    /* created to fit mpp demo requirements based on boards/mcxn9xxevk/eiq_examples/tflm_label_image/cm33_core0/armgcc/CMakeLists.txt */
