## Build for evkmimxrt1170 arm target

### 1. Install gcc toolchain

Toolchain version to use: 14.2-rel1-x86_64-arm-none-eabi
- Download the package here:
https://developer.arm.com/downloads/-/gnu-rm/
- Unzip the package in your folder of choice
- Then setup the following environment variable:
$ export ARMGCC_DIR=/home/b47544/toolchain/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi

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
Running script from sdk west repository
usage:
./build_mpp.sh [-ab:e:h?id:Dp:c:f:t:s:g:C:Sv]
 -h|?: help
 -b <board name>: {evkbmimxrt1170, frdmmcxn947, mimxrt700evk}
 -D: build mpp and hal APIs documentation
 -e <example name>: build the example app {camera_view, all, ...}
 -i: build for host (x86)
 -d: <log level> as follow:
        # LOG_LVL_ERR       0 (default)
        # LOG_LVL_INFO      1
        # LOG_LVL_DEBUG     2
 -p: <panel index> as follow:

Panel list supported for board evkbmimxrt1170
#define DEMO_PANEL_RK055AHD091 0 /* 720 * 1280, RK055AHD091-CTG(RK055HDMIPI4M) */
#define DEMO_PANEL_RK055IQH091 1 /* 540 * 960,  RK055IQH091-CTG */
#define DEMO_PANEL_RK055MHD091 2 /* 720 * 1280, RK055MHD091A0-CTG(RK055HDMIPI4MA0) */
#define DEMO_PANEL_RASPI_7INCH 5 /* 800 * 480, Raspberry Pi 7" */

 -t  <test name>: build the test app {test_image_display, all, ...}
 -c: build/config type <build_type>: {debug, release}
 -f: extra build flags: as follow {"-DFLAG1=1 -DFLAG2=1 -DFLAG3"}
 -a: rebuild libtflm.a from source
 -s <sdk_path>: specify the path to the mcuxsdk folder from sdk-next repo (DO NOT include mcuxsdk folder)
 -v: enable verbose for build
 -g <app_config_index> - the index of the app_config to be used for building the app
 -C <core_id> - specify the core id you want to build app for - if not set, default core (0) will be used
 -S: add --sysbuild option to the build command - useful for multicore builds
 ```
