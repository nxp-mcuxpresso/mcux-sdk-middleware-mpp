#!/bin/bash

# Exit on error
set -e

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
# SDK REMOVE: remove the entire sdk folder and re-install a fresh copy of sdk
SDK_REMOVE="false"
# Exclude repos from west update command
SDK_EXCLUDE=""
# Include repos in west update command
SDK_INCLUDE=""
# Flag to exclude mpp repo from SDK update
MPP_INCLUDE="false"

checkout_revision()
{
    revision=$1
    # Check if input is a tag, local branch, remote branch or a git commit hash
    if [[ `git show-ref --verify refs/tags/${SDK_VERSION} 2> /dev/null` != "" ]]; then
        echo "checkout tag ${SDK_VERSION}"
        git checkout ${SDK_VERSION};
    elif [[ `git show-ref --verify refs/heads/${SDK_VERSION} 2> /dev/null` != "" ]]; then
        echo "checkout local branch ${SDK_VERSION}"
        git checkout ${SDK_VERSION}
        echo "reset local branch to remote branch origin/${SDK_VERSION}"
        git reset --hard origin/${SDK_VERSION}
    elif [[ `git show-ref --verify refs/remotes/origin/${SDK_VERSION} 2> /dev/null` != "" ]]; then
        echo "checkout remote branch ${SDK_VERSION}"
        git checkout ${SDK_VERSION}
    elif [[ `git rev-parse --verify "${SDK_VERSION}^{commit}" 2> /dev/null` != "" ]]; then
        echo "checkout commit ${SDK_VERSION}"
        git checkout ${SDK_VERSION}
    else
        # This is an error case
        echo "Could not find revision ${SDK_VERSION}"
        set +e
        exit 1
    fi
}

create_symbolic_links()
{
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
}

remove_symbolic_links()
{
    # Remove symbolic links before running west update command again in an already cloned sdk
    # This is needed to avoid git fetch conflicts
    echo "Removing symbolic links..."
    for board in `ls ${MPP_DIR}/boards`; do
        # this boards are not supported by the SDK anymore
        if [ ${board} == "evkmimxrt1170" -o  ${board} == "mcxn9xxbrk" -o ${board} == "mcxn9xxevk" -o  ${board} == "evkbimxrt1050" ] ; then
            continue
        fi
        rm -rf "${SDK_INSTALL_DIRNAME}/mcuxsdk/examples/_boards/${board}/eiq_examples/mpp"
    done
    rm -rf ${SDK_INSTALL_DIRNAME}/mcuxsdk/examples/eiq_examples/mpp
    rm -rf ${SDK_INSTALL_DIRNAME}/mcuxsdk/middleware/eiq/mpp
    echo "Symbolic links removed"
}

west_update()
{
    # Run west update command with the provided input
    if [[ "${SDK_EXCLUDE}" != "" ]]; then
        west update `grep -v "${SDK_EXCLUDE}" ${MPP_DIR}/sdk/${SDK_COMP_FILE} | tr '\n' ' '` ${SDK_INCLUDE}
    else
        west update `tr '\n' ' ' < ${MPP_DIR}/sdk/${SDK_COMP_FILE}` ${SDK_INCLUDE}
    fi
}

usage()
{
    echo "usage:"
    echo "$0 [-d:v:c:re:i:mh?]"
    echo " -h|?: help"
    echo " -d <dir name>: name of the directory to install sdk"
    echo "    the directory will be created in the same folder with the mpp dir"
    echo "    default value is $SDK_INSTALL_DIRNAME"
    echo " -v <version>: version of the SDK to be installed"
    echo "    default value is $SDK_VERSION"
    echo " -c <file_name>: name of the file with sdk components to be installed"
    echo "    file should be placed inside sdk folder from mpp dir"
    echo "    default value is $SDK_COMP_FILE"
    echo " -r if the provided sdk folder exists, remove it and"
    echo "    clone the whole sdk"
    echo "    default value is $SDK_REMOVE"
    echo " -e <repos_list> list of repos to exclude from west update command separated by space"
    echo "    The repos are excluded only if they are in the sdk components file provided with -c option"
    echo "    default value is $SDK_EXCLUDE"
    echo " -i <repos_list> list of repos to include in west update command separated by space"
    echo "    specify repos that are not in the list provided with -c option"
    echo "    check all available options by running west list inside sdk directory"
    echo "    default value is $SDK_EXCLUDE"
    echo " -m include mpp in west update command"
    echo "    MPP repo is added to the list of repos to be update via west update command"
    echo "    default value is $MPP_INCLUDE"
    set +e
    exit 0
}

