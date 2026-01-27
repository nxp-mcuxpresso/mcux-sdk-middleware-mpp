#!/usr/bin/env python3

"""
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
"""

"""
Script to synchronize files between MPP repository and SDK structure.
"""

import argparse
import sys
import os
import shutil
import subprocess
from typing import List, Optional
from pathlib import Path

# Ensure we can import from the same directory as this script
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from mpp_logging import LogLevel, ColoredLogger


# Global logger instance
logger = ColoredLogger()


def is_in_mpp_sdk_path() -> bool:
    """
    Check if the current file path includes 'mcuxsdk/middleware/eiq/mpp'.

    Returns:
        bool: True if the path pattern is found, False otherwise
    """
    current_file_path = os.path.abspath(__file__)

    # Normalize path separators for cross-platform compatibility
    normalized_path = current_file_path.replace('\\', '/')

    # Check if the pattern exists in the path
    pattern = 'mcuxsdk/middleware/eiq/mpp'
    return pattern in normalized_path


def get_sdk_root_path(sdk_path: Optional[str] = None) -> str:
    """
    Get the SDK root path (mcuxsdk directory).

    Args:
        sdk_path: Provided SDK path or None

    Returns:
        Path to the mcuxsdk directory
    """
    if sdk_path:
        return os.path.abspath(sdk_path)

    # If running from within SDK, find the mcuxsdk root
    current_path = os.path.abspath(__file__)
    
    # Split the path into parts and find the exact 'mcuxsdk' folder
    path_parts = Path(current_path).parts
    
    try:
        # Find the index of 'mcuxsdk' in the path parts
        mcuxsdk_index = path_parts.index('mcuxsdk')
        # Reconstruct the path up to and including 'mcuxsdk'
        sdk_root = os.path.join(*path_parts[:mcuxsdk_index + 1])
        return sdk_root
    except ValueError:
        raise ValueError("Cannot determine SDK root path - 'mcuxsdk' folder not found in current path")

def validate_sdk_path(args):
    """
    Validate SDK path - either we're running from within the SDK or --sdk is provided.

    Args:
        args: Parsed arguments

    Raises:
        SystemExit: If validation fails
    """
    in_sdk_path = is_in_mpp_sdk_path()

    if not in_sdk_path and not args.sdk:
        logger.error("Script is not running from within the MPP SDK path.")
        logger.error("Please either:")
        logger.error("  1. Run the script from within the SDK directory structure (mcuxsdk/middleware/eiq/mpp/...)")
        logger.error("  2. Provide the SDK path using -s/--sdk argument")
        sys.exit(1)

    if args.sdk:
        # Validate that the provided SDK path exists and looks correct
        sdk_path = os.path.abspath(args.sdk)
        if not os.path.exists(sdk_path):
            logger.error(f"Provided SDK path does not exist: {sdk_path}")
            sys.exit(1)

        # Check if the path looks like an MPP SDK directory
        expected_subdirs = ['examples', 'middleware', 'ecosystem']
        missing_dirs = []
        for subdir in expected_subdirs:
            if not os.path.exists(os.path.join(sdk_path, subdir)):
                missing_dirs.append(subdir)

        if missing_dirs:
            logger.warning(f"SDK path may be incorrect. Missing expected directories: {', '.join(missing_dirs)}")

def is_mpp_folder_symlink(sdk_root_path, boards: list[str]):
    """
    Check if the middleware/eiq/mpp folder and optionally its subdirectories are symbolic links.

    Args:
        sdk_root_path (str or Path): The root path of the SDK
        boards (str): list of strings representing board names to check

    Returns:
        bool: True if the middleware/eiq/mpp folder is a symbolic link AND none of the boards have symlinked folders

    Raises:
        FileNotFoundError: If the middleware/eiq/mpp path doesn't exist
    """
    # Convert to Path object for cross-platform compatibility
    sdk_root = Path(sdk_root_path)
    mpp_folder_path = sdk_root / "middleware" / "eiq" / "mpp"
    mpp_boards_root_path = sdk_root / "examples" / "_boards"
    mpp_examples_root_path = sdk_root / "examples" / "eiq_examples" / "mpp"

    list_to_check = [mpp_folder_path, mpp_examples_root_path] + [mpp_boards_root_path / board / "eiq_examples" / "mpp" for board in boards]

    # Check if all paths exist
    file_exists = [path.exists() for path in list_to_check]
    if any(not exists for exists in file_exists):
        non_existing_paths = [str(path) for path, exists in zip(list_to_check, file_exists) if not exists]
        non_existing_paths_str = "\n\t".join(non_existing_paths)
        logger.error(f"Some paths do not exist:\n\t{non_existing_paths_str}")
        return True

    # Check if any file is symlink
    is_symlink_list = [path.is_symlink() for path in list_to_check]

    if any([is_symlink for is_symlink in is_symlink_list]):
        symlink_paths_str = "\n\t".join([str(path) for path, is_symlink in zip(list_to_check, is_symlink_list) if is_symlink])
        logger.error(f"Some paths are symbolic links: {symlink_paths_str}")
        return True

    # All checks passed
    logger.info("All the checked directories exist and are not symbolic links")
    return False

