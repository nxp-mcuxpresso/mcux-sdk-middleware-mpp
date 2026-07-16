#!/usr/bin/env python3

"""
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
"""

"""
convert_models.py - Convert TFLite models using the Neutron compiler.

Cross-platform equivalent of convert_models.sh.
On Linux the compiler binary is 'neutron-compiler'.
On Windows the compiler binary is 'neutron-compiler.exe'.

Environment variables required:
    CONVERTER_RT700_PATH  - path to the neutron-compiler for imxrt700 (npu64)
    CONVERTER_MCXN_PATH   - path to the neutron-compiler for mcxn94x  (npu16)

To add a new model, append an entry to the MODELS list below.
Each entry is a dict with the following keys:
    basename  - stem of the .tflite file (no extension)
    model_dir - path relative to MPP_DIR where the .tflite file lives
    h_name    - variable-name prefix used in the generated .h header file
                (omit or set to None to reuse 'basename')
    chips     - list of chips to target; omit to target all chips
                supported values: "mcxn94x", "imxrt700"
"""

import os
import re
import sys
import shutil
import platform
import subprocess
import tempfile
from pathlib import Path

# ---------------------------------------------------------------------------
# Model list - edit this section to add / remove models
# ---------------------------------------------------------------------------
MODELS = [
    {
        "basename":  "ultraface_slim_ultraslim",
        "model_dir": "models/ultraface_slim_quant_int8",
        "h_name":    None,  # same as basename
        "chips":     ["mcxn94x", "imxrt700"],
    },
    {
        "basename":  "nanodet_m_0.5x_nhwc_nopermute",
        "model_dir": "models/nanodet_m_320_quant_int8",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "mobilenet_v1_0.25_128_quant_int8",
        "model_dir": "models/mobilenet_v1_0.25_128_quant_int8",
        "h_name":    "mobilenetv1_model_data",
        "chips":     ["mcxn94x", "imxrt700"],
    },
    {
        "basename":  "persondetect_160_128",
        "model_dir": "models/persondetect",
        "h_name":    None,
        "chips":     ["mcxn94x", "imxrt700"],
    },
    {
        "basename":  "persondetect_220_220",
        "model_dir": "models/persondetect",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "antispoofing",
        "model_dir": "models/antispoofing",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "mobilefacenet_96_96",
        "model_dir": "models/mobilefacenet",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "scrfd_kps_500m_full_integer_quant_128x128",
        "model_dir": "models/scrfd_kps_500m_full_integer_quant",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "scrfd_kps_500m_full_integer_quant_320x256",
        "model_dir": "models/scrfd_kps_500m_full_integer_quant",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "scrfd_kps_500m_full_integer_quant_256x256",
        "model_dir": "models/scrfd_kps_500m_full_integer_quant",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "blaze_detector_ptq_int8",
        "model_dir": "models/blaze_detector_ptq",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "canned_gesture_classifier_i8_opt",
        "model_dir": "models/canned_gesture_classifier",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "gesture_embedder1_i8_opt",
        "model_dir": "models/gesture_embedder",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "hand_landmark_quant_int8",
        "model_dir": "models/hand_landmark",
        "h_name":    None,
        "chips":     ["imxrt700"],
    },
    {
        "basename":  "palm_detection_lite_int8",
        "model_dir": "models/blaze_detector_ptq",
        "h_name":    None,
        "chips":     ["imxrt700"],
    }
]

# ---------------------------------------------------------------------------
# Chip configuration
# ---------------------------------------------------------------------------
CHIP_CONFIG = {
    "mcxn94x": {
        "npu_version":   "npu16",
        "env_var":       "CONVERTER_MCXN_PATH",
    },
    "imxrt700": {
        "npu_version":   "npu64",
        "env_var":       "CONVERTER_RT700_PATH",
    },
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def compiler_binary(converter_path: str) -> str:
    """Return the full path to the neutron-compiler executable."""
    exe = "neutron-compiler.exe" if platform.system() == "Windows" else "neutron-compiler"
    return str(Path(converter_path) / exe)


def run(cmd: list, **kwargs):
    """Print and execute a command, raising on non-zero exit."""
    print(" ".join(str(c) for c in cmd))
    subprocess.run(cmd, check=True, **kwargs)


def xxd_i(tflite_path: Path) -> bytes:
    """
    Produce xxd -i style output for *tflite_path*.
    Pure-Python implementation so the script works on Windows without xxd.
    Only the data lines are returned (no variable declaration, no closing '};').
    """
    data = tflite_path.read_bytes()
    lines = []
    for i in range(0, len(data), 12):
        chunk = data[i:i + 12]
        hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"  {hex_str},")
    # Remove trailing comma from last line to match xxd -i behaviour
    if lines:
        lines[-1] = lines[-1].rstrip(",")
    return "\n".join(lines)


