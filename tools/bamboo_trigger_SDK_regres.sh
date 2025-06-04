#!/bin/bash

# Exit on error
set -e

# Constants
DAPENG_TMP_LOG_FILE="dapeng_out.txt"
COMPILER="armgcc"

# get the path to the MPP_DIR, independent of the 
# location from which the script is called
MPP_DIR=$(dirname $(readlink -f $0 | xargs dirname))

# Default parameters
BIN_DIR="${MPP_DIR}/../sdk-next/mcuxsdk"
BOARD="evkbmimxrt1170,frdmmcxn947"
OUTPUT_DIR="test_results"
RT1170_EXAMPLES="camera_mobilenet_view,camera_persondetect_view,camera_ultraface_view,camera_view,static_image_nanodet_view"
MCXN947_EXAMPLES="camera_mobilenet_view,camera_persondetect_view,camera_ultraface_view,camera_view"
EXAMPLES=""
JSON_CONFIG_FILE="dapeng_config.json"
MAIL_LIST=""
SDK_VERSION="main"
MCU_SDK_TEST_VERSION="main"
BUILD_CFG="release"
CLEAN_LOGS_IF_PASS="no"

# BAMBOO plan does not need the default BUILD_CFG set when running this script
if [[ "${bamboo_planKey}" != "" ]]; then
    BUILD_CFG=""
fi

usage()
{
    echo "usage:"
    echo "$0 [-d:b:o:e:m:s:t:c:rh?]"
    echo " -h|?: help"
    echo " -d <bin_dir_name>: name of the directory containing all the test binaries for all boards"
    echo "    each board should contain it's binaries in a directoy named build_<board> inside bin_dir_name"
    echo "    default value is $BIN_DIR"
    echo " -b <board>: board to run test on. May be a single element or a list of boards separated by comma"
    echo "    default value is $BOARD"
    echo " -o <output_dir>: name of the directory to download the build results"
    echo "    file should be placed inside sdk folder from mpp dir"
    echo "    default value is $OUTPUT_DIR"
    echo " -e <examples>: list of examples application sepparated by commas to run tests on"
    echo "    Default values:"
    echo "          board evkbmimxrt1170: ${RT1170_EXAMPLES}"
    echo "          board frdmmcxn947: ${MCXN947_EXAMPLES}"
    echo " -m <mail_list>: list of emails used by dapeng to send the test report"
    echo "    Can be a single email or a list separrated by comma"
    echo "    default value is $MAIL_LIST"
    echo " -s <sdk_version>: sdk version to be used"
    echo "    default value is $SDK_VERSION"
    echo " -t <mcu-sdk-test_version>: version to be used for mcu-sdk-test repository"
    echo "    default value is $MCU_SDK_TEST_VERSION"
    echo " -c <build_config>: build configuration (release or debug) to be used when creating paths to images"
    echo "    default value is $BUILD_CFG"
    echo " -r Flag to specify if output logs should be removed if build passed"
    echo "    Logs are always kept if build fails"
    set +e
    exit 0
}

#parse arguments
OPTIND=1
while getopts "d:b:o:e:m:s:t:c:rh?" opt; do
    case "$opt" in
    d)  BIN_DIR=$OPTARG
        ;;
    b)  BOARD=$OPTARG
        ;;
    o)  OUTPUT_DIR=$OPTARG
        ;;
    e)  EXAMPLES=$OPTARG
        ;;
    m)  MAIL_LIST=$OPTARG
        ;;
    s)  SDK_VERSION=$OPTARG
        ;;
    t)  MCU_SDK_TEST_VERSION=$OPTARG
        ;;
    c)  BUILD_CFG=$OPTARG
        ;;
    r)  CLEAN_LOGS_IF_PASS="yes"
        ;;
    h|\?)
        usage
        ;;
    esac
done

# Create test json file
if [[ "${bamboo_planKey}" != "" ]]; then
    build_name="MPP_${bamboo_planKey}_${bamboo_buildNumber}"
else
    build_name="MPP_test"
    if [[ "${MAIL_LIST}" == "" ]]; then
        echo "Please provide the email address for the test report using -m <email_address> argument"
        exit 1
    fi
fi

if [ ! $(which dapeng) ]; then
    echo "dapeng-cli is not installed. Installing it using pip"
    pip install -U dapeng-cli --no-input --extra-index-url https://kr-nxrm.sw.nxp.com/repository/mcupypi/simple
    if [ ! $(which dapeng) ]; then
        echo "Failed to install dapeng-cli"
        exit 1
    fi
fi

# Function used to search for an item in a list with elements spearated by comma
search_item_in_list()
{
    item=$1
    IFS=', ' read -r -a list <<< "$2"

    found=0
    for i in "${list[@]}"
    do
        if [[ "${item}" == "${i}" ]]; then
            found=1
            break
        fi
    done
    echo ${found}
}

