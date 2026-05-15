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

static int rtspsink_enqueue(_mpp_t *mpp)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = mpp->last_elem;
    _rtspsink_t *rtspsink = elem->dev.rtspsink;
    void *data = NULL;
    uint32_t size = 0;

    MPP_LOGD("++rtspsink_enqueue\r\n");

    if (elem->io.in_buf[0] == NULL)
    {
        MPP_LOGE("No input buffer available\r\n");
        return MPP_ERROR;
    }

    if (elem->io.in_buf[0]->status != MPP_BUFFER_READY)
    {
        MPP_LOGD("Input buffer not ready (status=%d)\r\n", elem->io.in_buf[0]->status);
        return MPP_SUCCESS;
    }

    elem->io.in_buf[0]->status = MPP_BUFFER_READING;

    data = elem->io.in_buf[0]->hw->addr;
    size = elem->io.in_buf[0]->compressed_size;

    if (data == NULL || size == 0)
    {
        MPP_LOGE("Invalid buffer data or size\r\n");
        elem->io.in_buf[0]->status = MPP_BUFFER_EMPTY;
        return MPP_ERROR;
    }

    /* Call HAL enqueue */
    ret = rtspsink->dev.ops->enqueue(&rtspsink->dev, data, size);
    if (ret == MPP_kStatus_HAL_RtspSinkSuccess)
    {
        MPP_LOGD("Enqueued: size=%u, frame_id=%d\r\n", size, elem->io.in_buf[0]->frame_id);
        elem->io.in_buf[0]->status = MPP_BUFFER_EMPTY;
    }
    else
    {
        MPP_LOGE("rtspsink enqueue failed\r\n");
        elem->io.in_buf[0]->status = MPP_BUFFER_EMPTY;
        ret = MPP_ERROR;
    }

    MPP_LOGD("--rtspsink_enqueue\r\n");
    return ret;
}

int mpp_rtspsink_add(mpp_t mpp, mpp_rtspsink_params_t *params, mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;

    if (!mpp)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_OPENED)
        return MPP_ERROR;

    if (params == NULL)
    {
        MPP_LOGE("RTSP sink parameters not provided\r\n");
        return MPP_INVALID_PARAM;
    }

    _elem_t *elem;
    ret = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    elem->type = MPP_TYPE_SINK;
    elem->sink_typ = MPP_SINK_RTSP;

    /* create rtspsink object */
    _rtspsink_t *rtspsink = hal_malloc(sizeof(*rtspsink));
    if (!rtspsink)
        return MPP_MALLOC_ERROR;
    memset(rtspsink, 0, sizeof(_rtspsink_t));
    elem->dev.rtspsink = rtspsink;

    /* copy params */
    memcpy(&rtspsink->params, params, sizeof(*params));

    /* setup HAL rtspsink structure */
    ret = setup_rtspsink(&rtspsink->dev);
    if (ret != MPP_SUCCESS)
    {
        hal_free(rtspsink);
        return ret;
    }

    /* init HAL function */
    ret = rtspsink->dev.ops->init(&rtspsink->dev, &rtspsink->params, NULL);
    if (ret != MPP_SUCCESS)
    {
        hal_free(rtspsink);
        return ret;
    }

    /* set enqueue function */
    elem->sink_enqueue = rtspsink_enqueue;

    /* pipeline has been closed */
    _mpp->status = MPP_CLOSED;

    /* set operating mode */
    elem->io.inplace = false;
    elem->io.nb_out_buf = 0;

    /* get input buffer from previous element */
    set_in_buff_from_prev_elem(elem);

    /* get buffer descriptor requirements */
    rtspsink->dev.ops->get_buf_desc(&rtspsink->dev, &elem->io.in_buf[0]->hw_req_cons, &elem->io.mem_policy);

    if (elem_h != NULL)
        *elem_h = (mpp_elem_handle_t)elem;

    return ret;
}
