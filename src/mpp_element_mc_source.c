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

static inline _elem_t *get_mc_src_elem(_elem_t *elem)
{
    _elem_t *mc_elem_p = NULL;

    while (elem != NULL)
    {
        if (elem->prev != NULL && elem->prev->type == MPP_TYPE_SOURCE && elem->prev->src_typ == MPP_SRC_MC)
        {
            mc_elem_p = elem->prev;
            break;
        }
        elem = elem->prev;
    }

    return mc_elem_p;
}

static inline hal_mc_status_t mc_src_lock_device(_multicore_dev_t *mc)
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

static inline hal_mc_status_t mc_src_unlock_device(_multicore_dev_t *mc)
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

static inline int mc_src_enqueue(_elem_t *elem, void *buf)
{
    int ret = MPP_SUCCESS;
    _mpp_t *mpp = elem->mpp;
    buf_desc_t *buf_desc = (buf_desc_t *)buf;
    uint32_t buf_index;

    /* Find camera element. Ussualy it is elem->prev
     * But there are situations when in_place processing is active for an element and
     * elem->prev is not actually camera element */
    _elem_t *mc_elem =  get_mc_src_elem(elem);

    if (mc_elem == NULL)
    {
        MPP_LOGE("Could not find mc source element\r\n");
        return MPP_INVALID_ELEM;
    }

    _multicore_dev_t *mc = mc_elem->dev.mc;

    if (mc == NULL)
    {
        MPP_LOGE("MC device is NULL\r\n");
        return MPP_ERROR;
    }

    if (mc_src_lock_device(mc) != kStatus_HAL_MultiCoreSuccess)
        return MPP_ERROR;

    /* Get the buffer index in the list to associate it with the stream */
    buf_index = get_out_buff_index(mc_elem, buf_desc);

    if (buf_index >= MAX_OUTPUT_PORTS)
    {
        MPP_LOGE("Invalid output buffer index (%d)\r\n", buf_index);
        MPP_LOGE("Could not find buf desc %p in mc src element %p\r\n", buf_desc, mc_elem);
        (void) mc_src_unlock_device(mc);
        return MPP_INVALID_ELEM;
    }

    /* Update request count only for configured exec type of the pipeline
     * those parameters are computed by the pipeline execution itself 
     * and we need to increment the crt_req_cnt only on this condition */
    if (mpp->params.exec_flag == mc->dev.config.req_exec_type)
    {
        mc->dev.config.crt_req_cnt++;
        mc->dev.config.requested[buf_index] = true;
    }
    else
    {
        /* Do not make the acutal enqueue call to the device
         * Wait for the enqueue coming from the pipelines with same exec_flag as req_cnt_type */
        mc->dev.config.requested[buf_index] = true;
        MPP_LOGD("mc source buffer %d requested\n", buf_index);

        if (mc_src_unlock_device(mc) != kStatus_HAL_MultiCoreSuccess)
            return MPP_ERROR;

        return MPP_SUCCESS;
    }

    /* Check if we received the expected number of enqueue calls */
    if (mc->dev.config.crt_req_cnt >= mc->dev.config.min_req_cnt)
    {
        /* Check stream config status */
        uint32_t req_cnt = 0;
        for (int i = 0; i < mc_elem->io.nb_out_buf; i++)
        {
            if (mc->dev.config.requested[i] == true)
            {
                req_cnt++;
                /* In case the buffer is in reading state, log a warning that
                 * the buffer my be overwritten while still in use */
                if ((mc_elem->io.out_buf[i]->status == MPP_BUFFER_READING))
                {
                    MPP_LOGI("Warning: MC source could overwrite buffer in use\n");
                }
                /* Set the buffer status to writting as it will be written by the previous element
                 * of the MC sink pair */
                MPP_LOGD("MC source: enqueuing buffer %p, hw addr %p\n", mc_elem->io.out_buf[i]->hw, mc_elem->io.out_buf[i]->hw->addr);
                mc_elem->io.out_buf[i]->status = MPP_BUFFER_WRITTING;
                mc->dev.config.buff_desc[i] = (void *) mc_elem->io.out_buf[i]->hw;
                mc->dev.config.enqueued[i] = true;
            }
        }

        if (req_cnt > 0)
        {
            /* Call camera enqueue function */
            ret = mc->dev.ops->enqueue(&mc->dev, NULL, NULL, NULL, NULL);
        }
        else
        {
            MPP_LOGI("No streams requested, skipping mc source enqueue\r\n");
            ret = MPP_SUCCESS;
        }

        /* Clear requested flag */
        for (int i = 0; i < mc_elem->io.nb_out_buf; i++)
        {
            mc->dev.config.requested[i] = false;
        }

        mc->dev.config.crt_req_cnt = 0;
    }

    if (mc_src_unlock_device(mc) != kStatus_HAL_MultiCoreSuccess)
        return MPP_ERROR;

    return ret;
}

