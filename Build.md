## Build for Linux PC emulator

This project supports running on a Linux PC emulator.
It has been proven to run on:
- Ubuntu 18.04
- Ubuntu 21.10
- Debian 11

Its only dependency is the OpenCV library.

### 1. Installing dependencies

`$ sudo apt install libopencv-dev`

### 2. Build FreeRTOS for Linux PC/Ubuntu

FreeRTOS is also a dependency to the emulator. But its source code are available.

`$ cd boards/linux-pc/FreeRTOS/Demo/Posix_GCC`

`$ make libfreertos.a`

or

`$ make -j x libfreertos.a` for parallel build (where x is your computer's number of CPUs/cores)

### 3. Build project for Linux PC
`$ cd boards/linux-pc/camera_emulator`

`$ cmake ..`

`$ make`

or

`$ make -j x` for parallel build  (where x is your computer's number of CPUs/cores)

Some examples applications should be available.
To verify that build went fine, run the most basic example app.

`$ ./camera_view rgb_sim rgb`

This should display the video in an openCV window.

## Build for evkmimxrt1170 arm target

### 1. Install gcc toolchain

Toolchain version to use: 10.3-2021.10
- Download the package here:
https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
- Unzip the package in your folder of choice
- Then setup the following environment variable:
$ export ARMGCC_DIR=/path/to/armgcc/toolchain/gcc-arm-none-eabi-10.3-2021.10/

### 2. Install provided SDK

`$ ./build_mpp.sh -s`

### 3. Build example

default values:
- example:    "camera_view"
- board:      "evkmimxrt1170"
- log level:  0
- panel:      0
- build type: release

`$ ./build_mpp.sh`
or:
`$ ./build_mpp.sh -b board_name -e example_name`

### Build script usage

`$ ./build_mpp.sh -h`
```
./build_mpp.sh [-e:ih?vsb:d:Dp:]
 -h|?: help
 -s: setup sdk
 -b <board name>: {evkmimxrt1170, ...}
 -D: build api documentation
 -e <example name>: build the example app {camera_view, all, ...}
 -i: build for host (x86)
 -d: <log level> as follow:
    # LOG_LVL_ERR       0 (default)
    # LOG_LVL_INFO      1
    # LOG_LVL_DEBUG     2
 -p: <panel index> as follow:
#define DEMO_PANEL_RK055AHD091 0 /* 720 * 1280 */
#define DEMO_PANEL_RK055IQH091 1 /* 540 * 960  */
#define DEMO_PANEL_RK055MHD091 2 /* 720 * 1280 */
 -t  <test name>: build the test app {test_image_display, all, ...}
 -c: build/config type <build_type>: {debug, release}
 -f: extra build flags: as follow {"-DFLAG1=1 -DFLAG2=1 -DFLAG3"}
 -v: enable verbose for build
```