def update_header(header_path: Path, new_array_data: str, new_size: int,
                  version: str, command: str):
    """
    Replace (or insert) the byte-array body, array size, converter version
    comment, and command comment inside an existing C header file.

    The expected header comment block looks like:
        // Converted by eIQ neutron-compiler version X.Y.Z
        // command:
        // ./neutron-compiler --input ... --output ... --target ...

    Insertion rules:
    - Version and command are always kept grouped together.
    - If version exists but command does not: insert command right after the
      version line (no extra blank line).
    - If command exists but version does not: insert version right before the
      command line (no extra blank line).
    - If neither exists: insert both before the first non-comment line of code
      and append one blank line after the inserted block.
    """
    text = header_path.read_text(encoding="utf-8")

    converted_by_line = f"// Converted by {version}"
    command_block     = f"// command:\n// {command}"

    # Patterns for each piece
    ver_pat = re.compile(r"(?im)^//\s*(converted by|Neutron Converter version)\s+.*$")
    cmd_pat = re.compile(r"(?m)^// command:\s*\n//[^\n]*$")

    has_ver = bool(ver_pat.search(text))
    has_cmd = bool(cmd_pat.search(text))

    if has_ver and has_cmd:
        # Both present: update in-place
        text = ver_pat.sub(converted_by_line, text, count=1)
        text = cmd_pat.sub(command_block, text, count=1)

    elif has_ver and not has_cmd:
        # Update version, then insert command block right after it
        text = ver_pat.sub(converted_by_line, text, count=1)
        m = re.search(r"(?im)^// Converted by .*$", text)
        ins = m.end()
        if ins < len(text) and text[ins] == "\n":
            ins += 1
        text = text[:ins] + command_block + "\n" + text[ins:]

    elif has_cmd and not has_ver:
        # Insert version right before the command block
        m = cmd_pat.search(text)
        text = text[:m.start()] + converted_by_line + "\n" + text[m.start():]
        # Command line is now correct; just update it in the shifted text
        text = cmd_pat.sub(command_block, text, count=1)

    else:
        # Neither exists: insert both after any leading copyright/license block,
        # but before the first non-comment line of code.
        #
        # Strategy: find the end of the leading comment block (either a /* ... */
        # block comment or a run of // lines at the very top of the file).
        # Insert right after that block (preserving any blank line that follows),
        # before the first actual code line.

        insert_block = converted_by_line + "\n" + command_block + "\n"

        # Try to find a leading /* ... */ block comment
        block_comment = re.match(r"[ \t]*/\*.*?\*/[ \t]*\n?", text, re.DOTALL)
        if block_comment:
            # Insert right after the closing block comment line
            pos = block_comment.end()
            # Preserve a blank line that may already follow the copyright block
            if pos < len(text) and text[pos] == "\n":
                pos += 1
            text = text[:pos] + insert_block + "\n" + text[pos:]
        else:
            # Try to find a leading run of // comment lines
            run_end = 0
            for m in re.finditer(r"(?m)^[ \t]*//[^\n]*\n", text):
                if m.start() == run_end:
                    run_end = m.end()
                else:
                    break
            if run_end > 0:
                # Insert right after the last // line in the leading block
                pos = run_end
                if pos < len(text) and text[pos] == "\n":
                    pos += 1
                text = text[:pos] + insert_block + "\n" + text[pos:]
            else:
                # No leading comment found; insert at the very beginning
                text = insert_block + "\n" + text

    # -- Replace array body -------------------------------------------------
    array_pattern = re.compile(
        r"(=\s*\{)\n(.*?)\n(^\};)",
        re.DOTALL | re.MULTILINE,
    )
    match = array_pattern.search(text)
    if not match:
        raise RuntimeError(f"Could not find array body in {header_path}")
    text = text[:match.start(2)] + new_array_data + text[match.end(2):]

    # -- Replace array size -------------------------------------------------
    text = re.sub(r"len\s*=\s*\d+\s*;", f"len = {new_size};", text)

    header_path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    # Resolve MPP root (parent of the directory containing this script)
    mpp_dir = Path(__file__).resolve().parent.parent

    # Validate environment variables and build converter map
    converters = {}
    errors = []
    for chip, cfg in CHIP_CONFIG.items():
        val = os.environ.get(cfg["env_var"], "")
        if not val:
            errors.append(f"Please set environment variable {cfg['env_var']}")
        else:
            converters[chip] = val

    if errors:
        for msg in errors:
            print(f"ERROR: {msg}", file=sys.stderr)
        sys.exit(1)

    for model in MODELS:
        basename  = model["basename"]
        model_dir = mpp_dir / model["model_dir"]
        h_name    = model.get("h_name") or basename
        chips     = model.get("chips", list(CHIP_CONFIG.keys()))

        for chip in chips:
            cfg         = CHIP_CONFIG[chip]
            npu_version = cfg["npu_version"]
            converter   = converters[chip]
            compiler    = compiler_binary(converter)

            ori_tflite  = model_dir / f"{basename}.tflite"
            conv_tflite = model_dir / f"{basename}_{npu_version}.tflite"
            conv_header = model_dir / f"{h_name}_{npu_version}_tflite.h"

            print(f"\n=== {basename} -> {chip} ({npu_version}) ===")

            # Get compiler version
            version_result = subprocess.run(
                [compiler, "--version"],
                check=True,
                capture_output=True,
                text=True,
            )
            version = version_result.stdout.strip() or version_result.stderr.strip()
            print(f"Compiler version: {version}")

            # Build the conversion command and run it
            conv_cmd = [compiler, "--input", str(ori_tflite),
                        "--output", str(conv_tflite), "--target", chip]
            run(conv_cmd)

            # Human-readable command string recorded in the header comment.
            # Use just the binary name (no full path) to keep the comment portable.
            compiler_name = Path(compiler).name
            command_str = (f"{compiler_name} --input {ori_tflite.name}"
                           f" --output {conv_tflite.name} --target {chip}")

            # Build new array data in-memory
            array_data = xxd_i(conv_tflite)
            new_size   = conv_tflite.stat().st_size

            # Update the header
            update_header(conv_header, array_data, new_size, version, command_str)
            print(f"Updated header: {conv_header}")

    print("\nDone.")


if __name__ == "__main__":
    main()
