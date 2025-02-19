#!/bin/bash

# get the path to the MPP_DIR, independent of the 
# location from which the script is called
MPP_DIR=$(dirname $(readlink -f $0 | xargs dirname))
CRT_DIR=${PWD}
cd $MPP_DIR

# Set default variables
# this directory will be created in the same folder with the mpp dir
SDK_INSTALL_DIRNAME=sdk-next
SDK_VERSION=main
# the file should be placed inside sdk dir from mpp
SDK_COMP_FILE=sdk_components.txt

usage()
{
    echo "usage:"
    echo "$0 [-d:v:c:h?]"
    echo " -h|?: help"
    echo " -d <dir name>: name of the directory to install sdk"
    echo "    the directory will be created in the same folder with the mpp dir"
    echo "    default value is $SDK_INSTALL_DIRNAME"
    echo " -v <version>: version of the SDK to be installed"
    echo "    default value is $SDK_VERSION"
    echo " -c <file_name>: name of the file with sdk components to be installed"
    echo "    file should be placed inside sdk folder from mpp dir"
    echo "    default value is $SDK_COMP_FILE"
    exit 0
}

#parse arguments
OPTIND=1
while getopts "d:v:c:h?" opt; do
    case "$opt" in
    d)  SDK_INSTALL_DIRNAME=$OPTARG
        ;;
    v)  SDK_VERSION=$OPTARG
        ;;
    c)  SDK_COMP_FILE=$OPTARG
        ;;
    h|\?)
        usage
        ;;
    esac
done

#install sdk
cd ..
echo "Installing SDK at location ${PWD}/${SDK_INSTALL_DIRNAME}, version ${SDK_VERSION}"
if [ -d "${SDK_INSTALL_DIRNAME}" ]; then
    echo "Directory ${SDK_INSTALL_DIRNAME} already exists. Removing it.."
    rm -rf ${SDK_INSTALL_DIRNAME}
fi
git config --global url.ssh://git@bitbucket.sw.nxp.com/mcucore/bifrost.git.insteadOf https://github.com/nxp-zephyr/bifrost
west init -m ssh://git@bitbucket.sw.nxp.com/mcucore/mcuxsdk-manifests.git ${SDK_INSTALL_DIRNAME} --mr ${SDK_VERSION}
cd ${SDK_INSTALL_DIRNAME}
west update bifrost
west config commands.allow_extensions true
west sdk_init
west update `tr '\n' ' ' < ${MPP_DIR}/sdk/${SDK_COMP_FILE}`
cd ${MPP_DIR}

# create symbolink links in sdk dir to all the boards
# defined in mpp repo in the boards directory
echo "Creating symbolic links..."
for board in `ls ${MPP_DIR}/boards`; do
    # this boards are not supported by the SDK anymore
    if [ ${board} == "evkmimxrt1170" -o  ${board} == "mcxn9xxbrk" -o ${board} == "mcxn9xxevk" -o  ${board} == "evkbimxrt1050" ] ; then
        continue
    fi
    # create the symbolic links
    rm -rf "../${SDK_INSTALL_DIRNAME}/mcuxsdk/examples/_boards/${board}/eiq_examples/mpp"
    ln -s "$PWD/boards/${board}" "../${SDK_INSTALL_DIRNAME}/mcuxsdk/examples/_boards/${board}/eiq_examples/mpp"
done

# create symbolic link to examples folder from mpp repo
rm -rf ../${SDK_INSTALL_DIRNAME}/mcuxsdk/examples/eiq_examples/mpp
ln -s "$PWD/examples" ../${SDK_INSTALL_DIRNAME}/mcuxsdk/examples/eiq_examples/mpp

# create symbolic link to mpp repo itself
rm -rf ../${SDK_INSTALL_DIRNAME}/mcuxsdk/middleware/eiq/mpp
ln -s "$PWD" ../${SDK_INSTALL_DIRNAME}/mcuxsdk/middleware/eiq/mpp
echo "Symbolic links created"

cd ${CRT_DIR}
