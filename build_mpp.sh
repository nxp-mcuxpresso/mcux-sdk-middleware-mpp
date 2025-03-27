#!/bin/bash

W_DIR=$(readlink -f $0 | xargs dirname)/
if [ -d ${W_DIR}/../../../../mcuxsdk ] && [ -d ${W_DIR}/../../../../manifests ]; then
    echo "Running script from sdk west repository"
    SDK_DIR=$(realpath ${W_DIR}/../../../../mcuxsdk/)
else
    SDK_DIR=$(realpath ${W_DIR}/../sdk-next/mcuxsdk/)
fi
# for make -j option, do not use all CPUs
NTASK=$(($(getconf _NPROCESSORS_ONLN) / 2))
MPP_COMMIT_ID=$(git describe --dirty --always --exclude='*')
GEN_DOC=false
LAST_BUILT_ELF=""
APP_CONFIG_INDEX=""

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
}

parse_app_config()
{
    CRT_DIR=${PWD}
    cd ${W_DIR}
    app_type=$1
    app_name=$2
    if [ "$APP_CONFIG_INDEX" != "" ] ; then
        if [ -f "tools/mpp_parse_configs.sh" ] && [ -f "boards/${BOARD}/${app_type}/${app_name}/${app_name}.conf" ]; then
            configs=$(/bin/bash tools/mpp_parse_configs.sh boards/${BOARD}/${app_type}/${app_name}/${app_name}.conf ${APP_CONFIG_INDEX})
            if [[ "${configs}" != "" ]]; then
                echo "Found app config ${APP_CONFIG_INDEX}: ${configs}"
                EXTRA_BUILD_FLAGS="${EXTRA_BUILD_FLAGS} ${configs}"
                echo "EXTRA_BUILD_FLAGS=${EXTRA_BUILD_FLAGS}"
            fi
        fi
    fi
    cd ${CRT_DIR}
}

