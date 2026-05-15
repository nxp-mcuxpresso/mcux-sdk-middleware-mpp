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

#ifndef _HAL_RTSPSINK_H_
#define _HAL_RTSPSINK_H_

/**
 * @file hal_rtspsink.h
 * @brief HAL RTSP Sink API for streaming video over RTSP/RTP.
 * 
 * This module provides an RTSP sink abstraction for the MPP framework,
 * enabling streaming of video data over network using RTSP/RTP protocols.
 */

#include "mpp_api_types.h"
#include "mpp_api.h"
#include "hal_types.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define IPV4_STR_LEN 16

typedef struct _rtspsink rtspsink_t;

/** @brief RTSP sink return status. */
typedef enum _hal_rtspsink_status
{
    MPP_kStatus_HAL_RtspSinkSuccess = 0,  /*!< Successfully */
    MPP_kStatus_HAL_RtspSinkError         /*!< Error occurs on HAL rtspsink */
} hal_rtspsink_status_t;

/*! \addtogroup HAL_OPERATIONS
 *  @{
 */

/** @brief Operation that needs to be implemented by an RTSP sink element */
typedef struct _rtspsink_operator
{
    hal_rtspsink_status_t (*init)(rtspsink_t *dev, mpp_rtspsink_params_t *config, void *param);
    hal_rtspsink_status_t (*deinit)(rtspsink_t *dev);
    hal_rtspsink_status_t (*start)(const rtspsink_t *dev);
    hal_rtspsink_status_t (*stop)(const rtspsink_t *dev);
    hal_rtspsink_status_t (*enqueue)(const rtspsink_t *dev, void *data, uint32_t size);
    hal_rtspsink_status_t (*get_buf_desc)(const rtspsink_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy);
} rtspsink_operator_t;

/** @} */

/*! \addtogroup HAL_TYPES
 *  @{
 */

/** @brief Structure that characterize the RTSP sink element. */
typedef struct
{
    uint16_t port;
    char ip_addr[IPV4_STR_LEN];
    uint32_t frame_width;
    uint32_t frame_height;
    rtp_payload_type_t payload_type;
    rtp_frame_type_t frame_type;
} rtspsink_config_t;

/** @brief Attributes of an RTSP sink element. */
struct _rtspsink
{
    const rtspsink_operator_t *ops;
    rtspsink_config_t config;
};

/** @} */

int setup_rtspsink(rtspsink_t *dev);

#endif /*_HAL_RTSPSINK_H_*/
