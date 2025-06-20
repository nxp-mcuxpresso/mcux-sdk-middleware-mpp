# Copyright 2023-2025 NXP
# All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

""" 
This script is used for raw image validation and raw image operations.

It performs:
- the display of the input image
- the checksum calculation of the input image.

The script can also optionally performs one of the following image conversion operation:
- conversion from JPEG to uncompressed format + rescale on the input image  
- channel reordering

For the JPEG conversion the supported pixel formats include:
- rgb888: 24-bit RGB [R, G, B]
- bgr888: 24-bit BGR [B, G, R]
- rgb565: 16-bit compact RGB
- bgra: 32-bit BGR with alpha channel
- gray8: 8-bit grayscale "Y800"
- yuyx, uyvy422, yuy422: YUV 4:2:2 packed formats

The channel reordering applies with groups of 4 bytes [b0,b1,b2,b3].
It inverts byte 0 with byte 2 and provides an output image reordered with the group of 4 bytes as [b2,b1,b0,b3].
- conversion from RGB565 to RGB888
- conversion from RGB565 to BGR888

The script supports the conversion 
Note:
----
In case of image conversion required, the display and checksum calculation
applies to the output image (image after conversion).

""" 
from PIL import Image, ImageOps
import  cv2
import numpy as np
import io
import argparse
import os
import sys


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input", help="Input file: -.jpg for conversion to raw image -.rgb for image operations", required=True)
    parser.add_argument("-o", "--output", help="Output file: raw image ", required=True)
    parser.add_argument("-W", "--width", type=int, help="Original input image width")
    parser.add_argument("-H", "--height", type=int, help="Original input image height")
    parser.add_argument("-C", "--channels", type=int, help="Input image channels (each channel is interpreted as a byte)")
    parser.add_argument("--rsz_w", type=int, help ="Width for resize" )
    parser.add_argument("--rsz_h", type=int, help ="Height for resize" )
    parser.add_argument("--rsz_mode",type=str, help ="Resize mode: exact (without aspect ratio) or with padding (maintains aspect ratio)",
                        choices=['exact', 'pad'])
    parser.add_argument("--img_op", 
                        choices=[ 'conv_JPEG2FMT', 'swap_ch0_ch2', 'conv_RGB5652RGB', 'conv_RGB5652BGR', 'conv_RGB2BGR'], help="Image operation")
    parser.add_argument("--pix_fmt", 
                        choices=['yuvx', 'vuyx', 'uyvy422', 'vyuy422', 'rgb888', 'bgr888', 'rgb565', 'bgra', 'gray8'], help="Input pixel format", required=True)

    return parser.parse_args()

args = parse_args()
print(args)

output_pix_fmt = ''