build()
{
    echo "BOARD=${BOARD}"
    echo "PANEL=${PANEL}"
    echo "BUILD_TYPE=${BUILD_TYPE}"
    echo "MPP_COMMIT_ID=${MPP_COMMIT_ID}"
    echo "EXTRA_BUILD_FLAGS=${EXTRA_BUILD_FLAGS}"
    echo "APP_CONFIG_INDEX=${APP_CONFIG_INDEX}"
    echo "EXAMPLE=${EXP}"
    echo "TEST=${TEST}"

    set -ex

    setup_toolchain_and_sdk_dir

    #list examples
    if [ "${EXP}" = "all" ] ; then
        EXPS=$( cat boards/${BOARD}/examples.conf )
    else
        EXPS=${EXP}
    fi
    #list tests
    if [ "${TEST}" = "all" ] ; then
        TESTS=$( cat boards/${BOARD}/tests.conf )
    else
        TESTS=${TEST}
    fi

    cd ${SDK_DIR}
    mkdir -p build_${BOARD}/${BUILD_REL_OR_DBG}

    if [ "${BOARD}" == "frdmmcxn947" ] ; then
        PANEL_CONFIG_DEFINE=""
    else
        PANELNAME=$(grep "define DEMO_PANEL_[[:alnum:]]* ${PANEL}" ${SDK_DIR}/examples/_boards/${BOARD}/display_support.h | cut -d ' ' -f 2)
        PANEL_CONFIG_DEFINE=-DCONFIG_${PANELNAME}="y"
    fi

    #build examples
    if [ -n "${EXPS}" ] ; then
        for APP in ${EXPS} ; do
            rm -fr build
            west build -b ${BOARD} examples/eiq_examples/mpp/${APP} -p always \
                       --config ${BUILD_TYPE} \
                       --toolchain armgcc \
                       -Dcore_id=${CORE_ID} \
                       ${PANEL_CONFIG_DEFINE} \
                       -DHAL_LOG_LEVEL="${LOG_LEVEL}" \
                       -DMPP_COMMIT=${MPP_COMMIT_ID} \
                       -DEXTRA_BUILD_FLAGS="${EXTRA_BUILD_FLAGS}"
            if [ "$APP_CONFIG_INDEX" != "" ] ; then
                cp build/${APP}_${CORE_ID}.elf build_${BOARD}/${BUILD_REL_OR_DBG}/${APP}_${CORE_ID}_config${APP_CONFIG_INDEX}.elf
                cp build/${APP}_${CORE_ID}.bin build_${BOARD}/${BUILD_REL_OR_DBG}/${APP}_${CORE_ID}_config${APP_CONFIG_INDEX}.bin
                LAST_BUILT_ELF="${APP}_${CORE_ID}_config${APP_CONFIG_INDEX}.elf"
            else
                cp build/${APP}_${CORE_ID}.* build_${BOARD}/${BUILD_REL_OR_DBG}/
                LAST_BUILT_ELF="${APP}_${CORE_ID}.elf"
            fi
        done
    fi

    #build tests
    if [ -n "${TESTS}" ] ; then
        for APP in ${TESTS} ; do
            rm -fr build
            parse_app_config "tests" "${APP}"
            west build -b ${BOARD} middleware/eiq/mpp/tests/${APP} -p always \
                       --config ${BUILD_TYPE} \
                       --toolchain armgcc \
                       -Dcore_id=${CORE_ID} \
                       ${PANEL_CONFIG_DEFINE} \
                       -DHAL_LOG_LEVEL="${LOG_LEVEL}" \
                       -DMPP_COMMIT=${MPP_COMMIT_ID} \
                       -DEXTRA_BUILD_FLAGS="${EXTRA_BUILD_FLAGS}"
            if [ "$APP_CONFIG_INDEX" != "" ] ; then
                cp build/${APP}_${CORE_ID}.elf build_${BOARD}/${BUILD_REL_OR_DBG}/${APP}_${CORE_ID}_config${APP_CONFIG_INDEX}.elf
                cp build/${APP}_${CORE_ID}.bin build_${BOARD}/${BUILD_REL_OR_DBG}/${APP}_${CORE_ID}_config${APP_CONFIG_INDEX}.bin
                LAST_BUILT_ELF="${APP}_${CORE_ID}_config${APP_CONFIG_INDEX}.elf"
            else
                cp build/${APP}_${CORE_ID}.* build_${BOARD}/${BUILD_REL_OR_DBG}/
                LAST_BUILT_ELF="${APP}_${CORE_ID}.elf"
            fi
        done
    fi
    cd ${W_DIR}
    
    get_version

    set +ex
}

