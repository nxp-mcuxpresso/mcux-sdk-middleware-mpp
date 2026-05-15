#!/usr/bin/env python3

"""
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
"""

"""
NXP MCUXpresso SDK Installation and Update Script

This script automates the installation and management of the NXP MCUXpresso SDK
for use with the MPP (Model Preparation and Packaging) framework.

Main Features:
- Install a fresh SDK from a specified version (tag, branch, or commit)
- Update an existing SDK installation to a different version
- Manage SDK components via west update with include/exclude filters
- Create symbolic links between MPP repository and SDK for development
- Remove symbolic links and update examples/mpp components only
- Support for both standalone MPP and MPP-in-SDK configurations
- Stash and restore local changes during SDK updates
- Clean untracked files with git clean option

Usage Examples:
    # Install SDK with default settings
    python install_sdk.py

    # Install specific SDK version
    python install_sdk.py -v MCUX_2.15.0

    # Update existing SDK excluding certain components
    python install_sdk.py -e "hal_nxp modules/hal/nxp"

    # Create only symbolic links (development mode)
    python install_sdk.py -l

    # Remove symbolic links and update examples/mpp only
    python install_sdk.py -u

    # Remove and reinstall SDK from scratch
    python install_sdk.py -r

    # Update SDK with git clean (removes all untracked files)
    python install_sdk.py -x

Arguments:
    -s, --sdk-dir: SDK installation directory name
    -v, --version: SDK version (tag, branch, or commit hash)
    -c, --components-file: File containing list of SDK components
    -r, --remove: Remove existing SDK and clone fresh copy
    -e, --exclude: Space-separated list of components to exclude
    -i, --include: Space-separated list of components to include
    -m, --mpp-include: Include MPP in west update command
    -n, --no-symlinks: Disable symbolic link creation
    -f, --full-sdk-update: Perform a complete SDK update including all components
    -u, --unlink: Remove symbolic links and update examples/mpp only
    -l, --only-symlinks: Only create symbolic links without SDK update
    -x, --git-clean: Run git clean -fdx to remove untracked files

Dependencies:
    - Python 3.8+
    - Git
    - West (Zephyr's meta-tool)
    - sync_sdk_files module from mpp/tools

Author: NXP Semiconductors
"""

import sys
import os
import argparse
import subprocess
import re
import shutil
import time
from pathlib import Path
from typing import Tuple, List, Optional
from datetime import datetime


def get_mpp_dir() -> Path:
    """Get the MPP_DIR path, independent of the location from which the script is called."""
    return Path(__file__).resolve().parent.parent


def is_in_mpp_sdk_path(mpp_dir: Path) -> bool:
    """Check if MPP directory is inside an SDK path structure."""
    # Check if mpp_dir is inside mcuxsdk/middleware/eiq/mpp
    mpp_parts = mpp_dir.parts
    if len(mpp_parts) >= 4:
        # Check if path ends with mcuxsdk/middleware/eiq/mpp
        if (mpp_parts[-4] == "mcuxsdk" and
            mpp_parts[-3] == "middleware" and
            mpp_parts[-2] == "eiq" and
                mpp_parts[-1] == "mpp"):
            return True
    return False


def get_sdk_root_path(mpp_dir: Path) -> Path:
    """Get SDK root path from MPP directory if MPP is inside SDK."""
    # If mpp_dir is mcuxsdk/middleware/eiq/mpp, return the SDK root (4 levels up)
    if is_in_mpp_sdk_path(mpp_dir):
        return mpp_dir.parent.parent.parent.parent
    return mpp_dir


def run_command(cmd: str | List[str], cwd: Optional[Path] = None, check: bool = True, shell: bool = False, get_output: bool = False) -> Tuple[int, str, str]:
    """Run a command and return the result."""
    if isinstance(cmd, List):
        cmd = [c for c in cmd if c != ""]

    if isinstance(cmd, str) and not shell:
        cmd = cmd.split()

    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            check=check,
            capture_output=get_output,
            text=True,
            shell=shell
        )
        if get_output:
            return result.returncode, result.stdout, result.stderr
        else:
            return result.returncode, "", ""
    except subprocess.CalledProcessError as e:
        return e.returncode, e.stdout, e.stderr


def git_show_ref(ref: str, cwd: Path) -> bool:
    """Check if a git reference exists."""
    returncode, stdout, _ = run_command(
        f"git show-ref --verify {ref}",
        cwd=cwd,
        check=False,
        get_output=True
    )
    return returncode == 0 and stdout.strip() != ""


