/*
 * Copyright 2020-2025 NXP.
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

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "mpp_debug.h"
#include "string.h"
#include "hal_utils.h"
#include "hal_os.h"

static inline int image_dequeue(_mpp_t *mpp)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = mpp->first_elem;
    _static_image_t *img = elem->dev.img;

    elem->io.out_buf[0]->status = MPP_BUFFER_WRITTING;

    /* source buffer is static image buffer */
    ret = img->elt.ops->dequeue(&img->elt, elem->io.out_buf[0]->hw, &elem->io.out_buf[0]->stripe_num);

    /* update buffer status */
    elem->io.out_buf[0]->status = MPP_BUFFER_READY;
    elem->io.out_buf[0]->frame_id++;
    return ret;
}

int mpp_static_img_add(mpp_t mpp, mpp_img_params_t *params, void *addr, mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;
    if (!mpp)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_CREATED)
        return MPP_ERROR;

    if ((params->format == MPP_PIXEL_JPEG) && (params->compressed_size == 0))
    {
        MPP_LOGE("compressed_size param cannot be 0 for MPP_PIXEL_JPEG format\n\r");
        return MPP_INVALID_PARAM;
    }

    _elem_t *elem;
    ret  = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    elem->type = MPP_TYPE_SOURCE;
    elem->src_typ = MPP_SRC_STATIC_IMAGE;

    /* create static image object */
    _static_image_t *img = hal_malloc(sizeof(*img));
    if (!img)
        return MPP_MALLOC_ERROR;
    memset(img, 0, sizeof(_static_image_t));
    elem->dev.img = img;

    /* copy params */
    memcpy(&img->params, params, sizeof(*params));

    /* setup HAL image structure */
    ret = setup_static_image_elt(&img->elt);
    if (ret != MPP_SUCCESS)
    {
        hal_free(img);
        return ret;
    }

    /* init HAL function */
    ret = img->elt.ops->init(&img->elt, &img->params, addr);
    if (ret != MPP_SUCCESS)
        return ret;

    /* set dequeue function */
    elem->src_dequeue = image_dequeue;

    /* pipeline has been opened */
    _mpp->status = MPP_OPENED;

    /* set operating mode */
    elem->io.inplace = false;
    elem->io.mem_policy = HAL_MEM_ALLOC_NONE;
    elem->io.nb_in_buf = 0;
    /* create output buffer parameters to be passed to next element */
    elem->io.nb_out_buf = 1;
    elem->io.out_buf[0] = hal_malloc(sizeof(buf_desc_t));
    if (elem->io.out_buf[0] == NULL)
    {
        MPP_LOGE("\nAllocation failed\n");
        hal_free(img);
        return MPP_MALLOC_ERROR;
    }
    /* set buffer descriptor */
    memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));
    elem->io.out_buf[0]->format = params->format;
    elem->io.out_buf[0]->width = params->width;
    elem->io.out_buf[0]->height = params->height;
    elem->io.out_buf[0]->compressed_size = params->compressed_size;
    /* init stripes */
    if (img->params.stripe)
        elem->io.out_buf[0]->stripe_num = 1;
    else
        elem->io.out_buf[0]->stripe_num = 0;

    /* set buffer requirements */
    elem->io.out_buf[0]->hw_req_prod.alignment = 0;
    elem->io.out_buf[0]->hw_req_prod.cacheable = false;
    elem->io.out_buf[0]->hw_req_prod.stride = 0;

    if (elem_h != NULL) {
        *elem_h = (mpp_elem_handle_t) elem;
    }

    return ret;
}

uint32_t mpp_static_image_update(_elem_t *elem, mpp_element_params_t *params)
{
    volatile uint32_t ret = MPP_SUCCESS;
    int height, img_size;
    mpp_img_params_t *img_params = &params->static_image.img_params;

    /* Compute input image size */
    int image_stride = img_params->width * get_bitpp(img_params->format) / 8;

    if (img_params->stripe)
        height = img_params->height / MPP_STRIPE_NUM;
    else
        height = img_params->height;

    if (img_params->compressed_size > 0)
        img_size = img_params->compressed_size;
    else
        img_size = height * image_stride;

    /* Check if new image fits in the allocated buffer */
    if (img_size > elem->io.out_buf[0]->hw->max_image_size) {
        MPP_LOGE("Input image size is bigger than the allocated output buffer size\n");
        MPP_LOGE("input image size = %d, allocated buffer size = %d\n",
                  img_size, elem->io.out_buf[0]->hw->max_image_size);
        MPP_LOGE("Image will not be changed\r\n");
        return MPP_ERROR;
    }

    /* Update element config params */
    elem->dev.img->elt.config.width = img_params->width;
    elem->dev.img->elt.config.height = img_params->height;
    elem->dev.img->elt.config.format = img_params->format;
    elem->dev.img->elt.config.stripe = img_params->stripe;
    elem->dev.img->elt.config.compressed_size = img_params->compressed_size;
    elem->dev.img->elt.buffer = params->static_image.img_buffer;

    /* Update output buffer params */
    elem->io.out_buf[0]->width = img_params->width;
    elem->io.out_buf[0]->height = img_params->height;
    elem->io.out_buf[0]->format = img_params->format;
    elem->io.out_buf[0]->compressed_size = img_params->compressed_size;
    /* set stripes */
    if (img_params->stripe)
        elem->io.out_buf[0]->stripe_num = 1;
    else
        elem->io.out_buf[0]->stripe_num = 0;

    /* Update image params */
    memcpy(&elem->dev.img->params, img_params, sizeof(mpp_img_params_t));

    return ret;
}
