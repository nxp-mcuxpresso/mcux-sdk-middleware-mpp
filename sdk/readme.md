# MPP SDK Environment Setup

This document describes how to set up the build environment for the MCU Media Processing Pipeline.

## Dependencies

### 1. Git

Make sure git is installed. If not, see https://git-scm.com/

### 2. Python >= 3.10

Install from https://wiki.python.org/moin/BeginnersGuide/Download

### 3. West >= 1.2.0

```bash
pip install -U west
```

### 4. CMake >= 3.30.0

If you have an older version, remove it first:

```bash
sudo apt purge cmake
```

**Option A: Build from source**

```bash
wget https://github.com/Kitware/CMake/releases/download/v3.31.5/cmake-3.31.5.tar.gz
tar -xzvf cmake-3.31.5.tar.gz
cd cmake-3.31.5
sudo apt-get install libssl-dev   # if OpenSSL errors occur
sudo ./bootstrap
sudo make
sudo make install
```

**Option B: Install pre-built binary (recommended)**

```bash
wget https://github.com/Kitware/CMake/releases/download/v3.31.5/cmake-3.31.5-linux-x86_64.sh
chmod +x cmake-3.31.5-linux-x86_64.sh
./cmake-3.31.5-linux-x86_64.sh --include-subdir --skip-license
sudo ln -s "$PWD/cmake-3.31.5-linux-x86_64/bin/cmake" /usr/bin/cmake
```

### 5. Ninja >= 1.12.1

```bash
sudo apt-get install ninja-build
```

### 6. ARM GCC toolchain

Recommended version: **14.2-rel1** (`arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi`)

```bash
# Download
wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz

# Extract
sudo tar -xvf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz

# Add to ~/.bashrc
export ARMGCC_DIR=/path/to/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi
```

The MPP build script also auto-detects the toolchain if it is in `PATH` or in common installation directories.

### 7. Doxygen (optional, for documentation generation)

```bash
sudo apt-get install --no-install-recommends doxygen
```

## Fetch and install the SDK

The `install_sdk.py` script fetches the NXP MCUXpresso SDK and creates symbolic links inside the SDK
folder pointing to MPP source directories.

```bash
cd mpp
python3 ./sdk/install_sdk.py
```

Use `-h` for all options:

```bash
python3 ./sdk/install_sdk.py -h
```

## Install Python virtual environment (optional)

```bash
cd mcuxsdk
sudo apt-get install python3.10-venv
python -m venv .venv
source .venv/bin/activate
pip install -r scripts/requirements.txt
deactivate
```

## Verify the build environment

Build a simple example to verify everything is set up correctly:

```bash
cd mcuxsdk
west build -p always examples/eiq_examples/mpp/camera_view \
    --toolchain armgcc \
    --config flexspi_nor_sdram_release \
    -b evkbmimxrt1170 \
    -Dcore_id=cm7
```

Or use the MPP build script from the mpp directory:

```bash
cd mpp
python3 ./build_mpp.py -b evkbmimxrt1170 -e camera_view
```
