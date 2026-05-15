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
#include "hal_rtspsink.h"
#include "mpp_config.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#ifdef HAL_ENABLE_RTSP_SINK
#include "rtsp_server/rtsp_server.h"

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netifapi.h"

/*******************************************************************************
  * Definitions
  ******************************************************************************/

/*******************************************************************************
  * Variables
  ******************************************************************************/

rtsp_context_t s_rtsp_ctx = {.listen_fd = -1};

/*******************************************************************************
  * Code
  ******************************************************************************/

/**
  * @brief Initialize the RTSP sink device
  */
hal_rtspsink_status_t HAL_RtspSink_Init(rtspsink_t *dev, mpp_rtspsink_params_t *config, void *param)
{
    HAL_LOGD("++HAL_RtspSink_Init\n");

    if (dev == NULL || config == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_RtspSinkError;
    }

    /* Initialize all sessions */
    for (int ii = 0; ii < RTSP_MAX_SESSION_NUM; ii++)
    {
        s_rtsp_ctx.sessions[ii].sess_sock = -1;
        s_rtsp_ctx.sessions[ii].state = RTSP_UNINIT;
    }

    s_rtsp_ctx.server_task_handle = NULL;
    s_rtsp_ctx.server_running = false;
    s_rtsp_ctx.listen_fd = -1;

    s_rtsp_ctx.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_rtsp_ctx.listen_fd < 0)
    {
        HAL_LOGE("socket() failed\r\n");
        return MPP_kStatus_HAL_RtspSinkError;
    }

    int opt = 1;
    setsockopt(s_rtsp_ctx.listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config->port);
    memcpy(&addr.sin_addr.s_addr, config->ip_addr, sizeof(config->ip_addr));

    strncpy(dev->config.ip_addr, inet_ntoa(addr.sin_addr), INET_ADDRSTRLEN - 1);
    dev->config.ip_addr[INET_ADDRSTRLEN - 1] = '\0';
    dev->config.port = config->port;
    dev->config.frame_width = config->frame_width;
    dev->config.frame_height = config->frame_height;
    dev->config.payload_type = config->payload_type;
    dev->config.frame_type = config->frame_type;

    if (bind(s_rtsp_ctx.listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        HAL_LOGE("ERROR: bind() failed: errno=%d\r\n", errno);
        if (errno == EADDRINUSE)
        {
            HAL_LOGE("Port %d is already in use!\r\n", config->port);
        }
        return MPP_kStatus_HAL_RtspSinkError;
    }

    HAL_LOGD("--HAL_RtspSink_Init\n");
    return MPP_kStatus_HAL_RtspSinkSuccess;
}

/**
  * @brief Deinitialize the RTSP sink device
  */
hal_rtspsink_status_t HAL_RtspSink_Deinit(rtspsink_t *dev)
{
    HAL_LOGD("++HAL_RtspSink_Deinit\n");

    if (dev == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_RtspSinkError;
    }

    /* Close all active sessions */
    for (int ii = 0; ii < RTSP_MAX_SESSION_NUM; ii++)
    {
        if (s_rtsp_ctx.sessions[ii].sess_sock != -1)
        {
            safe_close_socket(&s_rtsp_ctx.sessions[ii].sess_sock);
        }
        s_rtsp_ctx.sessions[ii].video.is_active = false;
        s_rtsp_ctx.sessions[ii].video.sps_sent = false;
        s_rtsp_ctx.sessions[ii].video.pps_sent = false;
        s_rtsp_ctx.sessions[ii].state = RTSP_UNINIT;
    }

    /* Close the listening socket */
    safe_close_socket(&s_rtsp_ctx.listen_fd);

    memset(&s_rtsp_ctx, 0, sizeof(s_rtsp_ctx));
    s_rtsp_ctx.listen_fd = -1;

    HAL_LOGD("--HAL_RtspSink_Deinit\n");
    return MPP_kStatus_HAL_RtspSinkSuccess;
}

/**
  * @brief Start the RTSP server task
  */
hal_rtspsink_status_t HAL_RtspSink_Start(const rtspsink_t *dev)
{
    HAL_LOGD("++HAL_RtspSink_Start\n");

    if (dev == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_RtspSinkError;
    }

    BaseType_t ret;

    s_rtsp_ctx.server_running = true;

    ret = xTaskCreate(
            rtsp_server_task,
            "rtsp_server",
            RTSP_TASKS_STACK_SIZE/2,
            (void *)dev,
            RTSP_TASK_PRIORITY,
            &s_rtsp_ctx.server_task_handle);

    if (pdPASS != ret)
    {
        HAL_LOGE("Failed to create rtsp_server task");
        s_rtsp_ctx.server_running = false;
        return MPP_kStatus_HAL_RtspSinkError;
    }

    HAL_LOGD("--HAL_RtspSink_Start\n");

    return MPP_kStatus_HAL_RtspSinkSuccess;
}

