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
JUNIT_TEST_REPORT_FILE="dapeng_test_report.xml"
DAPENG_TEST_REPORT_MD_FILE="dapeng_test_report.md"
MAIL_LIST=""
SDK_VERSION="main"
MCU_SDK_TEST_VERSION="main"
BUILD_CFG="release"
CLEAN_LOGS_IF_PASS="no"
NEXUS_DIR_LINK="https://${bamboo_NEXUS_INSTANCE}-nxrm.sw.nxp.com/#browse/browse:${bamboo_NEXUS_REPO}:${bamboo_NEXUS_DIRECTORY}%2F${bamboo_planKey}%2F${bamboo_buildNumber}"

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

if [ ! $(which bc) ]; then
    echo "bc is not installed. Installing it now"
    sudo apt-get update
    sudo apt-get install -y bc
    if [ ! $(which bc) ]; then
        echo "Failed to install bc"
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
        \"run_token\": \"TEST_RUN_EX\",
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

# Function used to create JUnit test report file
create_junit_header()
{
    echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > ${JUNIT_TEST_REPORT_FILE}
    echo "<testsuites>" >> ${JUNIT_TEST_REPORT_FILE}
}

# Function used to open a new test suite in the test report file
add_new_junit_suite()
{
    suite_name=$1
    total_tests=$2
    failed_tests=$3
    skipped_tests=$4
    timestamp=$5
    total_duration=$6

    echo -n "  <testsuite name=\"${suite_name}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo -n "tests=\"${total_tests}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo -n "failures=\"${failed_tests}\" " >> ${JUNIT_TEST_REPORT_FILE} 
    echo -n "skipped=\"${skipped_tests}\" " >> ${JUNIT_TEST_REPORT_FILE} 
    echo "timestamp=\"${timestamp}\" time=\"${total_duration}\">" >> ${JUNIT_TEST_REPORT_FILE}
}

# Function used to add a new passed test case to the test report file
add_new_junit_pass_test()
{
    classname=$1
    test_name=$2
    test_duration=$3

    echo -n "    <testcase classname=\"${classname}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo -n "name=\"${test_name}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo "time=\"${test_duration}\"/>" >> ${JUNIT_TEST_REPORT_FILE}
}

# Function used to add a new failed test case to the test report file
add_new_junit_fail_test()
{
    classname=$1
    test_name=$2
    failure_message=$3
    failure_type=$4
    test_duration=$5
    log_file_path=$6

    echo -n "    <testcase classname=\"${classname}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo -n "name=\"${test_name}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo "time=\"${test_duration}\">" >> ${JUNIT_TEST_REPORT_FILE}
    echo -n "      <failure message=\"${failure_message}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo "type=\"${failure_type}\">" >> ${JUNIT_TEST_REPORT_FILE}
    if [ -f ${log_file_path} ]; then
        echo "        <![CDATA[$(cat ${log_file_path})]]>" >> ${JUNIT_TEST_REPORT_FILE}
    else
        echo "        <![CDATA[${failure_message}]]>" >> ${JUNIT_TEST_REPORT_FILE}
    fi
    echo "      </failure>" >> ${JUNIT_TEST_REPORT_FILE}
    echo "    </testcase>" >> ${JUNIT_TEST_REPORT_FILE}
}

# Function used to add a new skipped test case to the test report file
add_new_junit_skipped_test()
{
    classname=$1
    test_name=$2
    test_duration=$3

    echo -n "    <testcase classname=\"${classname}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo -n "name=\"${test_name}\" " >> ${JUNIT_TEST_REPORT_FILE}
    echo "time=\"${test_duration}\">" >> ${JUNIT_TEST_REPORT_FILE}
    echo "      <skipped/>" >> ${JUNIT_TEST_REPORT_FILE}
    echo "    </testcase>" >> ${JUNIT_TEST_REPORT_FILE}
}

# Function used to close a test suite in the test report file
close_junit_suite()
{
    echo "  </testsuite>"  >> ${JUNIT_TEST_REPORT_FILE}
}

# Function used to close a JUnit test report file
close_junit_file()
{
    echo "</testsuites>"  >> ${JUNIT_TEST_REPORT_FILE}
}

