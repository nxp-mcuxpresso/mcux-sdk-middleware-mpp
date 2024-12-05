#!/bin/bash

W_DIR=$(readlink -f $0 | xargs dirname)/
SDK_DIR=$(realpath ${W_DIR}/../../../)
# for make -j option, do not use all CPUs
NTASK=$(($(getconf _NPROCESSORS_ONLN) / 2))
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
            CORE_ID="cm7"
            ;;
        evkbimxrt1050)
            CORE_ID="cm7"
            ;;
        mcxn9xxevk)
            CORE_ID="cm33_core0"
            ;;
        mcxn9xxbrk)
            CORE_ID="cm33_core0"
            ;;
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

build()
{
    echo "BOARD=${BOARD}"
    echo "PANEL=${PANEL}"
    echo "BUILD_TYPE=${BUILD_TYPE}"
    echo "MPP_COMMIT_ID=${MPP_COMMIT_ID}"
    echo "EXTRA_BUILD_FLAGS=${EXTRA_BUILD_FLAGS}"
    echo "TEST=${TEST}"

    set -ex

    setup_toolchain_and_sdk_dir

    #list examples
    if [ "${EXP}" = "all" ] ; then
        EXP=$( cat boards/${BOARD}/examples.conf )
    fi
    #list tests
    if [ "${TEST}" = "all" ] ; then
        TEST=$( cat boards/${BOARD}/tests.conf )
    fi

    cd ${SDK_DIR}
    mkdir -p build_${BOARD}/${BUILD_REL_OR_DBG}

    #build examples
    if [ -n "${EXP}" ] ; then
        for APP in ${EXP} ; do
            rm -fr build
            west build -b ${BOARD} examples/eiq_examples/mpp/${APP} -p always --config ${BUILD_TYPE} --toolchain armgcc -Dcore_id=${CORE_ID}
            cp build/${APP}_${CORE_ID}.* build_${BOARD}/${BUILD_REL_OR_DBG}/
        done
    fi

    #build tests
    if [ -n "${TEST}" ] ; then
        for APP in ${TEST} ; do
            rm -fr build
            west build -b ${BOARD} middleware/eiq/mpp/tests/${APP} -p always --config ${BUILD_TYPE} --toolchain armgcc -Dcore_id=${CORE_ID}
            cp build/${APP}_${CORE_ID}.* build_${BOARD}/${BUILD_REL_OR_DBG}/
        done
    fi
    cd ${W_DIR}
    
    set +ex
}

get_version()
{
    set -x
 
    cd build_${BOARD}/apps
    mpp_version=$(strings lib/${BUILD_TYPE}/libmpp.a | grep MPP_VERSION)
    echo ${mpp_version} > lib/${BUILD_TYPE}/mpp_version.txt
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
BOARDS=evkbmimxrt1170
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
    b)  BOARDS=$OPTARG
        setup_peripherals
        ;;
    e)  EXP=$OPTARG
        ;;
    h|\?)
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
    v)  verbose="VERBOSE=1"
        ;;
    esac
done

if [ ${BOARDS} == "all" ] ; then
	BOARDS="frdmmcxn947 evkbmimxrt1170 mimxrt700evk evkbimxrt1050"
fi

for BOARD in ${BOARDS} ; do 
	#adjust build type
	if [ ${BOARD} == "mcxn9xxevk" -o  ${BOARD} == "mcxn9xxbrk"  -o  ${BOARD} == "frdmmcxn947" ] ; then
		BUILD_TYPE=flash_${BUILD_REL_OR_DBG}
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

	# run build
	build
done

#generate doc
if [ ${GEN_DOC} == "true" ] ; then
    build_doc
    exit $?
fi

