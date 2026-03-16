## Build for evkmimxrt1170 arm target

### 1. Install gcc toolchain

Toolchain version to use: 14.2-rel1-x86_64-arm-none-eabi
- Download the package here:
https://developer.arm.com/downloads/-/gnu-rm/
- Unzip the package in your folder of choice
- Then setup the following environment variable:
$ export ARMGCC_DIR=/home/b47544/toolchain/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi

### 2. Install provided SDK

`$ python3 sdk/install_sdk.py -v <version> -s <sdk-dirname>`

### 3. Build example

default values:
- example:    "camera_view"
- board:      "evkmimxrt1170"
- log level:  0
- panel:      0
- build type: release

`$ python3 ./build_mpp.py`

or:

`$ python3 ./build_mpp.py -b board_name -e example_name`

### Build script usage

`$ python3 ./build_mpp.py -h`
```
usage: build_mpp.py [-h] [-a] [-b {evkbmimxrt1170,frdmmcxn947,mimxrt700evk,all}] [-e EXAMPLE] [-i] [-d LOG_LEVEL] [-D] [-p PANEL] [-c CONFIG] [-f FLAGS] [-t TEST] [-s SDK_PATH] [-g APP_CONFIG] [-C CORE_ID]
                    [-S] [-v] [-F] [-P PROBE_ID] [-G] [-J JLINKSCRIPT]

MPP Build Script

options:
  -h, --help            show this help message and exit
  -a, --rebuild-tflm    rebuild libtflm.a from source
  -b {evkbmimxrt1170,frdmmcxn947,mimxrt700evk,all}, --board {evkbmimxrt1170,frdmmcxn947,mimxrt700evk,all}
                        board name: {evkbmimxrt1170, frdmmcxn947, mimxrt700evk, all}
  -e EXAMPLE, --example EXAMPLE
                        build the example app {camera_view, all, ...}
  -i, --host            build for host (x86)
  -d LOG_LEVEL, --log-level LOG_LEVEL
                        log level (use the respective number from below list):
                        LOG_LVL_ERR       0 (default)
                        LOG_LVL_INFO      1
                        LOG_LVL_DEBUG     2
  -D, --doc             build mpp and hal APIs documentation
  -p PANEL, --panel PANEL
                        panel index as follows (use the respective number from below list):

                        Panel list supported for board evkbmimxrt1170
                        #define DEMO_PANEL_RK055AHD091 0 /* 720 * 1280, RK055AHD091-CTG(RK055HDMIPI4M) */
                        #define DEMO_PANEL_RK055IQH091 1 /* 540 * 960,  RK055IQH091-CTG */
                        #define DEMO_PANEL_RK055MHD091 2 /* 720 * 1280, RK055MHD091A0-CTG(RK055HDMIPI4MA0) */
                        #define DEMO_PANEL_RASPI_7INCH 5 /* 800 * 480, Raspberry Pi 7" */


                        Panel list supported for board mimxrt700evk
                        #define DEMO_PANEL_TFT_PROTO_5 4 /* MikroE TFT Proto 5" CAPACITIVE FlexIO/LCD DBI Display */
                        #define DEMO_PANEL_RK055AHD091 0 /* NXP "RK055HDMIPI4M" MIPI Rectangular Display */
                        #define DEMO_PANEL_RK055IQH091 1 /* NXP RESERVED */
                        #define DEMO_PANEL_RM67162     3 /* NXP "G1120B0MIPI" MIPI Circular Display */
                        #define DEMO_PANEL_RK055MHD091 2 /* NXP "RK055MHD091A0-CTG MIPI Rectangular Display */
                        #define DEMO_PANEL_RASPI_7INCH 5 /* Raspberry Pi panel 7 inch */
                        #define DEMO_PANEL_CO5300      6 /* NXP ZC143AC72MIPI MIPI Circular Display */
                        #define DEMO_PANEL_LCD_PAR_S035 8 /* LCD_PAR_S035 panel 8080.*/
  -c CONFIG, --config CONFIG
                        build/config type: {debug, release}
  -f FLAGS, --flags FLAGS
                        extra build flags
  -t TEST, --test TEST  build the test app {test_image_display, all, ...}
  -s SDK_PATH, --sdk-path SDK_PATH
                        specify the path to the mcuxsdk folder
  -g APP_CONFIG, --app-config APP_CONFIG
                        the index of the app_config to be used
  -C CORE_ID, --core-id CORE_ID
                        specify the core id you want to build app for (values like 0, 1, 2...)
  -S, --sysbuild        add --sysbuild option to the build command
  -v, --verbose         enable verbose for build
  -F, --flash           flash the built image to the board after building
  -P PROBE_ID, --probe-id PROBE_ID
                        probe ID for flashing (auto-detected if not provided)
  -G, --use-gdb         use GDB server for flashing (only supported with jlink)
  -J JLINKSCRIPT, --jlinkscript JLINKSCRIPT
                        path to JLink script file for GDB server (valid only when use_gdb is set)
```

In order to have the argument autocomplete function working (compatible with Linux bash only), you need to install the argcomplete package:

`$ pip install argcomplete`

Then activate it by running:

`$ activate-global-python-argcomplete3 --user`

And add the following line to your shell configuration file (~/.bashrc, ~/.zshrc, etc.):

`$ eval "$(register-python-argcomplete ./build_mpp.py)"`

or

`$ eval "$(register-python-argcomplete ./build_mpp.py)" >> ~/.bashrc`

After this you need to call the script directly without python3:
```
$ chmod +x ./build_mpp.py
$ ./build_mpp.py -h
```
