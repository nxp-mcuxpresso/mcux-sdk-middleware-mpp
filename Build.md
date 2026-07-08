# Building MPP Examples

## Prerequisites

### 1. Install GCC ARM toolchain

Recommended toolchain version: **14.2-rel1** (`arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi`)

- Download from: https://developer.arm.com/downloads/-/gnu-rm/
- Extract the archive and set the environment variable:

```bash
export ARMGCC_DIR=/path/to/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi
```

The build script automatically searches for the toolchain in common locations and in the `PATH`,
so setting `ARMGCC_DIR` is only required if auto-detection fails.

### 2. Install the SDK

```bash
python3 sdk/install_sdk.py
```

See `sdk/readme.md` for full dependency and SDK installation instructions.

### 3. Install Python dependencies

```bash
pip install argcomplete
```

## Building examples

Default build (board: evkbmimxrt1170, example: camera_view, config: release):

```bash
python3 ./build_mpp.py
```

Build a specific example on a specific board:

```bash
python3 ./build_mpp.py -b evkbmimxrt1170 -e camera_mobilenet_view
```

Build for all supported boards:

```bash
python3 ./build_mpp.py -b all -e camera_view
```

## Build script reference

```
python3 ./build_mpp.py -h
```

```
usage: build_mpp.py [-h] [-a]
                    [-b {evkbmimxrt1170,frdmmcxn947,mimxrt700evk,frdmimxrt1152,frdmimxrt700,all}]
                    [-e EXAMPLE] [-i] [-d LOG_LEVEL] [-D] [-p PANEL]
                    [-c CONFIG] [-f FLAGS] [-t TEST] [-s SDK_PATH]
                    [-g APP_CONFIG] [-C CORE_ID] [-S] [-v] [-F] [-P PROBE_ID]
                    [-G] [-J JLINKSCRIPT] [--debug_console {0,1,2}] [-V] [-I]

options:
  -h, --help                      show this help message and exit
  -a, --rebuild-tflm              rebuild libtflm.a from source
  -b BOARD, --board BOARD         board name: {evkbmimxrt1170, frdmmcxn947, mimxrt700evk,
                                  frdmimxrt1152, frdmimxrt700, all}
  -e EXAMPLE, --example EXAMPLE   build the example app {camera_view, all, ...}
  -d LOG_LEVEL, --log-level LOG_LEVEL
                                  log level:
                                    0  LOG_LVL_ERR (default)
                                    1  LOG_LVL_INFO
                                    2  LOG_LVL_DEBUG
  -D, --doc                       build MPP and HAL API documentation
  -p PANEL, --panel PANEL         display panel index (board-dependent, see -h output)
  -c CONFIG, --config CONFIG      build type: {debug, release} (default: release)
  -f FLAGS, --flags FLAGS         extra build flags: -f="-DFLAG1=VALUE1 -DFLAG2 ..."
  -t TEST, --test TEST            build the test app {test_image_display, all, ...}
  -s SDK_PATH, --sdk-path PATH    path to the mcuxsdk folder
  -g APP_CONFIG, --app-config N   index of the app_config to use
  -C CORE_ID, --core-id ID        core id to build for (0, 1, 2...)
  -S, --sysbuild                  add --sysbuild option (for multicore examples)
  -v, --verbose                   enable verbose build output
  -F, --flash                     flash the built image to the board after building
  -P PROBE_ID, --probe-id ID      probe ID for flashing (auto-detected if not provided)
  -G, --use-gdb                   use GDB server for flashing (jlink only)
  -J JLINKSCRIPT, --jlinkscript   path to JLink script file (with --use-gdb)
  --debug_console {0,1,2}         0: core0 console only (default)
                                  1: core1 console only
                                  2: both cores
  -V, --code-coverage             enable code coverage analysis
  -I, --ignore-conf-arguments     ignore extra build arguments from .conf file
```

## Generating API documentation

```bash
python3 ./build_mpp.py -D
```

This generates:
- `docs/mpp_api/mpp_api.md` - MPP public API documentation
- `docs/hal_api/hal_api.md` - HAL API documentation

Requires `doxygen` to be installed:

```bash
sudo apt-get install doxygen
```

## Tab completion (Linux bash)

Enable argument auto-completion with `argcomplete`:

```bash
pip install argcomplete
activate-global-python-argcomplete3 --user
```

Add to your shell configuration file (`~/.bashrc` or `~/.zshrc`):

```bash
eval "$(register-python-argcomplete ./build_mpp.py)"
```

Then make the script executable and run it directly:

```bash
chmod +x ./build_mpp.py
./build_mpp.py -h
```

## Multicore examples

For boards with dual-core support (mimxrt700evk, frdmimxrt700, evkbmimxrt1170), use the `--sysbuild` flag:

```bash
python3 ./build_mpp.py -b mimxrt700evk -e camera_usb_final_fr_app_view -S
```

To target core1 specifically:

```bash
python3 ./build_mpp.py -b mimxrt700evk -C 1 -e camera_view
```

## Debug console selection

By default only core0 console is enabled. Use `--debug_console` to change:

```bash
# Enable both core0 and core1 console
python3 ./build_mpp.py -b mimxrt700evk --debug_console 2 -e camera_view
```