def conv_from_JPGToFORMAT(out_file, pix_fmt):
    """
    Perform conversion on the .jpg file to uncompressed format 
    Perfroms resize on the input image 
    Use input file from argument as image to convert
    :param out_file: output converted image file
    :param pix_fmt: pixel format of converted image
    """
    # Reads the .jpg or .bmp file in full resolution rgb888 before resize
    # Sets values for width and height if resize values are not provided
    input_image = Image.open(args.input)
    args.width, args.height = input_image.size

    # Perform resize and maintains the aspect ratio with padding
    # Performes resize to desired (width, height) without aspect ratio 
    # Sets the width and height to the resized values to compute calc_checksum/display_with_cv
    if args.rsz_w and args.rsz_h:

    # Perform resize without maintaining the aspect ratio
        if args.rsz_mode == 'exact':
            input_image = input_image.resize((args.rsz_w, args.rsz_h), Image.Resampling.BICUBIC)
        else:
        # Perform resize with padding to maintain the aspect ratio
            input_image = ImageOps.pad(input_image, (args.rsz_w, args.rsz_h), method =Image.Resampling.BICUBIC, color=(0, 0, 0))
            
    args.width, args.height = args.rsz_w, args.rsz_h

    input_image = np.array(input_image)
    input_image = cv2.cvtColor(input_image, cv2.COLOR_RGB2BGR)

    # Convert to uncompressed pixel format
    # Sets the args.channels value according to the format to compute calc_checksum/display_with_cv
    if (pix_fmt == 'bgr888'):
        input_image.tofile(out_file, '')
    elif (pix_fmt == 'rgb888'):
        input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2RGB)
        input_image.tofile(out_file, '')
    elif (pix_fmt == 'rgb565'):
        b = (input_image[:, :, 0] >> 3).astype(np.uint16)
        g = (input_image[:, :, 1] >> 2).astype(np.uint16)
        r = (input_image[:, :, 2] >> 3).astype(np.uint16)
        input_image = (r << 11) | (g << 5) | b 
        input_image.tofile(out_file, '')
    elif (pix_fmt == 'bgra'):
        input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2BGRA)
        input_image.tofile(out_file, '')
    elif(pix_fmt == 'gray8'):
         input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2GRAY)
         input_image.tofile(out_file, '')
    elif (pix_fmt == 'vuyx'):
        input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2YCrCb)
        y = input_image[:, :, 0:1]
        cr = input_image[:, :, 1:2]
        cb = input_image[:, :, 2:3]
        x = np.full_like(y, 255, dtype=np.uint8)
        np.concatenate((cr, cb, y, x), axis=2).tofile(out_file, '')
    elif (pix_fmt == 'yuvx'):
        input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2YCrCb)
        y = input_image[:, :, 0:1]
        cr = input_image[:, :, 1:2]
        cb = input_image[:, :, 2:3]
        x = np.full_like(y, 255, dtype=np.uint8)
        np.concatenate((y, cb, cr, x), axis=2).tofile(out_file, '')
    elif (pix_fmt == 'vyuy422'):
        input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2YCrCb)
        y = input_image[:, :, 0]
        cb = input_image[:, :, 1]
        cr = input_image[:, :, 2]
        if (args.width % 2 !=0) :
            print("Error: width must be even for", args.pix_fmt, " format")
            sys.exit('Error: invalid width value')
        y0 = y [:, 0::2]
        y1 = y[:, 1::2]
        u = cb[:, 0::2]
        v = cr[:, 0::2]
        input_image = np.stack((u, y0, v, y1), axis=-1).reshape(y.shape[0], -1)
        input_image.tofile(out_file, '') 
    elif (pix_fmt == 'uyvy422'):
        input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2YCrCb)
        y = input_image[:, :, 0]
        cb = input_image[:, :, 1]
        cr = input_image[:, :, 2]
        if (args.width % 2 !=0) :
            print("Error: width must be even for", args.pix_fmt, " format")
            sys.exit('Error: invalid width value')
        y0 = y [:, 0::2]
        y1 = y[:, 1::2]
        u = cb[:, 0::2]
        v = cr[:, 0::2]
        input_image = np.stack((v, y0, u, y1), axis=-1).reshape(y.shape[0], -1)
        input_image.tofile(out_file, '') 
    else:
        print("Error: pixel format:", pix_fmt, " is not supported for conversion from JPEG")
        sys.exit('Error: invalid pixel format')


def invert_channel0_channel2():
    """
    Image conversion performing swap of byte 0 with byte 2
    """
    reshape_bytes = 4
    reshape_width = int(args.width*args.channels/reshape_bytes)
    reshape_height = args.height
    print('reshape_bytes:', reshape_bytes)
    print('reshape_width:', reshape_width)
    print('reshape_height:', reshape_height)
    global output_pix_fmt

    print('Image operation: Invert ch0 with ch2')

    array = np.fromfile(args.input, dtype=np.uint8).reshape(reshape_width,reshape_height,reshape_bytes)
    ch0, ch1, ch2, ch3 = array[:, :, 0], array[:, :, 1], array[:, :, 2], array[:, :, 3]
    output = np.dstack((ch2,ch1,ch0,ch3))

    # Update the output pixel format for display of the output image
    if (args.pix_fmt == 'yuvx'):
         output_pix_fmt = "vuyx"
    elif (args.pix_fmt == 'uyvy422'):
        output_pix_fmt = "vyuy422"
    else:
        print("Error: pixel format:", args.pix_fmt, " is not supported for image convert")
        sys.exit('Error: invalid pixel format')

    output.tofile(args.output, '')


