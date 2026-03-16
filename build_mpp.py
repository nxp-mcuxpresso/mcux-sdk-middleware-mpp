#!/usr/bin/env python3

"""
 * Copyright 2025-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
"""

import os
import sys
import subprocess
import argparse
import shutil
import tempfile
import multiprocessing
import platform
import re
from pathlib import Path
import argcomplete

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'internal'))

DEBUG_CONSOLE_DEFINES = {
    "0": {
        "flags": "",
        "help": "Enable core 0 console only (default mode)"
    },
    "1": {
        "flags": "-DDISABLE_CORE0_CONSOLE -DENABLE_CORE1_CONSOLE",
        "help": "Enable core 1 console only"
    },
    "2": {
        "flags": "-DENABLE_CORE1_CONSOLE",
        "help": "Enable both core 0 and core 1 console"
    }
}

class MPPBuilder:
    def __init__(self):
        self.w_dir = Path(__file__).resolve().parent
        self.sdk_dir = self._find_sdk_dir()
        self.ntask = multiprocessing.cpu_count() // 2
        self.mpp_commit_id = self._get_commit_id()
        self.gen_doc = False
        self.last_built_elf = ""
        self.app_config_index = ""
        self.input_core_id = ""
        self.sysbuild = ""
        self.armgcc_dir = ""
        self.core_id = ""
        self.is_windows = platform.system() == "Windows"
        self.flash_after_build = False
        self.probe_id = None
        self.use_gdb = False
        self.jlinkscript = None
        self.code_coverage_enable = False
        # Set core ID based on board
        self.board_core_map = {
            "frdmmcxn947": {
                "0": "cm33_core0",
                "1": "cm33_core1"
            },
            "mimxrt700evk": {
                "0": "cm33_core0",
                "1": "cm33_core1"
            },
            "evkbmimxrt1170": {
                "0": "cm7",
                "1": "cm4"
            }
        }
        self.app_core_folder = "core0"

    def _find_sdk_dir(self):
        """Find SDK directory based on repository structure"""
        mcux_path1 = self.w_dir / ".." / ".." / ".." / ".." / "mcuxsdk"
        manifests_path = self.w_dir / ".." / ".." / ".." / ".." / "manifests"

        if mcux_path1.exists() and manifests_path.exists():
            print("Running script from sdk west repository")
            return mcux_path1.resolve()
        else:
            mcux_path2 = self.w_dir / ".." / "sdk-next" / "mcuxsdk"
            return mcux_path2.resolve()

    def _get_commit_id(self):
        """Get git commit ID"""
        try:
            result = subprocess.run(
                ["git", "describe", "--dirty", "--always", "--exclude=*"],
                capture_output=True, text=True, cwd=self.w_dir
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except:
            pass
        return "unknown"

    def _find_executable(self, name):
        """Find executable in PATH with cross-platform support"""
        def is_executable(path):
            """Check if a file is executable"""
            return os.path.isfile(path) and os.access(path, os.X_OK)

        # Get PATH environment variable
        path_env = os.environ.get('PATH', '')
        if self.is_windows:
            path_separator = ';'
            # Common Windows executable extensions
            extensions = ['', '.exe', '.bat', '.cmd', '.com']
        else:
            path_separator = ':'
            extensions = ['']

        # Split PATH into directories
        path_dirs = [d.strip() for d in path_env.split(path_separator) if d.strip()]

        # Add common directories that might not be in PATH
        if self.is_windows:
            # Add common Windows program directories
            common_dirs = [
                r'C:\Program Files\Git\bin',
                r'C:\Program Files\Git\usr\bin',
                r'C:\Program Files (x86)\Git\bin',
                r'C:\Program Files (x86)\Git\usr\bin',
                r'C:\msys64\usr\bin',
                r'C:\cygwin64\bin',
                r'C:\Windows\System32',
                r'C:\Windows'
            ]
            # Add ARM GCC common locations
            program_files = [r'C:\Program Files', r'C:\Program Files (x86)']
            for pf in program_files:
                if os.path.exists(pf):
                    try:
                        for item in os.listdir(pf):
                            if 'arm' in item.lower() and 'gcc' in item.lower():
                                common_dirs.append(os.path.join(pf, item, 'bin'))
                            elif 'gnu' in item.lower() and 'arm' in item.lower():
                                common_dirs.append(os.path.join(pf, item, 'bin'))
                    except (PermissionError, OSError):
                        pass

            path_dirs.extend([d for d in common_dirs if os.path.exists(d)])
        else:
            # Add common Unix directories
            common_dirs = [
                '/usr/local/bin',
                '/usr/bin',
                '/bin',
                '/opt/bin',
                '/opt/toolchains'
            ]
            path_dirs.extend([d for d in common_dirs if os.path.exists(d)])

        # Search for the executable
        for directory in path_dirs:
            if not directory or not os.path.isdir(directory):
                continue

            for ext in extensions:
                full_path = os.path.join(directory, name + ext)
                if is_executable(full_path):
                    return full_path

        return None

    def _run_command(self, cmd, cwd=None, env=None, shell=None):
        """Run command with proper shell handling for Windows/Unix"""
        if shell is None:
            shell = self.is_windows

        try:
            result = subprocess.run(
                cmd,
                cwd=cwd,
                env=env,
                shell=shell,
                capture_output=True,
                text=True
            )
            return result
        except Exception as e:
            print(f"Error running command {cmd}: {e}")
            return None
    def _find_arm_gcc_dir(self):
        """Find ARM GCC installation directory"""
        candidates = []

        # First, check if ARMGCC_DIR environment variable is set
        env_armgcc_dir = os.environ.get('ARMGCC_DIR')
        if env_armgcc_dir:
            gcc_executable = os.path.join(env_armgcc_dir, 'bin', 'arm-none-eabi-gcc')
            if self.is_windows:
                gcc_executable += '.exe'

            if os.path.exists(gcc_executable):
                print(f"Found ARMGCC_DIR from environment variable: {env_armgcc_dir}")
                return env_armgcc_dir

        # Try to find arm-none-eabi-gcc in PATH
        gcc_path = self._find_executable("arm-none-eabi-gcc")

        if gcc_path:
            # Add the toolchain root to candidates
            toolchain_root = str(Path(gcc_path).parent.parent)
            candidates.append(toolchain_root)

        # Try common installation locations
        if self.is_windows:
            common_locations = [
                r'C:\Program Files\GNU Arm Embedded Toolchain',
                r'C:\Program Files (x86)\GNU Arm Embedded Toolchain',
                r'C:\Program Files\GNU Tools ARM Embedded',
                r'C:\Program Files (x86)\GNU Tools ARM Embedded',
                r'C:\arm-none-eabi',
                r'C:\gcc-arm-none-eabi',
            ]

            # Add home directory toolchains
            home_dir = Path.home()
            toolchains_dir = home_dir / 'toolchains'
            if toolchains_dir.exists():
                try:
                    for item in os.listdir(toolchains_dir):
                        item_lower = item.lower()
                        if any(keyword in item_lower for keyword in ['arm', 'gcc', 'gnu']):
                            common_locations.append(str(toolchains_dir / item))
                except (PermissionError, OSError):
                    pass

            # Add .mcuxpressotools directory
            mcuxpresso_tools_dir = home_dir / '.mcuxpressotools'
            if mcuxpresso_tools_dir.exists():
                try:
                    for item in os.listdir(mcuxpresso_tools_dir):
                        item_lower = item.lower()
                        if any(keyword in item_lower for keyword in ['arm', 'gcc', 'gnu']):
                            common_locations.append(str(mcuxpresso_tools_dir / item))
                except (PermissionError, OSError):
                    pass

            # Add MCUXpresso IDE tools
            nxp_dir = Path(r'C:\nxp')
            if nxp_dir.exists():
                try:
                    for item in os.listdir(nxp_dir):
                        if item.startswith('MCUXpressoIDE'):
                            tools_dir = nxp_dir / item / 'ide' / 'tools'
                            if tools_dir.exists():
                                common_locations.append(str(tools_dir))
                except (PermissionError, OSError):
                    pass

            for base_path in common_locations:
                if os.path.exists(base_path):
                    try:
                        # Look for version subdirectories
                        for item in os.listdir(base_path):
                            full_path = os.path.join(base_path, item)
                            if os.path.isdir(full_path):
                                gcc_bin = os.path.join(full_path, 'bin', 'arm-none-eabi-gcc.exe')
                                if os.path.exists(gcc_bin):
                                    candidates.append(full_path)

                        # Check if gcc is directly in this directory
                        gcc_bin = os.path.join(base_path, 'bin', 'arm-none-eabi-gcc.exe')
                        if os.path.exists(gcc_bin):
                            candidates.append(base_path)
                    except (PermissionError, OSError):
                        continue
        else:
            common_locations = [
                '/usr/local/arm-none-eabi',
                '/opt/arm-none-eabi',
                '/usr/arm-none-eabi',
                '/opt/gcc-arm-none-eabi',
                '/usr/local/gcc-arm-none-eabi',
            ]

            # Add home directory toolchains
            home_dir = Path.home()
            toolchains_dir = home_dir / 'toolchains'
            if toolchains_dir.exists():
                try:
                    for item in os.listdir(toolchains_dir):
                        item_lower = item.lower()
                        if any(keyword in item_lower for keyword in ['arm', 'gcc', 'gnu']):
                            common_locations.append(str(toolchains_dir / item))
                except (PermissionError, OSError):
                    pass

            # Add .mcuxpressotools directory
            mcuxpresso_tools_dir = home_dir / '.mcuxpressotools'
            if mcuxpresso_tools_dir.exists():
                try:
                    for item in os.listdir(mcuxpresso_tools_dir):
                        item_lower = item.lower()
                        if any(keyword in item_lower for keyword in ['arm', 'gcc', 'gnu']):
                            common_locations.append(str(mcuxpresso_tools_dir / item))
                except (PermissionError, OSError):
                    pass

            # Add MCUXpresso IDE tools
            nxp_dir = Path(r'/usr/local')
            if nxp_dir.exists():
                try:
                    for item in os.listdir(nxp_dir):
                        if item.startswith('mcuxpressoide'):
                            tools_dir = nxp_dir / item / 'ide' / 'tools'
                            if tools_dir.exists():
                                common_locations.append(str(tools_dir))
                except (PermissionError, OSError):
                    pass

            for base_path in common_locations:
                if os.path.exists(base_path):
                    try:
                        # Look for version subdirectories
                        for item in os.listdir(base_path):
                            full_path = os.path.join(base_path, item)
                            if os.path.isdir(full_path):
                                gcc_bin = os.path.join(full_path, 'bin', 'arm-none-eabi-gcc')
                                if os.path.exists(gcc_bin):
                                    candidates.append(full_path)

                        # Check if gcc is directly in this directory
                        gcc_bin = os.path.join(base_path, 'bin', 'arm-none-eabi-gcc')
                        if os.path.exists(gcc_bin):
                            candidates.append(base_path)
                    except (PermissionError, OSError):
                        continue

        # If we have candidates, find the newest version
        if candidates:
            # Remove duplicates while preserving order
            candidates = list(dict.fromkeys(candidates))

            # Try to extract version numbers and sort
            versioned_candidates = []
            for candidate in candidates:
                # Try to extract version from path or by running gcc --version
                version_str = None

                # First try to extract from path (e.g., "10 2021.10" or "9-2020-q2")
                import re
                version_match = re.search(r'(\d+)[.\-\s]+(\d+)[.\-\s]*(\d*)', candidate)
                if version_match:
                    try:
                        major = int(version_match.group(1))
                        minor = int(version_match.group(2))
                        patch = int(version_match.group(3)) if version_match.group(3) else 0
                        version_str = (major, minor, patch)
                    except ValueError:
                        pass

                # If no version in path, try running gcc --version
                if not version_str:
                    gcc_executable = os.path.join(candidate, 'bin', 'arm-none-eabi-gcc')
                    if self.is_windows:
                        gcc_executable += '.exe'

                    if os.path.exists(gcc_executable):
                        try:
                            result = subprocess.run(
                                [gcc_executable, '--version'],
                                capture_output=True,
                                text=True,
                                timeout=5
                            )
                            if result.returncode == 0:
                                # Parse version from output (e.g., "arm-none-eabi-gcc (GNU Arm Embedded Toolchain 10.3-2021.10) 10.3.1")
                                version_match = re.search(r'(\d+)\.(\d+)\.(\d+)', result.stdout)
                                if version_match:
                                    major = int(version_match.group(1))
                                    minor = int(version_match.group(2))
                                    patch = int(version_match.group(3))
                                    version_str = (major, minor, patch)
                        except (subprocess.TimeoutExpired, Exception):
                            pass

                # Add to list with version (or use 0,0,0 if no version found)
                versioned_candidates.append((version_str or (0, 0, 0), candidate))

            # Sort by version (newest first)
            versioned_candidates.sort(reverse=True, key=lambda x: x[0])

            # Return the newest version
            return versioned_candidates[0][1]

        return None

    def setup_toolchain_and_sdk_dir(self, board):
        """Setup toolchain and determine core ID based on board"""
        # Find ARMGCC directory if not set
        if not self.armgcc_dir:
            self.armgcc_dir = self._find_arm_gcc_dir()

        if not self.armgcc_dir:
            print("Error: ARMGCC_DIR not found and no arm-none-eabi-gcc found in common locations")
            print("Please set the ARMGCC_DIR environment variable to your ARM GCC installation directory")
            if self.is_windows:
                print("Example for Windows: ARMGCC_DIR=C:\\Program Files (x86)\\GNU Arm Embedded Toolchain\\10 2021.10")
            else:
                print("Example for Linux: ARMGCC_DIR=/usr/local/gcc-arm-none-eabi")
            sys.exit(1)

        print(f"Using ARMGCC_DIR: {self.armgcc_dir}")

        # Verify the toolchain works
        gcc_executable = os.path.join(self.armgcc_dir, 'bin', 'arm-none-eabi-gcc')
        if self.is_windows:
            gcc_executable += '.exe'

        if not os.path.exists(gcc_executable):
            print(f"Error: arm-none-eabi-gcc not found at {gcc_executable}")
            print("Please check your ARMGCC_DIR setting")
            sys.exit(1)

        if board not in self.board_core_map:
            print(f"Fail sdk board name: {board}")
            sys.exit(1)

        # Set default core ID
        self.core_id = self.board_core_map[board]["0"]

    def parse_app_config(self, app_type, app_name):
        """Parse application configuration"""
        if not self.app_config_index:
            return ""

        config_file = self.w_dir / "boards" / self.board / app_type / app_name / f"{app_name}.conf"
        parse_script = self.w_dir / "tools" / "mpp_parse_configs.sh"

        if not config_file.exists():
            config_file = self.w_dir / "boards" / self.board / app_type / app_name / self.core_id / f"{app_name}.conf"

        if parse_script.exists() and config_file.exists():
            try:
                bash_cmd = self._find_executable("bash")
                if bash_cmd:
                    cmd = [bash_cmd, str(parse_script), str(config_file), self.app_config_index]
                    result = self._run_command(cmd, cwd=self.w_dir)
                    if result and result.returncode == 0:
                        configs = result.stdout.strip()
                        if configs:
                            print(f"Found app config {self.app_config_index}: {configs}")
                            return configs
                else:
                    print("Warning: bash not found, skipping app config parsing")
            except Exception as e:
                print(f"Warning: Could not parse app config: {e}")
        return ""

    def get_examples_and_tests(self, board, exp, test):
        """Get list of examples and tests to build"""
        examples = []
        tests = []

        def parse_config_line(line):
            """Parse a config line to extract name and arguments"""
            line = line.strip()
            if not line or line.startswith('#'):
                return None
            parts = line.split(None, 1)  # Split on first whitespace
            if len(parts) == 1:
                return [parts[0], ""]
            else:
                return [parts[0], parts[1]]

        # Get examples
        if exp == "all":
            examples_file = self.w_dir / "boards" / board / "examples.conf"
            examples_internal_file = self.w_dir / "boards" / board / "examples_internal.conf"

            if examples_file.exists():
                try:
                    with open(examples_file, 'r') as f:
                        for line in f:
                            parsed = parse_config_line(line)
                            if parsed:
                                examples.append(parsed)
                except Exception as e:
                    print(f"Warning: Could not read examples.conf: {e}")

            internal_dir = self.w_dir / "internal"
            if internal_dir.exists() and examples_internal_file.exists():
                try:
                    with open(examples_internal_file, 'r') as f:
                        for line in f:
                            parsed = parse_config_line(line)
                            if parsed:
                                examples.append(parsed)
                except Exception as e:
                    print(f"Warning: Could not read examples_internal.conf: {e}")
        elif exp:
            examples = [[exp, ""]]

        # Get tests
        if test == "all":
            tests_file = self.w_dir / "boards" / board / "tests.conf"
            tests_internal_file = self.w_dir / "boards" / board / "tests_internal.conf"

            if tests_file.exists():
                try:
                    with open(tests_file, 'r') as f:
                        for line in f:
                            parsed = parse_config_line(line)
                            if parsed:
                                tests.append(parsed)
                except Exception as e:
                    print(f"Warning: Could not read tests.conf: {e}")

            internal_dir = self.w_dir / "internal"
            if internal_dir.exists() and tests_internal_file.exists():
                try:
                    with open(tests_internal_file, 'r') as f:
                        for line in f:
                            parsed = parse_config_line(line)
                            if parsed:
                                tests.append(parsed)
                except Exception as e:
                    print(f"Warning: Could not read tests_internal.conf: {e}")
        elif test:
            tests = [[test, ""]]

        return examples, tests

    def get_panel_config_define(self, board, panel):
        """Get panel configuration define"""
        if board == "frdmmcxn947":
            return ""

        display_support_file = self.sdk_dir / "examples" / "_boards" / board / "display_support.h"

        if not display_support_file.exists():
            return ""

        try:
            with open(display_support_file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            # Find panel name
            import re
            pattern = rf"define DEMO_PANEL_(\w+) {panel}"
            match = re.search(pattern, content)

            if match:
                panel_name = match.group(1)
                if panel_name == "RK055MHD091":
                    return f"-DCONFIG_{panel_name}A0=y"
                else:
                    return f"-DCONFIG_{panel_name}=y"
        except Exception as e:
            print(f"Warning: Could not read display_support.h: {e}")

        return ""

    def build_app(self, app, app_type, board, build_type, core_id, panel_config_define,
                  log_level, extra_build_flags, app_args=""):
        """Build a single application"""
        if self.sysbuild or "--sysbuild" in app_args:
            build_path = Path("build") / app
        else:
            build_path = Path("build")

        # Remove existing build directory
        build_dir = Path("build")
        if build_dir.exists():
            try:
                shutil.rmtree(build_dir)
            except Exception as e:
                print(f"Warning: Could not remove build directory: {e}")

        # Parse app config for tests
        if app_type == "tests":
            config_flags = self.parse_app_config("tests", app)
            if config_flags:
                if extra_build_flags:
                    extra_build_flags = f"{extra_build_flags} {config_flags}"
                else:
                    extra_build_flags = config_flags

        # Determine source path
        if app_type == "examples":
            source_path = f"examples/eiq_examples/mpp/{app}"
        else:  # tests
            source_path = f"middleware/eiq/mpp/tests/{app}"

        source_path_dir = Path(source_path) / self.app_core_folder

        if source_path_dir.exists():
            source_path = str(source_path_dir)

        # Build west command
        west_cmd = [
            "west", "build", "-b", board, source_path, "-p", "always",
            "--config", build_type,
            "--toolchain", "armgcc",
            f"-Dcore_id={core_id}",
            f"-DHAL_LOG_LEVEL={log_level}",
            f"-DMPP_COMMIT={self.mpp_commit_id}"
        ]

        if self.code_coverage_enable:
            west_cmd.append("-DENABLE_COVERAGE=1")

        if panel_config_define:
            west_cmd.append(panel_config_define)

        if extra_build_flags:
            west_cmd.append(f"-DEXTRA_CFLAGS={extra_build_flags}")
            west_cmd.append(f"-DEXTRA_CXXFLAGS={extra_build_flags}")

        # Add app-specific arguments to extra build flags
        if app_args:
            west_cmd.append(app_args)

        if self.sysbuild:
            west_cmd.append("--sysbuild")

        if self.sysbuild or "--sysbuild" in app_args:
            west_cmd.append(f"-D{app}_core1_HAL_LOG_LEVEL={log_level}")
            west_cmd.append(f"-D{app}_core1_MPP_COMMIT={self.mpp_commit_id}")
            if self.code_coverage_enable:
                west_cmd.append(f"-D{app}_core1_ENABLE_COVERAGE=1")
            if extra_build_flags:
                west_cmd.append(f"-D{app}_core1_EXTRA_CFLAGS={extra_build_flags}")
                west_cmd.append(f"-D{app}_core1_EXTRA_CXXFLAGS={extra_build_flags}")

        print(f"Building {app}...")
        print(f"Command: {' '.join(west_cmd)}")

        # Set up environment
        env = os.environ.copy()
        env["ARMGCC_DIR"] = self.armgcc_dir

        result = subprocess.run(west_cmd, cwd=self.sdk_dir, env=env)

        if result.returncode != 0:
            print(f"Failed to build {app}")
            return False

        # Copy built files
        build_output_dir = self.sdk_dir / f"build_{board}" / self.build_rel_or_dbg
        build_output_dir.mkdir(parents=True, exist_ok=True)

        if self.app_config_index:
            elf_src = self.sdk_dir / build_path / f"{app}_{core_id}.elf"
            bin_src = self.sdk_dir / build_path / f"{app}_{core_id}.bin"
            elf_dst = build_output_dir / f"{app}_{core_id}_config{self.app_config_index}.elf"
            bin_dst = build_output_dir / f"{app}_{core_id}_config{self.app_config_index}.bin"
            self.last_built_elf = f"{app}_{core_id}_config{self.app_config_index}.elf"
        else:
            elf_src = self.sdk_dir / build_path / f"{app}_{core_id}.elf"
            bin_src = self.sdk_dir / build_path / f"{app}_{core_id}.bin"
            elf_dst = build_output_dir / f"{app}_{core_id}.elf"
            bin_dst = build_output_dir / f"{app}_{core_id}.bin"
            self.last_built_elf = f"{app}_{core_id}.elf"

        # Copy files
        try:
            if elf_src.exists():
                shutil.copy2(elf_src, elf_dst)
                print(f"Copied {elf_src} to {elf_dst}")
            if bin_src.exists():
                shutil.copy2(bin_src, bin_dst)
                print(f"Copied {bin_src} to {bin_dst}")
        except Exception as e:
            print(f"Warning: Could not copy build artifacts: {e}")

        return True

    def flash_built_image(self, board):
        try:
            from auto_test import flash_files, auto_detect_probe # type: ignore
        except Exception as e:
            print(f"Could not import auto_test module --> Error: {str(e)}")
            return False

        """Flash the last built image to the board"""
        if not self.last_built_elf:
            print("Error: No ELF file was built to flash")
            return False

        build_output_dir = self.sdk_dir / f"build_{board}" / self.build_rel_or_dbg
        elf_path = build_output_dir / self.last_built_elf
        
        if not elf_path.exists():
            print(f"Error: Built ELF file not found: {elf_path}")
            return False

        print(f"\nFlashing {self.last_built_elf} to board {board}...")

        # Auto-detect probe and probe type
        current_os = platform.system()
        flash_log_file = None  # Use console output for flash logs

        probe_id, probe_type = auto_detect_probe(self.probe_id, board, current_os, flash_log_file)

        # Flash the file
        success = flash_files(
            str(elf_path),
            board,
            probe_id,
            probe_type,
            self.use_gdb,
            flash_log_file,
            self.jlinkscript
        )

        if success:
            print(f"Successfully flashed {self.last_built_elf}")
        else:
            print(f"Failed to flash {self.last_built_elf}")

        return success

    def build(self, board, panel, build_rel_or_dbg, build_type, log_level,
              extra_build_flags, exp, test):
        """Main build function"""
        self.board = board
        self.build_rel_or_dbg = build_rel_or_dbg

        print(f"Platform: {platform.system()}")
        print(f"BOARD={board}")
        print(f"EXAMPLE={exp}")
        print(f"TEST={test}")
        print(f"BUILD_TYPE={build_type}")
        print(f"PANEL={panel}")
        print(f"APP_CONFIG_INDEX={self.app_config_index}")
        print(f"EXTRA_BUILD_FLAGS={extra_build_flags}")
        print(f"MPP_COMMIT_ID={self.mpp_commit_id}")

        self.setup_toolchain_and_sdk_dir(board)

        if self.input_core_id:
            try:
                self.core_id = self.board_core_map[self.board][str(self.input_core_id)]
            except KeyError:
                self.core_id = self.board_core_map[self.board["0"]]
            if str(self.input_core_id) == "1":
                self.app_core_folder = "core1"
            else:
                self.app_core_folder = "core0"

        examples, tests = self.get_examples_and_tests(board, exp, test)
        panel_config_define = self.get_panel_config_define(board, panel)

        original_cwd = os.getcwd()
        try:
            os.chdir(self.sdk_dir)

            # Build examples
            for example_entry in examples:
                app_name = example_entry[0]
                app_args = example_entry[1]
                success = self.build_app(
                    app_name, "examples", board, build_type, self.core_id,
                    panel_config_define, log_level, extra_build_flags, app_args
                )
                if not success:
                    return False
                
                # Flash after building if requested
                if self.flash_after_build:
                    flash_success = self.flash_built_image(board)
                    if not flash_success:
                        print("Warning: Flash failed, continuing with next build...")

            # Build tests
            for test_entry in tests:
                app_name = test_entry[0]
                app_args = test_entry[1]
                success = self.build_app(
                    app_name, "tests", board, build_type, self.core_id,
                    panel_config_define, log_level, extra_build_flags, app_args
                )
                if not success:
                    return False

                # Flash after building if requested
                if self.flash_after_build:
                    flash_success = self.flash_built_image(board)
                    if not flash_success:
                        print("Warning: Flash failed, continuing with next build...")
        finally:
            os.chdir(original_cwd)

        self.get_version(board)
        return True

    def get_version(self, board):
        """Extract version from built ELF"""
        if not self.last_built_elf:
            return

        build_output_dir = self.sdk_dir / f"build_{board}" / self.build_rel_or_dbg
        elf_path = build_output_dir / self.last_built_elf

        if elf_path.exists():
            try:
                # Try to find strings command
                strings_cmd = self._find_executable("strings")

                if strings_cmd:
                    result = self._run_command([strings_cmd, str(elf_path)])
                    if result and result.returncode == 0:
                        for line in result.stdout.split('\n'):
                            if "MPP_VERSION" in line:
                                version_file = build_output_dir / "mpp_version.txt"
                                with open(version_file, 'w') as f:
                                    f.write(line)
                                print(f"Version extracted: {line}")
                                break
                else:
                    print("Warning: strings command not available, skipping version extraction")
            except Exception as e:
                print(f"Warning: Could not extract version: {e}")

    def build_api_doc(self, api_name, header_file, api_version, doxyfile_name, output_file_name):
        """Build API documentation"""
        # Check if doxygen is available
        doxygen_cmd = self._find_executable("doxygen")
        if not doxygen_cmd:
            print("Warning: doxygen not found, skipping documentation generation")
            return

        temp_dir = Path(tempfile.mkdtemp())
        pdf_path = temp_dir / "refman.pdf"
        rtf_path = Path("rtf") / "refman.rtf"

        try:
            # Copy dox directory
            dox_src = self.w_dir / "dox"
            dox_dst = temp_dir / "dox"
            if dox_src.exists():
                shutil.copytree(dox_src, dox_dst)

            # Set environment variables
            env = os.environ.copy()
            env["LATEX_OUTPUT"] = str(temp_dir)
            env["LATEX_HEADER"] = str(temp_dir / "dox" / header_file)
            env["PROJECT_NUMBER"] = api_version

            # Update header file
            header_path = temp_dir / "dox" / header_file
            if header_path.exists():
                try:
                    with open(header_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                    content = content.replace(f"{api_name} VERSION", f"{api_name} VERSION {api_version}")
                    with open(header_path, 'w', encoding='utf-8') as f:
                        f.write(content)
                except Exception as e:
                    print(f"Warning: Could not update header file: {e}")

            # Run doxygen
            doxyfile_path = self.w_dir / "dox" / doxyfile_name
            if doxyfile_path.exists():
                result = subprocess.run([doxygen_cmd, str(doxyfile_path)], env=env, cwd=self.w_dir)
                if result.returncode != 0:
                    print(f"Warning: doxygen failed for {doxyfile_name}")

            # Generate PDF (if make is available)
            make_cmd = self._find_executable("make")
            if make_cmd and not self.is_windows:
                subprocess.run([make_cmd, "pdf"], cwd=temp_dir)
            elif self.is_windows:
                print("Warning: PDF generation skipped on Windows (make not available)")

            # Copy output files
            if pdf_path.exists():
                shutil.copy2(pdf_path, f"{output_file_name}.pdf")
                print(f"Generated {output_file_name}.pdf")

            if rtf_path.exists():
                shutil.copy2(rtf_path, f"{output_file_name}.rtf")
                shutil.rmtree("rtf", ignore_errors=True)
                print(f"Generated {output_file_name}.rtf")

        except Exception as e:
            print(f"Warning: Documentation generation failed: {e}")
        finally:
            # Cleanup
            shutil.rmtree(temp_dir, ignore_errors=True)

    def build_doc(self, board):
        """Build MPP and HAL API documentation"""
        version_file = self.sdk_dir / f"build_{board}" / self.build_rel_or_dbg / "mpp_version.txt"

        mpp_version = ""
        if version_file.exists():
            try:
                with open(version_file, 'r') as f:
                    content = f.read().strip()
                    # Extract version from MPP_VERSION_x.x.x format
                    if "_" in content:
                        mpp_version = content.split("_")[-1]
            except Exception as e:
                print(f"Warning: Could not read version file: {e}")

        if not mpp_version:
            print("Warning: mpp version not found, using empty version")

        # Build MPP documentation
        self.build_api_doc("MPP", "header.tex", mpp_version, "Doxyfile", "mpp_api")

        # Build HAL documentation
        self.build_api_doc("MPP-HAL", "hal_header.tex", mpp_version, "HalDoxyfile", "hal_api")

    def list_panels(self, boards):
        """List supported panels for boards"""
        if boards == ["all"]:
            temp_boards = ["frdmmcxn947", "evkbmimxrt1170", "mimxrt700evk"]
        else:
            temp_boards = boards

        panel_info = []
        for board in temp_boards:
            if board == "frdmmcxn947":
                continue

            display_support_file = self.sdk_dir / "examples" / "_boards" / board / "display_support.h"

            if display_support_file.exists():
                panel_info.append(f"\nPanel list supported for board {board}")
                try:
                    with open(display_support_file, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                    panels = re.findall(r'^.*define DEMO_PANEL_(?!(?:HEIGHT|WIDTH)\b)\w+.*', content, re.MULTILINE)
                    for panel in panels:
                        panel_info.append(panel)
                except Exception as e:
                    panel_info.append(f"Error reading panel info: {e}")
                panel_info.append("")

        return "\n".join(panel_info)

def get_available_examples_tests(builder, board=None):
    """Get available examples and tests for a given board"""
    available_examples = ["all"]
    available_tests = ["all"]

    if board and board != "all":
        # Get examples from directory structure
        examples_dir = builder.w_dir / "boards" / board / "examples"
        if examples_dir.exists() and examples_dir.is_dir():
            try:
                for item in examples_dir.iterdir():
                    if item.is_dir() and not item.name.startswith('.'):
                        available_examples.append(item.name)
            except Exception as e:
                print(f"Warning: Could not read examples directory: {e}")

        # Get tests from directory structure
        tests_dir = builder.w_dir / "boards" / board / "tests"
        if tests_dir.exists() and tests_dir.is_dir():
            try:
                for item in tests_dir.iterdir():
                    if item.is_dir() and not item.name.startswith('.'):
                        available_tests.append(item.name)
            except Exception as e:
                print(f"Warning: Could not read tests directory: {e}")
    else:
        # For "all" boards or no board specified, collect examples and tests from all boards
        for board_name in ["evkbmimxrt1170", "frdmmcxn947", "mimxrt700evk"]:
            # Get examples from directory structure
            examples_dir = builder.w_dir / "boards" / board_name / "examples"
            if examples_dir.exists() and examples_dir.is_dir():
                try:
                    for item in examples_dir.iterdir():
                        if item.is_dir() and not item.name.startswith('.'):
                            available_examples.append(item.name)
                except Exception:
                    pass

            # Get tests from directory structure
            tests_dir = builder.w_dir / "boards" / board_name / "tests"
            if tests_dir.exists() and tests_dir.is_dir():
                try:
                    for item in tests_dir.iterdir():
                        if item.is_dir() and not item.name.startswith('.'):
                            available_tests.append(item.name)
                except Exception:
                    pass

        # Remove duplicates while preserving order
        available_examples = list(dict.fromkeys(available_examples))
        available_tests = list(dict.fromkeys(available_tests))

    # Remove duplicates while preserving order
    available_examples = list(dict.fromkeys(available_examples))
    available_tests = list(dict.fromkeys(available_tests))

    return available_examples, available_tests


class ExampleCompleter:
    """Custom completer for examples based on board selection"""
    def __init__(self, builder):
        self.builder = builder

    def __call__(self, prefix, parsed_args, **kwargs):
        board = getattr(parsed_args, 'board', 'evkbmimxrt1170')
        examples, _ = get_available_examples_tests(self.builder, board)
        return [""] + examples


class TestCompleter:
    """Custom completer for tests based on board selection"""
    def __init__(self, builder):
        self.builder = builder

    def __call__(self, prefix, parsed_args, **kwargs):
        board = getattr(parsed_args, 'board', 'evkbmimxrt1170')
        _, tests = get_available_examples_tests(self.builder, board)
        return [""] + tests


def main():
    builder = MPPBuilder()

    # Get log levels from CMakeLists.txt
    log_levels = ""
    cmake_file = builder.w_dir / "CMakeLists.txt"
    if cmake_file.exists():
        try:
            with open(cmake_file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            import re
            log_levels = "\n".join(re.findall(r'LOG_LVL_\w+.*', content, re.MULTILINE))
        except Exception as e:
            print(f"Warning: Could not read CMakeLists.txt: {e}")

    # Define supported boards
    supported_boards = ["evkbmimxrt1170", "frdmmcxn947", "mimxrt700evk", "all"]

    parser = argparse.ArgumentParser(description="MPP Build Script", formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("-a", "--rebuild-tflm", action="store_true",
                        help="rebuild libtflm.a from source")
    parser.add_argument("-b", "--board", default="evkbmimxrt1170",
                        choices=supported_boards,
                        help="board name: {evkbmimxrt1170, frdmmcxn947, mimxrt700evk, all}")
    parser.add_argument("-e", "--example", default="",
                        help="build the example app {camera_view, all, ...}").completer = ExampleCompleter(builder)
    parser.add_argument("-i", "--host", action="store_true",
                        help="build for host (x86)")
    parser.add_argument("-d", "--log-level", default="0",
                        help=f"log level (use the respective number from below list):\n{log_levels}")
    parser.add_argument("-D", "--doc", action="store_true",
                        help="build mpp and hal APIs documentation")
    parser.add_argument("-p", "--panel", default="",
                        help="panel index as follows (use the respective number from below list):\n" +
                        builder.list_panels(["all"]))
    parser.add_argument("-c", "--config", default="release",
                        help="build/config type: {debug, release}")
    parser.add_argument("-f", "--flags", default="",
                        help="extra build flags")
    parser.add_argument("-t", "--test", default="",
                        help="build the test app {test_image_display, all, ...}").completer = TestCompleter(builder)
    parser.add_argument("-s", "--sdk-path", default=None,
                        help="specify the path to the mcuxsdk folder")
    parser.add_argument("-g", "--app-config", default=None,
                        help="the index of the app_config to be used")
    parser.add_argument("-C", "--core-id", default=None,
                        help="specify the core id you want to build app for (values like 0, 1, 2...)")
    parser.add_argument("-S", "--sysbuild", action="store_true",
                        help="add --sysbuild option to the build command")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="enable verbose for build")
    parser.add_argument("-F", "--flash", action="store_true",
                        help="flash the built image to the board after building")
    parser.add_argument("-P", "--probe-id", default=None,
                        help="probe ID for flashing (auto-detected if not provided)")
    parser.add_argument("-G", "--use-gdb", action="store_true",
                        help="use GDB server for flashing (only supported with jlink)")
    parser.add_argument("-J", "--jlinkscript", default=None,
                        help="path to JLink script file for GDB server (valid only when use_gdb is set)")
    parser.add_argument("--debug_console", default="0", 
                        choices=DEBUG_CONSOLE_DEFINES.keys(),
                        help="\n".join([f'{k}: {v["help"]}' for k, v in DEBUG_CONSOLE_DEFINES.items()]))
    parser.add_argument("-V", "--code-coverage", action="store_true",
                        help="enable code coverage analysis during build")

    argcomplete.autocomplete(parser)

    args = parser.parse_args()

    # Validate example and test choices after parsing
    board_for_validation = args.board
    available_examples, available_tests = get_available_examples_tests(builder, board_for_validation)

    if args.example and args.example not in [""] + available_examples:
        parser.error(f"argument -e/--example: invalid choice: '{args.example}' (choose from {', '.join([''] + available_examples)})")

    if args.test and args.test not in [""] + available_tests:
        parser.error(f"argument -t/--test: invalid choice: '{args.test}' (choose from {', '.join([''] + available_tests)})")

    # Set SDK directory if provided
    if args.sdk_path:
        builder.sdk_dir = Path(args.sdk_path).resolve() / "mcuxsdk"

    if not builder.sdk_dir.exists():
        print(f"sdk directory {builder.sdk_dir} does not exist")
        print("specify the sdk dir using -s option")
        sys.exit(1)

    print(f"SDK_DIR={builder.sdk_dir}")
    # Set builder parameters
    builder.app_config_index = args.app_config or ""
    builder.input_core_id = args.core_id or ""
    builder.sysbuild = "--sysbuild" if args.sysbuild else ""
    builder.gen_doc = args.doc
    builder.code_coverage_enable = args.code_coverage

    # Set flash parameters
    builder.flash_after_build = args.flash
    builder.probe_id = args.probe_id
    builder.use_gdb = args.use_gdb
    builder.jlinkscript = args.jlinkscript

    # Handle board list
    boards = args.board.split() if args.board != "all" else ["frdmmcxn947", "evkbmimxrt1170", "mimxrt700evk"]

    # Set default example if neither example nor test specified
    exp = args.example
    test = args.test
    if not exp and not test:
        exp = "camera_view"

    args.flags = args.flags.strip("\'\" ") if args.flags else ""

    if DEBUG_CONSOLE_DEFINES[args.debug_console]["flags"]:
        if args.flags:
            args.flags = args.flags + " " + DEBUG_CONSOLE_DEFINES[args.debug_console]["flags"]
        else:
            args.flags = DEBUG_CONSOLE_DEFINES[args.debug_console]["flags"]

    # Build for each board
    for board in boards:
        # Adjust build type based on board
        build_rel_or_dbg = args.config
        if board  == "mimxrt700evk":
            if args.core_id == "1":
                build_type = build_rel_or_dbg
            else:
                build_type = f"flash_{build_rel_or_dbg}"
        elif board == "frdmmcxn947":
            build_type = build_rel_or_dbg
        else:
            if args.core_id  == "1":
                build_type = build_rel_or_dbg
            else:
                build_type = f"flexspi_nor_sdram_{build_rel_or_dbg}"

        # Set default panel
        if board in ["evkbmimxrt1170", "mimxrt700evk"]:
            default_panel = "2"
        else:
            default_panel = ""

        panel = args.panel or default_panel

        # Run build
        success = builder.build(
            board, panel, build_rel_or_dbg, build_type,
            args.log_level, args.flags, exp, test
        )

        if not success:
            sys.exit(1)

    # Generate documentation
    if builder.gen_doc:
        builder.build_doc(boards[0])  # Use first board for doc generation

if __name__ == "__main__":
    main()
