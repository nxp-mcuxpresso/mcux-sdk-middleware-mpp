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
        CORE_ID="cm33_core0"
        ;;
    mimxrt700evk)
        CORE_ID="cm33_core0"
        ;;
    evkbmimxrt1170)
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

# build all tests and examples for all build configurations
for build_config in "release" "debug"; do
    mkdir -p ${BUILD_OUTPUT}/${build_config}
    ./build_mpp.sh -e all -b "$BOARD" -p "$DISPLAY" -c "$build_config"
    ./build_mpp.sh -t all -b "$BOARD" -p "$DISPLAY" -c "$build_config"
    for app in ${SDK_DIR}/build_${BOARD}/${build_config}/*.bin; do
        app=$(basename ${app} .bin)
        mv ${SDK_DIR}/build_${BOARD}/${build_config}/${app}.bin ${BUILD_OUTPUT}/${build_config}/
        mv ${SDK_DIR}/build_${BOARD}/${build_config}/${app}.elf ${BUILD_OUTPUT}/${build_config}/
    done
done

build_app () {
    local app="$1"
    local configs="$2"
    local index="$3"
    local build_cfg="$5"
    local option
    if [[ "$4" == "tests" ]]; then option="-t"; else option="-e"; fi
    ./build_mpp.sh "${option}" "${app}" -b "$BOARD" -p "$DISPLAY" -f "${configs}" -c "${build_cfg}"
    mv ${SDK_DIR}/build_${BOARD}/${build_cfg}/${app}_${CORE_ID}.bin ${BUILD_OUTPUT}/${build_cfg}/${app}_${CORE_ID}_config${index}.bin
    mv ${SDK_DIR}/build_${BOARD}/${build_cfg}/${app}_${CORE_ID}.elf ${BUILD_OUTPUT}/${build_cfg}/${app}_${CORE_ID}_config${index}.elf
}

build_all_configs () {
    # type of application: "tests" or "examples"
    local type_app="$1"
    if [[ "${type_app}" != "tests" ]] && [[ "${type_app}" != "examples" ]]; then
        echo "Type applicaton ${type_app} is not valid."
        exit 1
    fi

    for build_config in "release" "debug"; do
        for app in `cat boards/${BOARD}/${type_app}.conf`; do
            if [ -f "tools/mpp_parse_configs.sh" ] && [ -f "boards/${BOARD}/${type_app}/${app}/${app}.conf" ]; then
                index=1
                while : ; do
                    configs=$(/bin/bash tools/mpp_parse_configs.sh boards/${BOARD}/${type_app}/${app}/${app}.conf ${index})
                    if [[ "${configs}" == "" ]]; then break ; fi
                    build_app "${app}" "${configs}" "${index}" "${type_app}" "${build_config}"
                    ((index++))
                done
            fi
        done
        if [ -d internal ] && [ -f boards/${BOARD}/${type_app}_internal.conf ]; then
            for app in `cat boards/${BOARD}/${type_app}_internal.conf`; do
                if [ -f "tools/mpp_parse_configs.sh" ] && [ -f "boards/${BOARD}/${type_app}/${app}/${app}.conf" ]; then
                    index=1
                    while : ; do
                        configs=$(/bin/bash tools/mpp_parse_configs.sh boards/${BOARD}/${type_app}/${app}/${app}.conf ${index})
                        if [[ "${configs}" == "" ]]; then break ; fi
                        build_app "${app}" "${configs}" "${index}" "${type_app}" "${build_config}"
                        ((index++))
                    done
                fi
            done
        fi
    done
}

build_all_configs "examples"
build_all_configs "tests"

ARTIFACTS_DIR="/efs/shared/artifacts/${bamboo_planKey}/${bamboo_buildNumber}"

# Archive binaries for artifacts
cd ${BUILD_OUTPUT}
tar zcf ${ARTIFACTS_DIR}/armgcc_build_west_${BOARD}_display${DISPLAY}.tar.gz ./*
cd "${TOPDIR}"
# Store binaries into /efs to share with Lava/Dapeng testing
if [[ "${bamboo_TEST_BUILD_CONFIG}" == "" ]]; then
    bamboo_TEST_BUILD_CONFIG="release"
fi
# Copy binaries only for correct display type
if [[ "${bamboo_TEST_DISPLAY}" == "${DISPLAY}" ]]; then
    mkdir -p  ${ARTIFACTS_DIR}/build_${BOARD}
    cp ${BUILD_OUTPUT}/${bamboo_TEST_BUILD_CONFIG}/* ${ARTIFACTS_DIR}/build_${BOARD}/
fi