def git_rev_parse(rev: str, cwd: Path) -> bool:
    """Check if a git revision exists."""
    returncode, stdout, _ = run_command(
        f"git rev-parse --verify {rev}^{{commit}}",
        cwd=cwd,
        check=False,
        get_output=True
    )
    return returncode == 0


def checkout_revision(sdk_version: str, cwd: Path) -> None:
    """Checkout the specified SDK revision."""
    # Check if input is a tag, local branch, remote branch or a git commit hash
    if git_show_ref(f"refs/tags/{sdk_version}", cwd):
        print(f"checkout tag {sdk_version}")
        run_command(f"git checkout {sdk_version}", cwd=cwd)
    elif git_show_ref(f"refs/heads/{sdk_version}", cwd):
        print(f"checkout local branch {sdk_version}")
        run_command(f"git checkout {sdk_version}", cwd=cwd)
        print(f"reset local branch to remote branch origin/{sdk_version}")
        run_command(f"git reset --hard origin/{sdk_version}", cwd=cwd)
    elif git_show_ref(f"refs/remotes/origin/{sdk_version}", cwd):
        print(f"checkout remote branch {sdk_version}")
        run_command(f"git checkout {sdk_version}", cwd=cwd)
    elif git_rev_parse(sdk_version, cwd):
        print(f"checkout commit {sdk_version}")
        run_command(f"git checkout {sdk_version}", cwd=cwd)
    else:
        print(f"Could not find revision {sdk_version}")
        sys.exit(1)


def set_components_list(mpp_dir: Path, sdk_comp_file: str, sdk_exclude: str, sdk_include: str, mpp_include: bool, sym_links_flag: bool, mpp_in_sdk: bool) -> Tuple[List[str], bool]:
    """Set the components list for west update command."""
    if not mpp_in_sdk and not sym_links_flag:
        mpp_include = True

    exclude_list = sdk_exclude.split() if sdk_exclude else []

    if not mpp_include:
        exclude_list.append("mpp")

    # Read components from file
    comp_file_path = mpp_dir / "sdk" / sdk_comp_file
    with open(comp_file_path, 'r') as f:
        all_components = [line.strip() for line in f if line.strip()]

    # Filter components
    if exclude_list:
        exclude_pattern = '|'.join(exclude_list)
        components_to_update = [
            c for c in all_components if not re.search(exclude_pattern, c)]
        excluded_components = [
            c for c in all_components if re.search(exclude_pattern, c)]

        if excluded_components:
            print("Components to be excluded from west update command:")
            for comp in excluded_components:
                print(comp)
    else:
        components_to_update = all_components

    print("Components to be included in west update command:")
    for comp in components_to_update:
        print(comp)

    if sdk_include:
        include_list = sdk_include.split()
        print('\n'.join(include_list))
        components_to_update.extend(include_list)

    return components_to_update, mpp_include


def create_symbolic_links(mpp_dir: Path, sdk_install_dir: Path) -> None:
    """Create symbolic links in SDK directory to MPP boards and examples."""
    print("Creating symbolic links...")

    boards_dir = mpp_dir / "boards"
    excluded_boards = ["evkmimxrt1170",
                       "mcxn9xxbrk", "mcxn9xxevk", "evkbimxrt1050"]

    for board in os.listdir(boards_dir):
        if board in excluded_boards:
            continue
        else:
            print(f"CREATED SL 4 {board}")

        board_path = boards_dir / board
        if not board_path.is_dir():
            continue

        # Create symbolic link for board
        link_path = sdk_install_dir / "mcuxsdk" / "examples" / \
            "_boards" / board / "eiq_examples" / "mpp"
        if link_path.exists() or link_path.is_symlink():
            if link_path.is_symlink():
                link_path.unlink()
            elif link_path.is_dir():
                shutil.rmtree(link_path)

        link_path.parent.mkdir(parents=True, exist_ok=True)

        # Create symlink (works on both Windows and Linux with Python 3.8+)
        try:
            link_path.symlink_to(board_path, target_is_directory=True)
        except OSError as e:
            print(f"Warning: Could not create symlink for {board}: {e}")
            print(
                "On Windows, you may need to run as Administrator or enable Developer Mode")

    # Create symbolic link to examples folder
    examples_link = sdk_install_dir / "mcuxsdk" / "examples" / "eiq_examples" / "mpp"
    if examples_link.exists() or examples_link.is_symlink():
        if examples_link.is_symlink():
            examples_link.unlink()
        elif examples_link.is_dir():
            shutil.rmtree(examples_link)

    examples_link.parent.mkdir(parents=True, exist_ok=True)
    try:
        examples_link.symlink_to(mpp_dir / "examples",
                                 target_is_directory=True)
    except OSError as e:
        print(f"Warning: Could not create symlink for examples: {e}")

    # Create symbolic link to mpp repo itself
    mpp_link = sdk_install_dir / "mcuxsdk" / "middleware" / "eiq" / "mpp"
    if mpp_link.exists() or mpp_link.is_symlink():
        if mpp_link.is_symlink():
            mpp_link.unlink()
        elif mpp_link.is_dir():
            shutil.rmtree(mpp_link)

    mpp_link.parent.mkdir(parents=True, exist_ok=True)
    try:
        mpp_link.symlink_to(mpp_dir, target_is_directory=True)
    except OSError as e:
        print(f"Warning: Could not create symlink for mpp: {e}")

    print("Symbolic links created")


