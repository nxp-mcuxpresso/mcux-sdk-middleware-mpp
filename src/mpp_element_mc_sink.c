/*
 * Copyright 2026 NXP
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "mpp_api_types.h"
#include "mpp_api.h"
#include "mpp_api_types_internal.h"
#include "hal.h"
#include "hal_mc.h"
#include "mpp_debug.h"
#include "hal_utils.h"

/* forward declaration */
_elem_t *get_mc_sink_elem(_elem_t *elem);

_elem_t *get_mc_sink_elem(_elem_t *elem)
{
    _elem_t *mc_elem_p = NULL;

    while (elem != NULL)
    {
        if (elem->type == MPP_TYPE_SINK && elem->sink_typ == MPP_SINK_MC)
        {
            mc_elem_p = elem;
            break;
        }

        /* Check all next elements (including branches) */
        int i;
        for (i = 0; i < MPP_MAX_BRANCH_NUM; i++)
        {
            if (elem->next[i] != NULL)
            {
                /* Recursively search this branch */
                mc_elem_p = get_mc_sink_elem(elem->next[i]);
                if (mc_elem_p != NULL)
                    return mc_elem_p;
            }
        }

        /* No more next elements to check */
        break;
    }

    return mc_elem_p;
}

static inline hal_mc_status_t mc_sink_lock_device(_multicore_dev_t *mc)
{
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    if (mc->dev.ops->lock != NULL)
    {
        ret = mc->dev.ops->lock(&mc->dev);
        if (ret != kStatus_HAL_MultiCoreSuccess)
        {
            MPP_LOGE("Failed to lock mc device\n");
            return ret;
        }
    }

    return ret;
}

static inline hal_mc_status_t mc_sink_unlock_device(_multicore_dev_t *mc)
{
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    if (mc->dev.ops->unlock != NULL)
    {
        ret = mc->dev.ops->unlock(&mc->dev);
        if (ret != kStatus_HAL_MultiCoreSuccess)
        {
            MPP_LOGE("Failed to unlock mc device\n");
            return ret;
        }
    }

    return ret;
}

