#!/bin/bash -ex

# This script is intended to be used by bamboo jobs
# to automate MPP builds

# Download and install toolchain
ARMGCC_ARCHIVE="arm-gnu-toolchain-${bamboo_ARMGCC_VERSION}-x86_64-arm-none-eabi.tar.xz"
wget https://developer.arm.com/-/media/Files/downloads/gnu/${bamboo_ARMGCC_VERSION}/binrel/${ARMGCC_ARCHIVE}
sudo mkdir -p /opt/toolchains/
sudo tar -xf ${ARMGCC_ARCHIVE} --directory /opt/toolchains/

# Install cmake
CMAKE_SCRIPT="cmake-${bamboo_CMAKE_VERSION}-linux-x86_64.sh"
wget https://github.com/Kitware/CMake/releases/download/v${bamboo_CMAKE_VERSION}/${CMAKE_SCRIPT}
chmod +x ${CMAKE_SCRIPT}
./${CMAKE_SCRIPT} --include-subdir --skip-license
sudo ln -s "$PWD/cmake-${bamboo_CMAKE_VERSION}-linux-x86_64/bin/cmake" /usr/bin/cmake
cmake --version

# Install west
python3.10 --version
pip3 install -U "west>=${bamboo_WEST_VERSION}"
west --version

# Install ninja
# Remove the already installed version and install newer one
sudo apt-get purge -y ninja-build
wget https://github.com/ninja-build/ninja/releases/download/v${bamboo_NINJA_VERSION}/ninja-linux.zip
sudo unzip ninja-linux.zip -d /opt/toolchains/
sudo ln -s /opt/toolchains/ninja /usr/bin/ninja
ninja --version

# Git config
git config --global url.ssh://git@bitbucket.sw.nxp.com/mcucore/bifrost.git.insteadOf https://github.com/nxp-zephyr/bifrost

# Install SSH keys
# Download overlay
curl "${bamboo_OVERLAY_SOURCE}" --output overlay.zip
# Unpack overlay at root directory
unzip -o -P "${bamboo_OVERLAY_SECRET}" overlay.zip -d /