/*
 * Copyright 2020-2026 NXP
 * All rights reserved.
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

#include <hal_static_image.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "hal.h"
#include "hal_utils.h"

#define IMG_NB_STRIPE 16

hal_image_status_t HAL_Image_Init(static_image_t *elt, mpp_img_params_t *config, void *param)
{
    hal_image_status_t ret = MPP_kStatus_HAL_ImageSuccess;
    HAL_LOGD("++HAL_STATIC_Image_Init\n");

    elt->config.width = config->width;
    elt->config.height = config->height;
    elt->config.format = config->format;
    elt->config.stripe = config->stripe;
    elt->config.compressed_size = config->compressed_size;    /* TODO get updated image size from buf_desc_t */
    elt->buffer = param ;
    HAL_LOGD("--HAL_STATIC_Image_Init\n");
    return ret;
}

hal_image_status_t HAL_Image_Dequeue(static_image_t *elt, hw_buf_desc_t *out_buf, int *stripe_num)
{
    hal_image_status_t ret = MPP_kStatus_HAL_ImageSuccess;
    static_image_static_config_t config = elt->config;
    /* INT32-C: Prevent multiplication overflow */
    int bitpp = get_bitpp(config.format);
    assert(config.width <= INT_MAX / bitpp);
    int image_stride = (config.width * bitpp) / 8;
    int dest_stride = out_buf->stride;
    HAL_LOGD("++HAL_IMAGE_Dequeue\n");

    if(config.compressed_size > 0)
    {
        if (config.format != MPP_PIXEL_JPEG)
        {
            HAL_LOGE("HAL Static compressed image format not supported\n");
        }
        /* compressed image, copy whole data */
        /* TODO get updated image size from buf_desc_t */
        memcpy(out_buf->addr, elt->buffer, config.compressed_size);
    }
    else
    {   /* uncompressed image */
        if(config.stripe)
        {
            /* INT32-C: Validate stripe index to prevent overflow */
            assert(elt->stripe_idx >= 0 && elt->stripe_idx < IMG_NB_STRIPE);
            
            /* copy stripe line by line */
            int stripe_h = config.height / IMG_NB_STRIPE;
            
            /* INT32-C: Validate stripe height multiplication */
            assert(stripe_h <= INT_MAX / IMG_NB_STRIPE);
            
            for (int y = 0; y < stripe_h; y++) {
                /* INT32-C: Prevent multiplication overflow in offset calculations */
                assert(dest_stride <= INT_MAX / stripe_h);
                assert(image_stride <= INT_MAX / config.height);
                
                memcpy(out_buf->addr + dest_stride*y,
                       elt->buffer + image_stride*(stripe_h*elt->stripe_idx + y),
                       image_stride);
            }
            /* INT32-C: Pass stripe number [1 ; IMG_NB_STRIPE] - safe range */
            *stripe_num = elt->stripe_idx + 1;
            /* INT32-C: Define next stripe index - bounds checked above */
            elt->stripe_idx++;
            if (elt->stripe_idx >= IMG_NB_STRIPE) elt->stripe_idx = 0;
        }
        else
        {
            /* INT32-C: Validate loop bounds for whole image copy */
            assert(dest_stride <= INT_MAX / config.height);
            assert(image_stride <= INT_MAX / config.height);
            
            /* copy whole image line by line */
            for (int y = 0; y < config.height; y++) {
                memcpy(out_buf->addr + dest_stride*y,
                       elt->buffer + image_stride*y,
                       image_stride);
            }
        }
    }
    HAL_LOGD("--HAL_IMAGE_Dequeue\n");
    return ret;
}

const static static_image_operator_t static_image_ops = {
    .init        = HAL_Image_Init,
    .dequeue     = HAL_Image_Dequeue,
};

int setup_static_image(static_image_t *elt)
{
    elt->ops = &static_image_ops;
    return 0;
}