def _get_git_branch(repo_path: str) -> str:
    """
    Get the current git branch of a repository.

    Args:
        repo_path: Path to the git repository

    Returns:
        Current branch name

    Raises:
        subprocess.CalledProcessError: If git command fails
    """
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to get git branch in {repo_path}: {e}")


def _checkout_git_branch(repo_path: str, branch: str):
    """
    Checkout a specific git branch in a repository.

    Args:
        repo_path: Path to the git repository
        branch: Branch name to checkout

    Raises:
        subprocess.CalledProcessError: If git command fails
    """
    try:
        logger.info(f"Checking out branch '{branch}' in {repo_path}")

        subprocess.run(
            ['git', 'checkout', branch],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to checkout branch '{branch}' in {repo_path}: {e}")


def _get_modified_files(repo_path: str) -> List[str]:
    """
    Get list of modified files in a git repository.

    Args:
        repo_path: Path to the git repository

    Returns:
        List of modified file paths relative to repo root

    Raises:
        subprocess.CalledProcessError: If git command fails
    """
    try:
        result = subprocess.run(
            ['git', 'status', '--porcelain'],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )

        modified_files = []
        for line in result.stdout.strip().split('\n'):
            if line:
                # Parse git status output (format: "XY filename")
                status = line[:2]
                filename = line[3:]
                # Include modified, added, and renamed files
                if 'M' in status or 'A' in status or 'R' in status:
                    modified_files.append(filename)

        return modified_files
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to get modified files in {repo_path}: {e}")


def _copy_modified_files(modified_files: List[str], source_root: str, dest_root: str):
    """
    Copy modified files from source to destination.

    Args:
        modified_files: List of file paths relative to source_root
        source_root: Source directory root
        dest_root: Destination directory root
    """
    for file_path in modified_files:
        source_file = os.path.join(source_root, file_path)
        dest_file = os.path.join(dest_root, file_path)

        if os.path.exists(source_file):
            # Create destination directory if it doesn't exist
            dest_dir = os.path.dirname(dest_file)
            os.makedirs(dest_dir, exist_ok=True)

            try:
                shutil.copy2(source_file, dest_file)
                logger.info(f"Copied modified file: {file_path}")
            except Exception as e:
                logger.warning(f"Failed to copy {file_path}: {e}")

def _copy_dir(source_dir: str, dest_dir: str, keep_non_existing_files: bool = False, skip_dirs: List[str] = None):
    """
    Copy directory contents with optional preservation of destination.

    Args:
        source_dir: Source directory path
        dest_dir: Destination directory path
        keep_non_existing_files: Whether to preserve existing files in destination
    """
    # Remove destination directory contents if not preserving
    if not keep_non_existing_files and os.path.exists(dest_dir):
        shutil.rmtree(dest_dir)

    # Create destination directory if it doesn't exist
    os.makedirs(dest_dir, exist_ok=True)

    # Copy directory contents
    for item in os.listdir(source_dir):
        source_item = os.path.join(source_dir, item)
        dest_item = os.path.join(dest_dir, item)

        if os.path.isfile(source_item):
            shutil.copy2(source_item, dest_item)
        elif os.path.isdir(source_item):
            if skip_dirs and item in skip_dirs:
                logger.info(f"Skipping directory: {item} (full path {source_item})")
                continue
            _copy_dir(source_item, dest_item, keep_non_existing_files)
        else:
            logger.error(f"Found object which is not file or directory: {source_item}")
            raise Exception(f"Unsupported file type: {source_item}")

def _copy_sdk_files(boards: List[str], mpp_root: str, sdk_root_path: str, mpp_to_sdk: bool = True, keep_non_existing_files: bool = False):
    """
    Copy files between MPP structure and SDK structure.

    Args:
        boards: List of board names to sync
        mpp_root: MPP repository root path
        sdk_root_path: SDK root path
        mpp_to_sdk: True to copy from MPP to SDK, False to copy from SDK to MPP
        keep_non_existing_files: True to preserve existing files in destination, False to remove them
    """
    # Copy board files for each board
    for board in boards:
        logger.info(f"Processing board: {board}")
        logger.info(f"MPP root: {mpp_root}")

        if mpp_to_sdk:
            source_board_dir = os.path.join(mpp_root, 'boards', board)
            dest_board_dir = os.path.join(sdk_root_path, 'examples', '_boards', board, 'eiq_examples', 'mpp')
        else:
            source_board_dir = os.path.join(sdk_root_path, 'examples', '_boards', board, 'eiq_examples', 'mpp')
            dest_board_dir = os.path.join(mpp_root, 'boards', board)

        if os.path.exists(source_board_dir):
            try:
                _copy_dir(source_board_dir, dest_board_dir, keep_non_existing_files)

                direction = "MPP -> SDK" if mpp_to_sdk else "SDK -> MPP"
                logger.info(f"Copied board files for {board} ({direction}): {source_board_dir} -> {dest_board_dir}")

            except Exception as e:
                logger.warning(f"Failed to copy board files for {board}: {e}")
        else:
            logger.warning(f"Board directory not found: {source_board_dir}")

    # Copy examples directory
    if mpp_to_sdk:
        source_examples_dir = os.path.join(mpp_root, 'examples')
        dest_examples_dir = os.path.join(sdk_root_path, 'examples', 'eiq_examples', 'mpp')
    else:
        source_examples_dir = os.path.join(sdk_root_path, 'examples', 'eiq_examples', 'mpp')
        dest_examples_dir = os.path.join(mpp_root, 'examples')

    if os.path.exists(source_examples_dir):
        try:
            # Copy all example directories
            _copy_dir(source_examples_dir, dest_examples_dir, keep_non_existing_files)

            direction = "MPP -> SDK" if mpp_to_sdk else "SDK -> MPP"
            logger.info(f"Copied examples ({direction}): {source_examples_dir} -> {dest_examples_dir}")

        except Exception as e:
            logger.warning(f"Failed to copy examples directory: {e}")
    else:
        logger.warning(f"Examples directory not found: {source_examples_dir}")

def _get_remote_url(repo_path: str, remote_name: str) -> str:
    """
    Get the URL of a specific remote.

    Args:
        repo_path: Path to the git repository
        remote_name: Name of the remote

    Returns:
        Remote URL

    Raises:
        RuntimeError: If git command fails
    """
    try:
        result = subprocess.run(
            ['git', 'remote', 'get-url', remote_name],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to get URL for remote '{remote_name}' in {repo_path}: {e}")


def _get_all_remotes(repo_path: str) -> dict:
    """
    Get all remotes and their URLs.

    Args:
        repo_path: Path to the git repository

    Returns:
        Dictionary mapping remote names to URLs

    Raises:
        RuntimeError: If git command fails
    """
    try:
        result = subprocess.run(
            ['git', 'remote', '-v'],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )

        remotes = {}
        for line in result.stdout.strip().split('\n'):
            if line:
                parts = line.split()
                if len(parts) >= 2 and '(fetch)' in line:
                    remote_name = parts[0]
                    remote_url = parts[1]
                    remotes[remote_name] = remote_url

        return remotes
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to get remotes in {repo_path}: {e}")


def _normalize_git_url(url: str) -> str:
    """
    Normalize git URL for comparison (handle SSH vs HTTPS, trailing slashes, etc.).

    Args:
        url: Git URL to normalize

    Returns:
        Normalized URL
    """
    # Remove trailing .git if present
    if url.endswith('.git'):
        url = url[:-4]

    # Remove trailing slash
    url = url.rstrip('/')

    # Convert SSH to HTTPS format for comparison
    if url.startswith('git@'):
        # Convert git@github.com:user/repo to https://github.com/user/repo
        if ':' in url:
            host_part, path_part = url.split(':', 1)
            host = host_part.replace('git@', '')
            url = f"https://{host}/{path_part}"

    # Ensure HTTPS URLs are lowercase for comparison
    if url.startswith('https://'):
        url = url.lower()

    return url


def _find_matching_remote(source_remotes: dict, target_remotes: dict, source_remote_name: str) -> Optional[str]:
    """
    Find a remote in target repository that has the same URL as source remote.

    Args:
        source_remotes: Dictionary of source repository remotes
        target_remotes: Dictionary of target repository remotes
        source_remote_name: Name of the remote in source repository

    Returns:
        Name of matching remote in target repository, or None if not found
    """
    if source_remote_name not in source_remotes:
        return None

    source_url = _normalize_git_url(source_remotes[source_remote_name])

    for target_remote_name, target_url in target_remotes.items():
        if _normalize_git_url(target_url) == source_url:
            return target_remote_name

    return None


def _get_git_remote_branch_with_remote_name(repo_path: str) -> tuple:
    """
    Get the remote tracking branch and remote name of the current branch.

    Args:
        repo_path: Path to the git repository

    Returns:
        Tuple of (remote_name, branch_name, full_remote_branch)

    Raises:
        RuntimeError: If git command fails or no remote branch found
    """
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--abbrev-ref', '--symbolic-full-name', '@{u}'],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
        full_remote_branch = result.stdout.strip()

        # Parse remote_name/branch_name
        if '/' in full_remote_branch:
            remote_name, branch_name = full_remote_branch.split('/', 1)
            return remote_name, branch_name, full_remote_branch
        else:
            raise RuntimeError(f"Invalid remote branch format: {full_remote_branch}")

    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to get remote branch in {repo_path}: {e}")


def _get_git_remote_branch(repo_path: str) -> str:
    """
    Get the remote tracking branch of the current branch.

    Args:
        repo_path: Path to the git repository

    Returns:
        Remote branch name (e.g., 'origin/main')

    Raises:
        RuntimeError: If git command fails or no remote branch found
    """
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--abbrev-ref', '--symbolic-full-name', '@{u}'],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to get remote branch in {repo_path}: {e}")


def _fetch_remotes(repo_path: str):
    """
    Fetch all remotes in a git repository.

    Args:
        repo_path: Path to the git repository

    Raises:
        RuntimeError: If git command fails
    """
    try:
        logger.info(f"Fetching remotes in {repo_path}")

        subprocess.run(
            ['git', 'fetch', '--all'],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to fetch remotes in {repo_path}: {e}")


def _remote_branch_exists(repo_path: str, remote_branch: str) -> bool:
    """
    Check if a remote branch exists in the repository.

    Args:
        repo_path: Path to the git repository
        remote_branch: Remote branch name (e.g., 'origin/main')

    Returns:
        True if remote branch exists, False otherwise
    """
    try:
        subprocess.run(
            ['git', 'rev-parse', '--verify', remote_branch],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
        return True
    except subprocess.CalledProcessError:
        return False


def _local_branch_exists(repo_path: str, branch_name: str) -> bool:
    """
    Check if a local branch exists in the repository.

    Args:
        repo_path: Path to the git repository
        branch_name: Local branch name

    Returns:
        True if local branch exists, False otherwise
    """
    try:
        subprocess.run(
            ['git', 'rev-parse', '--verify', branch_name],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
        return True
    except subprocess.CalledProcessError:
        return False


def _sync_local_to_remote(repo_path: str, branch_name: str, remote_branch: str, remote_name: str):
    """
    Sync local branch to remote branch (pull changes).

    Args:
        repo_path: Path to the git repository
        branch_name: Local branch name
        remote_branch: Remote branch name
        remote_name: Remote name

    Raises:
        RuntimeError: If git command fails
    """
    try:
        logger.info(f"Syncing local branch '{branch_name}' to remote '{remote_branch}' in {repo_path}")

        # Checkout the local branch first
        subprocess.run(
            ['git', 'checkout', branch_name],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )

        # Pull changes from remote
        subprocess.run(
            ['git', 'pull', remote_name, branch_name],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to sync local branch '{branch_name}' to remote in {repo_path}: {e}")


def _checkout_new_tracking_branch(repo_path: str, branch_name: str, remote_branch: str):
    """
    Create and checkout a new local branch that tracks a remote branch.

    Args:
        repo_path: Path to the git repository
        branch_name: Local branch name to create
        remote_branch: Remote branch to track

    Raises:
        RuntimeError: If git command fails
    """
    try:
        logger.info(f"Creating new tracking branch '{branch_name}' for '{remote_branch}' in {repo_path}")

        subprocess.run(
            ['git', 'checkout', '-b', branch_name, remote_branch],
            cwd=repo_path,
            capture_output=True,
            text=True,
            check=True
        )
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to create tracking branch '{branch_name}' for '{remote_branch}' in {repo_path}: {e}")

def _sync_git_and_copy(boards: List[str], mpp_root: str, sdk_root_path: str, keep_non_existing_files: bool = False):
    """
    Sync git branches and copy modified files, then copy SDK files.

    Args:
        boards: List of board names to sync
        mpp_root: MPP repository root path
        sdk_root_path: SDK root path
        keep_non_existing_files: True to preserve existing files in destination, False to remove them
    """
    try:
        # Get current branch from MPP repository
        current_branch = _get_git_branch(mpp_root)
        logger.info(f"Current MPP branch: {current_branch}")

        # Get remote tracking branch from MPP repository
        try:
            mpp_remote_name, mpp_branch_name, mpp_full_remote_branch = _get_git_remote_branch_with_remote_name(mpp_root)
            logger.info(f"Current MPP remote branch: {mpp_full_remote_branch} (remote: {mpp_remote_name}, branch: {mpp_branch_name})")
        except RuntimeError as e:
            logger.warning(f"Could not get remote branch from MPP: {e}")
            raise RuntimeError

        # Path to MPP directory in SDK
        sdk_mpp_path = os.path.join(sdk_root_path, 'middleware', 'eiq', 'mpp')

        if os.path.exists(sdk_mpp_path):
            try:
                # Fetch remotes in SDK MPP directory
                _fetch_remotes(sdk_mpp_path)

                # Get all remotes from both repositories
                mpp_remotes = _get_all_remotes(mpp_root)
                sdk_remotes = _get_all_remotes(sdk_mpp_path)

                logger.info(f"MPP remotes: {mpp_remotes}")
                logger.info(f"SDK remotes: {sdk_remotes}")

                # Find matching remote in SDK based on URL
                matching_sdk_remote = None
                if mpp_remote_name in mpp_remotes:
                    matching_sdk_remote = _find_matching_remote(mpp_remotes, sdk_remotes, mpp_remote_name)
                    if matching_sdk_remote:
                        logger.info(f"Found matching remote in SDK: '{matching_sdk_remote}' for MPP remote '{mpp_remote_name}'")
                    else:
                        logger.info(f"No matching remote found in SDK for MPP remote '{mpp_remote_name}'")

                # Construct the target remote branch name
                target_remote_branch = None
                if matching_sdk_remote:
                    target_remote_branch = f"{matching_sdk_remote}/{mpp_branch_name}"
                else:
                    raise RuntimeError

                # Check if target remote branch exists in SDK
                if target_remote_branch and _remote_branch_exists(sdk_mpp_path, target_remote_branch):
                    logger.info(f"Target remote branch '{target_remote_branch}' exists in SDK")

                    # Check if local branch exists
                    if _local_branch_exists(sdk_mpp_path, current_branch):
                        logger.info(f"Local branch '{current_branch}' exists in SDK, syncing to remote")
                        # Sync local branch to remote
                        _sync_local_to_remote(sdk_mpp_path, current_branch, target_remote_branch, matching_sdk_remote)
                    else:
                        logger.info(f"Local branch '{current_branch}' does not exist in SDK, creating tracking branch")
                        # Create new tracking branch
                        _checkout_new_tracking_branch(sdk_mpp_path, current_branch, target_remote_branch)
                else:
                    logger.info(f"Target remote branch '{target_remote_branch}' does not exist in SDK, falling back to simple checkout")
                    raise RuntimeError

            except RuntimeError as e:
                logger.warning(f"Git branch synchronization failed: {e}")
                logger.warning("Continuing with current SDK branch...")
                raise RuntimeError

            # Get modified files in MPP repository
            modified_files = _get_modified_files(mpp_root)
            if modified_files:
                logger.info(f"Found {len(modified_files)} modified files in MPP repository")

                # Copy modified files to SDK
                _copy_modified_files(modified_files, mpp_root, sdk_mpp_path)
            else:
                logger.info("No modified files found in MPP repository")
        else:
            logger.warning(f"SDK MPP directory not found: {sdk_mpp_path}")

        # Now copy files as if we were inside SDK
        _copy_sdk_files(boards, sdk_mpp_path if os.path.exists(sdk_mpp_path) else mpp_root, sdk_root_path, True, keep_non_existing_files)

    except RuntimeError as e:
        logger.error(f"Error during git synchronization: {e}")
        logger.error("Falling back to direct file copy...")

        # Fallback: Copy entire MPP directory to SDK
        sdk_mpp_path = os.path.join(sdk_root_path, 'middleware', 'eiq', 'mpp')
        try:
            logger.info(f"Copying entire MPP directory: {mpp_root} -> {sdk_mpp_path}")

            # Copy entire MPP directory
            _copy_dir(mpp_root, sdk_mpp_path, keep_non_existing_files=True, skip_dirs=['.git'])

            logger.info(f"Successfully copied entire MPP directory to SDK")

        except Exception as copy_error:
            logger.error(f"Error copying entire MPP directory: {copy_error}")
            logger.error("Continuing with available MPP source...")

        # Copy SDK files using the original MPP root or the copied one
        _copy_sdk_files(boards, sdk_mpp_path if os.path.exists(sdk_mpp_path) else mpp_root, sdk_root_path, True, keep_non_existing_files)

def parse_arguments():
    """
    Parse command line arguments.

    Returns:
        argparse.Namespace: Parsed arguments
    """
    parser = argparse.ArgumentParser(
        description="Synchronize files between MPP repository and SDK structure",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="""
Examples:
%(prog)s -b frdmmcxn947 evkbmimxrt1170 -s /path/to/mcuxsdk
%(prog)s -b frdmmcxn947 --direction sdk-to-mpp -s /path/to/mcuxsdk
%(prog)s -b frdmmcxn947 --no-git-sync -s /path/to/mcuxsdk
%(prog)s -b frdmmcxn947 --mpp-root /path/to/mpp -s /path/to/mcuxsdk
%(prog)s -s /path/to/mcuxsdk  # Uses default boards: mimxrt700evk, evkbmimxrt1170, frdmmcxn947
%(prog)s -s /path/to/mcuxsdk --log-level 3  # Enable info level logging
        """
    )

    parser.add_argument(
        '-b', '--boards',
        nargs='*',
        type=str,
        default=['mimxrt700evk', 'evkbmimxrt1170', 'frdmmcxn947'],
        help='List of board names to sync (e.g., frdmmcxn947, evkbmimxrt1170). Default: mimxrt700evk, evkbmimxrt1170, frdmmcxn947'
    )

    parser.add_argument(
        '-s', '--sdk',
        type=str,
        help='Path to the SDK root directory (mcuxsdk) (default: auto-detect from current script location)',
    )

    parser.add_argument(
        '--mpp-root',
        type=str,
        help='Path to the MPP root directory (default: auto-detect from script location)'
    )

    parser.add_argument(
        '--direction',
        choices=['mpp-to-sdk', 'sdk-to-mpp'],
        default='mpp-to-sdk',
        help='Direction of synchronization (default: mpp-to-sdk)'
    )

    parser.add_argument(
        '--no-git-sync',
        action='store_true',
        help="""\
Disable git synchronization (direct file copy only)
    When running script from a separate mpp folder (not included in the sdk middleware folder),
    this option gives you the posibility to disable the synchronization of local mpp git repo
    and the repo from SDK middleware folder (not applicable when running script from inside sdk)"""
    )

    parser.add_argument(
        '-l', '--log-level',
        type=int,
        choices=[level.value for level in LogLevel],
        default=LogLevel.ERROR.value,
        help=f'Log level: {", ".join([f"{level.value}={level.name}" for level in LogLevel])}'
    )

    parser.add_argument(
        '--remove-non-existing-files',
        action='store_true',
        help='Remove existing files in destination that are not in source'
    )

    return parser.parse_args()

def validate_arguments(args):
    """
    Validate command line arguments.

    Args:
        args: Parsed arguments

    Raises:
        SystemExit: If validation fails
    """

    # Validate MPP root if provided
    if args.mpp_root and not os.path.exists(args.mpp_root):
        logger.error(f"Error: MPP root path does not exist: {args.mpp_root}", file=sys.stderr)
        sys.exit(1)

    # Validate SDK path
    if args.sdk and not os.path.exists(args.sdk):
        logger.error(f"Error: SDK path does not exist: {args.sdk}", file=sys.stderr)
        sys.exit(1)

def sync_sdk_files(boards: List[str], sdk_root_path: str, mpp_root: Optional[str] = None,
                   direction: str = 'mpp-to-sdk', git_sync: bool = True, keep_non_existing_files: bool = False, log_level: LogLevel = LogLevel.ERROR):
    """
    Synchronize files between MPP repository and SDK structure.

    Args:
        boards: List of board names to sync
        sdk_root_path: Path to the SDK root (mcuxsdk directory)
        mpp_root: Path to MPP root directory (if None, auto-detect)
        direction: 'mpp-to-sdk' or 'sdk-to-mpp'
        git_sync: Enable git synchronization (only for mpp-to-sdk)
                  This option gives you the posibility to enable the synchronization of local mpp git repo
                  and the repo from SDK middleware folder (not applicable when running script from inside sdk)
        keep_non_existing_files: True to preserve existing files in destination, False to remove them
        log_level: Logging level
    """
    # Set global logger level
    global logger
    logger = ColoredLogger(log_level)

    if mpp_root is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        mpp_root = os.path.dirname(script_dir)  # One level up from tools/

    in_sdk_path = is_in_mpp_sdk_path()
    mpp_to_sdk = direction == 'mpp-to-sdk'

    if sdk_root_path is None:
        if in_sdk_path:
            sdk_root_path = get_sdk_root_path()
        else:
            raise ValueError("Unable to determine SDK root path. Please provide sdk_root_path using -s option.")

    if is_mpp_folder_symlink(sdk_root_path, boards):
        logger.error("Detected symlink in SDK, skipping sync_sdk_files...")
        return

    logger.info(f"MPP root directory: {mpp_root}")
    logger.info(f"SDK root directory: {sdk_root_path}")
    logger.info(f"Running from SDK path: {in_sdk_path}")
    logger.info(f"Direction: {direction}")
    logger.info(f"Git sync enabled: {git_sync}")
    if direction == 'sdk-to-mpp':
        # Always do direct copy for SDK to MPP
        _copy_sdk_files(boards, mpp_root, sdk_root_path, False, keep_non_existing_files)
    elif in_sdk_path or not git_sync:
        # We're inside SDK or git sync is disabled, copy files directly
        _copy_sdk_files(boards, mpp_root, sdk_root_path, True, keep_non_existing_files)
    else:
        # We're outside SDK and git sync is enabled, need to sync git branches and modified files first
        _sync_git_and_copy(boards, mpp_root, sdk_root_path, keep_non_existing_files)

def main():
    """Main entry point."""
    args = parse_arguments()

    # Set up logging level
    logger.set_log_level(args.log_level)

    # Validate that sdk and mpp paths exist and are valid
    validate_arguments(args)

    # Validate SDK path requirements
    validate_sdk_path(args)

    try:
        sync_sdk_files(
            boards=args.boards,
            sdk_root_path=args.sdk,
            mpp_root=args.mpp_root,
            direction=args.direction,
            git_sync=not args.no_git_sync,
            keep_non_existing_files= not args.remove_non_existing_files,
            log_level=args.log_level
        )

        direction_text = "MPP to SDK" if args.direction == 'mpp-to-sdk' else "SDK to MPP"
        logger.info(f"Successfully synchronized files ({direction_text}) for boards: {', '.join(args.boards)}")

    except Exception as e:
        logger.error(f"Error during synchronization: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()