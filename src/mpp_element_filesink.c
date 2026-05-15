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

static int filesink_enqueue(_mpp_t *mpp)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = mpp->last_elem;
    _filesink_t *filesink = elem->dev.filesink;
    void *data = NULL;
    uint32_t size = 0;

    MPP_LOGD("++filesink_enqueue\r\n");

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
    ret = filesink->dev.ops->enqueue(&filesink->dev, data, size);
    if (ret == MPP_kStatus_HAL_FileSinkSuccess)
    {
        MPP_LOGD("Enqueued: size=%u, frame_id=%d\r\n", size, elem->io.in_buf[0]->frame_id);
        elem->io.in_buf[0]->status = MPP_BUFFER_EMPTY;
    }
    else
    {
        MPP_LOGE("filesink enqueue failed\r\n");
        elem->io.in_buf[0]->status = MPP_BUFFER_EMPTY;
        ret = MPP_ERROR;
    }

    MPP_LOGD("--filesink_enqueue\r\n");
    return ret;
}

int mpp_filesink_add(mpp_t mpp, mpp_filesink_params_t *params, mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;

    if (!mpp)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_OPENED)
        return MPP_ERROR;

    _elem_t *elem;
    ret = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    elem->type = MPP_TYPE_SINK;
    elem->sink_typ = MPP_SINK_FILE;

    /* create filesink object */
    _filesink_t *filesink = hal_malloc(sizeof(*filesink));
    if (!filesink)
        return MPP_MALLOC_ERROR;
    memset(filesink, 0, sizeof(_filesink_t));
    elem->dev.filesink = filesink;

    /* copy params */
    memcpy(&filesink->params, params, sizeof(*params));

    /* setup HAL filesink structure */
    ret = setup_filesink(&filesink->dev);
    if (ret != MPP_SUCCESS)
    {
        hal_free(filesink);
        return ret;
    }

    /* init HAL function */
    ret = filesink->dev.ops->init(&filesink->dev, &filesink->params, NULL);
    if (ret != MPP_SUCCESS)
        return ret;

    /* set enqueue function */
    elem->sink_enqueue = filesink_enqueue;

    /* pipeline has been closed */
    _mpp->status = MPP_CLOSED;

    /* set operating mode */
    elem->io.inplace = false;
    elem->io.nb_out_buf = 0;

    /* get input buffer from previous element */
    set_in_buff_from_prev_elem(elem);

    filesink->dev.ops->get_buf_desc(&filesink->dev, &elem->io.in_buf[0]->hw_req_cons, &elem->io.mem_policy);

    if (elem_h != NULL)
        *elem_h = (mpp_elem_handle_t)elem;

    return ret;
}