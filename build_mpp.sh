#!/bin/bash

W_DIR=$(readlink -f $0 | xargs dirname)/
# for make -j option, do not use all CPUs
NTASK=$(($(getconf _NPROCESSORS_ONLN) / 2))
if [ -z "${SDK_VERSION}" ];then
    SDK_VERSION=2_16_0
fi
MPP_COMMIT_ID=$(git describe --dirty --always --exclude='*')
GEN_DOC=false

setup_toolchain_and_sdk_dir()
{
    # looking for ARMGCC and SDK directories
    if [ -f .build.env ];then
        source .build.env
    fi
    if [ -z ${ARMGCC_DIR} ]; then
        export ARMGCC_DIR=`dirname $(which arm-none-eabi-gcc)`/..
        if [ -z ${ARMGCC_DIR} ]; then
            ARMGCC_DIR=$(zenity --file-selection --directory --title "Please select ARMGCC toolchain directory" )
            ARMGCC_DIR=${ARMGCC_DIR}/
            echo export ARMGCC_DIR=${ARMGCC_DIR} >> .build.env
        fi
    fi

    case "${BOARD}" in
        evkmimxrt1170)
            SDK_NAME="SDK_${SDK_VERSION}_MIMXRT1170-EVK"
            ;;
        evkbimxrt1050)
            SDK_NAME="SDK_${SDK_VERSION}_EVKB-IMXRT1050"
            ;;
        mcxn9xxevk)
            SDK_NAME="SDK_${SDK_VERSION}_MCX-N9XX-EVK"
            ;;
        mcxn9xxbrk)
            SDK_NAME="SDK_${SDK_VERSION}_MCX-N9XX-BRK"
            ;;
        frdmmcxn947)
            SDK_NAME="SDK_${SDK_VERSION}_FRDM-MCXN947"
            ;;
        mimxrt700evk)
            SDK_NAME="SDK_${SDK_VERSION}_MIMXRT700-EVK"
            ;;
        evkbmimxrt1170)
            SDK_NAME="SDK_${SDK_VERSION}_MIMXRT1170-EVKB"
            ;;
        *)
            echo "Fail sdk board name"
            exit 1
    esac

    # toolchain file
    if [ -z "${SDK_DIR}" ];then
        SDK_DIR=${W_DIR}../${SDK_NAME}/
    fi
    TOOLCHAIN_FILE=${SDK_DIR}tools/cmake_toolchain_files/armgcc.cmake
    if [ ! -f ${TOOLCHAIN_FILE} ];then
        SDK_DIR=$(zenity --file-selection --directory --title "Please select SDK directory" )
        SDK_DIR=${SDK_DIR}/
        TOOLCHAIN_FILE=${SDK_DIR}tools/cmake_toolchain_files/armgcc.cmake
        if [ -f ${TOOLCHAIN_FILE} ];then
            echo export SDK_DIR=${SDK_DIR} >> .build.env
        else
            echo "SDK directory is missing, please install it before building"
            exit 1
        fi
    fi
}

set_default_build_flag()
{
    # If no extra build flag is set by user
    # => set extra build flag to default config with APP_CONFIG=0
    if [[ "${EXTRA_BUILD_FLAGS}" == "" ]]; then EXTRA_BUILD_FLAGS="-DAPP_CONFIG=0"; fi
    # workaround where following setting from component_serial_manager_uart.xx.cmake doesn't seem to take effect for the target "example" due to libsdk.a being linked with MCUX_SDK_PROJECT_NAME
    EXTRA_BUILD_FLAGS+=" -DSERIAL_PORT_TYPE_UART=1 "
    # hack to enable freertos statistics in libsdk.a
    EXTRA_BUILD_FLAGS+=" -DconfigGENERATE_RUN_TIME_STATS=1 "
}

