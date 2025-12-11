#!/bin/bash

# Exit on error
set -e

# get the path to the MPP_DIR, independent of the
# location from which the script is called
MPP_DIR=$(dirname $(readlink -f $0 | xargs dirname))
CRT_DIR=${PWD}
cd $MPP_DIR

# Set default variables based on MPP_DIR location
# Check if MPP_DIR is inside an SDK installation
if [[ "${MPP_DIR}" == *"mcuxsdk/middleware/eiq"* ]]; then
    # Extract the SDK directory path (parent of mcuxsdk)
    SDK_INSTALL_DIRNAME=$(echo "${MPP_DIR}" | sed 's|/mcuxsdk/middleware/eiq.*||')
    CREATE_SYMLINKS="false"
    MPP_IN_SDK="true"
else
    SDK_INSTALL_DIRNAME=sdk-next
    CREATE_SYMLINKS="true"
    MPP_IN_SDK="false"
fi

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
# Flag to enable git clean -fdx after git reset --hard
GIT_CLEAN="false"
# Flag to only create symbolic links
ONLY_SYMLINKS="false"
# Flag to only remove symbolic links
UNLINK_ONLY="false"
# Flag to enable full SDK update
FULL_SDK_UPDATE="false"

set_components_list()
{
    sym_links_flag=$1

    if [[ "${FULL_SDK_UPDATE}" == "true" ]]; then
        echo "Full SDK update mode enabled"
        return
    fi

    if [[ "${MPP_IN_SDK}" == "false" ]] && [[ "${sym_links_flag}" == "false" ]]; then
        MPP_INCLUDE="true"
    fi

    if [[ "${MPP_INCLUDE}" != "true" ]]; then
        SDK_EXCLUDE="${SDK_EXCLUDE} mpp"
    fi

    if [[ "${SDK_EXCLUDE}" != "" ]]; then
        SDK_EXCLUDE=$(echo "${SDK_EXCLUDE}" | xargs | sed 's/ /\\|/g')
        echo "Components to be excluded from west update command:"
        grep "${SDK_EXCLUDE}" ${MPP_DIR}/sdk/${SDK_COMP_FILE}
    fi

    echo "Components to be included in west update command:"
    if [[ "${SDK_EXCLUDE}" != "" ]]; then
        grep -v "${SDK_EXCLUDE}" ${MPP_DIR}/sdk/${SDK_COMP_FILE}
    else
        cat ${MPP_DIR}/sdk/${SDK_COMP_FILE}
    fi
    if [[ "${SDK_INCLUDE}" != "" ]]; then
        echo ${SDK_INCLUDE} | sed 's/ /\n/g'
    fi
}

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
        rm -rf "${SDK_INSTALL_DIR}/mcuxsdk/examples/_boards/${board}/eiq_examples/mpp"
        ln -s "$PWD/boards/${board}" "${SDK_INSTALL_DIR}/mcuxsdk/examples/_boards/${board}/eiq_examples/mpp"
    done

    # create symbolic link to examples folder from mpp repo
    rm -rf ${SDK_INSTALL_DIR}/mcuxsdk/examples/eiq_examples/mpp
    ln -s "$PWD/examples" ${SDK_INSTALL_DIR}/mcuxsdk/examples/eiq_examples/mpp

    # create symbolic link to mpp repo itself
    rm -rf ${SDK_INSTALL_DIR}/mcuxsdk/middleware/eiq/mpp
    ln -s "$PWD" ${SDK_INSTALL_DIR}/mcuxsdk/middleware/eiq/mpp
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
        rm -rf "${SDK_INSTALL_DIR}/mcuxsdk/examples/_boards/${board}/eiq_examples/mpp"
    done
    rm -rf ${SDK_INSTALL_DIR}/mcuxsdk/examples/eiq_examples/mpp
    rm -rf ${SDK_INSTALL_DIR}/mcuxsdk/middleware/eiq/mpp
    echo "Symbolic links removed"
}

west_update()
{
    # Run west update command with the provided input
    set +e
    if [[ "${FULL_SDK_UPDATE}" == "true" ]]; then
        west update
    elif [[ "${SDK_EXCLUDE}" != "" ]]; then
        west update `grep -v "${SDK_EXCLUDE}" ${MPP_DIR}/sdk/${SDK_COMP_FILE} | tr '\n' ' '` ${SDK_INCLUDE}
    else
        west update `tr '\n' ' ' < ${MPP_DIR}/sdk/${SDK_COMP_FILE}` ${SDK_INCLUDE}
    fi
    WEST_UPDATE_STATUS=$?
    set -e
    return ${WEST_UPDATE_STATUS}
}

