#!/usr/bin/env python3

"""
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
"""

"""
Script to generate YAML files for VSCode plugin to create MPP project configurations.
"""

import argparse
import sys
import os
import yaml
import shutil
from typing import List, Optional

# Ensure we can import from the same directory as this script
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from sync_sdk_files import sync_sdk_files, get_sdk_root_path, is_in_mpp_sdk_path, validate_sdk_path
from mpp_logging import LogLevel, ColoredLogger

# Initialize logger
logger = ColoredLogger()


def generate_description_from_name(project_name: str) -> str:
    """
    Generate a description from project name by replacing underscores with spaces.
    Also removes 'test' or 'example' prefix if present.

    Args:
        project_name: The project name

    Returns:
        Generated description
    """
    # Remove 'test_' or 'example_' prefix if present
    name = project_name.lstrip('test_').lstrip('example_')

    # Replace underscores with spaces
    return name.replace('_', ' ')

def filter_boards_in_config(config: dict, user_boards: List[str]) -> dict:
    """
    Filter the boards section to keep only boards specified by the user.

    Args:
        config: The loaded YAML configuration
        user_boards: List of boards specified by the user

    Returns:
        Modified configuration with filtered boards
    """

    # Get the project name (first key in the config)
    project_key = list(config.keys())[0]

    if 'boards' in config[project_key]:
        original_boards = config[project_key]['boards']
        filtered_boards = {}

        for board_key in original_boards:
            # Extract board name from keys like "mimxrt700evk@cm33_core0"
            board_name = board_key.split('@')[0]
            if board_name in user_boards:
                filtered_boards[board_key] = original_boards[board_key]

        config[project_key]['boards'] = filtered_boards

    return config


def create_ecosystem_yaml(project_name: str, sdk_root_path: str):
    """
    Create the ecosystem YAML file.

    Args:
        project_name: Name of the project
        sdk_root_path: Path to the SDK root (mcuxsdk directory)
    """

    # Create the ecosystem YAML content
    ecosystem_content = {
        '__load__': [f'examples/eiq_examples/mpp/{project_name}/example.yml'],
        project_name: {
            'contents': {
                'configuration': {
                    'tools': {
                        'mcux': {
                            'ignore': True
                        }
                    }
                }
            }
        }
    }

    # Create the directory path
    ecosystem_dir = os.path.join(sdk_root_path, 'ecosystem', 'examples', 'eiq_examples', 'mpp', project_name)
    os.makedirs(ecosystem_dir, exist_ok=True)

    # Write the ecosystem YAML file
    ecosystem_file_path = os.path.join(ecosystem_dir, f'{project_name}.yml')

    try:
        with open(ecosystem_file_path, 'w') as f:
            yaml.dump(ecosystem_content, f, default_flow_style=False, indent=2, sort_keys=False)

        logger.info(f"Created ecosystem file: {ecosystem_file_path}")

    except Exception as e:
        logger.error(f"Error writing ecosystem file: {e}")
        sys.exit(1)

