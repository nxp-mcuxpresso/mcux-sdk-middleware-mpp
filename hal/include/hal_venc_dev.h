/*
 * Copyright 2026 NXP.
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
 * @brief hal video encoder (venc) device declaration.
 * Video encoder devices can be used to perform compressing of image / video data.
 * Examples of decoder devices include the H.264 SW encoder present in SDK.
 */

#ifndef _HAL_VENC_DEV_H_
#define _HAL_VENC_DEV_H_

#include "mpp_api_types.h"
#include "hal_types.h"

/** Name of the H.264 encoder device using CPU operations **/
#define HAL_VENC_DEV_NAME "h264_CPU"

typedef struct _venc_h264_dev venc_h264_dev_t;

/** video encoder return status **/
typedef enum _hal_venc_status
{
    MPP_kStatus_HAL_VEncSuccess = 0,  /*!< Successfully */
	MPP_kStatus_HAL_VEncError         /*!< Error occurs on HAL Video Encoder */
} hal_venc_status_t;

/** @brief H.264 encode frame information structure */
typedef struct _venc_h264_frame_info
{
    /* Input parameters */
    mpp_pixel_format_t format;
    uint8_t *src_y;
    uint8_t *src_u;
    uint8_t *src_v;
    int32_t y_stride;
    int32_t uv_stride;
    int32_t width;
    int32_t height;
    
    /* Output parameters */
    uint8_t *dst;
    int32_t dst_size;

    /* Encoding parameters */
    bool intraframe;
} venc_h264_frame_info_t;

/** @} */

/*! \addtogroup HAL_OPERATIONS
*  @{
*/

/** @brief Operation that needs to be implemented by venc device */

/** @brief Operation that needs to be implemented by vdec device */

typedef struct
{
    /* initialize the dev */
    hal_venc_status_t (*init)(venc_h264_dev_t *dev, void *param);
    /* deinitialize the dev */
    hal_venc_status_t (*deinit)(const venc_h264_dev_t *dev);
    /* get buffer descriptors and policy */
    hal_venc_status_t (*get_buf_desc)(const venc_h264_dev_t *dev,
            hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy);
    /* encode the incoming data */
    hal_venc_status_t (*encode)(const venc_h264_dev_t *dev, venc_h264_frame_info_t *frame_info);
} venc_h264_dev_operator_t;

/*! @brief The mpp callback function prototype */
typedef int (*mpp_callback_t)(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data);

/** @} */

/*! \addtogroup HAL_TYPES
 *  @{
 */

struct _venc_h264_dev
{
    /* unique id */
    int id;
    /* operations */
    const venc_h264_dev_operator_t *ops;
    /* callback */
    mpp_callback_t callback;
    /* param for the callback */
    void *user_data;
};

/*!
 * @brief Register the H.264 SW encoder device
 *
 * @param[in] dev encoder device to register
 * @return error code (0: success, otherwise: failure)
 *
 */
hal_venc_status_t HAL_VencDev_H264_CPU_Register(venc_h264_dev_t *dev);

/** @} */

#endif /* _HAL_VENC_DEV_H_ */
