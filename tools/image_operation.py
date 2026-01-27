#!/usr/bin/env python3

"""
 * Copyright 2025-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
"""

"""
Image Converter Script for MPP

This script reads binary image files in various formats, displays them, and performs
operations such as format conversion, resizing, cropping, channel swapping, and
C header file generation for embedded systems.

Supported formats: RGB565, RGB, BGR, RGBA, BGRA, GRAY, YUV420, YUV444P, YUV1P444,
NV12, NV21, YUYV, UYVY422, VYUY422, YUVX, VUYX, YCBCRA, YCRCBA, JPEG, PNG, BMP

Usage:
python image_operation.py -i input.bin -W 640 -H 480 -f RGB565 [-ow 320] [-oh 240] [-of RGB]
                          [-o output.bin] [-k] [-b black|white] [--crop left:top[:width[:height]]]
                          [-s] [-gh output.h] [-q 95] [-c 9]
"""

import argparse
import numpy as np
import cv2
import sys
import os
from typing import Tuple, Optional
import contextlib
from datetime import datetime
from PIL import Image, ImageOps

# Define supported formats globally so we can reuse them
SUPPORTED_FORMATS = {
    'RGB565': 'RGB565',
    'RGB': 'RGB',
    'BGR': 'BGR',
    'RGBA': 'RGBA',
    'BGRA': 'BGRA',
    'GRAY': 'GRAY',
    'YUV420': 'YUV420',
    'YUV444P': 'YUV444P',
    'YUV1P444': 'YUV1P444',
    'NV12': 'NV12',
    'NV21': 'NV21',
    'YUYV': 'YUYV',
    'UYVY422': 'UYVY422',
    'VYUY422': 'VYUY422',
    'YUVX': 'YUVX',
    'VUYX': 'VUYX',
    'YCBCRA': 'YCBCRA',
    'YCRCBA': 'YCRCBA',
    'JPEG': 'JPEG',
    'PNG': 'PNG',
    'BMP': 'BMP'
}

@contextlib.contextmanager
def opencv_window_manager():
    """Context manager for OpenCV windows cleanup."""
    try:
        yield
    finally:
        cv2.destroyAllWindows()

@contextlib.contextmanager
def temp_file_manager(filename):
    """Context manager for temporary file cleanup."""
    try:
        yield filename
    finally:
        cleanup_temp_files(filename)

def parse_pixel_format(format_str: str) -> str:
    """Parse and validate pixel format string."""
    format_upper = format_str.upper()
    if format_upper not in SUPPORTED_FORMATS:
        raise ValueError(f"Unsupported format: {format_str}. Supported: {list(SUPPORTED_FORMATS.keys())}")

    return SUPPORTED_FORMATS[format_upper]

def get_bytes_per_pixel(format_str: str) -> float:
    """Get bytes per pixel for different formats."""
    format_bytes = {
        'RGB565': 2,
        'RGB': 3,
        'BGR': 3,
        'RGBA': 4,
        'BGRA': 4,
        'GRAY': 1,
        'YUV420': 1.5,  # 12 bits per pixel
        'YUV444P': 3,   # 24 bits per pixel (8 bits each for Y, U, V)
        'YUV1P444': 3,  # 24 bits per pixel (8 bits each for Y, U, V interleaved)
        'NV12': 1.5,    # 12 bits per pixel
        'NV21': 1.5,    # 12 bits per pixel
        'YUYV': 2,   # 16 bits per pixel (packed format)
        'UYVY422': 2,   # 16 bits per pixel (packed format)
        'VYUY422': 2,   # 16 bits per pixel (packed format)
        'YUVX': 4,      # 32 bits per pixel (Y, U, V, X)
        'VUYX': 4,      # 32 bits per pixel (V, U, Y, X)
        'YCBCRA': 4,    # 32 bits per pixel (Y, Cb, Cr, A)
        'YCRCBA': 4,    # 32 bits per pixel (Y, Cr, Cb, A)
        'JPEG': 0,      # Variable size, cannot predict
        'PNG': 0,       # Variable size, compressed format
        'BMP': 0        # Variable size, depends on bit depth
    }
    return format_bytes.get(format_str, 3)

def is_image_file(file_path: str) -> str:
    """Check file type by examining magic bytes and return format."""
    try:
        with open(file_path, 'rb') as f:
            header = f.read(8)

            # JPEG files start with FF D8 FF
            if len(header) >= 3 and header[:3] == b'\xff\xd8\xff':
                return 'JPEG'

            # PNG files start with 89 50 4E 47 0D 0A 1A 0A
            if len(header) >= 8 and header[:8] == b'\x89\x50\x4e\x47\x0d\x0a\x1a\x0a':
                return 'PNG'

            # BMP files start with 42 4D (BM)
            if len(header) >= 2 and header[:2] == b'\x42\x4d':
                return 'BMP'

        return None
    except (IOError, OSError, PermissionError) as e:
        print(f"Warning: Could not read file header: {e}")
        return None

# Replace the old is_jpeg_file function
def is_jpeg_file(file_path: str) -> bool:
    """Check if file is a JPEG by examining magic bytes."""
    return is_image_file(file_path) == 'JPEG'

def read_binary_image_chunked(file_path: str, expected_size: int, chunk_size: int = 8192) -> np.ndarray:
    """Read binary image file in chunks for large files."""

    if expected_size > 100 * 1024 * 1024:  # 100MB threshold
        print(f"Warning: Large file detected ({expected_size // (1024*1024)}MB). Reading in chunks...")

        data_chunks = []
        try:
            with open(file_path, 'rb') as f:
                while True:
                    chunk = f.read(chunk_size)
                    if not chunk:
                        break
                    data_chunks.append(chunk)
            data = b''.join(data_chunks)
        except MemoryError:
            raise MemoryError(f"File too large to fit in memory: {file_path}")
    else:
        with open(file_path, 'rb') as f:
            data = f.read()

    return data

def validate_data_size(data: bytes, width: int, height: int, format_str: str) -> None:
    """Validate that data size matches expected format requirements."""
    expected_size = int(width * height * get_bytes_per_pixel(format_str))
    actual_size = len(data)

    if format_str == 'JPEG':
        return  # JPEG size is variable

    if actual_size < expected_size:
        raise ValueError(f"Insufficient data: expected {expected_size} bytes for {width}x{height} {format_str}, got {actual_size} bytes")
    elif actual_size > expected_size * 1.1:  # Allow 10% tolerance
        print(f"Warning: Data size ({actual_size}) is larger than expected ({expected_size}) for {format_str}")

def safe_array_slice(data: bytes, dtype, max_elements: int) -> np.ndarray:
    """Safely slice array data with bounds checking."""
    array = np.frombuffer(data, dtype=dtype)
    if len(array) < max_elements:
        print(f"Warning: Array truncated from {max_elements} to {len(array)} elements")
        return array
    return array[:max_elements]

def save_jpeg(image: np.ndarray, output_path: str, quality: int = 95):
    """Save image as JPEG file."""
    # Check if input shape matches the RGB one
    if len(image.shape) != 3 or image.shape[2] != 3:
        raise ValueError("The shape of input image must be (height, width, 3) (RGB format) for JPEG image save")

    bgr_image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)

    # Ensure the output path has .jpg or .jpeg extension
    if not (output_path.lower().endswith('.jpg') or output_path.lower().endswith('.jpeg')):
        output_path = output_path + '.jpg'
        print(f"Added .jpg extension to output path: {output_path}")

    # Validate quality
    if not (1 <= quality <= 100):
        print(f"Warning: JPEG quality {quality} is out of range (1-100), using default 95")
        quality = 95

    try:
        # Save as JPEG
        success = cv2.imwrite(output_path, bgr_image, [cv2.IMWRITE_JPEG_QUALITY, quality])
        if not success:
            raise RuntimeError("Failed to write JPEG file")
        print(f"JPEG saved to: {output_path}")
    except Exception as e:
        raise RuntimeError(f"Error saving JPEG file: {e}")

    return output_path