# Function used to create MD test report file
create_md_header()
{
    if echo "${bamboo_planKey}" | grep "VTEC-MSVD\|VTEC-MCUSDKV"; then
        echo "# 🧪 DAPENG Test Report (kex build) 📊" > ${DAPENG_TEST_REPORT_MD_FILE}
    else
        echo "# 🧪 DAPENG Test Report (west build) 📊" > ${DAPENG_TEST_REPORT_MD_FILE}
    fi
    echo "" >> ${DAPENG_TEST_REPORT_MD_FILE}
    echo "|  Board  | ✅ Passed | ❌ Failed | ⏭️ Skipped | Total | Nexus binaries |" >> ${DAPENG_TEST_REPORT_MD_FILE}
    echo "|---------------|--------|--------|---------|-------|---------------|" >> ${DAPENG_TEST_REPORT_MD_FILE}
}

# Add new line in the MD test report table
add_new_md_entry()
{
    board_name=$1
    n_passed=$2
    n_failed=$3
    n_skipped=$4
    n_total=$5

    binaries_link="${NEXUS_DIR_LINK}%2Fbuild_${board_name}"

    echo "| ${board_name} | ${n_passed} | ${n_failed} | ${n_skipped} | ${n_total} | [nexus_${board_name}](${binaries_link}) |" >> ${DAPENG_TEST_REPORT_MD_FILE}
}

