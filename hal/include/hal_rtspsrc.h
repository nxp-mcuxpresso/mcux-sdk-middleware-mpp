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

#ifndef _HAL_RTSPSRC_H_
#define _HAL_RTSPSRC_H_

/**
 * @file hal_rtspsrc.h
 * @brief HAL RTSP Source API for receiving video streams over RTSP/RTP.
 *
 * This module provides an RTSP src abstraction for the MPP framework,
 * enabling receiving video data over the network using RTSP/RTP protocols.
 */

#include "mpp_api_types.h"
#include "mpp_api.h"
#include "hal_types.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define IPV4_STR_LEN     16
#define RTSP_URL_MAX_LEN 256

#define RTSPSRC_STOP_TIMEOUT_MS  10000
#define RTSPSRC_STOP_POLL_MS     10

typedef struct _rtspsrc rtspsrc_t;

/** @brief RTSP src return status. */
typedef enum _hal_rtspsrc_status
{
    MPP_kStatus_HAL_RtspSrcSuccess = 0,  /*!< Successfully */
    MPP_kStatus_HAL_RtspSrcError         /*!< Error occurs on HAL rtspsrc */
} hal_rtspsrc_status_t;

/*! \addtogroup HAL_OPERATIONS
 *  @{
 */

/** @brief Operations that need to be implemented by an RTSP source element */
typedef struct _rtspsrc_operator
{
    hal_rtspsrc_status_t (*init)(rtspsrc_t *dev, mpp_rtspsrc_params_t *config); /*!< initialize the RTSP source device */
    hal_rtspsrc_status_t (*deinit)(rtspsrc_t *dev); /*!< de-initialize the RTSP source device */
    hal_rtspsrc_status_t (*start)(const rtspsrc_t *dev); /*!< start streaming */
    hal_rtspsrc_status_t (*stop)(const rtspsrc_t *dev); /*!< stop streaming */
    hal_rtspsrc_status_t (*dequeue)(const rtspsrc_t *dev, void **data, uint32_t *size); /*!< dequeue a received buffer */
    hal_rtspsrc_status_t (*get_buf_desc)(const rtspsrc_t *dev, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy); /*!< get buffer descriptor and memory policy */
} rtspsrc_operator_t;

/** @} */

/*! \addtogroup HAL_TYPES
 *  @{
 */

/** @brief Configuration parameters kept inside the RTSP source device. */
typedef struct
{
    char rtsp_url[RTSP_URL_MAX_LEN];
} rtspsrc_config_t;

/** @brief Attributes of an RTSP source element. */
struct _rtspsrc
{
    const rtspsrc_operator_t *ops;         /*!< Operations */
    rtspsrc_config_t          config;      /*!< RTSP source config */
    uint8_t                  *buffer;      /*!< Frame reassembly buffer (HAL-allocated) */
    uint32_t                  buffer_size; /*!< Size of buffer in bytes */
};

/** @} */

int setup_rtspsrc(rtspsrc_t *dev);
int setup_rtspsrc_elt(rtspsrc_t *dev);

#endif /* _HAL_RTSPSRC_H_ */
