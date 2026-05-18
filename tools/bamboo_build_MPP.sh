#!/bin/bash -ex

# This script is intended to be used by bamboo jobs
# to automate MPP builds

# set environment
echo "Set environment"
export ARMGCC_DIR=/opt/toolchains/${bamboo_ARMGCC_DIR}
TOPDIR=$(pwd)
SDK_DIR=${TOPDIR}/sdk-next/mcuxsdk/
build_cmd="python3 ./build_mpp.py"

if [[ "${BOARD}" == "" ]]; then
    BOARD=${bamboo_BOARD}
fi
if [[ "${DISPLAY}" == "" ]]; then
    DISPLAY=${bamboo_DISPLAY}
fi
EXP=${bamboo_EXAMPLE}
echo "board= ${BOARD}"
echo "display= ${DISPLAY}"

# Parse optional arguments
APP_TYPE="all"
BUILD_CONFIG="all"
SKIP_ARTIFACTS=false
SKIP_COPY=false

while getopts "a:c:sC" opt; do
    case ${opt} in
        a)
            APP_TYPE=${OPTARG}
            ;;
        c)
            BUILD_CONFIG=${OPTARG}
            ;;
        s)
            SKIP_ARTIFACTS=true
            ;;
        C)
            SKIP_COPY=true
            ;;
        *)
            echo "Usage: $0 [-a all|examples|tests] [-c all|release|debug] [-s] [-C]"
            echo "  -a  App type to build (default: all)"
            echo "  -c  Build configuration (default: all)"
            echo "  -s  Skip artifacts generation (.tar.gz)"
            echo "  -C  Skip binaries copy to shared storage"
            exit 1
            ;;
    esac
done

# Validate APP_TYPE
if [[ "${APP_TYPE}" != "all" ]] && [[ "${APP_TYPE}" != "examples" ]] && [[ "${APP_TYPE}" != "tests" ]]; then
    echo "Invalid app type: ${APP_TYPE}. Must be 'all', 'examples', or 'tests'."
    exit 1
fi

# Validate BUILD_CONFIG
if [[ "${BUILD_CONFIG}" != "all" ]] && [[ "${BUILD_CONFIG}" != "release" ]] && [[ "${BUILD_CONFIG}" != "debug" ]]; then
    echo "Invalid build config: ${BUILD_CONFIG}. Must be 'all', 'release', or 'debug'."
    exit 1
fi

# Determine build configurations list
if [[ "${BUILD_CONFIG}" == "all" ]]; then
    BUILD_CONFIGS=("release" "debug")
else
    BUILD_CONFIGS=("${BUILD_CONFIG}")
fi

echo "app_type= ${APP_TYPE}"
echo "build_config= ${BUILD_CONFIG}"
echo "skip_artifacts= ${SKIP_ARTIFACTS}"
echo "skip_copy= ${SKIP_COPY}"

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

# build all tests and examples for selected build configurations
for build_config in "${BUILD_CONFIGS[@]}"; do
    mkdir -p ${BUILD_OUTPUT}/${build_config}
    if [[ "${APP_TYPE}" == "all" ]] || [[ "${APP_TYPE}" == "examples" ]]; then
        ${build_cmd} -e all -b "$BOARD" -p "$DISPLAY" -c "$build_config"
    fi
    if [[ "${APP_TYPE}" == "all" ]] || [[ "${APP_TYPE}" == "tests" ]]; then
        ${build_cmd} -t all -b "$BOARD" -p "$DISPLAY" -c "$build_config"
    fi
    for app in ${SDK_DIR}/build_${BOARD}/${build_config}/*.bin; do
        app=$(basename ${app} .bin)
        mv ${SDK_DIR}/build_${BOARD}/${build_config}/${app}.bin ${BUILD_OUTPUT}/${build_config}/
        mv ${SDK_DIR}/build_${BOARD}/${build_config}/${app}.elf ${BUILD_OUTPUT}/${build_config}/
    done
done

build_app () {
    local app="$1"
    local configs="\"$2\""
    local index="$3"
    local build_cfg="$5"
    local option
    if [[ "$4" == "tests" ]]; then option="-t"; else option="-e"; fi
    ${build_cmd} "${option}" "${app}" -b "$BOARD" -p "$DISPLAY" -f "${configs}" -c "${build_cfg}"
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

    for build_config in "${BUILD_CONFIGS[@]}"; do
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

if [[ "${APP_TYPE}" == "all" ]] || [[ "${APP_TYPE}" == "examples" ]]; then
    build_all_configs "examples"
fi
if [[ "${APP_TYPE}" == "all" ]] || [[ "${APP_TYPE}" == "tests" ]]; then
    build_all_configs "tests"
fi

ARTIFACTS_DIR="/efs/shared/artifacts/${bamboo_planKey}/${bamboo_buildNumber}"

# Archive binaries for artifacts
if [[ "${SKIP_ARTIFACTS}" == false ]]; then
    cd ${BUILD_OUTPUT}
    tar zcf ${ARTIFACTS_DIR}/armgcc_build_west_${BOARD}_display${DISPLAY}.tar.gz ./*
    cd "${TOPDIR}"
fi

# Store binaries into /efs to share with Lava/Dapeng testing
if [[ "${SKIP_COPY}" == false ]]; then
    if [[ "${bamboo_TEST_BUILD_CONFIG}" == "" ]]; then
        bamboo_TEST_BUILD_CONFIG="release"
    fi
    # Copy binaries only for correct display type
    if [[ "${bamboo_TEST_DISPLAY}" == "${DISPLAY}" ]]; then
        mkdir -p  ${ARTIFACTS_DIR}/build_${BOARD}
        cp ${BUILD_OUTPUT}/${bamboo_TEST_BUILD_CONFIG}/* ${ARTIFACTS_DIR}/build_${BOARD}/
    fi
fi