def parse_arguments():
    """
    Parse command line arguments for project name and supported boards.

    Returns:
        argparse.Namespace: Parsed arguments containing project_name, boards, and type flags
    """
    parser = argparse.ArgumentParser(
        description="Generate YAML files for VSCode plugin to create MPP project configurations",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s face_recognition -e -b frdmmcxn947 evkbmimxrt1170
  %(prog)s camera_view -e --boards frdmmcxn947
  %(prog)s object_detection -t -b evkbmimxrt1170 frdmmcxn947
  %(prog)s face_recognition -e  # Uses default boards: mimxrt700evk, evkbmimxrt1170, frdmmcxn947
  %(prog)s face_recognition -e -s /path/to/mcuxsdk/middleware/eiq/mpp  # Specify SDK path manually
  %(prog)s internal_example -e -i  # Internal example
  %(prog)s public_example -e  # Public example (non-internal)
  %(prog)s unit_test -t  # Test (always internal)
  %(prog)s face_recognition -e -l 3  # Enable info logging
        """
    )

    parser.add_argument(
        'project_name',
        type=str,
        help='Name of the project (test or example) to generate YAML configuration for'
    )

    parser.add_argument(
        '-e', '--example',
        action='store_true',
        help='Specify that the project_name is an example'
    )

    parser.add_argument(
        '-t', '--test',
        action='store_true',
        help='Specify that the project_name is a test (tests are always internal)'
    )

    parser.add_argument(
        '-i', '--internal',
        action='store_true',
        help='Specify that the example is internal. Only applicable to examples (-e). Tests are always internal.'
    )

    parser.add_argument(
        '-b', '--boards',
        nargs='*',
        type=str,
        default=['mimxrt700evk', 'evkbmimxrt1170', 'frdmmcxn947'],
        help='List of board names that support this project (e.g., frdmmcxn947, evkbmimxrt1170). Default: mimxrt700evk, evkbmimxrt1170, frdmmcxn947'
    )

    parser.add_argument(
        '-d', '--description',
        type=str,
        default='',
        help='Description of the project (default: generated from project name by replacing underscores with spaces)'
    )

    parser.add_argument(
        '-s', '--sdk',
        type=str,
        help='Path to the MPP SDK root directory (mcuxsdk/middleware/eiq/mpp). Required if not running from within the SDK path.'
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
    Validate that exactly one of -e or -t is specified and internal flag usage.

    Args:
        args: Parsed arguments

    Raises:
        SystemExit: If validation fails
    """
    if not args.example and not args.test:
        logger.error("You must specify either -e/--example or -t/--test")
        sys.exit(1)

    if args.example and args.test:
        logger.error("You cannot specify both -e/--example and -t/--test")
        sys.exit(1)

    # Check internal flag usage - warn if used with tests (since tests are always internal)
    if args.internal and args.test:
        logger.warning("Tests are always internal. The -i/--internal flag is redundant with -t/--test")

def generate_yml_files(project_name: str, is_example: bool, is_internal: bool = False,
                    boards: Optional[List[str]] = None, description: str = '',
                    sdk_path: Optional[str] = None, keep_non_existing_files: bool = False, log_level: LogLevel = LogLevel.ERROR):
    """
    Generate YAML files for VSCode MCUXpresso plugin.

    Args:
        project_name: Name of the project
        is_example: True if project is an example, False if it's a test
        is_internal: True if project is internal (for examples) or ignored (tests are always internal)
        boards: List of supported board names
        description: Project description
        sdk_path: Path to the MPP SDK (if provided)
        keep_non_existing_files: True to keep existing files in destination, False to remove them
        log_level: Logging level
    """
    project_type = "example" if is_example else "test"
    project_type_path = "examples" if is_example else "tests"

    # Tests are always internal, examples can be internal or external
    actual_is_internal = is_internal if is_example else True

    # Generate description if not provided
    if not description:
        description = generate_description_from_name(project_name)

    # Generate paths based on internal flag
    if actual_is_internal:
        meta_path = f"middleware/eiq/mpp/{project_type_path}"
        project_root_path = f"middleware/eiq/mpp/boards/${{board}}/{project_type_path}"
        board_readme_path = f"middleware/eiq/mpp/boards/${{board}}"
    else:
        meta_path = f"examples/eiq_examples/mpp"
        project_root_path = f"boards/${{board}}/eiq_examples/mpp"
        board_readme_path = f"boards/${{board}}/eiq_examples/mpp"

    # Get SDK root path
    try:
        sdk_root_path = get_sdk_root_path(sdk_path)
    except ValueError as e:
        logger.error(str(e))
        sys.exit(1)

    logger.info(f"Generating YAML files for {project_type}: {project_name}")
    logger.info(f"Internal: {actual_is_internal}")
    logger.info(f"Description: {description}")
    logger.info(f"Meta path: {meta_path}")
    logger.info(f"Project root path: {project_root_path}")
    logger.info(f"Board readme path: {board_readme_path}")
    logger.info(f"SDK root path: {sdk_root_path}")
    if boards:
        logger.info(f"Supported boards: {', '.join(boards)}")
    else:
        logger.info("No specific boards specified - creating generic configuration")

    # Show SDK path information
    in_sdk_path = is_in_mpp_sdk_path()
    logger.info(f"Running from MPP SDK path: {in_sdk_path}")
    if sdk_path:
        logger.info(f"Using provided SDK path: {sdk_path}")

    # Synchronize SDK files for the specified boards
    sync_sdk_files(boards=boards, sdk_root_path=sdk_root_path, keep_non_existing_files=keep_non_existing_files, log_level=log_level, direction='mpp-to-sdk')

    # Load template.yml
    script_dir = os.path.dirname(os.path.abspath(__file__))
    template_path = os.path.join(script_dir, 'template.yml')

    try:
        with open(template_path, 'r') as f:
            template_content = f.read()
    except FileNotFoundError:
        logger.error(f"Template file not found: {template_path}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Error reading template file: {e}")
        sys.exit(1)

    # Replace placeholders
    yaml_content = template_content.replace('{project_name}', project_name)
    yaml_content = yaml_content.replace('{project_type}', project_type)
    yaml_content = yaml_content.replace('{project_description}', description)
    yaml_content = yaml_content.replace('{meta_path}', meta_path)
    yaml_content = yaml_content.replace('{project-root-path}', project_root_path)
    yaml_content = yaml_content.replace('{board_readme_path}', board_readme_path)

    # Parse the YAML content
    try:
        config = yaml.safe_load(yaml_content)
    except yaml.YAMLError as e:
        logger.error(f"Error parsing YAML template: {e}")
        sys.exit(1)

    # Filter boards to keep only user-specified ones
    if boards:
        config = filter_boards_in_config(config, boards)

    # Create the main example YAML file directory and write file
    example_dir = os.path.join(sdk_root_path, 'examples', 'eiq_examples', 'mpp', project_name)
    os.makedirs(example_dir, exist_ok=True)

    example_file_path = os.path.join(example_dir, 'example.yml')
    try:
        with open(example_file_path, 'w') as f:
            yaml.dump(config, f, default_flow_style=False, indent=2, sort_keys=False)

        logger.info(f"Created main example file: {example_file_path}")

    except Exception as e:
        logger.error(f"Error writing main example file: {e}")
        sys.exit(1)

    # Copy CMakeLists.txt file from source project directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # Go one folder up from script location, then to tests/examples/{project_name}
    source_cmake_path = os.path.join(os.path.dirname(script_dir), project_type_path, project_name, 'CMakeLists.txt')
    destination_cmake_path = os.path.join(example_dir, 'CMakeLists.txt')

    try:
        if os.path.exists(source_cmake_path):
            shutil.copy2(source_cmake_path, destination_cmake_path)
            logger.info(f"Copied CMakeLists.txt from: {source_cmake_path}")
            logger.info(f"                        to: {destination_cmake_path}")
        else:
            logger.warning(f"CMakeLists.txt not found at: {source_cmake_path}")
    except Exception as e:
        logger.warning(f"Failed to copy CMakeLists.txt: {e}")

    # Create the ecosystem YAML file
    create_ecosystem_yaml(project_name, sdk_root_path)

def main():
    """Main entry point."""
    args = parse_arguments()

    # Set up logging level
    logger.set_log_level(args.log_level)

    # Validate that exactly one of -e or -t is specified
    validate_arguments(args)

    # Validate SDK path requirements
    validate_sdk_path(args)

    try:
        generate_yml_files(
            project_name=args.project_name,
            is_example=args.example,
            is_internal=args.internal,
            boards=args.boards,
            description=args.description,
            sdk_path=args.sdk,
            keep_non_existing_files= not args.remove_non_existing_files,
            log_level=args.log_level
        )

        project_type = "example" if args.example else "test"
        # Tests are always internal, examples can be internal or external
        actual_is_internal = args.internal if args.example else True
        internal_text = " (internal)" if actual_is_internal else ""
        logger.info(f"Successfully generated YAML files for {project_type}{internal_text} '{args.project_name}' with boards: {', '.join(args.boards)}")

    except Exception as e:
        logger.error(f"Error generating YAML files: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