build()
{
    set_default_build_flag

    echo "BOARD=${BOARD}"
    echo "PANEL=${PANEL}"
    echo "BUILD_TYPE=${BUILD_TYPE}"
    echo "MPP_COMMIT_ID=${MPP_COMMIT_ID}"
    echo "EXTRA_BUILD_FLAGS=${EXTRA_BUILD_FLAGS}"
    echo "TEST=${TEST}"

    set -x

    setup_toolchain_and_sdk_dir

    #use libtflm.a built by us.
    if [ "${TFLM_REBUILD}" == "true" ] ; then
        if [ -f "build_${BOARD}/eiq/lib/${BUILD_TYPE}/libtflm.a" ];then
            echo "reusing build_${BOARD}/eiq/lib/${BUILD_TYPE}/libtflm.a"
        else
            echo "re-building build_${BOARD}/eiq/lib/${BUILD_TYPE}/libtflm.a"
            mkdir -p build_${BOARD}/eiq
            cd build_${BOARD}/eiq
            cmake -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
                  -G "Unix Makefiles" \
                  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
                  -DSDK_DIR="${SDK_DIR}" \
                  -DBOARD="${BOARD}" \
                  ../../eiq/
            make -j ${NTASK} install ${verbose} || exit $?
        fi
    fi

    cd ${W_DIR}
    mkdir -p build_${BOARD}/apps
    cd build_${BOARD}/apps
    if [ -d "CMakeFiles" ];then rm -rf CMakeFiles; fi
    if [ -f "Makefile" ];then rm -f Makefile; fi
    if [ -f "cmake_install.cmake" ];then rm -f cmake_install.cmake; fi
    if [ -f "CMakeCache.txt" ];then rm -f CMakeCache.txt; fi

    #build examples
    if [ "${EXP}" = "all" ] ; then
        EXP=$( cat ../../boards/${BOARD}/examples.conf )
    fi

    if [ -n "${EXP}" ] ; then
        for APP in ${EXP} ; do
            cmake -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
                  -G "Unix Makefiles" \
                  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
                  -DSDK_DIR="${SDK_DIR}" \
                  -DAPPNAME="${APP}" \
                  -DBOARD="${BOARD}" \
                  -DHAL_LOG_LEVEL="${LOG_LEVEL}" \
                  -DDEMO_PANEL="${PANEL}" \
                  -DAPPTYPE="examples" \
                  -DMPP_COMMIT="${MPP_COMMIT_ID}" \
                  -DEXTRA_BUILD_FLAGS="${EXTRA_BUILD_FLAGS}" \
                  -DTFLM_REBUILD="${TFLM_REBUILD}" \
                  ../../
            make -j ${NTASK} install ${verbose} || exit $?
        done
    fi

    #build tests
    if [ "${TEST}" = "all" ] ; then
        TEST=$( cat ../../boards/${BOARD}/tests.conf )
    fi

    if [ -n "${TEST}" ] ; then
        for APP in ${TEST} ; do
            cmake -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
                  -G "Unix Makefiles" \
                  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
                  -DSDK_DIR="${SDK_DIR}" \
                  -DAPPNAME="${APP}" \
                  -DBOARD="${BOARD}" \
                  -DHAL_LOG_LEVEL="${LOG_LEVEL}" \
                  -DDEMO_PANEL="${PANEL}" \
                  -DAPPTYPE="tests" \
                  -DMPP_COMMIT="${MPP_COMMIT_ID}" \
                  -DEXTRA_BUILD_FLAGS="${EXTRA_BUILD_FLAGS}" \
                  ../../
            make -j ${NTASK} install ${verbose} || exit $?
        done
    fi
    cd ${W_DIR}

    get_version
}

get_version()
{
    set -x

    cd build_${BOARD}/apps
    mpp_version=$(strings lib/${BUILD_TYPE}/libmpp.a | grep MPP_VERSION)
    echo ${mpp_version} > lib/${BUILD_TYPE}/mpp_version.txt
    cd -
}

build_emulator()
{
    set -x
    make -C boards/linux-pc/FreeRTOS/Demo/Posix_GCC libfreertos.a

    cd boards/linux-pc/camera_emulator
    cmake ..
    make -j ${NTASK} ${verbose}
}