static inline int mc_sink_dequeue(_elem_t *elem, void *buf)
{
    int ret = MPP_SUCCESS;
    void *send_addr[MAX_OUTPUT_PORTS] = {NULL};
    buf_desc_t *buf_desc = (buf_desc_t *)buf;
    uint32_t buf_index;

    /* Find camera element. Ussualy it is elem->prev
     * But there are situations when in_place processing is active for an element and
     * elem->prev is not actually camera element */
    _elem_t *mc_elem =  get_mc_sink_elem(elem);

    if (mc_elem == NULL)
    {
        MPP_LOGE("Could not find mc sink element\r\n");
        return MPP_INVALID_ELEM;
    }

    _multicore_dev_t *mc = mc_elem->dev.mc;

    if (mc == NULL)
    {
        MPP_LOGE("MC device is NULL\r\n");
        return MPP_ERROR;
    }

    if (mc_sink_lock_device(mc) != kStatus_HAL_MultiCoreSuccess)
        return MPP_ERROR;

    /* Get the buffer index in the list to associate it with the stream */
    buf_index = get_in_buff_index(mc_elem, buf_desc);

    if (buf_index >= MAX_INPUT_PORTS)
    {
        MPP_LOGE("Invalid output buffer index (%d)\r\n", buf_index);
        MPP_LOGE("Could not find buf desc %p in mc src element %p\r\n", buf_desc, mc_elem);
        (void) mc_sink_unlock_device(mc);
        return MPP_INVALID_ELEM;
    }

    mc->dev.config.crt_req_cnt++;

    /* Check if requests for all input buffers have been sent 
     * Since an element can have more than one input only if the previous element
     * has multiple outputs and all of them connected to the same mc sink device 
     * always wait for requests for all inputs before dequeuing */
    if (mc->dev.config.crt_req_cnt >= mc_elem->io.nb_in_buf)
    {
        /* Dequeue the buffer from the multicore device */
        ret = mc->dev.ops->dequeue(&mc->dev, send_addr, NULL, NULL, NULL);
        if (ret != kStatus_HAL_MultiCoreSuccess)
        {
            MPP_LOGE("Failed to dequeue from mc device\n");
            (void) mc_sink_unlock_device(mc);
            return MPP_ERROR;
        }
    }
    else
    {
        /* Wait for all requests */
        if (mc_sink_unlock_device(mc) != kStatus_HAL_MultiCoreSuccess)
            return MPP_ERROR;

        return ret;
    }

    for (int i = 0; i < mc_elem->io.nb_in_buf; i++)
    {
        if (send_addr[i] != NULL)
        {
            mc->dev.config.requested[i] = true;
            mc->dev.config.buff_desc[i] = send_addr[i];
            MPP_LOGD("MC sink: dequeued buffer %p, hw addr %p\n", send_addr[i], ((hw_buf_desc_t *)send_addr[i])->addr);
            if (mc_elem->io.in_buf[i]->hw == &mc_elem->io.in_buf[i]->hw_req_cons)
            {
                /* Our buffer is selected as hw buff desc, set it to the address received from source dev */
                mc_elem->io.in_buf[i]->hw = (hw_buf_desc_t *) send_addr[i];
            }
            else
            {
                MPP_LOGD("Local buffer: %p, hw addr %p\n", mc_elem->io.in_buf[i]->hw, mc_elem->io.in_buf[i]->hw->addr);
            }
            /* Set the buffer status to indicate it's ready for writting new data to it */
            mc_elem->io.in_buf[i]->status = MPP_BUFFER_EMPTY;
        }
        else
        {
            mc->dev.config.requested[i] = false;
            mc->dev.config.buff_desc[i] = NULL;
            if (mc_elem->io.in_buf[i]->hw == NULL || (mc_elem->io.in_buf[i]->hw->addr == NULL))
            {
                MPP_LOGE("Invalid buffer descriptor for mc element input\r\n");
                (void) mc_sink_unlock_device(mc);
                return MPP_ERROR;
            }
        }
    }

    mc->dev.config.crt_req_cnt = 0;

    if (mc_sink_unlock_device(mc) != kStatus_HAL_MultiCoreSuccess)
        return MPP_ERROR;

    return ret;
}

static inline int mc_sink_enqueue(_mpp_t *mpp)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = mpp->last_elem;
    _multicore_dev_t *mc = elem->dev.mc;
    void *send_addr[MAX_OUTPUT_PORTS] = {NULL};
    uint32_t send_size[MAX_OUTPUT_PORTS] = {0};
    uint32_t stripe_num[MAX_OUTPUT_PORTS] = {0};
    uint32_t frame_id[MAX_OUTPUT_PORTS] = {0};

    if (mc == NULL)
    {
        MPP_LOGE("MC device is NULL\r\n");
        return MPP_ERROR;
    }

    if (mc_sink_lock_device(mc) != kStatus_HAL_MultiCoreSuccess)
        return MPP_ERROR;

    for (int i = 0; i < elem->io.nb_in_buf; i++)
    {
        if (mc->dev.config.requested[i] == false)
        {
            send_addr[i] = NULL;
            MPP_LOGI("Warning: MC sink input buffer %d was not requested\n", i);
            continue;
        }
        /* check if the previous element has finished writting data to buffer */
        if (elem->io.in_buf[i]->status != MPP_BUFFER_READY)
        {
            MPP_LOGI("Warning: MC sink might send an incomplete frame \n");
        }
        /* Set the buffer status to indicate it's in processing state */
        elem->io.in_buf[i]->status = MPP_BUFFER_READING;

        /* Write the info to be enqueued */
        send_addr[i] = elem->io.in_buf[i]->hw;
        send_size[i] = elem->io.in_buf[i]->compressed_size;
        stripe_num[i] = elem->io.in_buf[i]->stripe_num;
        frame_id[i] = elem->io.in_buf[i]->frame_id;
    }

    /* send current buffer */
    ret = mc->dev.ops->enqueue(&mc->dev, send_addr, send_size, stripe_num, frame_id);

    if (ret != kStatus_HAL_MultiCoreSuccess)
    {
        MPP_LOGE("MC enqueue failed\n");
        (void) mc_sink_unlock_device(mc);
        return MPP_ERROR;
    }

    for (int i = 0; i < elem->io.nb_in_buf; i++)
    {
        mc->dev.config.requested[i] = false;
        mc->dev.config.buff_desc[i] = NULL;
    }

    if (mc_sink_unlock_device(mc) != kStatus_HAL_MultiCoreSuccess)
        return MPP_ERROR;

    return ret;
}