# Function used to created the header part of the Dapeng job config file
create_header()
{
    echo -n "{
    \"header\": {
        \"name\": \"${build_name}\",
        \"project\": \"ksdk\",
        \"email\": \"${MAIL_LIST}\",
        \"environment\": {
            \"mcu-sdk-test\": \"${MCU_SDK_TEST_VERSION}\"
        },
        \"revision\": \"${SDK_VERSION}\",
        \"routing\": true,
        \"run_token\": \"TEST_RUN\",
        \"sdk_location\": \"mcuxsdk-manifests\",
        \"type\": \"run\",
        \"extra\": {
            \"skip_run_if_image_is_unchanged\": false
        }
    },
    \"detail\": [" > ${JSON_CONFIG_FILE}
}

# Function used to add a new test to the Dapeng job config file
generic_test_add()
{
    test_name=$1
    test_board=$2
    bin_file=$3

    echo -n "
        {
            \"name\": \"${test_name}\",
            \"compiler\": \"${COMPILER}\",
            \"platform\": \"${test_board}\",
            \"target\": \"${BUILD_CFG}\",
            \"runtimeout\": 1200,
            \"category\": \"eiq_examples\",
            \"bin\": \"${bin_file}\"
        }"
}

# Function used to add the first test to the list in Dapeng job config file
add_first_test()
{
    elf_file="${BIN_DIR}/build_$2/${BUILD_CFG}/$1_$3.elf"
    if [ ! -f ${elf_file} ]; then
        echo "binary file ${elf_file} does not exist"
        exit 1
    fi

    test_config=$(generic_test_add $1 $2 ${elf_file})

    echo -n "${test_config}" >> ${JSON_CONFIG_FILE}
}

# Function used to add a new test to the list in Dapeng job config file
add_next_test()
{
    elf_file="${BIN_DIR}/build_$2/${BUILD_CFG}/$1_$3.elf"
    if [ ! -f ${elf_file} ]; then
        echo "binary file ${elf_file} does not exist"
        exit 1
    fi

    test_config=$(generic_test_add $1 $2 ${elf_file})

    echo -n ",${test_config}" >> ${JSON_CONFIG_FILE}
}

# Function used to close the test list in Dapeng job config file
close_json_file()
{
    echo "" >> ${JSON_CONFIG_FILE}
    echo "    ]" >> ${JSON_CONFIG_FILE}
    echo "}" >> ${JSON_CONFIG_FILE}
}

# Create the header of the Dapeng json config file
create_header

# Parse the board list
IFS=', ' read -r -a BOARD_LIST <<< "$BOARD"
first_board=1

# Add the test configurations for each board
for board in "${BOARD_LIST[@]}"
do
    # Check board parameter
    # Set core id depending on board name
    case "${board}" in
    frdmmcxn947)
        CORE_ID="cm33_core0"
        ;;
    evkbmimxrt1170)
        CORE_ID="cm7"
        ;;
    mimxrt700evk)
        CORE_ID="cm33_core0"
        ;;
    *)
        echo "Board ${board} not supported yet for SDK regression"
        exit 1
    esac

    # Set default examples depending on board
    DEFAULT_EXAMPLES=$( cat ${MPP_DIR}/boards/${board}/examples.conf )
    DEFAULT_EXAMPLES=$( echo ${DEFAULT_EXAMPLES} | sed 's/ /,/g' )

    # If examples are not configured via command line arguments,
    # use the default list for each board (DEFAULT_EXAMPLES set above)
    if [[ "${EXAMPLES}" == "" ]]; then
        CRT_EXAMPLES=${DEFAULT_EXAMPLES}
    else
        CRT_EXAMPLES=${EXAMPLES}
    fi

    # Parse examples list (both default and current examples list)
    # We need to parse both lists to check if the configured test is supported
    IFS=', ' read -r -a DEFAULT_EXAMPLE_LIST <<< "${DEFAULT_EXAMPLES}"
    IFS=', ' read -r -a EXAMPLE_LIST <<< "${CRT_EXAMPLES}"

    # Check if the first test from the EXAMPLE_LIST is supported or not
    found=$(search_item_in_list "${EXAMPLE_LIST[0]}" "${DEFAULT_EXAMPLES}")
    if [[ "${found}" != "1" ]]; then
        echo "Example ${EXAMPLE_LIST[0]} not supported for board ${board}"
        echo "Examples supported for this board are:" "${DEFAULT_EXAMPLE_LIST[@]}"
        continue
    fi

    # Add first test to the Dapeng json config file only if this is the first board in the list
    if [[ "${first_board}" == "1" ]]; then
        add_first_test ${EXAMPLE_LIST[0]} ${board} ${CORE_ID}
        first_board=0
    else
        add_next_test ${EXAMPLE_LIST[0]} ${board} ${CORE_ID}
    fi

    # Add all the remaining tests to the list, only if example is supported in the default list
    for i in "${EXAMPLE_LIST[@]:1}"
    do
        # Search for the example in the default list
        found=$(search_item_in_list "${i}" "${DEFAULT_EXAMPLES}")
        if [[ "${found}" != "1" ]]; then
            echo "Example ${i} not supported for board ${board}"
            echo "Examples supported for this board are:" "${DEFAULT_EXAMPLE_LIST[@]}"
            continue
        fi
        # Add the test to the json config file
        add_next_test ${i} ${board} ${CORE_ID}
    done
