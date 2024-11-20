# Hardware Abstraction Layer

This is the documentation for the Hardware Abstraction Layer(HAL) API.

# HAL overview

The hardware abstraction layer is used to abstract hardware and software components.
With the usage of an HAL abstraction, the vision pipeline will be leveraging hardware accelerated components whenever possible.

## MPP hal description:

The HAL is presented with respect of the following points:

- A common header file "hal.h" includes all hardware top level functions.
- All hardware top level functions are using the prefix: "hal_ "
- For each platform all hal_ functions defined in hal.h should be implemented at least with an empty function.
 
Here is an overview:

\image latex mpp_hal_overview.png "HAL overview"
\image html mpp_hal_overview.png "HAL overview"

## MPP HAL components:

### Source elements HAL

- Camera
- Static image

### processing elements HAL

- Graphics driver
- Vision algorithms
- Labeled rectangle

### Sink elements HAL

- Display

## Supported devices:
At present, the MPP HAL supports the following devices:

- Cameras:
   * OV5640
   * MT9M114
   * OV7670
- Displays:
   * RK055AHD091
   * RK055MHD091
   * RK043FN02H-CT
   * Mikroe TFT Proto 5(SSD1963 controller)
   * NXP's LCD-PAR-S035 (ST7796S controller)
- Graphics:
   * PXP
   * CPU
   * GPU
## Supported boards:

Currently, the MPP HAL supports the following boards:

- evkmimxrt1170: 
The evkmimxrt1170 is supported with the following devices:
    * Cameras:  OV5640
    * Displays: RK055AHD091 and RK055MHD091.

- evkbimxrt1050:
The evkbimxrt1050 is supported with the following devices:
    * Cameras:  MT9M114
    * Displays: RK043FN02H-CT.

- evkbmimxrt1170
The evkbmimxrt1170 is supported by porting the following devices:
    * Cameras:  OV5640
    * Displays: RK055AHD091 and RK055MHD091.
    
- frdmmcxn947
The frdmmcxn947 is supported by porting the following devices:
    * Cameras:  OV7670
    * Displays: Mikroe TFT Proto 5" and NXP's LCD-PAR-S035.

- mimxrt700evk
The mimxrt700evk is supported by porting the following devices:
    * Cameras:  
    * Displays: RK055AHD091 and RK055MHD091.    
# How to port new boards/devices:
The MPP Hal provides the flexibility to the user to port new boards and devices(cameras and displays).
## Supporting new boards:
To support a new board a new file hal_{board_name} should be added under hal directory.
## Supporting new devices:
The hal components that can support new devices are:
- Cameras
- Display 
- Graphics processing

A new device can simply be supported by:
- Providing the approriate hal_{device_module} implementation.
- Adding his name and setup entry point to the appropriate device list in the associated board hal_{board_name} file.
## Enabling/Disabling Hal components and devices:
- The HAL components can be enabled/disabled from "mpp_config.h" using the compilation flags(HAL_ENABLE_{component_name}).
- The HAL devices can also be enabled/disabled from "mpp_config.h" using the compilation flags(HAL_ENABLE_{device_name}).