int mpp_mc_sink_add(mpp_t mpp, mpp_mc_params_t *params)
{
    int ret = MPP_SUCCESS;
    if (!mpp)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_OPENED)
        return MPP_ERROR;

    _elem_t *elem;
    ret  = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    elem->type = MPP_TYPE_SINK;
    elem->sink_typ = MPP_SINK_MC;

    _multicore_dev_t *mc = hal_malloc(sizeof(*mc));
    if (!mc)
        return MPP_MALLOC_ERROR;

    memset(mc, 0, sizeof(_multicore_dev_t));
    snprintf(mc->name, sizeof(mc->name), "MC_SINK_%d", (int) params->local_rpmsg_addr);
    elem->dev.mc = mc;

    /* fill in display parameters */
    memcpy(&mc->params, params, sizeof(*params));

    /* register/initialize mc dev with the framework */
    ret = hal_mc_dev_setup(mc->name, &mc->dev);
    if (ret != MPP_SUCCESS)
        return ret;

    /* set operating mode */
    elem->io.inplace = false;
    elem->io.nb_in_buf = 0;
    elem->io.nb_out_buf = 0;

    /* set input parameters from previous element */
    set_in_buff_from_prev_elem(elem);
    if (elem->io.nb_in_buf == 0) {
        MPP_LOGE("No input buffers found from previous element\n");
        return MPP_ERROR;
    }

    /* Set dev config parameters using the parameters from input buffers */
    for (int i = 0; i < elem->io.nb_in_buf; i++)
    {
        mc->dev.config.buf_config[i].height = elem->io.in_buf[i]->height;
        mc->dev.config.buf_config[i].width = elem->io.in_buf[i]->width;
        mc->dev.config.buf_config[i].format = elem->io.in_buf[i]->format;
        if (elem->io.in_buf[i]->stripe_num)
        {
            MPP_LOGE("Stripe mode not supported for MC sink\n");
            return MPP_INVALID_PARAM;
        }
        mc->dev.config.buf_config[i].stripe = false;
        mc->dev.config.buf_config[i].stripe_size = 0;
        mc->dev.config.buf_config[i].compressed_size = elem->io.in_buf[i]->compressed_size;
        mc->dev.config.buf_config[i].pitch = elem->io.in_buf[i]->width * get_bitpp(elem->io.in_buf[i]->format) / 8;

        elem->io.in_buf[i]->dequeue_cb = mc_sink_dequeue;
    }

    /* HAL init function */
    ret = mc->dev.ops->init(&mc->dev, &mc->params, kHAL_MultiCoreDevTypeSink);
    if (ret != MPP_SUCCESS)
        return ret;

    if (elem->prev == NULL)
    {
        MPP_LOGE("MC sink element must have a previous element\r\n");
        return MPP_ERROR;
    }

    /* get buffer requirements from HAL */
    mc->dev.ops->get_buf_desc(&mc->dev, &elem->io, elem->prev->io.mem_policy);

    elem->sink_enqueue = mc_sink_enqueue;

    /* pipeline has been closed */
    _mpp->status = MPP_CLOSED;

    return ret;
}