# Function used to close a MD test report file
close_md_file()
{
    job_url=$1

    echo "" >> ${DAPENG_TEST_REPORT_MD_FILE}
    echo "### [🔗 Full report on DAPENG](${job_url})" >> ${DAPENG_TEST_REPORT_MD_FILE}
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
dapeng_job_url_line=$(cat ${DAPENG_TMP_LOG_FILE} | grep "Dapeng Job URL")
dapeng_job_url_clean=$(echo "${dapeng_job_url_line}" | sed -r 's/\x1B\[[0-9;]*[mK]//g')
dapeng_job_url=$(echo "${dapeng_job_url_clean}" | grep -oE 'https?://[^ ]+')
dapeng_task_id=$(echo "${dapeng_job_url_line}" | cut -d "/" -f 6)
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

# Start the JUnit test report file
create_junit_header

# Create the MD report file
create_md_header

# Check the exit code for dapeng new command
if [[ "${dapeng_exit_code}" != 0 || "${n_na_tests}" != "0" ]]; then
    # If exit code is not 0, it means test job failed
    n_total_failed_tests=$(($n_failed_tests+$n_na_tests))
    echo "${n_total_failed_tests} test(s) FAILED"
    echo "Failed tests:"
fi

# Check which tests failed
task_output_path="${OUTPUT_DIR}/download_${dapeng_task_id}"
for board_log in ${task_output_path}/*
do
    board_output_path="${board_log}/eiq_examples"
    board_name=$(echo "${board_log}" | awk -F '/' '{print $NF}')
    board_total_tests=0
    board_passed_tests=0
    board_failed_tests=0
    board_skipped_tests=0
    suite_timestamp=""
    total_suite_duration="0.0"

    # Loop through all test for current board to check the status of the tests
    for test_log in ${board_output_path}/*
    do
        test_name=$(echo "${test_log}" | awk -F '/' '{print $NF}')
        log_output_path="${test_log}/${COMPILER}/${BUILD_CFG}"
        
        # Get the first timestamp from the log file; this will be the suitetimestamp
        if [ -f "${log_output_path}/app_test.log" ]; then
            crt_timestamp=$(grep -a -m 1 -oP '^\[\K[0-9]{2}-[0-9]{2}-[0-9]{4} [0-9]{2}:[0-9]{2}:[0-9]{2}(?= [^]]+\])' "${log_output_path}/app_test.log")
            crt_timestamp=$(echo "$crt_timestamp" | awk -F'[- :]' '{printf "%04d-%02d-%02dT%02d:%02d:%02d", $3, $2, $1, $4, $5, $6}')
            crt_duration=$(grep -a -m 1 -oP 'TASK DURATION: \K[0-9]+\.[0-9]+' ${log_output_path}/app_test.log)
        else
            crt_timestamp=$(date +"%Y-%m-%dT%H:%M:%S")
            crt_duration="0.0"
        fi
        if [[ "${suite_timestamp}" == "" ]]; then
            suite_timestamp=${crt_timestamp}
        else
            suite_timestamp_ep=$(date -d "$suite_timestamp" +%s)
            crt_timestamp_ep=$(date -d "$crt_timestamp" +%s)
            if (( crt_timestamp_ep < suite_timestamp_ep )); then
                suite_timestamp=${crt_timestamp}
            fi
        fi

        # Compute total duration of the suite
        total_suite_duration=$(echo "${total_suite_duration} + ${crt_duration}" | bc)

        # If the file runresult_Fail.txt exists, it means test failed
        if [ -f "${log_output_path}/runresult_Fail.txt" ]; then
            echo "    ${test_name} - board ${board_name}" 
            echo "          --> console log: ${log_output_path}/app_test.log"
            board_failed_tests=$(($board_failed_tests+1))
        # If the file runresult_NA.txt exists, it means no board available to run the test
        elif [ -f "${log_output_path}/runresult_NA.txt" ]; then
            echo "    ${test_name} - board ${board_name}" 
            echo "          --> No board available for this test"
            board_skipped_tests=$(($board_skipped_tests+1))
        # If the file runresult_Not Support.txt exists, it means no test script was found for this test
        elif [ -f "${log_output_path}/runresult_Not Support.txt" ]; then
            echo "    ${test_name} - board ${board_name}" 
            echo "          --> No test script found for this example"
            echo "          --> console log: ${log_output_path}/app_test.log"
            board_skipped_tests=$(($board_skipped_tests+1))
        else
            board_passed_tests=$(($board_passed_tests+1))
        fi
        board_total_tests=$(($board_total_tests+1))
    done

    # Add new entry in the MD report file
    add_new_md_entry ${board_name} ${board_passed_tests} ${board_failed_tests} ${board_skipped_tests} ${board_total_tests}

    # Fill the JUnit test report
    # Add a new suite per board
    add_new_junit_suite ${board_name} ${board_total_tests} ${board_failed_tests} ${board_skipped_tests} ${suite_timestamp} ${total_suite_duration}
    for test_log in ${board_output_path}/*
    do
        test_name=$(echo "${test_log}" | awk -F '/' '{print $NF}')
        log_output_path="${test_log}/${COMPILER}/${BUILD_CFG}"
        if [ -f "${log_output_path}/app_test.log" ]; then
            crt_duration=$(grep -a -m 1 -oP 'TASK DURATION: \K[0-9]+\.[0-9]+' ${log_output_path}/app_test.log)
        else
            crt_duration="0.0"
        fi
        # If the file runresult_Fail.txt exists, it means test failed
        if [ -f "${log_output_path}/runresult_Fail.txt" ]; then
            add_new_junit_fail_test ${board_name} ${test_name} "Test failed. Check the log file ${log_output_path}/app_test.log" "generic test failure" ${crt_duration} "${log_output_path}/app_test.log"
        # If the file runresult_NA.txt exists, it means no board available to run the test
        elif [ -f "${log_output_path}/runresult_NA.txt" ]; then
            add_new_junit_skipped_test ${board_name} ${test_name} ${crt_duration}
        # If the file runresult_Not Support.txt exists, it means no test script was found for this test
        elif [ -f "${log_output_path}/runresult_Not Support.txt" ]; then
            add_new_junit_skipped_test ${board_name} ${test_name} ${crt_duration}
        else
            add_new_junit_pass_test ${board_name} ${test_name} ${crt_duration}
        fi
    done
    # Close the test suite
    close_junit_suite
done

# Close the JUnit test report file
close_junit_file

# Close MD test report file
close_md_file ${dapeng_job_url}

if [[ "${dapeng_exit_code}" != 0 || "${n_na_tests}" != "0" ]]; then
    # Move json config file and output log file to test results directory
    mv ${DAPENG_TMP_LOG_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
    mv ${JSON_CONFIG_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
    mv ${JUNIT_TEST_REPORT_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
    cp ${DAPENG_TEST_REPORT_MD_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
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
        rm ${JUNIT_TEST_REPORT_FILE}
        rm ${DAPENG_TEST_REPORT_MD_FILE}
    else
        # Move json config file and output log file to test results directory
        mv ${DAPENG_TMP_LOG_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
        mv ${JSON_CONFIG_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
        mv ${JUNIT_TEST_REPORT_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
        cp ${DAPENG_TEST_REPORT_MD_FILE} ${OUTPUT_DIR}/download_${dapeng_task_id}/
        tar zcf ${OUTPUT_DIR}/test_results_${dapeng_task_id}.tar.gz ${OUTPUT_DIR}/download_${dapeng_task_id}/*
        echo "Check ${OUTPUT_DIR}/download_${dapeng_task_id} directory for full logs"
    fi
fi