def save_png(image: np.ndarray, output_path: str, compression: int = 9):
    """Save image as PNG file."""
    # Check if input shape matches the RGB format expected by OpenCV
    if len(image.shape) != 3 or image.shape[2] != 3:
        raise ValueError("The shape of input image must be (height, width, 3) (BGR format) for PNG image save")

    bgr_image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)

    # Ensure the output path has .png extension
    if not output_path.lower().endswith('.png'):
        output_path = output_path + '.png'
        print(f"Added .png extension to output path: {output_path}")

    # Validate compression level
    if not (0 <= compression <= 9):
        print(f"Warning: PNG compression level {compression} is out of range (0-9), using default 9")
        compression = 9

    try:
        # Save as PNG with compression level (0-9, where 9 is maximum compression)
        success = cv2.imwrite(output_path, bgr_image, [cv2.IMWRITE_PNG_COMPRESSION, compression])
        if not success:
            raise RuntimeError("Failed to write PNG file")
        print(f"PNG saved to: {output_path}")
    except Exception as e:
        raise RuntimeError(f"Error saving PNG file: {e}")

    return output_path

def save_bmp(image: np.ndarray, output_path: str):
    """Save image as BMP file."""
    # Check if input shape matches the RGB format expected by OpenCV
    if len(image.shape) != 3 or image.shape[2] != 3:
        raise ValueError("The shape of input image must be (height, width, 3) (BGR format) for BMP image save")

    bgr_image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)

    # Ensure the output path has .bmp extension
    if not output_path.lower().endswith('.bmp'):
        output_path = output_path + '.bmp'
        print(f"Added .bmp extension to output path: {output_path}")

    try:
        # Save as BMP
        success = cv2.imwrite(output_path, bgr_image)
        if not success:
            raise RuntimeError("Failed to write BMP file")
        print(f"BMP saved to: {output_path}")
    except Exception as e:
        raise RuntimeError(f"Error saving BMP file: {e}")

    return output_path

def convert_rgb_to_yuv444(rgb_image: np.ndarray) -> bytes:
    """Convert RGB image to YUV444 planar format."""
    yuv = cv2.cvtColor(rgb_image, cv2.COLOR_RGB2YUV)
    height, width = yuv.shape[:2]

    # Extract Y, U, V planes (no subsampling for YUV444)
    y_plane = yuv[:, :, 0]
    u_plane = yuv[:, :, 1]
    v_plane = yuv[:, :, 2]

    # Combine planes in planar format (Y plane, then U plane, then V plane)
    yuv444_data = np.concatenate([
        y_plane.flatten(),
        u_plane.flatten(),
        v_plane.flatten()
    ])

    return yuv444_data.astype(np.uint8).tobytes()

def convert_rgb_to_yuv444i(rgb_image: np.ndarray) -> bytes:
    """Convert RGB image to YUV444 interleaved format."""
    yuv = cv2.cvtColor(rgb_image, cv2.COLOR_RGB2YUV)

    # YUV1P444 format stores pixels as YUVYUVYUV... (interleaved)
    # Each pixel has Y, U, V components stored consecutively
    yuv444i_data = yuv.astype(np.uint8).tobytes()

    return yuv444i_data