usage()
{
    echo "usage:"
    echo "$0 [-s:v:c:re:i:mnxulfh?]"
    echo " -h|?: help"
    echo " -s <dir name>: name of the directory to install sdk"
    echo "    the directory will be created in the same folder with the mpp dir"
    echo "    default value is $SDK_INSTALL_DIRNAME"
    echo " -v <version>: version of the SDK to be installed"
    echo "    default value is $SDK_VERSION"
    echo " -c <file_name>: name of the file with sdk components to be installed"
    echo "    file should be placed inside sdk folder from mpp dir"
    echo "    default value is $SDK_COMP_FILE"
    echo " -r if the provided sdk folder exists, remove it and"
    echo "    clone the whole sdk. Option is ignored when running the script"
    echo "    from MPP repository inside sdk folder"
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
    echo " -n disable creation of symbolic links. This flag is ignored when sdk dir already exists"
    echo "    In this case, the previous state of the symbolic links will be preserved"
    echo " -u only remove symbolic links and update examples and mpp without updating entire SDK"
    echo "    This option requires -s to specify the SDK directory"
    echo "    default value is $UNLINK_ONLY"
    echo " -l only create symbolic links without installing/updating SDK"
    echo "    This option requires -s to specify the SDK directory"
    echo "    default value is $ONLY_SYMLINKS"
    echo " -f enable full SDK update (update all repositories without filtering)"
    echo "    This will run 'west update' without any component filtering"
    echo "    default value is $FULL_SDK_UPDATE"
    echo " -x enable git clean -fdx after git reset --hard"
    echo "    WARNING: This will remove all untracked files and directories"
    echo "    default value is $GIT_CLEAN"
    set +e
    exit 0
}

#parse arguments
OPTIND=1
while getopts "s:v:c:re:i:mnxulfh?" opt; do
    case "$opt" in
    s)  SDK_INSTALL_DIRNAME=$OPTARG
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
    n)  CREATE_SYMLINKS="false"
        ;;
    u)  UNLINK_ONLY="true"
        ;;
    l)  ONLY_SYMLINKS="true"
        ;;
    f)  FULL_SDK_UPDATE="true"
        ;;
    x)  GIT_CLEAN="true"
        ;;
    h|\?)
        usage
        ;;
    esac
done

# Handle -u option (only remove symbolic links and update examples/mpp)
if [ "$UNLINK_ONLY" = "true" ]; then
    if [ "$MPP_IN_SDK" = "true" ]; then
        echo "Error: MPP directory is already inside SDK path. Cannot remove symbolic links."
        echo "MPP_DIR: $MPP_DIR"
        echo "SDK path: $SDK_INSTALL_DIRNAME"
        set +e
        exit 1
    fi

    # Set SDK_INSTALL_DIR based on provided -s option
    SDK_INSTALL_PATH=$(dirname ${MPP_DIR})
    SDK_INSTALL_DIR="${SDK_INSTALL_PATH}/${SDK_INSTALL_DIRNAME}"

    # Check if SDK directory exists
    if [ ! -d "$SDK_INSTALL_DIR" ]; then
        echo "Error: SDK directory does not exist: $SDK_INSTALL_DIR"
        echo "Please install SDK first or provide correct SDK directory with -s option"
        exit 1
        set +e
    fi

    echo "Removing symbolic links from SDK at: $SDK_INSTALL_DIR"
    remove_symbolic_links

    echo "Updating mcu-sdk-examples and mpp middleware after removing the symbolic links"
    STASH_TIMESTAMP=$(date +%H_%M_%S)
    echo "Stashing changes in examples directory with timestamp: $STASH_TIMESTAMP"
    cd $SDK_INSTALL_DIR/mcuxsdk/examples
    git stash push -m "sdk_update_backup_stash_${STASH_TIMESTAMP}" || true
    cd $SDK_INSTALL_DIR
    echo "Warning: Stash sdk_update_backup_stash_${STASH_TIMESTAMP} will not be applied automatically. You can apply it later"
    echo "Updating examples and mpp to the current manifest version using west update"
    west update mcu-sdk-examples mcux-sdk-middleware-mpp

    cd $CRT_DIR
    set +e
    exit 0
fi

# Handle -l option (only create symbolic links)
if [[ "${ONLY_SYMLINKS}" == "true" ]]; then
    # Check if MPP_DIR is inside SDK
    if [[ "${MPP_IN_SDK}" == "true" ]]; then
        echo "Error: MPP directory is already inside SDK path. Cannot create symbolic links."
        echo "MPP_DIR: ${MPP_DIR}"
        echo "SDK path: ${SDK_INSTALL_DIRNAME}"
        set +e
        exit 1
    fi

    # Set SDK_INSTALL_DIR based on provided -s option
    SDK_INSTALL_PATH=$(dirname ${MPP_DIR})
    SDK_INSTALL_DIR="${SDK_INSTALL_PATH}/${SDK_INSTALL_DIRNAME}"

    # Check if SDK directory exists
    if [ ! -d "${SDK_INSTALL_DIR}" ]; then
        echo "Error: SDK directory does not exist: ${SDK_INSTALL_DIR}"
        echo "Please install SDK first or provide correct SDK directory with -s option"
        set +e
        exit 1
    fi

    echo "Creating symbolic links to SDK at: ${SDK_INSTALL_DIR}"
    create_symbolic_links
    cd ${CRT_DIR}
    set +e
    exit 0
fi