def remove_symbolic_links(mpp_dir: Path, sdk_install_dir: Path) -> None:
    """Remove symbolic links before running west update command."""
    print("Removing symbolic links...")

    boards_dir = mpp_dir / "boards"
    excluded_boards = ["evkmimxrt1170",
                       "mcxn9xxbrk", "mcxn9xxevk", "evkbimxrt1050"]

    for board in os.listdir(boards_dir):
        if board in excluded_boards:
            continue

        link_path = sdk_install_dir / "mcuxsdk" / "examples" / \
            "_boards" / board / "eiq_examples" / "mpp"
        if link_path.exists() and link_path.is_symlink():
            link_path.unlink()

    # Remove examples link
    examples_link = sdk_install_dir / "mcuxsdk" / "examples" / "eiq_examples" / "mpp"
    if examples_link.exists() and examples_link.is_symlink():
        examples_link.unlink()

    # Remove mpp link
    mpp_link = sdk_install_dir / "mcuxsdk" / "middleware" / "eiq" / "mpp"
    if mpp_link.exists() and mpp_link.is_symlink():
        mpp_link.unlink()

    print("Symbolic links removed")


def check_symlinks_exist(mpp_dir: Path, sdk_install_dir: Path) -> bool:
    """Check if symbolic links exist."""
    boards_dir = mpp_dir / "boards"
    excluded_boards = ["evkmimxrt1170",
                       "mcxn9xxbrk", "mcxn9xxevk", "evkbimxrt1050"]

    for board in os.listdir(boards_dir):
        if board in excluded_boards:
            continue

        link_path = sdk_install_dir / "mcuxsdk" / "examples" / \
            "_boards" / board / "eiq_examples" / "mpp"
        if link_path.is_symlink():
            return True

    examples_link = sdk_install_dir / "mcuxsdk" / "examples" / "eiq_examples" / "mpp"
    mpp_link = sdk_install_dir / "mcuxsdk" / "middleware" / "eiq" / "mpp"

    return examples_link.is_symlink() or mpp_link.is_symlink()


