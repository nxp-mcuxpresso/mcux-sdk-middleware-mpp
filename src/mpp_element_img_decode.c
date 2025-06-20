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

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "hal_os.h"
#include "string.h"

#include "mpp_debug.h"
#include "hal_vdec_dev.h"
#include "hal.h"

/* element processing function */
static int decode_func(_elem_t *elem)
{
    int ret;
    ret = elem->dev.vdec->ops->decode(  elem->dev.vdec,
                                        elem->io.in_buf[0]->hw->addr,
                                        elem->io.out_buf[0]->hw->addr,
                                        elem->io.in_buf[0]->compressed_size,
                                        elem->io.out_buf[0]->hw->stride);
    return ret;
}

/* element setup function */
unsigned int elem_img_decode_setup(_elem_t *elem)
{
    int ret = MPP_SUCCESS;
    vdec_dev_t *vdec = NULL;

    do {
        /* sanity checks */
        if (elem == NULL)
        {
            MPP_LOGE("invalid input buffer - elem (0x%x)\n", elem);
            ret = MPP_INVALID_PARAM;
            break;
        }
        if ((elem->type != MPP_TYPE_PROC) || (elem->proc_typ != MPP_ELEMENT_IMG_DECODE))
        {
            MPP_LOGE("invalid element %s (expected element IMG_DECODE)\n", elem_name(elem->proc_typ));
            ret = MPP_INVALID_PARAM;
            break;
        }
        /* setup the device */
        vdec = hal_malloc(sizeof(vdec_dev_t));
        if (!vdec)
        {
            MPP_LOGE("\nImage Decoder: element allocation failed\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        memset(vdec, 0, sizeof(vdec_dev_t));
        elem->dev.vdec = vdec;

        _mpp_t *mpp = elem->mpp;
        if (!mpp)
        {
            ret = MPP_INVALID_PARAM;
            break;
        }

        if (  (elem->params.decode.width != elem->prev->io.out_buf[0]->width)
           || (elem->params.decode.height != elem->prev->io.out_buf[0]->height) )
        {
            MPP_LOGE("invalid parameters for Image Decode\r\n");
            ret = MPP_INVALID_PARAM;
            break;
        }

        /* set operating mode */
        elem->io.inplace = false;
        /* input buffer points to previous element buffer */
        elem->io.nb_in_buf = 1;
        elem->io.in_buf[0] = elem->prev->io.out_buf[0];
        /* create output buffer parameters to be passed to next element */
        elem->io.nb_out_buf = 1;
        elem->io.out_buf[0] = hal_malloc(sizeof(buf_desc_t));
        if (elem->io.out_buf[0] == NULL)
        {
            MPP_LOGE("\nImage Decoder: buffer descriptors allocation failed\n");
            ret = MPP_MALLOC_ERROR;
            break;
        }
        /* set buffer descriptor */
        memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));
        elem->io.out_buf[0]->format = elem->params.decode.out_format;
        elem->io.out_buf[0]->width = elem->params.decode.width;
        elem->io.out_buf[0]->height = elem->params.decode.height;

        /* init stripes: none */
        elem->io.out_buf[0]->stripe_num = 0;

        vdec->callback  = mpp->params.evt_callback_f;
        vdec->user_data = mpp->params.cb_userdata;

        /* setup HAL vdec structure */
        ret  = hal_img_decoder_setup(elem->params.decode.dev_name, vdec);
        if (ret != MPP_SUCCESS)
        {
            MPP_LOGE ("Setup HAL image decode fails with ret=%d\n", ret);
            break;
        }
        if (vdec->ops == NULL)
        {
            MPP_LOGE ("Setup HAL image decode fails: vdec->ops is NULL \n");
            ret = MPP_ERROR;
            break;
        }

        /* init HAL function */
        if (vdec->ops->init != NULL)
            vdec->ops->init(vdec, &elem->params);
        else
        {
            MPP_LOGE ("Setup HAL image decode fails: vdec->ops->init is NULL \n");
            ret = MPP_ERROR;
        }

        /* retrieve buffer requirements from HAL */
        ret = vdec->ops->get_buf_desc(  vdec, 
                                        &elem->io.in_buf[0]->hw_req_cons, 
                                        &elem->io.out_buf[0]->hw_req_prod, 
                                        &elem->io.mem_policy);
        if (ret != MPP_SUCCESS) break;

        /* assign element entry/function */
        elem->entry = decode_func;

    } while (false);

    if (ret != MPP_SUCCESS)
    {
        if (elem != NULL && elem->io.out_buf[0] != NULL)
            hal_free(elem->io.out_buf[0]);
        if (vdec != NULL)
            hal_free(vdec);
    }

    return ret;
}

