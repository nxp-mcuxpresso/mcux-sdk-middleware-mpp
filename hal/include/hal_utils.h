/*
 * Copyright 2022-2026 NXP.
 *
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef HAL_UTILS_H_
#define HAL_UTILS_H_

/*! \addtogroup HAL_TYPES
*  @{
*/

#include "hal_camera_dev.h"
#include "hal_display_dev.h"
#include "hal_static_image.h"
#include "hal_filesrc.h"
#include "hal_rtspsrc.h"
#include "hal_graphics_dev.h"
#include "hal_vdec_dev.h"

/** @brief Video decoder setup function pointer type */
typedef int (*img_decoder_setup_func_t) (vdec_dev_t *);

/** @brief Video decoder setup descriptor */
typedef struct
{
    const char* vdec_dev_name;                   /*!< name of the video decoder device */
    img_decoder_setup_func_t decoder_setup_func; /*!< pointer to the decoder setup function */
} hal_img_decoder_setup_t;

/** @brief Graphics setup function pointer type */
typedef int (*graphic_setup_func_t) (gfx_dev_t *);

/** @brief Graphics device setup descriptor */
typedef struct
{
    const char* gfx_dev_name;               /*!< name of the graphics device */
    graphic_setup_func_t gfx_setup_func;    /*!< pointer to the graphics setup function */
} hal_graphics_setup_t;

/** @brief Display setup function pointer type */
typedef int (*display_setup_func_t) (display_dev_t *);

/** @brief Display device setup descriptor */
typedef struct
{
    const char* display_name;                    /*!< name of the display device */
    display_setup_func_t display_setup_func;     /*!< pointer to the display setup function */
} hal_display_setup_t;

/** @brief Camera setup function pointer type */
typedef int (*camera_setup_func_t)(const char *, camera_dev_t *);

/** @brief Camera device setup descriptor */
typedef struct
{
    const char* camera_name;                 /*!< name of the camera device */
    camera_setup_func_t camera_setup_func;   /*!< pointer to the camera setup function */
} hal_camera_setup_t;

/*!
 * @brief Register the static image source element
 *
 * @param[in] elt static image element to register
 * @return error code (0: success, otherwise: failure)
 */
int setup_static_image_elt(static_image_t *elt);

/*!
 * @brief Register the file source element
 *
 * @param[in] elt file source element to register
 * @return error code (0: success, otherwise: failure)
 */
int setup_filesrc_elt(filesrc_t *elt);

/*!
 * @brief Register the RTSP source element
 *
 * @param[in] elt RTSP source element to register
 * @return error code (0: success, otherwise: failure)
 */
int setup_rtspsrc_elt(rtspsrc_t *elt);

/*!
 * @brief Compute a 32-bit checksum over a memory buffer
 *
 * @param[in] size_b buffer size in bytes
 * @param[in] pbuf   pointer to the buffer
 * @return 32-bit checksum value
 */
uint32_t calc_checksum(int size_b, void *pbuf);

/** checksum calculation method */
typedef enum
{
    CHECKSUM_TYPE_PISANO,     /*!< checksum computed using Pisano */
    CHECKSUM_TYPE_CRC_ELCDIF, /*!< checksum computed CRC from ELCDIF */
} checksum_type_t;

/** computed checksum */
typedef struct
{
    checksum_type_t type;     /*!< checksum calculation method */
    uint32_t value;           /*!< checksum value */
} checksum_data_t;

/** @} */

/*! \addtogroup HAL_OPERATIONS
*  @{
*/

/**
 * @brief Returns the number of bits per pixel for a given format.
 * @param[in] type pixel format
 * @return bits per pixel, or 0 for unknown format
 */
static inline int get_bitpp(mpp_pixel_format_t type)
{
    int ret;

    switch(type) {
    case MPP_PIXEL_ARGB:
    case MPP_PIXEL_BGRA:
    case MPP_PIXEL_RGBA:
    case MPP_PIXEL_BGRX:
    case MPP_PIXEL_RGBX:
    case MPP_PIXEL_GRAY888X:
    case MPP_PIXEL_YUV1P444:
        ret = 32;
        break;
    case MPP_PIXEL_RGB:
    case MPP_PIXEL_BGR:
    case MPP_PIXEL_GRAY888:
        ret = 24;
        break;
    case MPP_PIXEL_RGB565:
    case MPP_PIXEL_DEPTH16:
    case MPP_PIXEL_GRAY16:
    case MPP_PIXEL_UYVY1P422:
    case MPP_PIXEL_VYUY1P422:
    case MPP_PIXEL_YUYV1P422:
    case MPP_PIXEL_YUYV:
        ret = 16;
        break;
    case MPP_PIXEL_GRAY:
    case MPP_PIXEL_DEPTH8:
        ret = 8;
        break;
    case MPP_PIXEL_YUV420P:
        ret = 12;
        break;
    case MPP_PIXEL_INVALID:
    default:
        ret = 0;
        break;
    }
    return ret;
}

/** Swaps a buffer's MSB and LSB bytes
 *  @param data: pointer to the buffer to be converted(from little endian to big endian and vice-versa).
 *  @param size: buffer size.
 *   */
void swap_2_bytes(uint8_t *data, int size);

/** @} */

#endif /* HAL_UTILS_H_ */