static inline int mc_src_dequeue(_mpp_t *mpp)
{
    int ret = MPP_SUCCESS;
    _elem_t *elem = mpp->first_elem;
    _multicore_dev_t *mc = elem->dev.mc;
    void *recv_addr[MAX_OUTPUT_PORTS] = {NULL};
    uint32_t recv_size[MAX_OUTPUT_PORTS] = {0};
    uint32_t stripe_num[MAX_OUTPUT_PORTS] = {0};
    uint32_t frame_id[MAX_OUTPUT_PORTS] = {0};

    if (mc == NULL)
    {
        MPP_LOGE("MC device is NULL\r\n");
        return MPP_ERROR;
    }

    if (mc_src_lock_device(mc) != kStatus_HAL_MultiCoreSuccess)
        return MPP_ERROR;

    /* Need to make first enqueue call here because we don't have the out_buf[i]->hw 
     * address available up until this point */
    if (mc->dev.config.initial_enqueue_done == false)
    {
        for (int i = 0; i < elem->io.nb_out_buf; i++)
        {
            elem->io.out_buf[i]->status = MPP_BUFFER_WRITTING;
            MPP_LOGD("MC source: enqueuing buffer %p, hw addr %p\n", elem->io.out_buf[i]->hw, elem->io.out_buf[i]->hw->addr);
            mc->dev.config.buff_desc[i] = (void *) elem->io.out_buf[i]->hw;
            mc->dev.config.requested[i] = true;
            mc->dev.config.enqueued[i] = true;
        }
        mc->dev.config.initial_enqueue_done = true;

        ret = mc->dev.ops->enqueue(&mc->dev, NULL, NULL, NULL, NULL);

        if (ret != kStatus_HAL_MultiCoreSuccess)
        {
            MPP_LOGE("MC initial enqueue failed\n");
            (void) mc_src_unlock_device(mc);
            return MPP_ERROR;
        }

        /* Clear requested flag */
        for (int i = 0; i < elem->io.nb_out_buf; i++)
        {
            mc->dev.config.requested[i] = false;
        }
    }

    for (int i = 0; i < elem->io.nb_out_buf; i++)
    {
        /* check buffer status 
         * The buffer should not be in reading state 
         * (this means the next element did not finish processing it yet) */
        if ((elem->io.out_buf[i]->status == MPP_BUFFER_READING))
        {
            MPP_LOGI("Warning: MC source could overwrite buffer in use\n");
        }
    }

    /* receive current buffer */
    ret = mc->dev.ops->dequeue(&mc->dev, recv_addr, recv_size, stripe_num, frame_id);

    if (ret != kStatus_HAL_MultiCoreSuccess)
    {
        MPP_LOGE("MC dequeue failed\n");
        (void) mc_src_unlock_device(mc);
        return MPP_ERROR;
    }

    for (int i = 0; i < elem->io.nb_out_buf; i++)
    {
        /* Write the dequeued info to the elem output buffer */
        if (recv_addr[i] != elem->io.out_buf[i]->hw)
        {
            MPP_LOGE("MC dequeue address mismatch at index %d\n", i);
            (void) mc_src_unlock_device(mc);
            return MPP_ERROR;
        }
        elem->io.out_buf[i]->compressed_size = recv_size[i];
        elem->io.out_buf[i]->stripe_num = stripe_num[i];
        elem->io.out_buf[i]->frame_id = frame_id[i];

        /* Mark buffer as ready for consumption by next element */
        elem->io.out_buf[i]->status = MPP_BUFFER_READY;

        mc->dev.config.enqueued[i] = false;
    }

    if (mc_src_unlock_device(mc) != kStatus_HAL_MultiCoreSuccess)
        return MPP_ERROR;

    return ret;
}