get_version()
{
    set -x
 
    cd ${SDK_DIR}/build_${BOARD}/${BUILD_REL_OR_DBG}
    mpp_version=$(strings ${LAST_BUILT_ELF} | grep MPP_VERSION)
    echo ${mpp_version} > mpp_version.txt
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
    
    mpp_version=$(cat ${SDK_DIR}/build_${BOARD}/${BUILD_REL_OR_DBG}/mpp_version.txt | awk -F_ '{print $NF}')
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

list_pannels()
{
    if [ "${BOARDS}" == "all" ] ; then
        TEMP_BOARDS="frdmmcxn947 evkbmimxrt1170 mimxrt700evk"
    else
        TEMP_BOARDS="${BOARDS}"
    fi
    for BOARD in ${TEMP_BOARDS} ; do
        if [ "${BOARD}" == "frdmmcxn947" ] ; then
            continue
        fi
        panel_list+="\nPanel list supported for board ${BOARD}\n"
        panel_list+=$(grep "define DEMO_PANEL_" ${SDK_DIR}/examples/_boards/${BOARD}/display_support.h | egrep -v "DEMO_PANEL_HEIGHT|DEMO_PANEL_WIDTH")
        panel_list+="\n"
    done
}

usage()
{
    echo "usage:"
    echo "$0 [-ab:e:h?id:Dp:c:f:t:s:g:v]"
    echo " -h|?: help"
    echo " -b <board name>: {evkbmimxrt1170, frdmmcxn947, mimxrt700evk}"
    echo " -D: build mpp and hal APIs documentation"
    echo " -e <example name>: build the example app {camera_view, all, ...}"
    echo " -i: build for host (x86)"
    echo -e " -d: <log level> as follow:\n${log_levels}"
    echo -e " -p: <panel index> as follow:\n${panel_list}"
    echo " -t  <test name>: build the test app {test_image_display, all, ...}"
    echo " -c: build/config type <build_type>: {debug, release}"
    echo " -f: extra build flags: as follow {\"-DFLAG1=1 -DFLAG2=1 -DFLAG3\"}"
    echo " -a: rebuild libtflm.a from source"
    echo " -s <sdk_path>: specify the path to the mcuxsdk folder from sdk-next repo (DO NOT include mcuxsdk folder)"
    echo " -v: enable verbose for build"
    echo " -g <app_config_index> - the index of the app_config to be used for building the app"
    exit 0
}

# default example will be set later in this script
EXP=""
# no test to build by default
TEST=""
# default board
BOARDS=evkbmimxrt1170
BUILD_REL_OR_DBG=release
BUILD_TYPE=flexspi_nor_sdram_${BUILD_REL_OR_DBG}
EXTRA_BUILD_FLAGS=""
TFLM_REBUILD=false

log_levels=$(grep LOG_LVL_ CMakeLists.txt)
panel_list=""

#parse arguments
OPTIND=1
while getopts "ab:e:h?id:Dp:c:f:t:s:g:v" opt; do
    case "$opt" in
    a)  TFLM_REBUILD=true
        ;;
    b)  BOARDS=$OPTARG
        ;;
    e)  EXP=$OPTARG
        ;;
    h|\?)
        list_pannels
        usage
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
    s)  SDK_DIR=$(realpath ${W_DIR}/$OPTARG/mcuxsdk/)
        ;;
    g)  APP_CONFIG_INDEX+=$OPTARG
        ;;
    v)  verbose="VERBOSE=1"
        ;;
    esac
done

if [ ! -d ${SDK_DIR} ]; then
    echo "sdk directory ${SDK_DIR} does not exist"
    echo "specify the sdk dir using -s option (see ./build_mpp -h for more details)"
    exit 1
fi
echo "SDK_DIR=${SDK_DIR}"

if [ "${BOARDS}" == "all" ] ; then
	BOARDS="frdmmcxn947 evkbmimxrt1170 mimxrt700evk"
fi

# default example camera_view (only if EXP and TEST are not set after parsing arguments)
if [ "${EXP}" == "" -a "${TEST}" == "" ] ; then
    EXP=camera_view
fi

for BOARD in ${BOARDS} ; do 
	#adjust build type
	if [ "${BOARD}" == "frdmmcxn947" -o "${BOARD}" == "mimxrt700evk" ] ; then
		BUILD_TYPE=flash_${BUILD_REL_OR_DBG}
	else
		BUILD_TYPE=flexspi_nor_sdram_${BUILD_REL_OR_DBG}
	fi

	#Set default panel depending on board:
	#RT700: Default Panel 4 
	#RT1170 and RT1050: Default Panel 0
	if [ "${BOARD}" == "evkbmimxrt1170" -o "${BOARD}" == "mimxrt700evk" ] ; then
		default_panel="DEMO_PANEL_RK055MHD091A0"
	else
        default_panel=""
    fi

	#use default panel if not passed by user
	PANEL="${PANEL:=${default_panel}}"

	# run build
	build
done

#generate doc
if [ ${GEN_DOC} == "true" ] ; then
    build_doc
    exit $?
fi