#parse arguments
OPTIND=1
while getopts "d:v:c:re:i:mh?" opt; do
    case "$opt" in
    d)  SDK_INSTALL_DIRNAME=$OPTARG
        ;;
    v)  SDK_VERSION=$OPTARG
        ;;
    c)  SDK_COMP_FILE=$OPTARG
        ;;
    r)  SDK_REMOVE="true"
        ;;
    e)  SDK_EXCLUDE=$OPTARG
        ;;
    i)  SDK_INCLUDE=$OPTARG
        ;;
    m)  MPP_INCLUDE="true"
        ;;
    h|\?)
        usage
        ;;
    esac
done

if [[ "${MPP_INCLUDE}" != "true" ]]; then
    SDK_EXCLUDE="${SDK_EXCLUDE} mpp"
fi

if [[ "${SDK_EXCLUDE}" != "" ]]; then
    SDK_EXCLUDE=$(echo "${SDK_EXCLUDE}" | xargs | sed 's/ /\\|/g')
    echo "Components to be excluded from west update command:"
    grep "${SDK_EXCLUDE}" ${MPP_DIR}/sdk/${SDK_COMP_FILE}
fi

echo "Components to be included in west update command:"
grep -v "${SDK_EXCLUDE}" ${MPP_DIR}/sdk/${SDK_COMP_FILE}
if [[ "${SDK_INCLUDE}" != "" ]]; then
    echo ${SDK_INCLUDE} | sed 's/ /\n/g'
fi

#install sdk
cd ..
echo "Installing SDK at location ${PWD}/${SDK_INSTALL_DIRNAME}, version ${SDK_VERSION}"
if [ -d "${SDK_INSTALL_DIRNAME}" ]; then
    if [[ "${SDK_REMOVE}" == "true" ]]; then
        echo "Directory ${SDK_INSTALL_DIRNAME} already exists. Removing it.."
        rm -rf ${SDK_INSTALL_DIRNAME}
    else
        echo "Directory ${SDK_INSTALL_DIRNAME} already exists. Updating already installed sdk to the selected version"
        remove_symbolic_links

        cd ${SDK_INSTALL_DIRNAME}
        # Run git clean -fdx command for all repos
        # WARNING: These commands will discard any changes in SDK repos
        west forall -c "git reset --hard"
        west forall -c "git clean -fdx"

        # Update manifests repo to the configured version
        cd manifests
        git fetch --prune --tags --all
        checkout_revision ${SDK_VERSION}

        # Run west update
        cd ..
        west_update
        cd ${MPP_DIR}

        # Create symbolic links to mpp repo
        create_symbolic_links

        cd ${CRT_DIR}
        set +e
        exit 0
    fi
fi

# Initialize west and bifrost
git config --global url.ssh://git@bitbucket.sw.nxp.com/mcucore/bifrost.git.insteadOf https://github.com/nxp-zephyr/bifrost
west init -m ssh://git@bitbucket.sw.nxp.com/mcucore/mcuxsdk-manifests.git ${SDK_INSTALL_DIRNAME} --mr ${SDK_VERSION}
cd ${SDK_INSTALL_DIRNAME}
west update bifrost
west config commands.allow_extensions true
west sdk_init

# Run west update
west_update
cd ${MPP_DIR}

# Create symbolic links to mpp repo
create_symbolic_links

cd ${CRT_DIR}

set +e