def convert_rgb_to_yuv420(rgb_image: np.ndarray) -> bytes:
    """Convert RGB image to YUV420 planar format."""
    yuv = cv2.cvtColor(rgb_image, cv2.COLOR_RGB2YUV)
    height, width = yuv.shape[:2]

    # Extract Y plane
    y_plane = yuv[:, :, 0]

    # Downsample U and V planes
    u_plane = cv2.resize(yuv[:, :, 1], (width//2, height//2), interpolation=cv2.INTER_LINEAR)
    v_plane = cv2.resize(yuv[:, :, 2], (width//2, height//2), interpolation=cv2.INTER_LINEAR)

    # Combine planes
    yuv420_data = np.concatenate([
        y_plane.flatten(),
        u_plane.flatten(),
        v_plane.flatten()
    ])

    return yuv420_data.astype(np.uint8).tobytes()

def convert_rgb_to_nv12(rgb_image: np.ndarray) -> bytes:
    """Convert RGB image to NV12 format."""
    yuv = cv2.cvtColor(rgb_image, cv2.COLOR_RGB2YUV)
    height, width = yuv.shape[:2]

    # Extract Y plane
    y_plane = yuv[:, :, 0]

    # Downsample and interleave U and V planes
    u_plane = cv2.resize(yuv[:, :, 1], (width//2, height//2), interpolation=cv2.INTER_LINEAR)
    v_plane = cv2.resize(yuv[:, :, 2], (width//2, height//2), interpolation=cv2.INTER_LINEAR)

    # Interleave U and V
    uv_interleaved = np.stack([u_plane, v_plane], axis=2).flatten()

    # Combine Y and UV
    nv12_data = np.concatenate([y_plane.flatten(), uv_interleaved])

    return nv12_data.astype(np.uint8).tobytes()

def convert_rgb_to_nv21(rgb_image: np.ndarray) -> bytes:
    """Convert RGB image to NV21 format."""
    # Similar to NV12 but with V,U order instead of U,V
    yuv = cv2.cvtColor(rgb_image, cv2.COLOR_RGB2YUV)
    height, width = yuv.shape[:2]

    # Extract Y plane
    y_plane = yuv[:, :, 0]

    # Downsample and interleave U and V planes
    u_plane = cv2.resize(yuv[:, :, 1], (width//2, height//2), interpolation=cv2.INTER_LINEAR)
    v_plane = cv2.resize(yuv[:, :, 2], (width//2, height//2), interpolation=cv2.INTER_LINEAR)

    # Interleave V and U (opposite of NV12)
    vu_interleaved = np.stack([v_plane, u_plane], axis=2).flatten()

    # Combine Y and UV
    nv21_data = np.concatenate([y_plane.flatten(), vu_interleaved])

    return nv21_data.astype(np.uint8).tobytes()

def generate_variable_name(filename: str, width: int, height: int, format_str: str) -> str:
    """Generate a valid C variable name from filename, dimensions, and format."""
    # Extract base filename without extension
    base_name = os.path.splitext(os.path.basename(filename))[0]

    # Replace invalid characters with underscores
    base_name = ''.join(c if c.isalnum() else '_' for c in base_name)

    # Remove consecutive underscores
    while '__' in base_name:
        base_name = base_name.replace('__', '_')

    # Remove leading/trailing underscores
    base_name = base_name.strip('_')

    # Ensure it doesn't start with a number
    if base_name and base_name[0].isdigit():
        base_name = 'img_' + base_name

    # If empty, use default
    if not base_name:
        base_name = 'image'

    # Convert format to lowercase for variable name
    format_lower = format_str.lower()

    if os.path.splitext(os.path.basename(filename))[1] == '.h':
        return base_name
    else:
        return f"{base_name}_{width}_{height}_{format_lower}"

def get_mpp_pixel_format(format_str: str) -> str:
    """Map format string to MPP pixel format enum value."""
    format_mapping = {
        'RGB': 'MPP_PIXEL_RGB',
        'BGR': 'MPP_PIXEL_BGR',
        'RGBA': 'MPP_PIXEL_RGBA',
        'BGRA': 'MPP_PIXEL_BGRA',
        'RGB565': 'MPP_PIXEL_RGB565',
        'GRAY': 'MPP_PIXEL_GRAY',
        'YUYV': 'MPP_PIXEL_YUYV',
        'UYVY422': 'MPP_PIXEL_UYVY1P422',
        'VYUY422': 'MPP_PIXEL_VYUY1P422',
        'YUV420': 'MPP_PIXEL_YUV420P',
        'YUVX': 'MPP_PIXEL_YUV1P444',
        'VUYX': 'MPP_PIXEL_YUV1P444',
        'JPEG': 'MPP_PIXEL_JPEG',
        # Add more mappings as needed
    }

    if format_str in format_mapping:
        return format_mapping[format_str]
    else:
        print(f"Warning: Format '{format_str}' not found in mpp_pixel_format_t enum, using string representation")
        return f'"{format_str}"'

def get_channels_number(format_str: str) -> int:
    """Get number of channels for different formats."""
    channels_mapping = {
        'RGB565': 3,
        'RGB': 3,
        'BGR': 3,
        'RGBA': 4,
        'BGRA': 4,
        'GRAY': 1,
        'YUV420': 3,
        'YUV444P': 3,
        'YUV1P444': 3,
        'NV12': 3,
        'NV21': 3,
        'YUYV': 3,
        'UYVY422': 3,
        'VYUY422': 3,
        'YUVX': 4,
        'VUYX': 4,
        'YCBCRA': 4,
        'YCRCBA': 4,
        'JPEG': 3,
        'PNG': 3,
        'BMP': 3
    }
    return channels_mapping.get(format_str, 3)

def read_binary_image(file_path: str, width: int, height: int, format_str: str) -> np.ndarray:
    """Read binary image file and convert to numpy array."""
    try:
        # Handle standard image formats (JPEG, PNG, BMP)
        if format_str in ['JPEG', 'PNG', 'BMP']:
            # For standard image formats, we can read directly with OpenCV
            image = cv2.imread(file_path, cv2.IMREAD_COLOR)
            if image is None:
                # Try reading as binary data and decode
                with open(file_path, 'rb') as f:
                    image_data = f.read()

                # Decode image from binary data
                nparr = np.frombuffer(image_data, np.uint8)
                image = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

                if image is None:
                    raise ValueError(f"Failed to decode {format_str} data")

            # Convert BGR to RGB for consistency with other formats
            image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)

            # If specific dimensions were provided, resize to match
            if width > 0 and height > 0 and image.shape[:2] != (height, width):
                print(f"Warning: {format_str} dimensions ({image.shape[1]}x{image.shape[0]}) differ from specified ({width}x{height})")
                print(f"Using actual {format_str} dimensions: {image.shape[1]}x{image.shape[0]}")

            return image

        # Handle other binary formats
        expected_size = int(width * height * get_bytes_per_pixel(format_str))

        data = read_binary_image_chunked(file_path, expected_size)

        # Add validation
        validate_data_size(data, width, height, format_str)

        # Convert binary data to numpy array based on format
        if format_str == 'RGB565':
            raw_data = safe_array_slice(data, np.uint16, width * height)
            if len(raw_data) < width * height:
                # Pad with zeros if insufficient data
                padded_data = np.zeros(width * height, dtype=np.uint16)
                padded_data[:len(raw_data)] = raw_data
                raw_data = padded_data
            raw_data = raw_data.reshape((height, width))

            # Extract RGB components with proper bit replication
            r = (raw_data & 0xF800) >> 11  # Extract 5-bit red
            g = (raw_data & 0x07E0) >> 5   # Extract 6-bit green
            b = (raw_data & 0x001F)        # Extract 5-bit blue

            # Proper bit expansion with replication for better precision
            r = ((r << 3) | (r >> 2)).astype(np.uint8)  # 5->8 bits
            g = ((g << 2) | (g >> 4)).astype(np.uint8)  # 6->8 bits
            b = ((b << 3) | (b >> 2)).astype(np.uint8)  # 5->8 bits

            image = np.stack([r, g, b], axis=2)

        elif format_str in ['RGB', 'BGR']:
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*3]
            image = raw_data.reshape((height, width, 3))

        elif format_str in ['RGBA', 'BGRA']:
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*4]
            image = raw_data.reshape((height, width, 4))

        elif format_str == 'GRAY':
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height]
            image = raw_data.reshape((height, width))

        elif format_str == 'YUYV':
            # YUYV: Packed format - Y0 U0 Y1 V0 (4 bytes for 2 pixels)
            # Each pair of pixels shares U and V components
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*2]

            # Reshape to get YUYV pairs
            yuyv_data = raw_data.reshape((height, width // 2, 4))

            # Create YUV image
            yuv = np.zeros((height, width, 3), dtype=np.uint8)

            # Extract Y, U, V components
            yuv[:, 0::2, 0] = yuyv_data[:, :, 0]  # Y0
            yuv[:, 1::2, 0] = yuyv_data[:, :, 2]  # Y1
            yuv[:, 0::2, 1] = yuyv_data[:, :, 1]  # U0 (shared)
            yuv[:, 1::2, 1] = yuyv_data[:, :, 1]  # U0 (shared)
            yuv[:, 0::2, 2] = yuyv_data[:, :, 3]  # V0 (shared)
            yuv[:, 1::2, 2] = yuyv_data[:, :, 3]  # V0 (shared)

            # Convert YUV to RGB
            image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

        elif format_str == 'UYVY422':
            # UYVY422: Packed format - U0 Y0 V0 Y1 (4 bytes for 2 pixels)
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*2]

            # Reshape to get UYVY pairs
            uyvy_data = raw_data.reshape((height, width // 2, 4))

            # Create YUV image
            yuv = np.zeros((height, width, 3), dtype=np.uint8)

            # Extract Y, U, V components from UYVY format
            yuv[:, 0::2, 0] = uyvy_data[:, :, 1]  # Y0 from position 1
            yuv[:, 1::2, 0] = uyvy_data[:, :, 3]  # Y1 from position 3
            yuv[:, 0::2, 1] = uyvy_data[:, :, 0]  # U0 from position 0 (shared)
            yuv[:, 1::2, 1] = uyvy_data[:, :, 0]  # U0 from position 0 (shared)
            yuv[:, 0::2, 2] = uyvy_data[:, :, 2]  # V0 from position 2 (shared)
            yuv[:, 1::2, 2] = uyvy_data[:, :, 2]  # V0 from position 2 (shared)

            # Convert YUV to RGB
            image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

        elif format_str == 'VYUY422':
            # VYUY422: Packed format - V0 Y0 U0 Y1 (4 bytes for 2 pixels)
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*2]

            # Reshape to get VYUY pairs
            vyuy_data = raw_data.reshape((height, width // 2, 4))

            # Create YUV image
            yuv = np.zeros((height, width, 3), dtype=np.uint8)

            # Extract Y, U, V components from VYUY format
            yuv[:, 0::2, 0] = vyuy_data[:, :, 1]  # Y0 from position 1
            yuv[:, 1::2, 0] = vyuy_data[:, :, 3]  # Y1 from position 3
            yuv[:, 0::2, 1] = vyuy_data[:, :, 2]  # U0 from position 2 (shared)
            yuv[:, 1::2, 1] = vyuy_data[:, :, 2]  # U0 from position 2 (shared)
            yuv[:, 0::2, 2] = vyuy_data[:, :, 0]  # V0 from position 0 (shared)
            yuv[:, 1::2, 2] = vyuy_data[:, :, 0]  # V0 from position 0 (shared)

            # Convert YUV to RGB
            image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

        elif format_str == 'YUVX':
            # YUVX: 32-bit format - Y U V X (4 bytes per pixel)
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*4]
            yuvx_data = raw_data.reshape((height, width, 4))

            # Extract YUV components (ignore X component)
            yuv = yuvx_data[:, :, :3]

            # Convert YUV to RGB
            image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

        elif format_str == 'VUYX':
            # VUYX: 32-bit format - V U Y X (4 bytes per pixel)
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*4]
            vuyx_data = raw_data.reshape((height, width, 4))

            # Reorder components from VUYX to YUV (ignore X component)
            yuv = np.zeros((height, width, 3), dtype=np.uint8)
            yuv[:, :, 0] = vuyx_data[:, :, 2]  # Y from position 2
            yuv[:, :, 1] = vuyx_data[:, :, 1]  # U from position 1
            yuv[:, :, 2] = vuyx_data[:, :, 0]  # V from position 0

            # Convert YUV to RGB
            image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

        elif format_str == 'YCBCRA':
            # YCbCrA: 32-bit format - Y Cb Cr A (4 bytes per pixel)
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*4]
            ycbcra_data = raw_data.reshape((height, width, 4))

            # Extract YCbCr components (ignore A component for display)
            ycbcr = ycbcra_data[:, :, :3]

            # Convert YCbCr to RGB (OpenCV uses YCrCb, so we need to swap Cb and Cr)
            ycrcb = np.zeros_like(ycbcr)
            ycrcb[:, :, 0] = ycbcr[:, :, 0]  # Y
            ycrcb[:, :, 1] = ycbcr[:, :, 2]  # Cr from Cr
            ycrcb[:, :, 2] = ycbcr[:, :, 1]  # Cb from Cb

            image = cv2.cvtColor(ycrcb, cv2.COLOR_YCrCb2RGB)

        elif format_str == 'YCRCBA':
            # YCrCbA: 32-bit format - Y Cr Cb A (4 bytes per pixel)
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*4]
            ycrcba_data = raw_data.reshape((height, width, 4))

            # Extract YCrCb components (ignore A component for display)
            ycrcb = ycrcba_data[:, :, :3]

            # Convert YCrCb to RGB (OpenCV uses YCrCb format directly)
            image = cv2.cvtColor(ycrcb, cv2.COLOR_YCrCb2RGB)

        elif format_str in ['YUV420', 'YUV444P', 'YUV1P444', 'NV12', 'NV21']:
            # For YUV formats, we'll convert to RGB for display
            if format_str == 'YUV1P444':
                # YUV1P444 interleaved format: YUVYUVYUV... (each pixel has Y, U, V)
                expected_size = width * height * 3

                if len(data) < expected_size:
                    raise ValueError(f"Insufficient data for YUV1P444 format. Expected {expected_size} bytes, got {len(data)}")

                raw_data = np.frombuffer(data, dtype=np.uint8)[:expected_size]

                # Reshape to get YUV interleaved data
                yuv = raw_data.reshape((height, width, 3))

                # Convert YUV to RGB
                image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

            elif format_str == 'YUV444P':
                # YUV444P planar format: Y plane, U plane, V plane (no subsampling)
                y_size = width * height
                u_size = width * height
                v_size = width * height
                total_size = y_size + u_size + v_size

                if len(data) < total_size:
                    raise ValueError(f"Insufficient data for YUV444P format. Expected {total_size} bytes, got {len(data)}")

                raw_data = np.frombuffer(data, dtype=np.uint8)[:total_size]

                # Extract Y, U, V planes
                y = raw_data[:y_size].reshape((height, width))
                u = raw_data[y_size:y_size + u_size].reshape((height, width))
                v = raw_data[y_size + u_size:].reshape((height, width))

                # Create full YUV image
                yuv = np.stack([y, u, v], axis=2)
                image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

            else:
                y_size = width * height
                uv_size = y_size // 2

                if len(data) < y_size + uv_size:
                    raise ValueError(f"Insufficient data for {format_str} format. Expected {y_size + uv_size} bytes, got {len(data)}")

                raw_data = np.frombuffer(data, dtype=np.uint8)[:y_size + uv_size]

                if format_str == 'YUV420':
                    # Extract Y, U, V planes
                    y = raw_data[:y_size].reshape((height, width))
                    u = raw_data[y_size:y_size + y_size//4].reshape((height//2, width//2))
                    v = raw_data[y_size + y_size//4:].reshape((height//2, width//2))

                    # Proper upsampling of U and V
                    u = cv2.resize(u, (width, height), interpolation=cv2.INTER_LINEAR)
                    v = cv2.resize(v, (width, height), interpolation=cv2.INTER_LINEAR)

                    yuv = np.stack([y, u, v], axis=2)
                    image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

                else:  # NV12 or NV21
                    y = raw_data[:y_size].reshape((height, width))
                    uv = raw_data[y_size:].reshape((height//2, width//2, 2))

                    # Proper upsampling for interleaved UV
                    if format_str == 'NV12':
                        u_plane = cv2.resize(uv[:, :, 0], (width, height), interpolation=cv2.INTER_LINEAR)
                        v_plane = cv2.resize(uv[:, :, 1], (width, height), interpolation=cv2.INTER_LINEAR)
                    else:  # NV21
                        u_plane = cv2.resize(uv[:, :, 1], (width, height), interpolation=cv2.INTER_LINEAR)
                        v_plane = cv2.resize(uv[:, :, 0], (width, height), interpolation=cv2.INTER_LINEAR)

                    # Create full YUV image
                    yuv = np.zeros((height, width, 3), dtype=np.uint8)
                    yuv[:, :, 0] = y
                    yuv[:, :, 1] = u_plane
                    yuv[:, :, 2] = v_plane

                    image = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)

        else:
            raise ValueError(f"Unsupported format for reading: {format_str}")

        return image

    except FileNotFoundError:
        raise FileNotFoundError(f"File '{file_path}' not found")
    except PermissionError:
        raise PermissionError(f"Permission denied accessing file '{file_path}'")
    except MemoryError:
        raise MemoryError(f"Insufficient memory to load file '{file_path}'")
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)

def read_raw_binary_image(file_path: str, width: int, height: int, format_str: str) -> np.ndarray:
    """Read binary image file as raw data without format conversion."""
    try:
        # Handle standard image formats (JPEG, PNG, BMP) - these can't be read as raw
        if format_str in ['JPEG', 'PNG', 'BMP']:
            raise ValueError(f"Cannot read {format_str} files in raw format. Use regular read_binary_image instead.")

        # Calculate expected size
        expected_size = int(width * height * get_bytes_per_pixel(format_str))

        # Read binary data
        data = read_binary_image_chunked(file_path, expected_size)

        # Validate data size
        validate_data_size(data, width, height, format_str)

        # Return raw data as numpy array based on format
        if format_str == 'RGB565':
            raw_data = safe_array_slice(data, np.uint16, width * height)
            if len(raw_data) < width * height:
                padded_data = np.zeros(width * height, dtype=np.uint16)
                padded_data[:len(raw_data)] = raw_data
                raw_data = padded_data
            return raw_data.reshape((height, width))

        elif format_str in ['RGB', 'BGR']:
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*3]
            return raw_data.reshape((height, width, 3))

        elif format_str in ['RGBA', 'BGRA', 'YUVX', 'VUYX', 'YCBCRA', 'YCRCBA']:
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*4]
            return raw_data.reshape((height, width, 4))

        elif format_str == 'GRAY':
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height]
            return raw_data.reshape((height, width))

        elif format_str in ['YUYV', 'UYVY422', 'VYUY422']:
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*2]
            return raw_data.reshape((height, width, 2))

        elif format_str == 'YUV1P444':
            raw_data = np.frombuffer(data, dtype=np.uint8)[:width*height*3]
            return raw_data.reshape((height, width, 3))

        elif format_str in ['YUV420', 'YUV444P', 'NV12', 'NV21']:
            # For planar formats, return as 1D array since channel structure varies
            raw_data = np.frombuffer(data, dtype=np.uint8)
            return raw_data

        else:
            raise ValueError(f"Unsupported format for raw reading: {format_str}")

    except Exception as e:
        raise RuntimeError(f"Error reading raw binary image: {e}")

def convert_image_format(image: np.ndarray, input_format: str, output_format: str) -> np.ndarray:
    """Convert image from input format to output format."""

    # Convert to BGR first (OpenCV's default)
    if input_format in ['RGB', 'JPEG', 'PNG', 'BMP']:
        bgr_image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
    elif input_format == 'BGR':
        bgr_image = image
    elif input_format == 'RGBA':
        bgr_image = cv2.cvtColor(image, cv2.COLOR_RGBA2BGR)
    elif input_format == 'BGRA':
        bgr_image = cv2.cvtColor(image, cv2.COLOR_BGRA2BGR)
    elif input_format == 'GRAY':
        bgr_image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
    elif input_format in ['RGB565', 'YUYV', 'UYVY422', 'VYUY422', 'YUVX', 'VUYX', 'YCBCRA', 'YCRCBA' , 'YUV420', 'YUV444P', 'YUV1P444', 'NV12', 'NV21']:
        # These formats are already converted to RGB during reading, convert to BGR
        bgr_image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
    else:
        print("Conert from rgb565")
        bgr_image = image  # Assume already in BGR or compatible format

    # Convert from BGR to target format
    if output_format == 'RGB':
        return cv2.cvtColor(bgr_image, cv2.COLOR_BGR2RGB)
    elif output_format == 'BGR':
        return bgr_image
    elif output_format == 'RGBA':
        return cv2.cvtColor(bgr_image, cv2.COLOR_BGR2RGBA)
    elif output_format == 'BGRA':
        return cv2.cvtColor(bgr_image, cv2.COLOR_BGR2BGRA)
    elif output_format == 'GRAY':
        return cv2.cvtColor(bgr_image, cv2.COLOR_BGR2GRAY)
    elif output_format == 'RGB565':
        # Convert to RGB565 format
        print("Convert to rgb565")
        rgb = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2RGB)
        r = (rgb[:, :, 0] >> 3).astype(np.uint16)
        g = (rgb[:, :, 1] >> 2).astype(np.uint16)
        b = (rgb[:, :, 2] >> 3).astype(np.uint16)
        rgb565 = (r << 11) | (g << 5) | b
        return rgb565
    elif output_format == 'YUYV':
        # Convert BGR to YUV first
        yuv = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2YUV)
        height, width = yuv.shape[:2]

        # Ensure width is even for YUYV format
        if width % 2 != 0:
            yuv = yuv[:, :-1, :]  # Remove last column if width is odd
            width -= 1

        # Create YUYV packed format: Y0 U0 Y1 V0 (correct size)
        yuyv = np.zeros((height, width * 2), dtype=np.uint8)

        for i in range(0, width, 2):
            # Get Y values for both pixels
            y0 = yuv[:, i, 0]      # Y for first pixel
            y1 = yuv[:, i + 1, 0]  # Y for second pixel

            # Average U and V values from both pixels for proper chroma subsampling
            u = ((yuv[:, i, 1].astype(np.uint16) + yuv[:, i + 1, 1].astype(np.uint16)) // 2).astype(np.uint8)
            v = ((yuv[:, i, 2].astype(np.uint16) + yuv[:, i + 1, 2].astype(np.uint16)) // 2).astype(np.uint8)

            # Pack into YUYV format: Y0 U Y1 V
            yuyv[:, i * 2] = y0         # Y0
            yuyv[:, i * 2 + 1] = u      # U (shared)
            yuyv[:, i * 2 + 2] = y1     # Y1
            yuyv[:, i * 2 + 3] = v      # V (shared)

        return yuyv
    elif output_format == 'UYVY422':
        # Convert BGR to YUV first
        yuv = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2YUV)
        height, width = yuv.shape[:2]

        # Ensure width is even for UYVY format
        if width % 2 != 0:
            yuv = yuv[:, :-1, :]
            width -= 1

        # Create UYVY packed format: U0 Y0 V0 Y1
        uyvy = np.zeros((height, width * 2), dtype=np.uint8)

        for i in range(0, width, 2):
            y0 = yuv[:, i, 0]
            y1 = yuv[:, i + 1, 0]
            u = ((yuv[:, i, 1].astype(np.uint16) + yuv[:, i + 1, 1].astype(np.uint16)) // 2).astype(np.uint8)
            v = ((yuv[:, i, 2].astype(np.uint16) + yuv[:, i + 1, 2].astype(np.uint16)) // 2).astype(np.uint8)

            # Pack into UYVY format: U Y0 V Y1
            uyvy[:, i * 2] = u          # U (shared)
            uyvy[:, i * 2 + 1] = y0     # Y0
            uyvy[:, i * 2 + 2] = v      # V (shared)
            uyvy[:, i * 2 + 3] = y1     # Y1

        return uyvy
    elif output_format == 'VYUY422':
        # Convert BGR to YUV first
        yuv = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2YUV)
        height, width = yuv.shape[:2]

        # Ensure width is even for VYUY format
        if width % 2 != 0:
            yuv = yuv[:, :-1, :]
            width -= 1

        # Create VYUY packed format: V0 Y0 U0 Y1
        vyuy = np.zeros((height, width * 2), dtype=np.uint8)

        for i in range(0, width, 2):
            y0 = yuv[:, i, 0]
            y1 = yuv[:, i + 1, 0]
            u = ((yuv[:, i, 1].astype(np.uint16) + yuv[:, i + 1, 1].astype(np.uint16)) // 2).astype(np.uint8)
            v = ((yuv[:, i, 2].astype(np.uint16) + yuv[:, i + 1, 2].astype(np.uint16)) // 2).astype(np.uint8)

            # Pack into VYUY format: V Y0 U Y1
            vyuy[:, i * 2] = v          # V (shared)
            vyuy[:, i * 2 + 1] = y0     # Y0
            vyuy[:, i * 2 + 2] = u      # U (shared)
            vyuy[:, i * 2 + 3] = y1     # Y1

        return vyuy
    elif output_format == 'YUVX':
        # Convert BGR to YUVX (YUV with padding byte)
        yuv = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2YUV)
        height, width = yuv.shape[:2]

        # Create YUVX format: Y U V X (X is padding, set to 0)
        yuvx = np.zeros((height, width, 4), dtype=np.uint8)
        yuvx[:, :, :3] = yuv  # Copy YUV components
        yuvx[:, :, 3] = 0     # Set X (padding) to 0

        return yuvx
    elif output_format == 'VUYX':
        # Convert BGR to VUYX (VUY with padding byte)
        yuv = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2YUV)
        height, width = yuv.shape[:2]

        # Create VUYX format: V U Y X (X is padding, set to 0)
        vuyx = np.zeros((height, width, 4), dtype=np.uint8)
        vuyx[:, :, 0] = yuv[:, :, 2]  # V to position 0
        vuyx[:, :, 1] = yuv[:, :, 1]  # U to position 1
        vuyx[:, :, 2] = yuv[:, :, 0]  # Y to position 2
        vuyx[:, :, 3] = 0             # Set X (padding) to 0

        return vuyx
    elif output_format == 'YCBCRA':
        # Convert BGR to YCbCrA (YCbCr with alpha channel)
        ycrcb = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2YCrCb)
        height, width = ycrcb.shape[:2]

        # Create YCbCrA format: Y Cb Cr A (A is alpha, set to 255 for opaque)
        ycbcra = np.zeros((height, width, 4), dtype=np.uint8)
        ycbcra[:, :, 0] = ycrcb[:, :, 0]  # Y
        ycbcra[:, :, 1] = ycrcb[:, :, 2]  # Cb from Cr position
        ycbcra[:, :, 2] = ycrcb[:, :, 1]  # Cr from Cb position
        ycbcra[:, :, 3] = 255             # Set A (alpha) to 255 (opaque)

        return ycbcra
    elif output_format == 'YCRCBA':
        # Convert BGR to YCrCbA (YCrCb with alpha channel)
        ycrcb = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2YCrCb)
        height, width = ycrcb.shape[:2]

        # Create YCrCbA format: Y Cr Cb A (A is alpha, set to 255 for opaque)
        ycrcba = np.zeros((height, width, 4), dtype=np.uint8)
        ycrcba[:, :, 0] = ycrcb[:, :, 0]  # Y
        ycrcba[:, :, 1] = ycrcb[:, :, 1]  # Cr
        ycrcba[:, :, 2] = ycrcb[:, :, 2]  # Cb
        ycrcba[:, :, 3] = 255             # Set A (alpha) to 255 (opaque)

        return ycrcba
    elif output_format in ['YUV420', 'YUV444P', 'YUV1P444', 'NV12', 'NV21']:
        # For YUV planar formats, return RGB for save_binary_image to handle
        # This maintains consistency with the save workflow
        return cv2.cvtColor(bgr_image, cv2.COLOR_BGR2RGB)
    elif output_format in ['JPEG', 'PNG', 'BMP']:
        # For standard image formats, we'll return the RGB image and handle encoding separately
        return cv2.cvtColor(bgr_image, cv2.COLOR_BGR2RGB)
    else:
        return bgr_image

def swap_format_name_channels(format_str: str) -> str:
    """Swap channels 0 and 2 in the format name.

    Args:
        format_str: Input format string (e.g., 'RGB', 'RGBA', 'YUYV')

    Returns:
        Format string with channels 0 and 2 swapped (e.g., 'BGR', 'BGRA', 'VYUV')

    Examples:
        'RGB' -> 'BGR'
        'BGR' -> 'RGB'
        'RGBA' -> 'BGRA'
        'BGRA' -> 'RGBA'
        'YUYV' -> 'VYUY'
        'UYVY422' -> 'UYVY422' (no change, as it's a packed format identifier)
        'YCBCRA' -> 'ACBCRY'
    """
    format_upper = format_str.upper()

    # Handle special cases where format names don't directly map to channel order
    special_mappings = {
        'RGB': 'BGR',
        'BGR': 'RGB',
        'RGBA': 'BGRA',
        'BGRA': 'RGBA',
        'RGB565': 'BGR565',
        'BGR565': 'RGB565',
        'YUYV': 'YUYV',
        'VYUY422': 'VYUY422',
        'UYVY422': 'UYVY422',  # No swap for this format
        'YUVX': 'VUYX',
        'VUYX': 'YUVX',
        'YCBCRA': 'CRCBYA',
        'YCRCBA': 'CBCRYA',
        'NV12': 'NV12',  # No swap for planar formats
        'NV21': 'NV21',
        'YUV420': 'YUV420',
        'YUV444P': 'YUV444P',
        'YUV1P444': 'YUV1P444',
        'GRAY': 'GRAY',  # Single channel, no swap
        'JPEG': 'JPEG',  # Compressed format, no swap
        'PNG': 'PNG',
        'BMP': 'BMP'
    }

    if format_upper in special_mappings:
        return special_mappings[format_upper]

    # For generic formats, swap first and third characters
    if len(format_str) >= 3:
        chars = list(format_str)
        chars[0], chars[2] = chars[2], chars[0]
        return ''.join(chars)

    # If format is too short to swap, return as is
    return format_str

def swap_channels_0_2(image: np.ndarray) -> np.ndarray:
    """Swap channels 0 and 2 of the input image."""
    if len(image.shape) < 3 or image.shape[2] < 3:
        raise ValueError("Image must have at least 3 channels to swap channels 0 and 2")

    swapped_image = image.copy()
    swapped_image[:, :, 0], swapped_image[:, :, 2] = image[:, :, 2].copy(), image[:, :, 0].copy()
    return swapped_image

def parse_crop_params(crop_str: str, image_width: int, image_height: int) -> tuple:
    """Parse crop parameters and validate against image dimensions.

    Args:
        crop_str: Crop string in format "left:top[:width[:height]]"
        image_width: Width of the source image
        image_height: Height of the source image

    Returns:
        Tuple of (left, top, width, height)
    """
    try:
        parts = crop_str.split(':')
        if len(parts) < 2 or len(parts) > 4:
            raise ValueError("Crop format must be: left:top[:width[:height]]")

        left = int(parts[0])
        top = int(parts[1])

        # Calculate maximum possible dimensions
        max_width = image_width - left
        max_height = image_height - top

        # Use provided width/height or maximum
        width = int(parts[2]) if len(parts) > 2 else max_width
        height = int(parts[3]) if len(parts) > 3 else max_height

        # Validate crop parameters
        if left < 0 or top < 0:
            raise ValueError(f"Crop left ({left}) and top ({top}) must be non-negative")

        if left >= image_width or top >= image_height:
            raise ValueError(f"Crop position ({left}, {top}) is outside image bounds ({image_width}x{image_height})")

        if width <= 0 or height <= 0:
            raise ValueError(f"Crop dimensions ({width}x{height}) must be positive")

        if left + width > image_width:
            raise ValueError(f"Crop region exceeds image width: {left} + {width} > {image_width}")

        if top + height > image_height:
            raise ValueError(f"Crop region exceeds image height: {top} + {height} > {image_height}")

        return (left, top, width, height)

    except ValueError as e:
        if "invalid literal" in str(e):
            raise ValueError("Crop parameters must be integers")
        raise

def crop_image(image: np.ndarray, left: int, top: int, width: int, height: int) -> np.ndarray:
    """Crop image using NumPy array slicing (OpenCV doesn't have a dedicated crop function).

    Args:
        image: Input image array
        left: Left coordinate of crop region
        top: Top coordinate of crop region
        width: Width of crop region
        height: Height of crop region

    Returns:
        Cropped image array
    """
    # OpenCV/NumPy uses array slicing for cropping: image[y:y+h, x:x+w]
    if len(image.shape) == 2:
        # Grayscale image
        return image[top:top+height, left:left+width].copy()
    else:
        # Color image (with channels)
        return image[top:top+height, left:left+width, :].copy()

def resize_with_aspect_ratio(image: np.ndarray, target_width: int, target_height: int,
                             keep_aspect: bool, background_color: str = 'black') -> np.ndarray:
    """Resize image with optional aspect ratio preservation and padding."""
    # Resize and pad image using Image object from PIL
    input_image = Image.fromarray(image)

    if not keep_aspect:
        # Simple resize without aspect ratio preservation
        output_image = input_image.resize((target_width, target_height), Image.Resampling.BICUBIC)
        return np.array(output_image)

    if background_color.lower() == 'white':
        if len(image.shape) == 2:  # Grayscale
            color = (255, )
        elif len(image.shape) == 3:  # Color
            color = (255, 255, 255)
        else:
            color = (255, )
    else:  # black or default
        if len(image.shape) == 2:  # Grayscale
            color = (0, )
        elif len(image.shape) == 3:  # Color
            color = (0, 0, 0)
        else:
            color = (0, )

    # Resize and pad image using Image object from PIL
    output_image = ImageOps.pad(input_image, (target_width, target_height), method=Image.Resampling.BICUBIC, color=color)
    return np.array(output_image)

def save_binary_image(image: np.ndarray, output_path: str, format_str: str):
    """Save image in the specified binary format."""
    try:
        # Handle YUV formats properly
        if format_str == 'YUV1P444':
            if len(image.shape) == 3 and image.shape[2] == 3:
                binary_data = convert_rgb_to_yuv444i(image)
            else:
                raise ValueError("Invalid image format for YUV1P444 conversion")

        elif format_str == 'YUV444P':
            if len(image.shape) == 3 and image.shape[2] == 3:
                binary_data = convert_rgb_to_yuv444(image)
            else:
                raise ValueError("Invalid image format for YUV444P conversion")

        elif format_str == 'YUV420':
            # Convert RGB back to YUV420 planar
            if len(image.shape) == 3 and image.shape[2] == 3:
                binary_data = convert_rgb_to_yuv420(image)
            else:
                raise ValueError("Invalid image format for YUV420 conversion")

        elif format_str == 'NV12':
            if len(image.shape) == 3 and image.shape[2] == 3:
                binary_data = convert_rgb_to_nv12(image)
            else:
                raise ValueError("Invalid image format for NV12 conversion")

        elif format_str == 'NV21':
            if len(image.shape) == 3 and image.shape[2] == 3:
                binary_data = convert_rgb_to_nv21(image)
            else:
                raise ValueError("Invalid image format for NV21 conversion")

        elif format_str == 'RGB565':
            # Image should already be in RGB565 format (uint16)
            binary_data = image.astype(np.uint16).tobytes()

        elif format_str in ['RGB', 'BGR']:
            binary_data = image.astype(np.uint8).tobytes()

        elif format_str in ['RGBA', 'BGRA']:
            binary_data = image.astype(np.uint8).tobytes()

        elif format_str == 'GRAY':
            binary_data = image.astype(np.uint8).tobytes()

        elif format_str in ['YUYV', 'UYVY422', 'VYUY422']:
            # Packed YUV data should be saved as is
            binary_data = image.astype(np.uint8).tobytes()

        elif format_str in ['YUVX', 'VUYX']:
            binary_data = image.astype(np.uint8).tobytes()

        elif format_str == 'YCBCRA':
            binary_data = image.astype(np.uint8).tobytes()

        elif format_str == 'YCRCBA':
            binary_data = image.astype(np.uint8).tobytes()

        else:
            # Default: save as RGB
            print(f"Warning: Unknown format {format_str}, saving as RGB")
            binary_data = image.astype(np.uint8).tobytes()

        # Write binary data to file
        with open(output_path, 'wb') as f:
            f.write(binary_data)

        print(f"Binary image saved to: {output_path} (format: {format_str})")

    except Exception as e:
        raise RuntimeError(f"Error saving image: {e}")

def save_raw_binary_image(image: np.ndarray, output_path: str, format_str: str):
    """Save raw image data without format conversion."""
    try:
        # Convert to bytes and save
        binary_data = image.astype(image.dtype).tobytes()

        with open(output_path, 'wb') as f:
            f.write(binary_data)

        print(f"Raw binary image saved to: {output_path} (format: {format_str})")

    except Exception as e:
        raise RuntimeError(f"Error saving raw binary image: {e}")

def display_image(image: np.ndarray, title: str, format_str: str):
    """Display image using OpenCV with robust format handling."""
    if image is None or image.size == 0:
        print(f"Warning: Cannot display empty image for {title}")
        return

    display_image = image.copy()

    try:
        # Handle different image formats robustly
        if len(image.shape) == 2:
            # Grayscale image
            if format_str == 'GRAY':
                pass  # Already in correct format
            else:
                # Convert single channel to 3-channel for display
                display_image = cv2.cvtColor(display_image, cv2.COLOR_GRAY2BGR)

        elif len(image.shape) == 3:
            channels = image.shape[2]

            if channels == 1:
                # Single channel in 3D array
                display_image = cv2.cvtColor(display_image.squeeze(), cv2.COLOR_GRAY2BGR)
            elif channels == 3:
                # 3-channel image
                if format_str in ['RGB', 'YUV420', 'YUV444P', 'YUV1P444', 'NV12', 'NV21', 'YUYV', 'UYVY422', 'VYUY422', 'JPEG', 'PNG', 'BMP']:
                    display_image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
                elif format_str in ['BGR']:
                    pass  # Already in BGR
                else:
                    # Default: assume RGB and convert to BGR
                    print("Display rgb565")
                    display_image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
            elif channels == 4:
                # 4-channel image
                if format_str == 'RGBA':
                    display_image = cv2.cvtColor(image, cv2.COLOR_RGBA2BGR)
                elif format_str == 'BGRA':
                    display_image = cv2.cvtColor(image, cv2.COLOR_BGRA2BGR)
                elif format_str in ['YUVX', 'VUYX']:
                    # Extract YUV components and convert
                    if format_str == 'YUVX':
                        yuv = image[:, :, :3]  # Y U V order
                    else:  # VUYX
                        # Reorder from VUYX to YUV
                        yuv = np.zeros((image.shape[0], image.shape[1], 3), dtype=np.uint8)
                        yuv[:, :, 0] = image[:, :, 2]  # Y from position 2
                        yuv[:, :, 1] = image[:, :, 1]  # U from position 1
                        yuv[:, :, 2] = image[:, :, 0]  # V from position 0

                    rgb = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB)
                    display_image = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
                elif format_str == 'YCBCRA':
                    # Extract YCbCr components and convert (ignore alpha)
                    ycbcr = image[:, :, :3]
                    # Convert YCbCr to YCrCb for OpenCV
                    ycrcb = np.zeros_like(ycbcr)
                    ycrcb[:, :, 0] = ycbcr[:, :, 0]  # Y
                    ycrcb[:, :, 1] = ycbcr[:, :, 2]  # Cr from Cr
                    ycrcb[:, :, 2] = ycbcr[:, :, 1]  # Cb from Cb

                    rgb = cv2.cvtColor(ycrcb, cv2.COLOR_YCrCb2RGB)
                    display_image = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
                elif format_str == 'YCRCBA':
                    # Extract YCrCb components and convert (ignore alpha)
                    ycrcb = image[:, :, :3]

                    rgb = cv2.cvtColor(ycrcb, cv2.COLOR_YCrCb2RGB)
                    display_image = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
                else:
                    # Default: take first 3 channels and assume RGB
                    display_image = cv2.cvtColor(image[:, :, :3], cv2.COLOR_RGB2BGR)
            else:
                raise ValueError(f"Unsupported number of channels: {channels}")
        else:
            raise ValueError(f"Unsupported image shape: {image.shape}")

        # Ensure image is in valid range
        if display_image.dtype != np.uint8:
            if display_image.max() <= 1.0:
                display_image = (display_image * 255).astype(np.uint8)
            else:
                display_image = np.clip(display_image, 0, 255).astype(np.uint8)

        cv2.imshow(title, display_image)
        print(f"Displaying: {title} - Press any key to continue...")
        cv2.waitKey(0)

    except Exception as e:
        print(f"Error displaying image {title}: {e}")
        print(f"Image shape: {image.shape}, dtype: {image.dtype}, format: {format_str}")

def cleanup_temp_files(*file_paths):
    """Clean up temporary files safely."""
    for file_path in file_paths:
        try:
            if os.path.exists(file_path):
                os.remove(file_path)
                print(f"Cleaned up temporary file: {file_path}")
        except OSError as e:
            print(f"Warning: Could not remove temporary file {file_path}: {e}")

def generate_header_file(data: bytes, filename: str, width: int, height: int, format_str: str,
                        command_line: str, output_path: str, input_src_file: str):
    """Generate a C header file with the image data."""

    # Generate variable name
    var_name = generate_variable_name(filename, width, height, format_str)

    # Replace filename with "{var_name}.h"
    output_path = os.path.join(os.path.dirname(output_path), f"{var_name}.h")

    # Get current date for copyright
    current_year = datetime.now().year

    # Get MPP pixel format and channels number
    mpp_format = get_mpp_pixel_format(format_str)
    channels_number = get_channels_number(format_str)

    # Create header content
    header_content = f"""/*
 * Copyright {current_year} NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Generated by: python {command_line}
 * Source file: {input_src_file}
 * Dimensions: {width}x{height}
 * Format: {format_str}
 * Data size: {len(data)} bytes
 */

#ifndef __{var_name.upper()}_H__
#define __{var_name.upper()}_H__

#define SRC_IMAGE_{var_name.upper()}_WIDTH           {width}
#define SRC_IMAGE_{var_name.upper()}_HEIGHT          {height}
#define SRC_IMAGE_{var_name.upper()}_CHANNELS_NUMBER {channels_number}
#define SRC_IMAGE_{var_name.upper()}_FORMAT          {mpp_format}

static const unsigned char {var_name.lower()}_data[] = {{
"""

    # Add data bytes (12 bytes per line)
    for i in range(0, len(data), 12):
        line_data = data[i:i+12]
        hex_values = ', '.join(f'0x{b:02x}' for b in line_data)

        # Add comment with byte offset
        if i + 12 < len(data):
            header_content += f"  {hex_values},\n"
        else:
            header_content += f"  {hex_values}"

    # Close the array and header guard
    header_content += f"""
}};
const unsigned int {var_name.lower()}_data_len = {len(data)};

#endif /* __{var_name.upper()}_H__ */
"""

    # Write to file
    try:
        with open(output_path, 'w') as f:
            f.write(header_content)
        print(f"Header file generated: {output_path}")
        print(f"Variable name: {var_name}_data")
        print(f"Width define: {var_name.upper()}_WIDTH")
        print(f"Height define: {var_name.upper()}_HEIGHT")
        print(f"Format define: {var_name.upper()}_FORMAT")
        print(f"Channels define: {var_name.upper()}_CHANNELS_NUMBER")
    except Exception as e:
        raise RuntimeError(f"Error writing header file: {e}")

def calc_checksum(file, width, height):
    """
    Computes the checksum of the image file
    using Pisano with End-Around Carry algorithm
    which is almost as reliable but faster than CRC32.
    See https://hackaday.io/project/178998-peac-pisano-with-end-around-carry-algorithm
    Note: with odd image size, last byte is ignored.
    :param file: image file
    :param size: size in bytes of image
    :param channels: channels of image
    The computed 32bit checksum is printed to the console.
    """
    id = 0
    # shape to uint16
    array = np.fromfile(file, dtype=np.uint16)
    channels = array.nbytes // (width * height)
    size = width * height
    reshape_size = int(size * channels/2)
    x = 0x1234
    y = int("0xABCD", 16)
    c = size * channels
    # 16b word count
    w_cnt = c/2
    array = array.reshape(reshape_size)
    print(f"Checksum compute parameters: WIDTH {width} HEIGHT {height} CHANNELS {channels}")
    while w_cnt > 0:
        c += x
        c += y
        y  = int(x) + int(array[id])
        x  = c&0xFFFF
        c = c>>16
        w_cnt -=1
        id +=1
    print(f"CHECKSUM = {hex(((x | (y << 16)) & 0xFFFFFFFF))}")

def main():
    # Create formatted list of supported formats for help text
    formats_list = ', '.join(sorted(SUPPORTED_FORMATS.keys()))

    parser = argparse.ArgumentParser(description='Convert and display binary image files')
    parser.add_argument('-i', '--input', required=True, help='Input binary image file path')
    parser.add_argument('-W', '--width', type=int, help='Input image width (not required for JPEG)')
    parser.add_argument('-H', '--height', type=int, help='Input image height (not required for JPEG)')
    parser.add_argument('-f', '--format', required=True,
                        help=f'Input pixel format. Supported formats: {formats_list}')
    parser.add_argument('-ow', '--output-width', type=int, help='Output image width (optional)')
    parser.add_argument('-oh', '--output-height', type=int, help='Output image height (optional)')
    parser.add_argument('-of', '--output-format',
                        help=f'Output pixel format (optional). Supported formats: {formats_list}')
    parser.add_argument('-o', '--output', help='Output file path (optional, saves converted image)')
    parser.add_argument('-q', '--quality', type=int, default=95, help='JPEG quality (1-100, default: 95)')
    parser.add_argument('-c', '--compression', type=int, default=9,
                        help='PNG compression level (0-9, default: 9)')
    parser.add_argument('-k', '--keep-aspect-ratio', action='store_true',
                        help='Keep aspect ratio when resizing (adds padding if needed)')
    parser.add_argument('-b', '--background', choices=['black', 'white'], default='black',
                        help='Background color for padding when keeping aspect ratio (default: black)')
    parser.add_argument('-gh', '--generate-header', help='Generate C header file with image data at specified path')
    parser.add_argument('-s', '--swap-ch0-ch2', action='store_true',
                        help='Swap channels 0 and 2 of input image and save to output file (no display)')
    parser.add_argument('--crop',  help='Crop region in format: left:top[:width[:height]]. Width and height are optional (uses maximum if not specified)')
    parser.add_argument('--quality-metrics', action='store_true', help='Calculate and display image quality metrics (brightness and contrast)')

    args = parser.parse_args()

    if args.output_format and args.output_format.upper() in ['JPEG', 'PNG', 'BMP'] and not args.output:
        temp_file_name = 'temp_converted_image.' + args.output_format.lower()
    else:
        temp_file_name = 'temp_converted_image.bin'

    try:
        with opencv_window_manager(), temp_file_manager(temp_file_name) as temp_file_name:
            # Parse and validate formats
            input_format = parse_pixel_format(args.format)
            output_format = parse_pixel_format(args.output_format) if args.output_format else input_format

            # Validate JPEG quality
            if not (1 <= args.quality <= 100):
                raise ValueError("JPEG quality must be between 1 and 100")

            # Auto-detect image files
            detected_format = is_image_file(args.input)
            if detected_format and input_format not in ['JPEG', 'PNG', 'BMP']:
                print(f"Auto-detected {detected_format} file, switching input format to {detected_format}")
                input_format = detected_format

            # For standard image formats, width and height are not required
            if input_format not in ['JPEG', 'PNG', 'BMP'] and (args.width is None or args.height is None):
                raise ValueError("Width (-W) and height (-H) are required for binary formats")

            # Set default dimensions for JPEG
            width = args.width or 0
            height = args.height or 0

            print(f"Reading binary image: {args.input}")
            if input_format == 'JPEG':
                print(f"Input format: {input_format}")
            else:
                print(f"Input dimensions: {width}x{height}, format: {input_format}")

            # Read the binary image
            image = read_binary_image(args.input, width, height, input_format)
            # Update dimensions for JPEG files
            if input_format == 'JPEG':
                height, width = image.shape[:2]
                print(f"Actual JPEG dimensions: {width}x{height}")

            # Apply crop if specified (before any other operations)
            if args.crop:
                crop_left, crop_top, crop_width, crop_height = parse_crop_params(args.crop, width, height)
                print(f"Cropping image: left={crop_left}, top={crop_top}, width={crop_width}, height={crop_height}")
                image = crop_image(image, crop_left, crop_top, crop_width, crop_height)

                # Update dimensions after crop
                width = crop_width
                height = crop_height
                print(f"Image dimensions after crop: {width}x{height}")

            # Display original image
            display_image(image, f"Original Image ({width}x{height}, {input_format})", input_format)

            # Variables for header generation
            header_data = None
            header_format = input_format
            header_width = width
            header_height = height
            header_source_file = args.input

            # Handle channel swap operation
            if args.swap_ch0_ch2:
                # Handle output saving
                if args.output:
                    file_name = args.output
                else:
                    file_name = temp_file_name

                # For channel swap, read raw binary data
                if input_format in ['JPEG', 'PNG', 'BMP']:
                    # For standard image formats, use regular reading
                    print(f"Reading {input_format} image for channel swap...")
                    raw_image = image  # Use already read image
                    swapped_image = swap_channels_0_2(raw_image)

                    # Save using appropriate format function
                    if input_format == 'JPEG':
                        file_name = save_jpeg(swapped_image, file_name, args.quality)
                    elif input_format == 'PNG':
                        file_name = save_png(swapped_image, file_name, args.compression)
                    elif input_format == 'BMP':
                        file_name = save_bmp(swapped_image, file_name)
                else:
                    # For binary formats, read raw data
                    print(f"Reading raw binary data for channel swap...")
                    raw_image = read_raw_binary_image(args.input, width, height, input_format)

                    # Perform channel swap on raw data
                    print(f"Swapping channels 0 and 2 in raw format...")
                    swapped_image = swap_channels_0_2(raw_image)

                    # Save raw binary data
                    save_raw_binary_image(swapped_image, file_name, input_format)

                print(f"Channel-swapped image saved to: {file_name}")

                if input_format not in ['JPEG', 'PNG', 'BMP']:
                    calc_checksum(file_name, width, height)

                # Update header generation variables
                header_data = None
                header_format = swap_format_name_channels(input_format)
                header_width = width
                header_height = height
                header_source_file = file_name

            # Convert if output parameters are specified
            elif args.output_width or args.output_height or args.output_format:
                converted_image = image.copy()

                # Resize if dimensions are specified
                if args.output_width and args.output_height:
                    print(f"Resizing to: {args.output_width}x{args.output_height}")

                    if args.keep_aspect_ratio:
                        print(f"Keeping aspect ratio with {args.background} background padding")

                    converted_image = resize_with_aspect_ratio(
                        converted_image,
                        args.output_width,
                        args.output_height,
                        args.keep_aspect_ratio,
                        args.background
                    )

                    width = args.output_width
                    height = args.output_height

                # Convert format
                print(f"Converting format: {input_format} -> {output_format}")
                converted_image = convert_image_format(converted_image, input_format, output_format)

                # Handle output saving
                if args.output:
                    file_name = args.output
                else:
                    file_name = temp_file_name

                # Save image to file
                if output_format == 'JPEG':
                    file_name = save_jpeg(converted_image, file_name, args.quality)
                elif output_format == 'PNG':
                    file_name = save_png(converted_image, file_name, args.compression)
                elif output_format == 'BMP':
                    file_name = save_bmp(converted_image, file_name)
                else:
                    save_binary_image(converted_image, file_name, output_format)

                # Update header generation variables to use output format
                header_format = output_format
                header_width = args.output_width or width
                header_height = args.output_height or height
                header_source_file = file_name

                if output_format not in ['JPEG', 'PNG', 'BMP']:
                    calc_checksum(file_name, width, height)

                # Re-read the temp binary image to ensure proper format conversion and display
                converted_image = read_binary_image(file_name, width, height, output_format)

                # Display converted image
                out_w = args.output_width or width
                out_h = args.output_height or height
                display_image(converted_image, f"Converted Image ({out_w}x{out_h}, {output_format})", output_format)

                print(f"Conversion completed successfully!")
            else:
                print("No conversion parameters specified. Only displaying original image.")

                if input_format not in ['JPEG', 'PNG', 'BMP']:
                    calc_checksum(args.input, width, height)

            # Generate header file if requested
            if args.generate_header:
                print(f"Generating header file from {header_format} format...")

                # Read the binary data for header generation
                with open(header_source_file, 'rb') as f:
                    header_data = f.read()

                # Build command line string for header comment
                command_line = ' '.join(sys.argv)

                # Generate the header file
                generate_header_file(
                    header_data,
                    os.path.basename(args.generate_header),
                    header_width,
                    header_height,
                    header_format,
                    command_line,
                    args.generate_header,
                    args.input
                )

            # Calculate and print quality metrics if requested
            if args.quality_metrics:
                print(f"Calculating quality metrics...")
                # Brightness and contrast can be calculated from the luma component of YCbCr
                ycbcra_converted_image = convert_image_format(image, input_format, "YCBCRA")
                y_channel = ycbcra_converted_image[:, :, 0]  # Y
                brightness = np.mean(y_channel)
                contrast = np.std(y_channel)
                print(f"  Brightness - mean of luma component (Y): {int(brightness)}")
                print(f"  Contrast - std of luma component (Y): {int(contrast)}")

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