setup_sdk()
{
    set -x
    cd ..
    archive=$(ls ${W_DIR}sdk/*.tbz)
    echo "${archive} will be installed"
    tar -xf ${archive}
    ret=$?
    set +x
    if [ ${ret} -eq 0 ];then
        echo Success
    else
        echo Failed
    fi
    cd -
}

build_api_doc()
{
    api_name=$1
    header_file=$2
    api_version=$3
    doxyfile_name=$4
    output_file_name=$5
    dir=$(mktemp -d)
    pdf=${dir}/refman.pdf
    rtf=rtf/refman.rtf
    cp -rf dox ${dir}
    export LATEX_OUTPUT=${dir}
    export LATEX_HEADER=${dir}/dox/${header_file}
    export PROJECT_NUMBER=${api_version}
    sed -i "s/${api_name} VERSION/${api_name} VERSION ${api_version}/g" ${LATEX_HEADER}

    if [ $? -ne 0 ];then
        cp dox/${header_file} ${LATEX_HEADER}
    fi
    doxygen dox/${doxyfile_name}
    
    ls ${dir}
    make pdf -C ${dir}
    if [ -f ${pdf} ]; then
        cp ${pdf} ${output_file_name}.pdf
    fi
    if [ -f ${rtf} ]; then
        cp ${rtf} ${output_file_name}.rtf
        rm -rf rtf
    fi
}

# build MPP and HAL APIs documentation
build_doc ()
{
    set -x
    
    mpp_version=$(cat build_${BOARD}/apps/lib/${BUILD_TYPE}/mpp_version.txt | awk -F_ '{print $NF}')
    if [ $? -ne 0 ];then
        mpp_version=""
        echo "Fail: mpp version doesn't exist!"
    fi
    
    # build mpp documentation for MPP
    build_api_doc "MPP" "header.tex" ${mpp_version} "Doxyfile" "mpp_api"
    
    # build mpp documentation for HAL
    build_api_doc "MPP-HAL" "hal_header.tex" ${mpp_version} "HalDoxyfile" "hal_api"
    
    set +x
}

setup_peripherals()
{
    if [ ${BOARD} == "evkmimxrt1170" -o  ${BOARD} == "evkbimxrt1050" -o ${BOARD} == "mimxrt700evk" -o ${BOARD} == "evkbmimxrt1170" ] ; then
    	panel_list=$(grep "define DEMO_PANEL_" boards/${BOARD}/inc/display_support.h | egrep -v "DEMO_PANEL_HEIGHT|DEMO_PANEL_WIDTH")
    else
        panel_list=""
    fi
}

usage()
{
    echo "usage:"
    echo "$0 [-e:ih?vsb:d:Dp:]"
    echo " -h|?: help"
    echo " -s: setup sdk"
    echo " -b <board name>: {evkmimxrt1170, ...}"
    echo " -D: build mpp and hal APIs documentation"
    echo " -e <example name>: build the example app {camera_view, all, ...}"
    echo " -i: build for host (x86)"
    echo -e " -d: <log level> as follow:\n${log_levels}"
    echo -e " -p: <panel index> as follow:\n${panel_list}"
    echo " -t  <test name>: build the test app {test_image_display, all, ...}"
    echo " -c: build/config type <build_type>: {debug, release}"
    echo " -f: extra build flags: as follow {\"-DFLAG1=1 -DFLAG2=1 -DFLAG3\"}"
    echo " -a: rebuild libtflm.a from source"
    echo " -v: enable verbose for build"
    exit 0
}

# default example camera_view
EXP=camera_view
# no test to build by default
TEST=""
# default board
BOARD=evkmimxrt1170
BUILD_REL_OR_DBG=release
BUILD_TYPE=flexspi_nor_sdram_${BUILD_REL_OR_DBG}
EXTRA_BUILD_FLAGS=""
TFLM_REBUILD=false

log_levels=$(grep LOG_LVL_ CMakeLists.txt)
panel_list=""
setup_peripherals

#parse arguments
OPTIND=1
while getopts "ab:e:h?id:Dp:c:f:t:sv" opt; do
    case "$opt" in
    a)  TFLM_REBUILD=true
        ;;
    b)  BOARD=$OPTARG
        setup_peripherals
        ;;
    e)  EXP=$OPTARG
        ;;
    h|\?)
        usage
        ;;
    i)  build_emulator
        exit $?
        ;;
    d)  LOG_LEVEL=$OPTARG
        ;;
    D)  GEN_DOC=true
        ;;
    p)  PANEL=$OPTARG
        ;;
    t)  TEST=$OPTARG
        ;;
    c)  BUILD_REL_OR_DBG=$OPTARG
        ;;
    f)  EXTRA_BUILD_FLAGS+=$OPTARG
        ;;
    s)  # setup sdk
        setup_sdk;
        exit $?
        ;;
    v)  verbose="VERBOSE=1"
        ;;
    esac
done

#adjust build type
if [ ${BOARD} == "mcxn9xxevk" -o  ${BOARD} == "mcxn9xxbrk"  -o  ${BOARD} == "frdmmcxn947" ] ; then
	BUILD_TYPE=${BUILD_REL_OR_DBG}
elif [ ${BOARD} == "mimxrt700evk" ] ; then
        BUILD_TYPE=flash_${BUILD_REL_OR_DBG}
else
	BUILD_TYPE=flexspi_nor_sdram_${BUILD_REL_OR_DBG}
fi

#Set default panel depending on board:
#RT700: Default Panel 4 
#RT1170 and RT1050: Default Panel 0
if [ ${BOARD} == "mimxrt700evk" ] ; then
        default_panel=4
else
        default_panel=0
fi

#use default panel if not passed by user
PANEL="${PANEL:=${default_panel}}"

#generate doc
if [ ${GEN_DOC} == "true" ] ; then
    build_doc
    exit $?
fi

# run build
build
