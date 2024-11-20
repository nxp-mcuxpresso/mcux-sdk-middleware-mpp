#!/bin/bash -ex

# This script is intended to be used by bamboo jobs
# to automate MPP builds

# set environment
echo "Set environment"
export ARMGCC_DIR=/opt/toolchains/${bamboo_ARMGCC_DIR}
TOPDIR=$(pwd)

if [[ "${BOARD}" == "" ]]; then
    BOARD=${bamboo_BOARD}
fi
if [[ "${DISPLAY}" == "" ]]; then
    DISPLAY=${bamboo_DISPLAY}
fi
if [[ "${SDK_VERSION_BAMBOO}" == "ENABLED" ]] && [[ "${bamboo_SDK_VERSION}" != "" ]]; then
    # use customized SDK version as set by bamboo variable
    SDK_VERSION=${bamboo_SDK_VERSION}
    export SDK_VERSION=${SDK_VERSION}
else
    # extract SDK_VERSION as set from build_mpp.sh
    SDK_VERSION=$(grep "SDK_VERSION=" ${TOPDIR}/mpp/build_mpp.sh | awk -F"=" '{print $NF}')
fi
EXP=${bamboo_EXAMPLE}
echo "board= ${BOARD}"
echo "display= ${DISPLAY}"
echo "SDK_version= ${SDK_VERSION}"

case "${BOARD}" in
    evkmimxrt1170)
        SDK_ARCHIVE="mpp-sdk/MIMXRT1170-EVK/armgcc/SDK_${SDK_VERSION}_MIMXRT1170-EVK.tbz"
        LIB_DIR="flexspi_nor_sdram_release"
        ;;
    evkbimxrt1050)
        SDK_ARCHIVE="mpp-sdk/EVKB-IMXRT1050/armgcc/SDK_${SDK_VERSION}_EVKB-IMXRT1050.tbz"
        LIB_DIR="flexspi_nor_sdram_release"
        ;;
    mcxn9xxevk)
        SDK_ARCHIVE="mpp-sdk/MCX-N9XX-EVK/armgcc/SDK_${SDK_VERSION}_MCX-N9XX-EVK.tbz"
        LIB_DIR="release"
        ;;
    mcxn9xxbrk)
        SDK_ARCHIVE="mpp-sdk/MCX-N9XX-BRK/armgcc/SDK_${SDK_VERSION}_MCX-N9XX-BRK.tbz"
        LIB_DIR="release"
        ;;
    frdmmcxn947)
        SDK_ARCHIVE="mpp-sdk/FRDM-MCXN947/armgcc/SDK_${SDK_VERSION}_FRDM-MCXN947.tbz"
        LIB_DIR="release"
        ;;
    mimxrt700evk)
        SDK_ARCHIVE="mpp-sdk/MIMXRT700-EVK/armgcc/SDK_${SDK_VERSION}_MIMXRT700-EVK.tbz"
        LIB_DIR="release"
        ;;
    evkbmimxrt1170)
        SDK_ARCHIVE="mpp-sdk/MIMXRT1170-EVKB/armgcc/SDK_${SDK_VERSION}_MIMXRT1170-EVKB.tbz"
        LIB_DIR="flexspi_nor_sdram_release"
	;;
    *)
        echo "Fail sdk board name"
        exit 1
esac

# install SDK
tar -xf ${SDK_ARCHIVE}

# build MPP
cd "$TOPDIR"/mpp
rm -rf build_${BOARD}
BUILD_OUTPUT="build_${BOARD}_output"
mkdir -p ${BUILD_OUTPUT}

# build all tests and examples with default configuration
./build_mpp.sh -e all -b "$BOARD" -p "$DISPLAY"
./build_mpp.sh -t all -b "$BOARD" -p "$DISPLAY"
for app in `ls build_${BOARD}/apps/*.bin`; do
    app=$(basename ${app} .bin)
    mv build_${BOARD}/apps/${app}.bin ${BUILD_OUTPUT}/
    mv build_${BOARD}/apps/${app} ${BUILD_OUTPUT}/
done

# Library and MPP version
cp build_${BOARD}/apps/lib/${LIB_DIR}/* ${BUILD_OUTPUT}/

build_app () {
    local app="$1"
    local configs="$2"
    local index="$3"
    local option
    if [[ "$4" == "tests" ]]; then option="-t"; else option="-e"; fi
    ./build_mpp.sh "${option}" "${app}" -b "$BOARD" -p "$DISPLAY" -f "${configs}"
    mv build_${BOARD}/apps/${app}.bin ${BUILD_OUTPUT}/${app}_config${index}.bin
    mv build_${BOARD}/apps/${app}     ${BUILD_OUTPUT}/${app}_config${index}
}

build_all_configs () {
    # type of application: "tests" or "examples"
    local type_app="$1"
    if [[ "${type_app}" != "tests" ]] && [[ "${type_app}" != "examples" ]]; then
        echo "Type applicaton ${type_app} is not valid."
        exit 1
    fi

    for app in `cat boards/${BOARD}/${type_app}.conf`; do
        if [ -f "tools/mpp_parse_configs.sh" ] && [ -f "boards/${BOARD}/${type_app}/${app}/${app}.conf" ]; then
            index=1
            while : ; do
                configs=$(/bin/bash tools/mpp_parse_configs.sh boards/${BOARD}/${type_app}/${app}/${app}.conf ${index})
                if [[ "${configs}" == "" ]]; then break ; fi
                build_app "${app}" "${configs}" "${index}" "${type_app}"
                ((index++))
            done
        fi
    done
}

build_all_configs "examples"
build_all_configs "tests"

ARTIFACTS_DIR="/efs/shared/artifacts/${bamboo_planKey}/${bamboo_buildNumber}"

# Store binaries into /efs to share with Dapeng testing
tar zcf ${ARTIFACTS_DIR}/build_${BOARD}.tar.gz ${BUILD_OUTPUT}/*
