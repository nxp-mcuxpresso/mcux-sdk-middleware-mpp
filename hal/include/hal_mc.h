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

/**
 * @defgroup HAL_TYPES HAL Types
 *
 * This section provides the detailed documentation for the MPP HAL types
 *
 * @{
 */

/**
 * @brief hal mc device declaration. This multicore hardware abstraction layer provides 
 * interfaces for multicore communication between elements of pipeline running on different
 * core (multicore sink and multicore source elements).
 */

#ifndef _HAL_MC_H_
#define _HAL_MC_H_

#include "hal_types.h"
#include "mpp_api_types.h"
#include "mpp_config.h"

#if (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)
#include "mcmgr.h"
#endif /* (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1) */

typedef struct _multicore_dev multicore_dev_t;

/** multicore return status*/
typedef enum _hal_mc_status
{
    kStatus_HAL_MultiCoreSuccess = 0,  /*!< HAL MC successful */
    kStatus_HAL_MultiCoreError,        /*!< Error occurs on HAL MC */
    kStatus_HAL_MultiCoreNoDataReq     /*!< No data request available */
} hal_mc_status_t;

typedef enum _hal_mc_dev_type
{
    kHAL_MultiCoreDevTypeSink = 0,   /*!< Multicore sink device */
    kHAL_MultiCoreDevTypeSource      /*!< Multicore source device */
} hal_mc_dev_type_t;

/** @brief Operation that needs to be implemented by a mc device */
typedef struct _mc_dev_operator
{
    hal_mc_status_t (*init)(multicore_dev_t *dev, mpp_mc_params_t *config, hal_mc_dev_type_t type); /*!< initialize the dev */
    hal_mc_status_t (*deinit)(multicore_dev_t *dev); /*!< deinitialize the dev */
    hal_mc_status_t (*start)(multicore_dev_t *dev); /*!< start the dev */
    hal_mc_status_t (*stop)(multicore_dev_t *dev); /*!< stop the dev */
    hal_mc_status_t (*enqueue)(const multicore_dev_t *dev, void **data, uint32_t *compressed_size, uint32_t *stripe, uint32_t *frame_id); /*!< enqueue a buffer to the dev */
    hal_mc_status_t (*dequeue)(const multicore_dev_t *dev, void **data, uint32_t *compressed_size, uint32_t *stripe, uint32_t *frame_id); /*!< dequeue a buffer from the dev (blocking) */
    hal_mc_status_t (*get_buf_desc)(const multicore_dev_t *dev, void *io_desc, mpp_memory_policy_t prev_mem_policy); /*!< get buffer descriptors and policy */
    hal_mc_status_t (*lock)(const multicore_dev_t *dev); /*!< lock the device for exclusive access and operations */
    hal_mc_status_t (*unlock)(const multicore_dev_t *dev); /*!< unlock the device after exclusive operations */
} mc_dev_operator_t;

/** @brief Static buffer configuration for mc device. */
typedef struct
{
    int height;                  /*!< buffer height */
    int width;                   /*!< buffer width */
    int pitch;                   /*!< buffer pitch */
    mpp_pixel_format_t format;   /*!< pixel format */
    int stripe_size;             /*!< stripe size in bytes */
    bool stripe;                 /*!< stripe mode */
    uint32_t compressed_size;    /*!< compressed buffer size in bytes */
} mc_dev_static_buf_config;

/** @brief Structure that characterizes the mc device. */
typedef struct
{
    hal_mc_dev_type_t dev_type;  /*!< device type (sink or source) */
    mc_dev_static_buf_config buf_config[MAX_OUTPUT_PORTS];  /*!< buffer configuration */
    bool initial_enqueue_done; /*!< flag to indicate if initial enqueue is done */
    uint32_t min_req_cnt; /*!< minimum number of buffers required */
    uint32_t crt_req_cnt; /*!< current number of buffers requested */
    mpp_exec_flag_t req_exec_type; /*<! execution type for buffer requests */
    bool requested[MAX_OUTPUT_PORTS]; /*!< buffer request status */
    bool enqueued[MAX_OUTPUT_PORTS]; /*!< buffer enqueued status */
    void *buff_desc[MAX_OUTPUT_PORTS]; /*!< hw address to receive data */
} mc_dev_static_config_t;

/** @brief Attributes of a multicore device. */
struct _multicore_dev
{
    int id; /*!< unique id which is assigned by mc manager during registration */
    char name[HAL_DEVICE_NAME_MAX_LENGTH]; /*!< name of the device */
    const mc_dev_operator_t *ops;          /*!< operations */
    mc_dev_static_config_t config;         /*!< static configurations */
    void *data;                            /*!< device private data */
};

/** @} */

int HAL_MultiCoreDev_setup(const char *name, multicore_dev_t *dev);

#if (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)
void hal_mc_rpmsg_remote_ev_handler(mcmgr_core_t coreNum, uint16_t eventData, void *context);
#endif /* (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1) */

#endif /* _HAL_MC_H_ */