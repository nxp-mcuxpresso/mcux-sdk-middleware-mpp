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

static int filesrc_dequeue(_mpp_t *mpp)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = mpp->first_elem;
    _filesrc_t *filesrc = elem->dev.filesrc;
    void *data = NULL;
    uint32_t size = 0;

    MPP_LOGD("++filesrc_dequeue\r\n");

    if (elem->io.out_buf[0] == NULL)
    {
        MPP_LOGE("No output buffer available\r\n");
        return MPP_ERROR;
    }

    if (elem->io.out_buf[0]->status == MPP_BUFFER_READING)
    {
        MPP_LOGI("Warning: filesrc may overwrite buffer in use.\n");
    }
    elem->io.out_buf[0]->status = MPP_BUFFER_WRITTING;

    /* Call HAL dequeue */
    ret = filesrc->dev.ops->dequeue(&filesrc->dev, &data, &size);
    if (ret == MPP_kStatus_HAL_FileSrcSuccess)
    {
        if (data != NULL && size > 0)
        {
            elem->io.out_buf[0]->hw->addr = (uint8_t *)data;
            elem->io.out_buf[0]->compressed_size = size;
            elem->io.out_buf[0]->status = MPP_BUFFER_READY;
            elem->io.out_buf[0]->frame_id++;

            MPP_LOGD("Dequeued: size=%u, frame_id=%d\r\n", size, elem->io.out_buf[0]->frame_id);
        }
        else
        {
            elem->io.out_buf[0]->status = MPP_BUFFER_EMPTY;
            ret = MPP_ERROR; /* Signal to stop pipeline */
        }
    }
    else
    {
        MPP_LOGE("filesrc dequeue failed\r\n");
        elem->io.out_buf[0]->status = MPP_BUFFER_EMPTY;
        ret = MPP_ERROR;
    }

    MPP_LOGD("--filesrc_dequeue\r\n");
    return ret;
}

int mpp_filesrc_add(mpp_t mpp, mpp_filesrc_params_t *params, void *addr, mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;

    if (!mpp)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_CREATED)
        return MPP_ERROR;

    if (params->file_buffer_size == 0)
    {
        MPP_LOGE("file_buffer_size param cannot be 0\n\r");
        return MPP_INVALID_PARAM;
    }

    _elem_t *elem;
    ret  = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    elem->type = MPP_TYPE_SOURCE;
    elem->sink_typ = MPP_SRC_FILE;

    /* create static image object */
    _filesrc_t *filesrc = hal_malloc(sizeof(*filesrc));
    if (!filesrc)
        return MPP_MALLOC_ERROR;
    memset(filesrc, 0, sizeof(_filesrc_t));
    elem->dev.filesrc = filesrc;

    /* copy params */
    memcpy(&filesrc->params, params, sizeof(*params));

    /* setup HAL image structure */
    ret = setup_filesrc_elt(&filesrc->dev);
    if (ret != MPP_SUCCESS)
    {
        hal_free(filesrc);
        return ret;
    }

    /* init HAL function */
    ret = filesrc->dev.ops->init(&filesrc->dev, &filesrc->params, addr);
    if (ret != MPP_SUCCESS)
        return ret;

    /* set dequeue function */
    elem->src_dequeue = filesrc_dequeue;

    /* pipeline has been opened */
    _mpp->status = MPP_OPENED;

    /* set operating mode */
    elem->io.inplace = false;
    elem->io.nb_in_buf = 0;
    /* create output buffer parameters to be passed to next element */
    elem->io.nb_out_buf = 1;
    elem->io.out_buf[0] = hal_malloc(sizeof(buf_desc_t));
    if (elem->io.out_buf[0] == NULL)
    {
        MPP_LOGE("\nAllocation failed\n");
        return MPP_MALLOC_ERROR;
    }
    /* set buffer descriptor */
    memset(elem->io.out_buf[0], 0, sizeof(buf_desc_t));
    elem->io.out_buf[0]->compressed_size = params->file_buffer_size;

    filesrc->dev.ops->get_buf_desc(&filesrc->dev, &elem->io.out_buf[0]->hw_req_prod, &elem->io.mem_policy);

    return ret;
}