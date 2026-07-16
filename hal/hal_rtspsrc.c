/*
 * Copyright 2026 NXP
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

#include <limits.h>
#include "hal.h"
#include "hal_utils.h"
#include "hal_os.h"
#include "hal_rtspsrc.h"
#include "mpp_config.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#ifdef HAL_ENABLE_RTSP_SRC
#include "rtsp_client/rtsp_client.h"

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netifapi.h"
#include "fsl_debug_console.h"

/*******************************************************************************
  * Variables
  ******************************************************************************/
rtsp_client_context_t s_rtsp_client_ctx = {0};

/*******************************************************************************
  * Code
  ******************************************************************************/

/**
  * @brief Initialize the RTSP source device.
  */
hal_rtspsrc_status_t HAL_RtspSrc_Init(rtspsrc_t *dev, mpp_rtspsrc_params_t *config)
{
    HAL_LOGD("++HAL_RtspSrc_Init\n");

    if (dev == NULL || config == NULL)
    {
        HAL_LOGE("Invalid parameters\n");
        return MPP_kStatus_HAL_RtspSrcError;
    }

    if (s_rtsp_client_ctx.client_running ||
        s_rtsp_client_ctx.session.handler != NULL ||
        s_rtsp_client_ctx.session.rtp_task_handler != NULL)
    {
        HAL_LOGE("HAL_RtspSrc_Init: session already active - call Stop first\n");
        return MPP_kStatus_HAL_RtspSrcError;
    }

    memset(&s_rtsp_client_ctx, 0, sizeof(s_rtsp_client_ctx));

    s_rtsp_client_ctx.session.sess_sock      = -1;
    s_rtsp_client_ctx.session.rtp_sock       = -1;
    s_rtsp_client_ctx.session.state          = RTSP_CLIENT_UNINIT;
    s_rtsp_client_ctx.client_task_handle     = NULL;
    s_rtsp_client_ctx.client_running         = false;

    strncpy(dev->config.rtsp_url, config->rtsp_url, RTSP_URL_MAX_LEN - 1);
    dev->config.rtsp_url[RTSP_URL_MAX_LEN - 1] = '\0';

    /* Allocate the ping-pong reassembly buffer internally.
     * The buffer is split into two equal halves: rtp_buf (RTP task fills)
     * and dec_buf (decoder reads). Halves are swapped on each Dequeue - no memcpy needed. */
    dev->buffer_size = (uint32_t)config->rtsp_buffer_size;
    dev->buffer      = (uint8_t *)hal_malloc(dev->buffer_size);
    if (dev->buffer == NULL)
    {
        HAL_LOGE("HAL_RtspSrc_Init: failed to allocate reassembly buffer (%u bytes)\n",
                 dev->buffer_size);
        return MPP_kStatus_HAL_RtspSrcError;
    }

    uint32_t half = dev->buffer_size / 2u;
    s_rtsp_client_ctx.session.rtp_buf      = dev->buffer;
    s_rtsp_client_ctx.session.rtp_buf_size = half;
    s_rtsp_client_ctx.session.dec_buf      = dev->buffer + half;
    s_rtsp_client_ctx.session.frame_pending = false;
    s_rtsp_client_ctx.session.frame_size    = 0;

    HAL_LOGD("--HAL_RtspSrc_Init\n");
    return MPP_kStatus_HAL_RtspSrcSuccess;
}

/**
  * @brief Deinitialize the RTSP source device.
  */
hal_rtspsrc_status_t HAL_RtspSrc_Deinit(rtspsrc_t *dev)
{
    HAL_LOGD("++HAL_RtspSrc_Deinit\n");

    if (dev == NULL)
    {
        HAL_LOGE("Device is NULL\n");
        return MPP_kStatus_HAL_RtspSrcError;
    }

    if (s_rtsp_client_ctx.session.frame_ready_sem != NULL)
    {
        vSemaphoreDelete(s_rtsp_client_ctx.session.frame_ready_sem);
        s_rtsp_client_ctx.session.frame_ready_sem = NULL;
    }
    if (s_rtsp_client_ctx.session.buf_mutex != NULL)
    {
        vSemaphoreDelete(s_rtsp_client_ctx.session.buf_mutex);
        s_rtsp_client_ctx.session.buf_mutex = NULL;
    }

    if (dev->buffer != NULL)
    {
        hal_free(dev->buffer);
        dev->buffer      = NULL;
        dev->buffer_size = 0u;
    }

    memset(&s_rtsp_client_ctx, 0, sizeof(s_rtsp_client_ctx));
    s_rtsp_client_ctx.session.sess_sock = -1;
    s_rtsp_client_ctx.session.rtp_sock  = -1;

    HAL_LOGD("--HAL_RtspSrc_Deinit\n");
    return MPP_kStatus_HAL_RtspSrcSuccess;
}

/**
  * @brief Start the RTSP source - creates the RTSP client task.
  */
