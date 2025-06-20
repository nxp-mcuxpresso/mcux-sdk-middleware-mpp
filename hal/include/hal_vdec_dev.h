/*
 * Copyright 2025 NXP.
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

/** @} */

/*! \addtogroup HAL_OPERATIONS
*  @{
*/

/** @brief Operation that needs to be implemented by vdec device */

typedef struct
{
    /* initialize the dev */
    int (*init)(vdec_dev_t *dev, void *param);
    /* deinitialize the dev */
    int (*deinit)(const vdec_dev_t *dev);
    /* get buffer descriptors and policy */
    int (*get_buf_desc)(const vdec_dev_t *dev,
            hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy);
    /* blit the source surface to the destination surface */
    int (*decode)(
        const vdec_dev_t *dev, uint8_t *pSrc, uint8_t *pDst, int32_t jpg_size, uint32_t row_stride);
} vdec_dev_operator_t;


/*! @brief The mpp callback function prototype */
typedef int (*mpp_callback_t)(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data);

/** @} */

/*! \addtogroup HAL_TYPES
 *  @{
 */

struct _vdec_dev
{
    /* unique id */
    int id;
    /* operations */
    const vdec_dev_operator_t *ops;
    /* callback */
    mpp_callback_t callback;
    /* param for the callback */
    void *user_data;
};

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

/** @} */

#endif /* _HAL_VDEC_DEV_H_ */
