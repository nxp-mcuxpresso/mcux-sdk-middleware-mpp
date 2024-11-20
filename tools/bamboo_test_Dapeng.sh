#!/bin/bash -ex

# This script is intended to be used by bamboo jobs
# to run Dapeng testing for MPP.

###################
### set environment
###################
echo "Set environment"

# Get time of the build
hours=$(date +%H)

if [[ "${BOARD}" == "" ]]; then
    BOARD=${bamboo_BOARD}
fi
if [[ "${SINGLE_TEST}" == "" ]]; then
    SINGLE_TEST=${bamboo_SINGLE_TEST}
fi
# If ALL_TESTS variable is not defined, it will be set to
# - YES if it is executed during night time
# - NO otherwise
if [[ "${ALL_TESTS}" == "" ]]; then
    if [[ "${bamboo_ALL_TESTS}" == "" ]]; then
        if [ ${hours} -ge 20 ] || [ ${hours} -le 5 ]; then
            ALL_TESTS="YES"
	else
            ALL_TESTS="NO"
	fi
    else
        ALL_TESTS=${bamboo_ALL_TESTS}
    fi
fi
echo "board= ${BOARD}"
echo "single test= ${SINGLE_TEST}"
echo "all tests= ${ALL_TESTS}"

######################################
### deploy binaries from build archive
######################################
TOPDIR=$(pwd)
ARTIFACTS_DIR="/efs/shared/artifacts/${bamboo_planKey}/${bamboo_buildNumber}"
TEST_DIR="dapeng_test_result"

ls -lh ${ARTIFACTS_DIR}
cd "$TOPDIR"/${TEST_DIR}

if [ ! -f "${ARTIFACTS_DIR}/build_${BOARD}.tar.gz" ]; then
    echo "ERROR: Build for ${BOARD} is not available."
    exit 0
fi
tar xvf ${ARTIFACTS_DIR}/build_${BOARD}.tar.gz
BINARY_DIR=`find . -name *.bin -print -quit | xargs dirname`
ls -l

####################
### run dapeng tests
####################
BRANCH_NAME="dev"
if [[ "${bamboo_DAPENG_APP_DATA_BRANCH}" != "" ]]; then
    BRANCH_NAME="${bamboo_DAPENG_APP_DATA_BRANCH}"
fi
TEST_PARAMS=""
if [ ${bamboo_MPP_TEST_ITERATION_CHECK} -ne 0 ]; then
    TEST_PARAMS="@${bamboo_MPP_TEST_ITERATION_CHECK}"
fi

# All Dapeng test should run even if one Dapeng test fails.
# Bamboo plan should not exit if one Dapeng test fails.
if [ ${ALL_TESTS} == "YES" ]; then
    for test_binary in `ls ${BINARY_DIR}/*.bin`; do
        test_binary=${BINARY_DIR}/$(basename ${test_binary} .bin)
        echo "binary=${test_binary}"
        dapeng test -f ${test_binary} -n mpp_auto_tests${TEST_PARAMS} -b ${BOARD} -A ${BRANCH_NAME} -T soplpuats52 --no-routing | tee dapeng.out || true
        touch dapeng_fail_${BOARD}.txt
        grep "Test result: Pass" dapeng.out || echo "$(basename ${test_binary} .bin)" >> dapeng_fail_${BOARD}.txt
    done
else
    test_binary=${BINARY_DIR}/${SINGLE_TEST}
    dapeng test -f ${test_binary} -n mpp_auto_tests${TEST_PARAMS} -b ${BOARD} -A ${BRANCH_NAME} -T soplpuats52 --no-routing | tee dapeng.out || true
    touch dapeng_fail_${BOARD}.txt
    grep "Test result: Pass" dapeng.out || echo "$(basename ${test_binary} .bin)" >> dapeng_fail_${BOARD}.txt
fi
