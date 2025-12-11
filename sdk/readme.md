## Steps to prepare SDK next build env
	
### 1. Make sure you have git installed (if not check https://git-scm.com/)

### 2. Install python, version >= 3.10 --> https://wiki.python.org/moin/BeginnersGuide/Download

### 3. Install West using below command (version >= 1.2.0)

`$ pip install -U west` 

### 4. Install cmake, version >= 3.30.0

If you have an older version, remove it with below command 

`$ sudo apt purge cmake`

Then follow the instructions to install the newer version: 

#### a. go to https://cmake.org/download/
#### b. Download the tar.gz file for the latest version (or the version you want) - for example cmake-3.31.5.tar.gz - https://github.com/Kitware/CMake/releases/download/v3.31.5/cmake-3.31.5.tar.gz
#### c. Extract cmake

`$ run sudo tar -xzvf cmake-3.31.5.tar.gz`

#### d. Go to extracted folder

`$ cd cmake-3.31.5`

#### e. Run bootstrap script

`$ sudo ./bootstrap`

In case you get errors with OpenSSL as below

```
CMake Error at Utilities/cmcurl/CMakeLists.txt:772 (message):
  Could not find OpenSSL.  Install an OpenSSL development package or
  configure CMake with -DCMAKE_USE_OPENSSL=OFF to build without OpenSSL
```

run command 

`$ sudo apt-get install libssl-dev` 

then re-run the command

`$ sudo ./bootstrap`

#### f. Build cmake

`$ sudo make`

#### g. Install cmake

`$ sudo make install`

### 4.1 Alternative method to install cmake (execute below commands)
```
$ wget https://github.com/Kitware/CMake/releases/download/v3.31.5/cmake-3.31.5-linux-x86_64.sh
$ chmod +x cmake-3.31.5-linux-x86_64.sh
$ ./cmake-3.31.5-linux-x86_64.sh --include-subdir --skip-license
$ sudo ln -s "$PWD/cmake-3.31.5-linux-x86_64/bin/cmake" /usr/bin/cmake
```

### 5. Install ninja, version >= 1.12.1 (worked with 1.10.1 as well)

`$ sudo apt-get install ninja-build`

### 6. Install ARMGCC toolchain

#### a. Download the archive from https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz
#### b. Extract the toolchain

`$ sudo tar -xvf arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz`

#### c. open ~/.bashrc using vi and add the following line:

`export ARMGCC_DIR=/<path to extract location>/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi`

### 7. Install doxygen

`$ sudo apt-get install --no-install-recommends doxygen graphviz librsvg2-bin texlive-latex-base texlive-latex-extra latexmk texlive-fonts-recommended imagemagick`

### 8. Fetch the sdk repo by running the below commands:

The script install_sdk.py will also create symbolink links inside sdk folder to the respective folders in mpp repository
`$ cd mpp`

`$ python3 ./sdk/install_sdk.py` check the usage with python3 ./sdk/install_sdk.py -h

### 9. Install Python Dependency (optional)
```
$ cd mcuxsdk
$ sudo apt-get install python3.10-venv
$ python -m venv .venv
$ source .venv/bin/activate
$ pip install -r scripts/requirements.txt
$ deactivate
```

### 10. Check the build using the eiq examples from sdk

`$ west build -p always examples/eiq_examples/mpp/camera_view --toolchain armgcc --config flexspi_nor_sdram_debug -b evkbmimxrt1170 -Dcore_id=cm7`

### 11. Check the build using the mpp build script:
```
$ cd mpp
$ python3 ./build_mpp.py
```