def calc_checksum(file, size, channels):
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
    reshape_size = int(size * channels/2)
    x = 0x1234
    y = int("0xABCD", 16)
    c = size * channels
    # 16b word count
    w_cnt = c/2
    array = np.fromfile(file, dtype=np.uint16).reshape(reshape_size)
    while w_cnt > 0:
        c += x
        c += y
        y  = int(x) + int(array[id])
        x  = c&0xFFFF
        c = c>>16
        w_cnt -=1
        id +=1
    print('CHECKSUM=', hex(((x | (y << 16)) & 0xFFFFFFFF)))

def display_with_cv(file, pix_fmt, channels):
    """
    Display image contained in file using opencv
    :param file: image file
    :param pix_fmt: image pixel format
    :param channels: channels number of the image to display
    """
    # OpenCV imshow() loads as a NumPy array ndarray of row (height) x column (width) x color (3). The order of color is BGR (blue, green, red).
    array = np.fromfile(file, dtype=np.uint8).reshape((args.height,args.width,channels))
    print("Display image with pixel format:", pix_fmt)

    # Color conversion should apply to display BGR format
    if (pix_fmt == 'rgb565'):
        array = np.fromfile(file, dtype=np.uint16).reshape((args.height,args.width))
        r = ((array >> 11) & 0x1F) << 3
        g = ((array >> 5) & 0x3F) << 2
        b = (array & 0x1F) << 3
        array = np.stack((b, g, r), axis=-1).astype(np.uint8)
        # Already stored in BGR above
        cv2_cvtColor=''
    elif (pix_fmt == 'rgb888'):
         cv2_cvtColor='cv2.COLOR_RGB2BGR'
    elif (pix_fmt == 'bgr888'):
        cv2_cvtColor=''
    elif (pix_fmt == 'bgra'):
        cv2_cvtColor='cv2.COLOR_BGRA2BGR'
    elif (pix_fmt == 'gray8'):
        # grayscale display (1 channel, shape = [H,W])
        cv2_cvtColor='cv2.COLOR_GRAY2BGR'
    elif (pix_fmt == 'uyvy422'):
        cv2_cvtColor='cv2.COLOR_YUV2BGR_Y422'
    elif (pix_fmt == 'vyuy422'):
        cv2_cvtColor='cv2.COLOR_YUV2RGB_Y422'
    elif (pix_fmt == 'yuvx'):
        cv2_cvtColor='cv2.COLOR_YUV2BGR'
        # remove alpha channel
        ch0, ch1, ch2, ch3 = array[:, :, 0], array[:, :, 1], array[:, :, 2], array[:, :, 3]
        array = np.dstack((ch0,ch1,ch2))
    elif (pix_fmt == 'vuyx'):
        cv2_cvtColor='cv2.COLOR_YUV2BGR'
        # remove alpha channel
        ch0, ch1, ch2, ch3 = array[:, :, 0], array[:, :, 1], array[:, :, 2], array[:, :, 3]
        array = np.dstack((ch2,ch1,ch0))
    elif (pix_fmt == 'vuyx'):
        cv2_cvtColor='cv2.COLOR_YUV2BGR'
        # remove alpha channel and swap 'Y' with 'V' to comply with COLOR_YUV2BGR
        ch0, ch1, ch2, ch3 = array[:, :, 0], array[:, :, 1], array[:, :, 2], array[:, :, 3]
        array = np.dstack((ch2,ch1,ch0))
    else:
        print("Error: pixel format:", pix_fmt, " is not supported for display")
        sys.exit('Error: invalid pixel format')

    if (cv2_cvtColor != ''):
        array = cv2.cvtColor(array, eval(cv2_cvtColor))

    cv2.imshow('', array)
    cv2.waitKey()
    cv2.destroyAllWindows()

