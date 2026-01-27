#!/usr/bin/env python3
# Copyright 2026 NXP
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

import os
import sys
import argparse
import platform

# Add internal directory to path to import from auto_test
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'internal'))
from auto_test import flash_files, auto_detect_probe # type: ignore


def parse_args():
    """
    Parse command-line arguments for flashing a binary to the board.
    """
    parser = argparse.ArgumentParser(
        description="Flash a binary image to a target board using LinkServer or JLink."
    )
    parser.add_argument(
        "image",
        type=str,
        help="Path to the binary image (.elf file) to flash"
    )
    parser.add_argument(
        "-b", "--board",
        type=str,
        required=True,
        choices=['frdmmcxn947', 'mimxrt700evk', 'evkbmimxrt1170'],
        help="Board type"
    )
    parser.add_argument(
        "-p", "--probe_id",
        type=str,
        help="Probe ID (auto-detected if not provided)"
    )
    parser.add_argument(
        "-g", "--use_gdb",
        action='store_true',
        help="Use GDB server for flashing (only supported with jlink)"
    )
    parser.add_argument(
        "-j", "--jlinkscript",
        type=str,
        help="Path to the JLink script file for GDB server (valid only when use_gdb is set)"
    )
    parser.add_argument(
        "-l", "--flash_log",
        type=str,
        help="Path to the flash log file (if not provided, output is printed to console)"
    )

    return parser.parse_args()


def main():
    """
    Main function to flash a binary image to the board.
    """
    args = parse_args()

    # Validate image path
    if not os.path.isfile(args.image):
        print(f"Error: Image file not found: {args.image}")
        sys.exit(1)

    # Check if it's an ELF file
    try:
        with open(args.image, 'rb') as f:
            if f.read(4) != b'\x7FELF':
                print(f"Error: File is not a valid ELF binary: {args.image}")
                sys.exit(1)
    except Exception as e:
        print(f"Error: Could not read image file: {e}")
        sys.exit(1)

    # Auto-detect probe and probe type
    current_os = platform.system()
    probe_id, probe_type = auto_detect_probe(args.probe_id, args.board, current_os, args.flash_log)

    # Validate jlinkscript argument
    if args.jlinkscript and not args.use_gdb:
        print("Warning: --jlinkscript is only used when --use_gdb is set")

    if args.use_gdb and args.jlinkscript and not os.path.isfile(args.jlinkscript):
        print(f"Error: JLink script file not found: {args.jlinkscript}")
        sys.exit(1)

    print(f"Board: {args.board}")
    print(f"Probe type: {probe_type}")
    print(f"Probe ID: {probe_id}")
    print(f"Image: {args.image}")
    if args.flash_log:
        print(f"Flash log: {args.flash_log}")
    else:
        print(f"Flash log: console output only")
    print()

    # Flash the image
    flash_success = flash_files(
        filepath=args.image,
        board_id=args.board,
        probe_id=probe_id,
        probe_type=probe_type,
        use_gdb=args.use_gdb,
        flash_log_file=args.flash_log,
        jlinkscript=args.jlinkscript
    )

    if flash_success:
        print(f"\n\033[92mFlash operation completed successfully!\033[0m")
        if args.flash_log:
            print(f"Flash log saved to: {args.flash_log}")
        sys.exit(0)
    else:
        print(f"\n\033[91mFlash operation failed!\033[0m")
        if args.flash_log:
            print(f"Check flash log for details: {args.flash_log}")
        sys.exit(1)


if __name__ == "__main__":
    main()