/**
  * @brief Stop the RTSP task
  */
hal_rtspsink_status_t HAL_RtspSink_Stop(const rtspsink_t *dev)
{
    HAL_LOGD("++HAL_RtspSink_Stop\n");

    if (dev == NULL)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_RtspSinkError;
    }

    /* Signal server task to stop */
    s_rtsp_ctx.server_running = false;

    if (s_rtsp_ctx.server_task_handle != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));  /* 2 second timeout */
        
        /* If task is still running, delete it forcefully */
        if (eTaskGetState(s_rtsp_ctx.server_task_handle) != eDeleted)
        {
            vTaskDelete(s_rtsp_ctx.server_task_handle);
        }
        s_rtsp_ctx.server_task_handle = NULL;
    }

    /* Stop all active sessions */
    for (int ii = 0; ii < RTSP_MAX_SESSION_NUM; ii++)
    {
        s_rtsp_ctx.sessions[ii].video.is_active = false;
        s_rtsp_ctx.sessions[ii].video.sps_sent = false;
        s_rtsp_ctx.sessions[ii].video.pps_sent = false;
        
        s_rtsp_ctx.sessions[ii].state = RTSP_UNINIT;
        
        if (s_rtsp_ctx.sessions[ii].handler != NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            if (eTaskGetState(s_rtsp_ctx.sessions[ii].handler) != eDeleted)
            {
                vTaskDelete(s_rtsp_ctx.sessions[ii].handler);
            }
            s_rtsp_ctx.sessions[ii].handler = NULL;
        }
    }

    HAL_LOGD("--HAL_RtspSink_Stop\n");
    return MPP_kStatus_HAL_RtspSinkSuccess;
}

/**
  * @brief Communicate buffer requirements
  */
hal_rtspsink_status_t HAL_RtspSink_Getbufdesc(const rtspsink_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy)
{
    hal_rtspsink_status_t ret = MPP_kStatus_HAL_RtspSinkSuccess;
    HAL_LOGD("++HAL_RtspSink_Getbufdesc(in_buf=[%p])\r\n", in_buf);

    if ((in_buf == NULL) || (policy == NULL))
    {
        HAL_LOGE("NULL pointer to buffer descriptor\r\n");
        return MPP_kStatus_HAL_RtspSinkError;
    }

    /* set memory policy - user provides input buffer */
    *policy = HAL_MEM_ALLOC_NONE;
    in_buf->alignment = 0;
    in_buf->cacheable = false;
    in_buf->stride = 0;
    in_buf->addr = NULL;

    HAL_LOGD("--HAL_RtspSink_Getbufdesc\r\n");
    return ret;
}

/**
  * @brief Enqueue a frame for streaming
  */
hal_rtspsink_status_t HAL_RtspSink_Enqueue(const rtspsink_t *dev, void *data, uint32_t size)
{
    HAL_LOGD("++HAL_RtspSink_Enqueue (size=%u)\n", size);

    if (dev == NULL || data == NULL || size == 0)
    {
        HAL_LOGE("Invalid parameters\r\n");
        return MPP_kStatus_HAL_RtspSinkError;
    }

    /* Find active sessions and send frame to their video streams */
    for (int ii = 0; ii < RTSP_MAX_SESSION_NUM; ii++)
    {
        if (s_rtsp_ctx.sessions[ii].state == RTSP_PLAYING && 
            s_rtsp_ctx.sessions[ii].video.is_active)
        {
            media_stream_t *stream = &s_rtsp_ctx.sessions[ii].video;

            /* Queue the frame */
            rtp_task_pdu(data, size, stream);
        }
    }

    HAL_LOGD("--HAL_RtspSink_Enqueue\n");
    return MPP_kStatus_HAL_RtspSinkSuccess;
}

const static rtspsink_operator_t rtspsink_ops = {
    .init         = HAL_RtspSink_Init,
    .deinit       = HAL_RtspSink_Deinit,
    .start        = HAL_RtspSink_Start,
    .stop         = HAL_RtspSink_Stop,
    .enqueue      = HAL_RtspSink_Enqueue,
    .get_buf_desc = HAL_RtspSink_Getbufdesc,
};

int setup_rtspsink(rtspsink_t *dev)
{
    dev->ops = &rtspsink_ops;
    return 0;
}

#else /* HAL_ENABLE_RTSP_SINK */
int setup_rtspsink(rtspsink_t *dev)
{
    HAL_LOGE("RTSP Sink not enabled\r\n");
    return -1;
}
#endif /* HAL_ENABLE_RTSP_SINK */