# RGB565 format
RGB565_RMASK  = 0x1F
RGB565_GMASK  = 0x3F
RGB565_BMASK  = 0x1F
RGB565_RSHIFT = 11
RGB565_GSHIFT = 5
RGB565_BSHIFT = 0

def conv_from_RGB565ToRGB(out_file, out_pix_fmt):
    """
    Perform conversion from RGB565 to RGB888 and BGR888
    Use input file from argument as image to convert
    :param out_file: output converted image file
    :param out_pix_fmt: pixel format of converted image
    """
    array = np.fromfile(args.input, dtype=np.uint16).reshape(args.height,args.width,1)

    r = ((array >> RGB565_RSHIFT) & RGB565_RMASK) << 3
    g = ((array >> RGB565_GSHIFT) & RGB565_GMASK) << 2
    b = (array & RGB565_BMASK) << 3

    # Compose into one 3-dimensional matrix of 8-bit integers
    bgr = np.dstack((b,g,r)).astype(np.uint8)
    rgb = np.dstack((r,g,b)).astype(np.uint8)

    if (out_pix_fmt == 'rgb888'):
        rgb.tofile(out_file, '')
    elif (out_pix_fmt == 'bgr888'):
        bgr.tofile(out_file, '')
    else:
        print("Error: pixel format:", out_pix_fmt, " is not supported for conversion from RGB565")
        sys.exit('Error: invalid pixel format')

def conv_from_RGBoBGR(out_file, out_pix_fmt):
    """
    Perform conversion from RGB888 to BGR888
    Use input file from argument as image to convert
    :param out_file: output converted image file
    :param out_pix_fmt: pixel format of converted image
    """
    rgb = np.fromfile(args.input, dtype=np.uint8).reshape(args.height,args.width,3)

    bgr = np.flip(rgb, axis=2)

    if (out_pix_fmt == 'bgr888'):
        bgr.tofile(out_file, '')
    else:
        print("Error: pixel format:", out_pix_fmt, " is not supported for conversion from RGB888")
        sys.exit('Error: invalid pixel format')


# This file is used to store the output converted image
# and to be reused for checksum calculation and for display.
conv_file = "conv_file.raw"

# Image conversion (if required) + Display + Checksum calculation
if (args.img_op == 'swap_ch0_ch2'):
    invert_channel0_channel2()
    display_with_cv(args.output, output_pix_fmt, args.channels)
    calc_checksum(args.output, args.width*args.height, args.channels)
elif (args.img_op == 'conv_RGB5652RGB'):
    conv_from_RGB565ToRGB(conv_file, "rgb888")
    display_with_cv(conv_file, "rgb888", 3)
    calc_checksum(conv_file, args.width*args.height, 3)
elif (args.img_op == 'conv_RGB5652BGR'):
    conv_from_RGB565ToRGB(conv_file, "bgr888")
    display_with_cv(conv_file, "bgr888", 3)
    calc_checksum(conv_file, args.width*args.height, 3)
elif (args.img_op == 'conv_RGB2BGR'):
    conv_from_RGBoBGR(args.output, "bgr888")
    display_with_cv(args.output, "bgr888", 3)
    calc_checksum(args.output, args.width*args.height, 3)
elif (args.img_op == 'conv_JPEG2FMT'):
    conv_from_JPGToFORMAT(args.output, args.pix_fmt)
    display_with_cv(args.output, args.pix_fmt, args.channels)
    calc_checksum(args.output, args.width*args.height, args.channels)
else:
    display_with_cv(args.input, args.pix_fmt, args.channels)
    calc_checksum(args.input, args.width*args.height, args.channels)
