/*
 * Copyright 2025-2026 NXP.
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

/*! \addtogroup HAL_TYPES
 *  @{
 */

/**
 * @brief hal video decoder (vdec) device declaration.
 * Video decoder devices can be used to perform decompression of image.
 * Examples of decoder devices include the PNG/JPEG HW or SW found on many i.MXRT series MCUs.
 */

#ifndef _HAL_VDEC_DEV_H_
#define _HAL_VDEC_DEV_H_

#include "mpp_api_types.h"
#include "hal_types.h"

/** Name of the jpeg decoder device using CPU operations **/
#define HAL_VDEC_DEV_NAME "jpeg_CPU"

typedef struct _vdec_dev vdec_dev_t;
typedef struct _vdec_h264_dev vdec_h264_dev_t;

/** video decoder return status*/
typedef enum _hal_vdec_status
{
    MPP_kStatus_HAL_VDecSuccess = 0,  /*!< Successfully */
    MPP_kStatus_HAL_VDecSkipped,      /*!< Skip */
	MPP_kStatus_HAL_VDecError         /*!< Error occurs on HAL Video Decoder */

} hal_vdec_status_t;

/** @brief H.264 decode frame information structure */
typedef struct _vdec_h264_frame_info
{
    uint8_t *src_data;   /*!< pointer to input compressed H.264 bitstream */
    int32_t src_size;    /*!< size of input bitstream in bytes */
    uint8_t *dst_y;      /*!< pointer to output luma (Y) plane */
    uint8_t *dst_u;      /*!< pointer to output chroma U plane */
    uint8_t *dst_v;      /*!< pointer to output chroma V plane */
    int32_t width;       /*!< decoded frame width in pixels */
    int32_t height;      /*!< decoded frame height in pixels */
    int32_t y_stride;    /*!< luma plane stride in bytes */
    int32_t uv_stride;   /*!< chroma plane stride in bytes */
} vdec_h264_frame_info_t;

/** @} */

/*! \addtogroup HAL_OPERATIONS
*  @{
*/

/** @brief Operation that needs to be implemented by vdec device */

/** @brief Operation that needs to be implemented by a JPEG/image decoder device */
typedef struct
{
    int (*init)(vdec_dev_t *dev, void *param);                               /*!< initialize the dev */
    int (*deinit)(const vdec_dev_t *dev);                                    /*!< deinitialize the dev */
    int (*get_buf_desc)(const vdec_dev_t *dev,
            hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy); /*!< get buffer descriptors and policy */
    int (*decode)(
        const vdec_dev_t *dev, uint8_t *pSrc, uint8_t *pDst, int32_t jpg_size, uint32_t row_stride); /*!< decode image data */
} vdec_dev_operator_t;

/** @brief Operation that needs to be implemented by an H.264 decoder device */
typedef struct
{
    hal_vdec_status_t (*init)(vdec_h264_dev_t *dev, void *param);            /*!< initialize the dev */
    hal_vdec_status_t (*deinit)(const vdec_h264_dev_t *dev);                 /*!< deinitialize the dev */
    hal_vdec_status_t (*get_buf_desc)(const vdec_h264_dev_t *dev,
            hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy); /*!< get buffer descriptors and policy */
    hal_vdec_status_t (*decode)(const vdec_h264_dev_t *dev, vdec_h264_frame_info_t *frame_info); /*!< decode H.264 frame */
} vdec_h264_dev_operator_t;

/*! @brief The mpp callback function prototype */
typedef int (*mpp_callback_t)(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data);

/** @} */

/*! \addtogroup HAL_TYPES
 *  @{
 */

/** @brief Attributes of a JPEG/image decoder device */
struct _vdec_dev
{
    int id;                             /*!< unique id */
    const vdec_dev_operator_t *ops;     /*!< operations */
    mpp_callback_t callback;            /*!< callback */
    void *user_data;                    /*!< param for the callback */
};

/** @brief Attributes of an H.264 decoder device */
struct _vdec_h264_dev
{
    int id;                                  /*!< unique id */
    const vdec_h264_dev_operator_t *ops;     /*!< operations */
    mpp_callback_t callback;                 /*!< callback */
    void *user_data;                         /*!< param for the callback */
};

/*!
 * @brief Print JPEG decoding validation statistics to the debug console
 */
void HAL_JPEG_PrintValidationStats(void);

/*!
 * @brief Register the jpeg SW decoder device
 *
 * @param[in] dev decoder device to register
 * @return error code (0: success, otherwise: failure)
 *
 */
int HAL_JPEG_CPU_Register(vdec_dev_t *dev);

/*!
 * @brief Register the jpeg HW decoder device
 *
 * @param[in] dev decoder device to register
 * @return error code (0: success, otherwise: failure)
 *
 */
int HAL_JPEG_HW_Register(vdec_dev_t *dev);

/*!
 * @brief Register the H.264 SW decoder device
 *
 * @param[in] dev decoder device to register
 * @return error code (0: success, otherwise: failure)
 *
 */
hal_vdec_status_t HAL_VdecDev_H264_CPU_Register(vdec_h264_dev_t *dev);

/** @} */

#endif /* _HAL_VDEC_DEV_H_ */
