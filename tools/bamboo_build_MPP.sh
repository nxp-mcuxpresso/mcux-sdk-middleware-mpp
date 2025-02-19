#!/bin/bash -ex

# This script is intended to be used by bamboo jobs
# to automate MPP builds

# set environment
echo "Set environment"
export ARMGCC_DIR=/opt/toolchains/${bamboo_ARMGCC_DIR}
TOPDIR=$(pwd)
SDK_DIR=${TOPDIR}/sdk-next/mcuxsdk/

if [[ "${BOARD}" == "" ]]; then
    BOARD=${bamboo_BOARD}
fi
if [[ "${DISPLAY}" == "" ]]; then
    DISPLAY=${bamboo_DISPLAY}
fi
EXP=${bamboo_EXAMPLE}
echo "board= ${BOARD}"
echo "display= ${DISPLAY}"

case "${BOARD}" in
    frdmmcxn947)
        LIB_DIR="release"
        CORE_ID="cm33_core0"
        ;;
    mimxrt700evk)
        LIB_DIR="release"
        CORE_ID="cm33_core0"
        ;;
    evkbmimxrt1170)
        LIB_DIR="flexspi_nor_sdram_release"
        CORE_ID="cm7"
	;;
    *)
        echo "Fail sdk board name"
        exit 1
esac

# build MPP
cd "$TOPDIR"/mpp
rm -rf ${SDK_DIR}/build_${BOARD}
BUILD_OUTPUT="${SDK_DIR}/build_${BOARD}_output"
mkdir -p ${BUILD_OUTPUT}

# build all tests and examples with default configuration
./build_mpp.sh -e all -b "$BOARD" -p "$DISPLAY"
./build_mpp.sh -t all -b "$BOARD" -p "$DISPLAY"
for app in `ls ${SDK_DIR}/build_${BOARD}/release/*.bin`; do
    app=$(basename ${app} .bin)
    mv ${SDK_DIR}/build_${BOARD}/release/${app}.bin ${BUILD_OUTPUT}/
    mv ${SDK_DIR}/build_${BOARD}/release/${app}.elf ${BUILD_OUTPUT}/
done

build_app () {
    local app="$1"
    local configs="$2"
    local index="$3"
    local option
    if [[ "$4" == "tests" ]]; then option="-t"; else option="-e"; fi
    ./build_mpp.sh "${option}" "${app}" -b "$BOARD" -p "$DISPLAY" -f "${configs}"
    mv ${SDK_DIR}/build_${BOARD}/release/${app}_${CORE_ID}.bin ${BUILD_OUTPUT}/${app}_${CORE_ID}_config${index}.bin
    mv ${SDK_DIR}/build_${BOARD}/release/${app}_${CORE_ID}.elf ${BUILD_OUTPUT}/${app}_${CORE_ID}_config${index}.elf
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