#install sdk
if [[ "${MPP_IN_SDK}" == "false" ]]; then
    SDK_INSTALL_PATH=$(dirname ${MPP_DIR})
    SDK_INSTALL_DIR="${SDK_INSTALL_PATH}/${SDK_INSTALL_DIRNAME}"
else
    SDK_INSTALL_PATH=$(dirname ${SDK_INSTALL_DIRNAME})
    SDK_INSTALL_DIR="${SDK_INSTALL_DIRNAME}"
    SDK_REMOVE="false"
fi

cd ${SDK_INSTALL_PATH}
echo "Installing SDK at location ${SDK_INSTALL_DIR}, version ${SDK_VERSION}"
if [ -d "${SDK_INSTALL_DIR}" ]; then
    if [[ "${SDK_REMOVE}" == "true" ]]; then
        echo "Directory ${SDK_INSTALL_DIR} already exists. Removing it.."
        rm -rf ${SDK_INSTALL_DIR}
    else
        echo "Directory ${SDK_INSTALL_DIR} already exists. Updating already installed sdk to the selected version"

        # Check if symbolic links exist before removing them
        SYMLINKS_EXIST="false"
        for board in `ls ${MPP_DIR}/boards`; do
            # this boards are not supported by the SDK anymore
            if [ ${board} == "evkmimxrt1170" -o  ${board} == "mcxn9xxbrk" -o ${board} == "mcxn9xxevk" -o  ${board} == "evkbimxrt1050" ] ; then
                continue
            fi
            if [ -L "${SDK_INSTALL_DIR}/mcuxsdk/examples/_boards/${board}/eiq_examples/mpp" ]; then
                SYMLINKS_EXIST="true"
                break
            fi
        done

        if [ -L "${SDK_INSTALL_DIR}/mcuxsdk/examples/eiq_examples/mpp" ] || [ -L "${SDK_INSTALL_DIR}/mcuxsdk/middleware/eiq/mpp" ]; then
            SYMLINKS_EXIST="true"
        fi

        if [[ "${SYMLINKS_EXIST}" == "true" ]]; then
            remove_symbolic_links
        fi

        # Set the componets to be update via west update command
        set_components_list "${SYMLINKS_EXIST}"

        cd ${SDK_INSTALL_DIR}
        # Run git clean -fdx command for all repos
        # WARNING: These commands will discard any changes in SDK repos (after stash)
        # First, stash any modified changes before resetting with timestamp
        STASH_TIMESTAMP=$(date +%H_%M_%S)
        west forall -c "git stash push -m 'sdk_update_backup_stash_${STASH_TIMESTAMP}' || true"
        west forall -c "git reset --hard"

        # Run git clean -fdx if enabled
        if [[ "${GIT_CLEAN}" == "true" ]]; then
            echo "Running git clean -fdx to remove all untracked files and directories..."
            west forall -c "git clean -fdx"
        fi

        # Update manifests repo to the configured version
        cd manifests
        git fetch --prune --tags --all
        checkout_revision ${SDK_VERSION}

        # Run west update
        cd ..
        west_update
        WEST_STATUS=$?

        # Apply and drop the stash with the timestamp if it exists
        west -q forall -c "( git stash list | grep -q 'sdk_update_backup_stash_${STASH_TIMESTAMP}' && git stash pop \$(git stash list | grep 'sdk_update_backup_stash_${STASH_TIMESTAMP}' | head -1 | cut -d: -f1 )  > /dev/null && echo 'applied sdk_update_backup_stash_${STASH_TIMESTAMP}' ) || true"

        cd ${MPP_DIR}

        # Create symbolic links to mpp repo based on SYMLINKS_EXIST flag
        # If west_update failed, create symlinks if SYMLINKS_EXIST is true
        if [[ "${SYMLINKS_EXIST}" == "true" ]] || [[ ${WEST_STATUS} -ne 0 && "${SYMLINKS_EXIST}" == "true" ]]; then
            create_symbolic_links
        fi

        cd ${CRT_DIR}
        set +e
        exit 0
    fi
fi

# Initialize west and bifrost
git config --global url.ssh://git@bitbucket.sw.nxp.com/mcucore/bifrost.git.insteadOf https://github.com/nxp-zephyr/bifrost
west init -m ssh://git@bitbucket.sw.nxp.com/mcucore/mcuxsdk-manifests.git ${SDK_INSTALL_DIR} --mr ${SDK_VERSION}
cd ${SDK_INSTALL_DIR}
west update bifrost
west config commands.allow_extensions true
west sdk_init

# Set the componets to be update via west update command
set_components_list "${CREATE_SYMLINKS}"

# Run west update
west_update
WEST_STATUS=$?
cd ${MPP_DIR}

# Create symbolic links to mpp repo based on CREATE_SYMLINKS flag
# If west_update failed, create symlinks if CREATE_SYMLINKS is true
if [[ "${CREATE_SYMLINKS}" == "true" ]] || [[ ${WEST_STATUS} -ne 0 && "${CREATE_SYMLINKS}" == "true" ]]; then
    create_symbolic_links
fi

cd ${CRT_DIR}

set +e