int mpp_mc_source_add(mpp_t mpp, mpp_mc_params_t *params, mpp_elem_handle_t *elem_h)
{
    int ret = MPP_SUCCESS;
    if (!mpp)
        return MPP_INVALID_PARAM;
    _mpp_t *_mpp = (_mpp_t *)mpp;
    if (_mpp->status != MPP_CREATED)
        return MPP_ERROR;

    _elem_t *elem;
    ret  = mpp_create_elem(_mpp, &elem);
    if (ret != MPP_SUCCESS)
        return ret;

    elem->type = MPP_TYPE_SOURCE;
    elem->src_typ = MPP_SRC_MC;

    _multicore_dev_t *mc = hal_malloc(sizeof(*mc));
    if (!mc)
        return MPP_MALLOC_ERROR;

    memset(mc, 0, sizeof(_multicore_dev_t));
    snprintf(mc->name, sizeof(mc->name), "MC_SOURCE_%d", (int) params->local_rpmsg_addr);
    elem->dev.mc = mc;

    /* copy params */
    memcpy(&mc->params, params, sizeof(*params));
    memcpy(&elem->params.mc_source, params, sizeof(*params));

    /* register/initialize mc dev with the framework */
    ret = hal_mc_dev_setup(mc->name, &mc->dev);
    if (ret != MPP_SUCCESS)
        return ret;

    /* set operating mode */
    elem->io.inplace = false;
    elem->io.nb_in_buf = 0;

    /* HAL init function */
    ret = mc->dev.ops->init(&mc->dev, &mc->params, kHAL_MultiCoreDevTypeSource);
    if (ret != MPP_SUCCESS)
        return ret;

    MPP_LOGD("Adding MC source %s\n", mc->name);

    /* get buffer requirements from HAL */
    /* we set previous policy to ALLOC_NONE because it is a source element */
    mc->dev.ops->get_buf_desc(&mc->dev, &elem->io, HAL_MEM_ALLOC_NONE);

    if (elem->io.nb_out_buf == 0)
    {
        MPP_LOGE("MC source number of output buffers is 0\r\n");
        return MPP_ERROR;
    }

    /* Set dev config parameters using the parameters received from MC sink pair */
    for (int i = 0; i < elem->io.nb_out_buf; i++)
    {
        mc->dev.config.buf_config[i].height = elem->io.out_buf[i]->height;
        mc->dev.config.buf_config[i].width = elem->io.out_buf[i]->width;
        mc->dev.config.buf_config[i].format = elem->io.out_buf[i]->format;
        if (elem->io.out_buf[i]->stripe_num)
        {
            MPP_LOGE("Stripe mode not supported for MC source\n");
            return MPP_INVALID_PARAM;
        }
        mc->dev.config.buf_config[i].stripe = false;
        mc->dev.config.buf_config[i].stripe_size = 0;
        mc->dev.config.buf_config[i].compressed_size = elem->io.out_buf[i]->compressed_size;
        mc->dev.config.buf_config[i].pitch = elem->io.out_buf[i]->width * get_bitpp(elem->io.out_buf[i]->format) / 8;

        elem->io.out_buf[i]->enqueue_cb = mc_src_enqueue;
    }

    /* set dequeue function */
    elem->src_dequeue = mc_src_dequeue;

    /* pipeline has been opened */
    _mpp->status = MPP_OPENED;

    if (elem_h != NULL) {
        *elem_h = (mpp_elem_handle_t) elem;
    }

    return ret;
}