hal_rtspsrc_status_t HAL_RtspSrc_Start(const rtspsrc_t *dev)
{
    HAL_LOGD("++HAL_RtspSrc_Start\n");

    if (dev == NULL)
    {
        HAL_LOGE("Invalid parameters\n");
        return MPP_kStatus_HAL_RtspSrcError;
    }

    s_rtsp_client_ctx.client_running = true;
    s_rtsp_client_ctx.session.dev    = (rtspsrc_t *)dev;

    BaseType_t ret = xTaskCreate(
            rtsp_client_task,
            "rtsp_client",
            RTSP_CLIENT_TASK_STACK_SIZE,
            (void *)&s_rtsp_client_ctx.session,
            RTSP_CLIENT_TASK_PRIORITY,
            &s_rtsp_client_ctx.session.handler);

    if (pdPASS != ret)
    {
        HAL_LOGE("Failed to create rtsp_client task\n");
        s_rtsp_client_ctx.client_running = false;
        return MPP_kStatus_HAL_RtspSrcError;
    }

    HAL_LOGD("--HAL_RtspSrc_Start\n");
    return MPP_kStatus_HAL_RtspSrcSuccess;
}

/**
  * @brief Stop the RTSP source.
  */
hal_rtspsrc_status_t HAL_RtspSrc_Stop(const rtspsrc_t *dev)
{
    HAL_LOGD("++HAL_RtspSrc_Stop\n");

    if (dev == NULL)
    {
        HAL_LOGE("Invalid parameters\n");
        return MPP_kStatus_HAL_RtspSrcError;
    }

    s_rtsp_client_ctx.client_running = false;

    /* Wait for both the RTSP client task and the RTP receive task to finish. */
    uint32_t stop_elapsed = 0;
    while (s_rtsp_client_ctx.session.handler != NULL ||
           s_rtsp_client_ctx.session.rtp_task_handler != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(RTSPSRC_STOP_POLL_MS));
        stop_elapsed += RTSPSRC_STOP_POLL_MS;

        if (stop_elapsed >= RTSPSRC_STOP_TIMEOUT_MS)
        {
            HAL_LOGE("HAL_RtspSrc_Stop: timeout waiting for tasks to stop\n");
            break;
        }
    }

    HAL_LOGD("--HAL_RtspSrc_Stop\n");
    return MPP_kStatus_HAL_RtspSrcSuccess;
}

/**
  * @brief Dequeue a completed frame from the RTP reassembly buffer.
  */
hal_rtspsrc_status_t HAL_RtspSrc_Dequeue(const rtspsrc_t *dev, void **data, uint32_t *size)
{
    HAL_LOGD("++HAL_RtspSrc_Dequeue\n");

    if (dev == NULL || data == NULL || size == NULL)
    {
        HAL_LOGE("Invalid parameters\n");
        return MPP_kStatus_HAL_RtspSrcError;
    }

    rtsp_client_session_t *session = &s_rtsp_client_ctx.session;

    if (session->frame_ready_sem == NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        *data = NULL;
        *size = 0;
        return MPP_kStatus_HAL_RtspSrcError;
    }

    if (xSemaphoreTake(session->frame_ready_sem, pdMS_TO_TICKS(2000)) != pdTRUE)
    {
        HAL_LOGD("HAL_RtspSrc_Dequeue: timeout waiting for frame\n");
        *data = NULL;
        *size = 0;
        return MPP_kStatus_HAL_RtspSrcError;
    }

    /* Ping-pong swap: give the filled half to the decoder, recycle the idle
     * half back to the RTP task for the next frame. */
    uint32_t fsize  = session->frame_size;
    uint8_t *filled = session->rtp_buf;
    uint8_t *idle   = session->dec_buf;

    xSemaphoreTake(session->buf_mutex, portMAX_DELAY);
    session->rtp_buf = idle;
    session->dec_buf = filled;
    session->frame_pending = false;
    xSemaphoreGive(session->buf_mutex);

    *data = filled;
    *size = fsize;

    HAL_LOGD("--HAL_RtspSrc_Dequeue (size=%u)\n", *size);
    return MPP_kStatus_HAL_RtspSrcSuccess;
}

/**
  * @brief Communicate buffer requirements to the MPP framework.
  */
hal_rtspsrc_status_t HAL_RtspSrc_Getbufdesc(const rtspsrc_t *dev,
                                              hw_buf_desc_t *out_buf,
                                              mpp_memory_policy_t *policy)
{
    HAL_LOGD("++HAL_RtspSrc_Getbufdesc(out_buf=[%p])\n", out_buf);

    if ((out_buf == NULL) || (policy == NULL))
    {
        HAL_LOGE("NULL pointer to buffer descriptor\n");
        return MPP_kStatus_HAL_RtspSrcError;
    }

    *policy             = HAL_MEM_ALLOC_OUTPUT;
    out_buf->alignment  = 0;
    out_buf->cacheable  = false;
    out_buf->stride     = 0;
    out_buf->addr       = NULL;

    HAL_LOGD("--HAL_RtspSrc_Getbufdesc\n");
    return MPP_kStatus_HAL_RtspSrcSuccess;
}

const static rtspsrc_operator_t rtspsrc_ops = {
    .init         = HAL_RtspSrc_Init,
    .deinit       = HAL_RtspSrc_Deinit,
    .start        = HAL_RtspSrc_Start,
    .stop         = HAL_RtspSrc_Stop,
    .dequeue      = HAL_RtspSrc_Dequeue,
    .get_buf_desc = HAL_RtspSrc_Getbufdesc,
};

int setup_rtspsrc(rtspsrc_t *dev)
{
    dev->ops = &rtspsrc_ops;
    return 0;
}

#else /* HAL_ENABLE_RTSP_SRC */
int setup_rtspsrc(rtspsrc_t *dev)
{
    HAL_LOGE("RTSP Source not enabled\n");
    return -1;
}
#endif /* HAL_ENABLE_RTSP_SRC */
