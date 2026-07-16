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

#ifndef _MPP_MULTICORE_H
#define _MPP_MULTICORE_H

#include "mpp_api.h"
#include "mpp_api_types_internal.h"

#if defined(MCMGR_USED)
#include "mcmgr.h"
#include "rpmsg_lite.h"
#include "rpmsg_queue.h"

/* Timeout for remote API calls in milliseconds */
#define MPP_REMOTE_API_TIMEOUT_MS 5000

/* API IDs for remote calls */
typedef enum {
    MPP_API_START = 0,
    MPP_API_STOP,
    MPP_API_FORCE_UPDATE,
    MPP_API_ELEMENT_UPDATE,
    MPP_API_SET_N_PASSES,
    MPP_API_MULTIPASS_ELEMENT_UPDATE,
    MPP_API_RESPONSE
} mpp_remote_api_id_t;

/* Remote API message structure */
typedef struct {
    mpp_remote_api_id_t api_id;
    uint32_t req_id;
    union {
        struct {
            mpp_t mpp;
            int last;
            bool force_update;
        } start_params;
        struct {
            mpp_t mpp;
        } stop_params;
        struct {
            mpp_t mpp;
        } force_update_params;
        struct {
            mpp_t mpp;
            mpp_elem_handle_t elem_h;
            mpp_element_params_t params;
            bool force_update;
        } element_update_params;
        struct {
            mpp_t mpp;
            uint32_t n_passes;
        } set_n_passes_params;
        struct {
            mpp_t mpp;
            mpp_elem_handle_t elem_h;
            uint32_t n_params;
            bool force_update;
            /* pointer into shared memory accessible from both cores */
            mpp_element_params_t *params_array;
        } multipass_element_update_params;
        struct {
            mpp_t mpp;
        } is_running_params;
        struct {
            int ret_code;
            mpp_remote_api_id_t req_api_id;
            uint32_t req_id;
        } response;
    } payload;
} mpp_remote_api_msg_t;

/* Remote API context structure */
typedef struct {
    uint32_t core_id;
    struct rpmsg_lite_instance *rpmsg;
    struct rpmsg_lite_endpoint *ept_recv;
    struct rpmsg_lite_endpoint *ept_send;
    mpp_mcmgr_event_data_e event_data;
    volatile mpp_mcmgr_event_data_e recv_event;
    uint32_t addr_local_recv;
    uint32_t addr_local_send;
    uint32_t addr_remote_recv;
    uint32_t addr_remote_send;
    rpmsg_queue_handle queue_recv;
    rpmsg_queue_handle queue_send;
    volatile bool ready;
    hal_mutex_t mutex;
    mpp_remote_api_msg_t last_resp;
} mpp_remote_ctx_t;

/**
 * @brief Tear down multicore support for MPP
 *
 * Releases all resources created by mpp_mc_multicore_init():
 * - Stops and deletes the remote API handler task
 * - Destroys per-core RPMSG endpoints and queues
 * - Destroys the queue set used for multiplexing
 * - Destroys the event database mutex
 * - Resets the event database
 *
 * @return MPP_SUCCESS on success, MPP_ERROR on failure
 */
int mpp_mc_multicore_deinit(void);

/**
 * @brief Initialize multicore support for MPP
 * 
 * Sets up communication channels with remote cores including:
 * - Event database for initialization synchronization
 * - RPMSG endpoints and queues for bidirectional communication
 * - Queue set for multiplexing messages from multiple cores
 * - Remote API handler task
 * - Core synchronization (Core 0 waits, other cores signal)
 * 
 * @param params MPP API parameters containing multicore configuration
 * @param pr_task_prio Priority for the remote API handler task
 * @return MPP_SUCCESS on success, MPP_ERROR on failure
 */
int mpp_mc_multicore_init(mpp_api_params_t *params, int pr_task_prio);

/**
 * @brief Send a remote API call to another core
 * 
 * Sends an API request to a remote core and optionally waits for the response.
 * Behavior is controlled by CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE:
 * - If enabled (1): Blocks until response is received or timeout occurs
 * - If disabled (0): Returns immediately after sending without waiting
 * 
 * @param msg Pointer to the API message to send
 * @param response Pointer to store the response (can be NULL if not waiting)
 * @param core_id Target core ID
 * @return MPP_SUCCESS on success, MPP_ERROR on failure
 */
int mpp_mc_remote_api_send(mpp_remote_api_msg_t *msg, mpp_remote_api_msg_t *response, uint32_t core_id);

/**
 * @brief Get remote context by core ID
 * 
 * Searches the remote context array for a matching core ID.
 * 
 * @param core_id Core ID to search for
 * @return Pointer to remote context if found, NULL otherwise
 */
mpp_remote_ctx_t *mpp_mc_remote_context_get_by_core_id(uint32_t core_id);

#endif /* MCMGR_USED */

#endif /* _MPP_MULTICORE_H */