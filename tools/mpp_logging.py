#!/usr/bin/env python3

"""
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
"""

"""
Logging utilities for MPP tools.
"""

import sys
from enum import IntEnum


class LogLevel(IntEnum):
    """Log levels for the script."""
    ERROR = 1
    WARNING = 2
    INFO = 3


class ColoredLogger:
    """Simple colored logger for the script."""
    
    # ANSI color codes
    RED = '\033[91m'
    YELLOW = '\033[93m'
    RESET = '\033[0m'
    
    def __init__(self, log_level: LogLevel = LogLevel.ERROR):
        self.log_level = log_level

    def set_log_level(self, log_level: LogLevel):
        """Set the logging level for the logger.

        Args:
            log_level: Desired logging level from LogLevel enum
        """
        self.log_level = log_level
    
    def error(self, message: str):
        """Log error message (always shown)."""
        print(f"{self.RED}ERROR: {message}{self.RESET}", file=sys.stderr)
    
    def warning(self, message: str):
        """Log warning message (shown at WARNING level and above)."""
        if self.log_level >= LogLevel.WARNING:
            print(f"{self.YELLOW}WARNING: {message}{self.RESET}", file=sys.stderr)
    
    def info(self, message: str):
        """Log info message (shown at INFO level and above)."""
        if self.log_level >= LogLevel.INFO:
            print(f"INFO: {message}")