done

# Close the json file (close the list of tests)
close_json_file

# Create the output directory if it does not exist
if [ ! -d ${OUTPUT_DIR} ]; then
    mkdir -p ${OUTPUT_DIR}
fi

# Trigger dapeng job and wait for it to finish
echo "Running Dapeng job..."
echo ""
set +e
dapeng new -c ${JSON_CONFIG_FILE} --wait --check 2>&1 | tee ${DAPENG_TMP_LOG_FILE}
dapeng_exit_code=${PIPESTATUS[0]}
set -e
echo ""

# Download job results
dapeng_task_id=$(cat ${DAPENG_TMP_LOG_FILE} | grep "Dapeng Job URL" | cut -d "/" -f 6)
if [[ "${dapeng_task_id}" == "" ]]; then
    echo "Could not determine Dapeng task id"
    echo "Check log file ${DAPENG_TMP_LOG_FILE}"
    exit 1
fi
echo "Downloading job results for task id ${dapeng_task_id}..."
echo ""
dapeng download ${dapeng_task_id} -T run -d ${OUTPUT_DIR} 2>&1 | tee -a ${DAPENG_TMP_LOG_FILE}
echo ""

# Get the number for failed tests, if any
test_summary=$(cat ${DAPENG_TMP_LOG_FILE} | grep "run total:")
test_summary=$(echo "${test_summary//=}")
n_total_tests=$(echo "${test_summary}" | cut -d "," -f 1 | cut -d ":" -f 2)
n_total_tests=$(echo "${n_total_tests// }")
n_failed_tests=$(echo "${test_summary}" | cut -d "," -f 2 | cut -d ":" -f 2)
n_failed_tests=$(echo "${n_failed_tests// }")
n_passed_tests=$(echo "${test_summary}" | cut -d "," -f 3 | cut -d ":" -f 2)
n_passed_tests=$(echo "${n_passed_tests// }")
n_na_tests=$(($n_total_tests-$n_failed_tests-$n_passed_tests))
if [[ "${n_na_tests}" != "0" ]]; then
    echo "Number of tests with status unavailable: ${n_na_tests}"
fi

# Check the exit code for dapeng new command
if [[ "${dapeng_exit_code}" != 0 || "${n_na_tests}" != "0" ]]; then
    # If exit code is not 0, it means test job failed
    n_total_failed_tests=$(($n_failed_tests+$n_na_tests))
    echo "${n_total_failed_tests} test(s) FAILED"
    echo "Failed tests:"
    task_output_path="${OUTPUT_DIR}/download_${dapeng_task_id}"
    # Check which tests failed
    for board_log in ${task_output_path}/*
    do
        board_output_path="${board_log}/eiq_examples"
        for test_log in ${board_output_path}/*
        do
            log_output_path="${test_log}/${COMPILER}/${BUILD_CFG}"
            # If the file runresult_Fail.txt exists, it means test failed
            if [ -f "${log_output_path}/runresult_Fail.txt" ]; then
                echo "    ${test_log} - board ${board_log}" 
                echo "          --> console log: ${log_output_path}/app_test.log"
            fi
            # If the file runresult_NA.txt exists, it means no board available to run the test
            if [ -f "${log_output_path}/runresult_NA.txt" ]; then
                echo "    ${test_log} - board ${board_log}" 
                echo "          --> No board available for this test"
            fi
            # If the file runresult_Not Support.txt exists, it means no test script was found for this test
            if [ -f "${log_output_path}/runresult_Not Support.txt" ]; then
                echo "    ${test_log} - board ${board_log}" 
                echo "          --> No test script found for this example"
                echo "          --> console log: ${log_output_path}/app_test.log"
            fi
        done
    done
    # Move json config file and output log file to test results directory
    mv ${DAPENG_TMP_LOG_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
    mv ${JSON_CONFIG_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
    tar zcf ${OUTPUT_DIR}/test_results_${dapeng_task_id}.tar.gz ${OUTPUT_DIR}/download_${dapeng_task_id}/*
    echo "Check ${OUTPUT_DIR}/download_${dapeng_task_id} directory for full logs"
    # Exit the script with error
    exit ${dapeng_exit_code}
else
    echo "All tests PASSED"
    # Remove all output logs if command line -r option is provided
    if [[ "${CLEAN_LOGS_IF_PASS}" == "yes" ]]; then
        echo "Removing all output logs (-r option used)"
        rm -rf "${OUTPUT_DIR}/download_${dapeng_task_id}/"
        rm ${DAPENG_TMP_LOG_FILE}
        rm ${JSON_CONFIG_FILE}
    else
        # Move json config file and output log file to test results directory
        mv ${DAPENG_TMP_LOG_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
        mv ${JSON_CONFIG_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
        tar zcf ${OUTPUT_DIR}/test_results_${dapeng_task_id}.tar.gz ${OUTPUT_DIR}/download_${dapeng_task_id}/*
        echo "Check ${OUTPUT_DIR}/download_${dapeng_task_id} directory for full logs"
    fi
fi