def west_update(components: List[str], sdk_install_dir: Path, disable_fast_update: bool = False) -> int:
    """Run west update command with the provided components."""
    if disable_fast_update:
        cmd = ["west", "update"] + components
    else:
        cmd = ["west", "update", "-n", "-o=--depth=1"] + components

    start_time = time.time()

    returncode, _, _ = run_command(cmd, cwd=sdk_install_dir, check=False)

    print(f"West update completed in {time.time() - start_time:.2f} seconds")

    return returncode


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Install or update NXP MCUXpresso SDK",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # Get MPP directory
    mpp_dir = get_mpp_dir()
    crt_dir = Path.cwd()

    # Set default variables based on MPP_DIR location
    mpp_in_sdk = is_in_mpp_sdk_path(mpp_dir)

    if mpp_in_sdk:
        sdk_install_dirname = str(get_sdk_root_path(mpp_dir))
        default_create_symlinks = False
    else:
        sdk_install_dirname = "sdk-next"
        default_create_symlinks = True

    parser.add_argument("-s", "--sdk-dir", default=sdk_install_dirname,
                        help=f"Name of the directory to install SDK (default: {sdk_install_dirname}). The folder will be created in the same folder as MPP directory.")
    parser.add_argument("-v", "--version", default="main",
                        help="Version of the SDK to be installed (default: main)")
    parser.add_argument("-c", "--components-file", default="sdk_components.txt",
                        help="Name of the file with SDK components to be installed (default: sdk_components.txt)")
    parser.add_argument("-r", "--remove", action="store_true",
                        help="Remove existing SDK folder and clone fresh copy")
    parser.add_argument("-e", "--exclude", default="",
                        help="List of repos to exclude from west update command (space-separated)")
    parser.add_argument("-i", "--include", default="",
                        help="List of repos to include in west update command (space-separated)")
    parser.add_argument("-m", "--mpp-include", action="store_true",
                        help="Include MPP in west update command")
    parser.add_argument("-f", "--full-sdk-update", action="store_true",
                        help="Perform a full SDK update including all components. -i, -e and -m flags are ignored")
    parser.add_argument("-n", "--no-symlinks", action="store_true",
                        help="Disable creation of symbolic links\n"
                        "This flag is ignored when sdk dir already exists\n"
                        "In this case, the previous state of the symbolic links will be preserved")
    parser.add_argument("-u", "--unlink", action="store_true",
                        help="Only remove symbolic links and update examples and mpp without updating entire SDK")
    parser.add_argument("-l", "--link", action="store_true",
                        help="Only create symbolic links without installing/updating SDK")
    parser.add_argument("-x", "--git-clean", action="store_true",
                        help="Enable git clean -fdx after git reset --hard (WARNING: removes all untracked files)")
    parser.add_argument("--disable-fast-update", action="store_true",
                        help="Disable fast update mode (west update with depth=1)")

    args = parser.parse_args()

    # Process arguments
    sdk_version = args.version
    sdk_comp_file = args.components_file
    sdk_remove = args.remove and not mpp_in_sdk  # Ignore remove flag when in SDK
    sdk_exclude = args.exclude
    sdk_include = args.include
    mpp_include = args.mpp_include
    create_symlinks = default_create_symlinks and not args.no_symlinks
    only_symlinks = args.link or args.unlink
    git_clean = args.git_clean
    sdk_install_dirname = args.sdk_dir
    full_sdk_update = args.full_sdk_update

    # Handle -l option (only create symbolic links)
    if only_symlinks:
        if mpp_in_sdk:
            print(
                "Error: MPP directory is already inside SDK path. Cannot create or remove symbolic links.")
            print(f"MPP_DIR: {mpp_dir}")
            print(f"SDK path: {sdk_install_dirname}")
            sys.exit(1)

        # Set SDK_INSTALL_DIR based on provided -s option
        sdk_install_path = mpp_dir.parent
        sdk_install_dir = sdk_install_path / sdk_install_dirname

        # Check if SDK directory exists
        if not sdk_install_dir.exists():
            print(f"Error: SDK directory does not exist: {sdk_install_dir}")
            print(
                "Please install SDK first or provide correct SDK directory with -s option")
            sys.exit(1)

        if args.link:
            print(f"Creating symbolic links to SDK at: {sdk_install_dir}")
            create_symbolic_links(mpp_dir, sdk_install_dir)
        if args.unlink:
            print(f"Removing symbolic links from SDK at: {sdk_install_dir}")
            remove_symbolic_links(mpp_dir, sdk_install_dir)
            print(
                "Updating mcu-sdk-examples and mpp middleware after removing the symbolic links")
            stash_timestamp = datetime.now().strftime("%H_%M_%S")
            print(
                f"Stashing changes in examples directory with timestamp: {stash_timestamp}")
            run_command(
                f"git stash push -m 'sdk_update_backup_stash_{stash_timestamp}' || true",
                cwd=sdk_install_dir / "mcuxsdk" / "examples",
                shell=True
            )
            print(
                f"Warning: Stash sdk_update_backup_stash_{stash_timestamp} will not be applied automatically. You can apply it later")
            print(
                "Updating examples and mpp to the current manifest version using west update")
            west_update(["mcu-sdk-examples", "mcux-sdk-middleware-mpp"],
                        sdk_install_dir, args.disable_fast_update)

        os.chdir(crt_dir)
        sys.exit(0)

    # Install SDK
    if not mpp_in_sdk:
        sdk_install_path = mpp_dir.parent
        sdk_install_dir = sdk_install_path / sdk_install_dirname
    else:
        sdk_install_path = Path(sdk_install_dirname).parent
        sdk_install_dir = Path(sdk_install_dirname)
        sdk_remove = False

    os.chdir(sdk_install_path)
    print(
        f"Installing SDK at location {sdk_install_dir}, version {sdk_version}")

    if sdk_install_dir.exists():
        if sdk_remove:
            print(f"Directory {sdk_install_dir} already exists. Removing it..")
            shutil.rmtree(sdk_install_dir)
        else:
            print(
                f"Directory {sdk_install_dir} already exists. Updating already installed sdk to the selected version")

            # Check if symbolic links exist before removing them
            symlinks_exist = check_symlinks_exist(mpp_dir, sdk_install_dir)

            if symlinks_exist:
                remove_symbolic_links(mpp_dir, sdk_install_dir)

            # Set the components to be updated via west update command
            if not full_sdk_update:
                components_to_update, mpp_include = set_components_list(
                    mpp_dir, sdk_comp_file, sdk_exclude, sdk_include, mpp_include, symlinks_exist, mpp_in_sdk
                )
            else:
                # Update all componeents if full_sdk_update is True
                print("Updating all SDK components...")
                components_to_update = [""]

            os.chdir(sdk_install_dir)

            # Run git clean -fdx command for all repos
            # WARNING: These commands will discard any changes in SDK repos (after stash)
            # First, stash any modified changes before resetting with timestamp
            stash_timestamp = datetime.now().strftime("%H_%M_%S")
            print(f"Stashing changes with timestamp: {stash_timestamp}")
            run_command(
                f"west forall -c \"git stash push -m 'sdk_update_backup_stash_{stash_timestamp}' || true\"",
                cwd=sdk_install_dir,
                shell=True
            )
            run_command("west forall -c \"git reset --hard\"",
                        cwd=sdk_install_dir, shell=True)

            # Run git clean -fdx if enabled
            if git_clean:
                print(
                    "Running git clean -fdx to remove all untracked files and directories...")
                run_command("west forall -c \"git clean -fdx\"",
                            cwd=sdk_install_dir, shell=True)

            # Update manifests repo to the configured version
            manifests_dir = sdk_install_dir / "manifests"
            os.chdir(manifests_dir)
            run_command("git fetch --prune --tags --all", cwd=manifests_dir)
            checkout_revision(sdk_version, manifests_dir)

            # Run west update
            os.chdir(sdk_install_dir)
            west_status = west_update(
                components_to_update, sdk_install_dir, args.disable_fast_update)

            # Apply and drop the stash with the timestamp if it exists
            stash_script = sdk_install_dir / "apply_stash.py"
            stash_script.write_text(f"""
import subprocess
import sys
result = subprocess.run(['git', 'stash', 'list'], capture_output=True, text=True)
for line in result.stdout.splitlines():
    if 'sdk_update_backup_stash_{stash_timestamp}' in line:
        stash_ref = line.split(':')[0]
        subprocess.run(['git', 'stash', 'pop', stash_ref])
        print(f'applied sdk_update_backup_stash_{stash_timestamp}')
        break
""")
            run_command(
                f"west forall -c \"python {stash_script.as_posix()}\"",
                cwd=sdk_install_dir / "mcuxsdk",
                shell=True,
                check=False
            )
            stash_script.unlink()  # Clean up the temporary script

            os.chdir(mpp_dir)

            # Create symbolic links to mpp repo based on symlinks_exist flag
            # If west_update failed, create symlinks if symlinks_exist is true
            if symlinks_exist or (west_status != 0 and symlinks_exist):
                create_symbolic_links(mpp_dir, sdk_install_dir)

            os.chdir(crt_dir)
            sys.exit(0)

    # Initialize west and bifrost
    print("Initializing new SDK installation...")
    run_command(
        "git config --global url.ssh://git@bitbucket.sw.nxp.com/mcucore/bifrost.git.insteadOf https://github.com/nxp-zephyr/bifrost",
        check=False
    )
    run_command(
        f"west init -m ssh://git@bitbucket.sw.nxp.com/mcucore/mcuxsdk-manifests.git {sdk_install_dir} --mr {sdk_version}",
        cwd=sdk_install_path
    )

    os.chdir(sdk_install_dir)
    if args.disable_fast_update:
        run_command("west update bifrost", cwd=sdk_install_dir)
    else:
        run_command("west update -n -o=--depth=1 bifrost", cwd=sdk_install_dir)
    run_command("west config commands.allow_extensions true",
                cwd=sdk_install_dir)
    run_command("west sdk_init", cwd=sdk_install_dir)

    # Set the components to be updated via west update command
    if not full_sdk_update:
        components_to_update, mpp_include = set_components_list(
            mpp_dir, sdk_comp_file, sdk_exclude, sdk_include, mpp_include, create_symlinks, mpp_in_sdk
        )
    else:
        print("Installing all SDK components...")
        components_to_update = [""]

    # Run west update
    west_status = west_update(components_to_update,
                              sdk_install_dir, args.disable_fast_update)

    os.chdir(mpp_dir)

    # Create symbolic links to mpp repo based on create_symlinks flag
    # If west_update failed, create symlinks if create_symlinks is true
    if create_symlinks or (west_status != 0 and create_symlinks):
        create_symbolic_links(mpp_dir, sdk_install_dir)

    os.chdir(crt_dir)
    sys.exit(0)


if __name__ == "__main__":
    main()
