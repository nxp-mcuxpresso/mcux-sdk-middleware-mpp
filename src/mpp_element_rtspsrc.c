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

#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "mpp_heap.h"
#include "mpp_debug.h"
#include "string.h"
#include "hal_utils.h"
#include "hal_os.h"

static int rtspsrc_dequeue(_mpp_t *mpp)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = mpp->first_elem;
    _rtspsrc_t *rtspsrc = elem->dev.rtspsrc;
    void *data = NULL;
    uint32_t size = 0;

    MPP_LOGD("++rtspsrc_dequeue\r\n");

    if (elem->io.out_buf[0] == NULL)
    {
        MPP_LOGE("No output buffer available\r\n");
        return MPP_ERROR;
    }

    if (elem->io.out_buf[0]->status == MPP_BUFFER_READING)
    {
        MPP_LOGI("Warning: rtspsrc may overwrite buffer in use.\n");
    }
    elem->io.out_buf[0]->status = MPP_BUFFER_WRITTING;

    /* Call HAL dequeue - blocks until a complete RTP/JPEG frame is ready */
    ret = rtspsrc->dev.ops->dequeue(&rtspsrc->dev, &data, &size);
    if (ret == MPP_kStatus_HAL_RtspSrcSuccess)
    {
        if (data != NULL && size > 0)
        {
            elem->io.out_buf[0]->hw->addr    = (uint8_t *)data;
            elem->io.out_buf[0]->compressed_size = size;
            elem->io.out_buf[0]->status      = MPP_BUFFER_READY;
            elem->io.out_buf[0]->frame_id++;

            MPP_LOGD("rtspsrc_dequeue: size=%u, frame_id=%d\r\n",
                     size, elem->io.out_buf[0]->frame_id);
        }
        else
        {
            elem->io.out_buf[0]->status = MPP_BUFFER_EMPTY;
            ret = MPP_ERROR;
        }
    }
    else
    {
        MPP_LOGD("rtspsrc_dequeue: no frame yet\r\n");
        elem->io.out_buf[0]->status = MPP_BUFFER_EMPTY;
        ret = MPP_ERROR;
    }

    MPP_LOGD("--rtspsrc_dequeue\r\n");
    return ret;
}

int mpp_rtspsrc_add(mpp_t mpp, mpp_rtspsrc_params_t *params, mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;

    if (!mpp)
        return MPP_INVALID_PARAM;

    _mpp_t *_mpp = (_mpp_t *)mpp;

    if (_mpp->status != MPP_CREATED)
        return MPP_ERROR;

    if (params == NULL || params->rtsp_url == NULL)
    {
        MPP_LOGE("RTSP source parameters not provided\r\n");
        return MPP_INVALID_PARAM;
    }

    if (params->rtsp_buffer_size == 0)
    {
        MPP_LOGE("rtsp_buffer_size cannot be 0\r\n");
        return MPP_INVALID_PARAM;
    }

    _elem_t *elem;
    ret = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    elem->type     = MPP_TYPE_SOURCE;
    elem->src_typ  = MPP_SRC_RTSP;

    /* Create rtspsrc object */
    _rtspsrc_t *rtspsrc = hal_malloc(sizeof(*rtspsrc));
    if (!rtspsrc)
        return MPP_MALLOC_ERROR;
    memset(rtspsrc, 0, sizeof(_rtspsrc_t));
    elem->dev.rtspsrc = rtspsrc;

    /* Copy params */
    memcpy(&rtspsrc->params, params, sizeof(*params));

    /* Setup HAL rtspsrc structure */
    ret = setup_rtspsrc_elt(&rtspsrc->dev);
    if (ret != MPP_SUCCESS)
    {
        hal_free(rtspsrc);
        return ret;
    }

    ret = rtspsrc->dev.ops->init(&rtspsrc->dev, &rtspsrc->params);
    if (ret != MPP_SUCCESS)
    {
        hal_free(rtspsrc);
        return ret;
    }

    /* Set dequeue function */
    elem->src_dequeue = rtspsrc_dequeue;

    /* Set operating mode */
    elem->io.inplace   = false;
    elem->io.nb_in_buf = 0;

    /* Create output buffer parameters to be passed to the next element */
    elem->io.nb_out_buf = 1;
    elem->io.out_buf[0] = hal_malloc(sizeof(buf_desc_t));
    if (elem->io.out_buf[0] == NULL)
    {
        MPP_LOGE("Allocation failed\n");
        hal_free(rtspsrc);
        return MPP_MALLOC_ERROR;
    }
    memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));

    /* The compressed_size field tells the framework the maximum buffer size */
    elem->io.out_buf[0]->compressed_size = (int)params->rtsp_buffer_size;
    elem->io.out_buf[0]->width  = params->width;
    elem->io.out_buf[0]->height = params->height;
    elem->io.out_buf[0]->format = params->format;

    rtspsrc->dev.ops->get_buf_desc(&rtspsrc->dev,
                                   &elem->io.out_buf[0]->hw_req_prod,
                                   &elem->io.mem_policy);

    /* Pipeline has been opened */
    _mpp->status = MPP_OPENED;

    if (elem_h != NULL)
        *elem_h = (mpp_elem_handle_t)elem;

    return ret;
}
