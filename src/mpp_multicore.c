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

#include "mpp_multicore.h"
#include "mpp_api.h"
#include "mpp_debug.h"
#include "hal_os.h"
#include "hal_mc.h"
#include "string.h"

#if defined(MCMGR_USED)
/* Configuration: Enable blocking wait for remote API responses */
#ifndef CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE
#define CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE 1
#endif

#define REMOTE_API_TASK_STACK_SZ    1024
#define MPP_EVENT_DB_SIZE (MPP_MAX_CORES - 1)

/* Counter(id) for sent messages (added to message request for debugging) */
static uint32_t crt_req_id = 0;

/* Remote API communication contexts - one per remote core */
static mpp_remote_ctx_t remote_ctx[MPP_MAX_CORES - 1] = {0};

/* Event database for storing event data from remote cores during initialization */
typedef struct {
    mcmgr_core_t core_num; /* Core number that sent the event */
    uint16_t event_data;   /* Event data payload */
    volatile bool valid;   /* Entry is valid and contains data */
} mpp_event_db_entry_t;

/* Task handle for the remote API handler task */
static hal_task_t task_remote_api = NULL;

/* Queue set for multiplexing messages from multiple remote cores */
static void *queueset_remote = NULL;

/* Event database storage - used during multicore initialization */
static mpp_event_db_entry_t event_db[MPP_EVENT_DB_SIZE] = {0};

/* Flag indicating if event database has been initialized */
static volatile bool event_db_init = false;

/* Mutex protecting access to the event database */
static hal_mutex_t event_db_mutex = NULL;

/**
 * @brief Convert API ID to human-readable string for debugging
 * 
 * @param api_id The remote API identifier
 * @return String representation of the API ID
 */
static char *_mpp_mc_api_id_to_string(mpp_remote_api_id_t api_id)
{
    char *str = NULL;
    switch(api_id)
    {
        case MPP_API_START:
            str = "MPP_API_START";
            break;

        case MPP_API_STOP:
            str = "MPP_API_STOP";
            break;

        case MPP_API_FORCE_UPDATE:
            str = "MPP_API_FORCE_UPDATE";
            break;

        case MPP_API_ELEMENT_UPDATE:
            str = "MPP_API_ELEMENT_UPDATE";
            break;

        case MPP_API_SET_N_PASSES:
            str = "MPP_API_SET_N_PASSES";
            break;
        case MPP_API_MULTIPASS_ELEMENT_UPDATE:
            str = "MPP_API_MULTIPASS_ELEMENT_UPDATE";
            break;
        case MPP_API_RESPONSE:
            str = "MPP_API_RESPONSE";
            break;

        default:
            str = "UNKNOWN_API";
            break;
    }

    return str;
}

/**
 * @brief Initialize the event database and its mutex
 * 
 * The event database is used to store initialization events from remote cores
 * before the main communication channels are fully established.
 * 
 * @return MPP_SUCCESS on success, MPP_ERROR on failure
 */
static int _mpp_mc_event_database_init(void)
{
    if (event_db_init) {
        return MPP_SUCCESS;
    }

    int ret = hal_mutex_create(&event_db_mutex);
    if (ret != MPP_SUCCESS) {
        MPP_LOGE("Failed to create event database mutex\r\n");
        return ret;
    }

    event_db_init = true;
    return MPP_SUCCESS;
}

/**
 * @brief Store an event in the local database
 * 
 * This function stores events from remote cores during initialization.
 * It supports both lock-free operation (for early events before mutex init)
 * and mutex-protected operation (normal case).
 * 
 * @param core_num Core number that sent the event
 * @param event_data Event data to store
 * @return MPP_SUCCESS on success, MPP_ERROR if database is full
 */
static int _mpp_mc_event_database_store(mcmgr_core_t core_num, uint16_t event_data)
{
    int ret = MPP_ERROR;

    /* If mutex not initialized yet, use lock-free approach for early events */
    if (!event_db_init || event_db_mutex == NULL) {
        for (int i = 0; i < MPP_EVENT_DB_SIZE; i++) {
            /* Reuse existing entry for same core or find empty slot */
            if (!event_db[i].valid || event_db[i].core_num == core_num) {
                event_db[i].core_num = core_num;
                event_db[i].event_data = event_data;
                event_db[i].valid = true;
                return MPP_SUCCESS;
            }
        }
        return MPP_ERROR;
    }

    /* Normal operation with mutex protection */
    if (hal_mutex_lock(event_db_mutex) != MPP_SUCCESS) {
        MPP_LOGE("Failed to lock event database mutex\r\n");
        return MPP_ERROR;
    }

    for (int i = 0; i < MPP_EVENT_DB_SIZE; i++) {
        /* Reuse existing entry for same core or find empty slot */
        if (!event_db[i].valid || event_db[i].core_num == core_num) {
            event_db[i].core_num = core_num;
            event_db[i].event_data = event_data;
            event_db[i].valid = true;
            ret = MPP_SUCCESS;
            break;
        }
    }

    hal_mutex_unlock(event_db_mutex);
    return ret;
}

/**
 * @brief Retrieve an event from the local database
 * 
 * Searches for and retrieves event data from a specific core.
 * 
 * @param core_num Core number to search for
 * @param event_data Pointer to store retrieved event data
 * @return MPP_SUCCESS if event found, MPP_ERROR otherwise
 */
static int _mpp_mc_event_database_retrieve(mcmgr_core_t core_num, uint16_t *event_data)
{
    int ret = MPP_ERROR;

    /* If mutex not initialized yet, use lock-free read */
    if (!event_db_init || event_db_mutex == NULL) {
        for (int i = 0; i < MPP_EVENT_DB_SIZE; i++) {
            if (event_db[i].valid && event_db[i].core_num == core_num) {
                *event_data = event_db[i].event_data;
                return MPP_SUCCESS;
            }
        }
        return MPP_ERROR;
    }

    /* Normal operation with mutex protection */
    if (hal_mutex_lock(event_db_mutex) != MPP_SUCCESS) {
        MPP_LOGE("Failed to lock event database mutex\r\n");
        return MPP_ERROR;
    }

    for (int i = 0; i < MPP_EVENT_DB_SIZE; i++) {
        if (event_db[i].valid && event_db[i].core_num == core_num) {
            *event_data = event_db[i].event_data;
            ret = MPP_SUCCESS;
            break;
        }
    }

    hal_mutex_unlock(event_db_mutex);
    return ret;
}

/* Get remote context by core ID */
mpp_remote_ctx_t *mpp_mc_remote_context_get_by_core_id(uint32_t core_id)
{
    for (int i = 0; i < MPP_MAX_CORES - 1; i++)
    {
        if (remote_ctx[i].core_id == core_id && remote_ctx[i].rpmsg != NULL)
        {
            return &remote_ctx[i];
        }
    }
    return NULL;
}

/**
 * @brief Get remote context by receive queue
 * 
 * Internal function to find which remote core a received message came from
 * based on the queue that received it.
 * 
 * @param queue Queue handle to search for
 * @return Pointer to remote context if found, NULL otherwise
 */
static mpp_remote_ctx_t *_mpp_mc_remote_context_get_by_queue(void *queue)
{
    for (int i = 0; i < MPP_MAX_CORES - 1; i++)
    {
        if (remote_ctx[i].queue_recv == queue && remote_ctx[i].rpmsg != NULL)
        {
            return &remote_ctx[i];
        }
    }
    return NULL;
}

/* Remote API event handler for multicore communication */
void mpp_mc_remote_event_handler(mcmgr_core_t coreNum, uint16_t eventData, void *context)
{
    if (_mpp_mc_event_database_store(coreNum, eventData) == MPP_SUCCESS)
    {
        MPP_LOGI("Remote core %d ready for MPP API calls (event: %d)\r\n", coreNum, eventData);
    }
    else
    {
        MPP_LOGE("Failed to store event (%d) from core %d in database\r\n", eventData, coreNum);
    }
}

/**
 * @brief Remote API task handler
 * 
 * This task runs continuously and handles incoming API requests from remote cores.
 * It waits on a queue set that multiplexes messages from all remote cores,
 * processes the API call, and sends back a response if configured to do so.
 * Behavior is controlled by CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE:
 * - If enabled (1): Send back the response to the remote core
 * - If disabled (0): Do not send back the response to remote core to avoid queue overflow
 * @param arg Task argument (unused)
 */
static void _mpp_mc_remote_api_task_handler(void *arg)
{
    MPP_LOGI("remoteApiTask starting\r\n");
    mpp_remote_api_msg_t recv_msg;
    uint32_t msg_size;
    uint32_t remote_addr;
    int32_t rpmsg_ret;
    int ret_code;
#if CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE
    mpp_remote_api_msg_t response_msg;
#endif

    for(;;) {
        /* Blocking wait on queue set for messages from any remote core */
        void *available_queue = hal_queueset_receive(queueset_remote, HAL_MAX_TIMEOUT);
        if (available_queue == NULL)
        {
            MPP_LOGE("hal_queueset_receive timeout or error\r\n");
            hal_task_delay_ms(100);
            continue;
        }

        /* Identify which remote core sent the message */
        mpp_remote_ctx_t *ctx = _mpp_mc_remote_context_get_by_queue(available_queue);
        if (ctx == NULL)
        {
            MPP_LOGE("Failed to find remote context for received queue\r\n");
            continue;
        }

        /* Non-blocking receive from the identified queue */
        rpmsg_ret = rpmsg_queue_recv(ctx->rpmsg,
                                     ctx->queue_recv,
                                     &remote_addr,
                                     (char *)&recv_msg,
                                     sizeof(mpp_remote_api_msg_t),
                                     &msg_size,
                                     RL_DONT_BLOCK);

        if (rpmsg_ret == RL_SUCCESS)
        {
            /* Verify the message came from the expected remote address */
            if (remote_addr != ctx->addr_remote_send)
            {
                MPP_LOGE("Received API message from unexpected remote address %d\r\n", remote_addr);
                hal_task_delay_ms(100);
                continue;
            }

            MPP_LOGD("Received API message:\r\n\t%s\r\n\treq_id %d\r\n", _mpp_mc_api_id_to_string(recv_msg.api_id), recv_msg.req_id);

            /* Dispatch to appropriate MPP API function based on message type */
            switch (recv_msg.api_id)
            {
                case MPP_API_START:
                    ret_code = mpp_start(
                        recv_msg.payload.start_params.mpp,
                        recv_msg.payload.start_params.last,
                        recv_msg.payload.start_params.force_update);
                    break;

                case MPP_API_STOP:
                    ret_code = mpp_stop(recv_msg.payload.stop_params.mpp);
                    break;

                case MPP_API_FORCE_UPDATE:
                    ret_code = mpp_force_update(recv_msg.payload.force_update_params.mpp);
                    break;

                case MPP_API_ELEMENT_UPDATE:
                    ret_code = mpp_element_update(
                        recv_msg.payload.element_update_params.mpp,
                        recv_msg.payload.element_update_params.elem_h,
                        &recv_msg.payload.element_update_params.params,
                        recv_msg.payload.element_update_params.force_update);
                    break;

                case MPP_API_SET_N_PASSES:
                    ret_code = mpp_set_n_passes(
                        recv_msg.payload.set_n_passes_params.mpp,
                        recv_msg.payload.set_n_passes_params.n_passes);
                    break;

                case MPP_API_MULTIPASS_ELEMENT_UPDATE:
                    ret_code = mpp_multipass_element_update(
                        recv_msg.payload.multipass_element_update_params.mpp,
                        recv_msg.payload.multipass_element_update_params.elem_h,
                        recv_msg.payload.multipass_element_update_params.params_array,
                        recv_msg.payload.multipass_element_update_params.n_params,
                        recv_msg.payload.multipass_element_update_params.force_update);
                    break;

                default:
                    MPP_LOGE("Unknown remote API ID: %d\r\n", recv_msg.api_id);
                    ret_code = MPP_INVALID_PARAM;
                    break;
            }

#if CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE
            /* Process the API call and prepare response */
            response_msg.api_id = MPP_API_RESPONSE;
            response_msg.payload.response.ret_code = ret_code;
            response_msg.payload.response.req_api_id = recv_msg.api_id;
            response_msg.payload.response.req_id = recv_msg.req_id;
            /* Send response back to the requesting core */
            MPP_LOGD("Sending API response:\r\n\t%s\r\n\treq_id %d\r\n", _mpp_mc_api_id_to_string(recv_msg.api_id), recv_msg.req_id);
            rpmsg_ret = rpmsg_lite_send(ctx->rpmsg,
                                       ctx->ept_send,
                                       ctx->addr_remote_send,
                                       (char *)&response_msg,
                                       sizeof(mpp_remote_api_msg_t),
                                       RL_DONT_BLOCK);

            if (rpmsg_ret != RL_SUCCESS)
            {
                MPP_LOGE("Failed to send API response: %d\r\n", rpmsg_ret);
            }
#else
            /* Do not send response to the remote core */
            MPP_LOGD("Not sending response to remote core(ret code = %d)\r\n", ret_code);
#endif
        }
        else
        {
            MPP_LOGE("Failed to receive remote API message: %d\r\n", rpmsg_ret);
        }
    }
}

/* Send remote API call */
int mpp_mc_remote_api_send(mpp_remote_api_msg_t *msg, mpp_remote_api_msg_t *response, uint32_t core_id)
{
    int32_t rpmsg_ret;
#if CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE
    uint32_t remote_addr;
    uint32_t msg_size;
    uint32_t start_time;
    mpp_remote_api_id_t expected_api_id;
    uint32_t expected_req_id;
#endif

    /* Find the remote context for the target core */
    mpp_remote_ctx_t *ctx = mpp_mc_remote_context_get_by_core_id(core_id);
    if (ctx == NULL)
    {
        MPP_LOGE("Remote context not found for core %d\r\n", core_id);
        return MPP_ERROR;
    }

    /* Verify the remote core is ready for API calls */
    if (!ctx->ready)
    {
        MPP_LOGE("Remote core not ready for API calls\r\n");
        return MPP_ERROR;
    }

    /* Lock API mutex to serialize remote calls to this core */
    if (hal_mutex_lock(ctx->mutex) != MPP_SUCCESS)
    {
        MPP_LOGE("Failed to lock API mutex\r\n");
        return MPP_ERROR;
    }

    /* Send the API call message to the remote core */
    MPP_LOGD("Send API remote call:\r\n\tapi_id %s\r\n\treq_id %d\r\n", _mpp_mc_api_id_to_string(msg->api_id), crt_req_id);
    msg->req_id = crt_req_id++;
    rpmsg_ret = rpmsg_lite_send(ctx->rpmsg,
                               ctx->ept_send,
                               ctx->addr_remote_recv,
                               (char *)msg,
                               sizeof(mpp_remote_api_msg_t),
                               RL_DONT_BLOCK);

    if (rpmsg_ret != RL_SUCCESS)
    {
        MPP_LOGE("Failed to send remote API call: %d\r\n", rpmsg_ret);
        hal_mutex_unlock(ctx->mutex);
        return MPP_ERROR;
    }

#if CONFIG_MPP_MC_API_WAIT_FOR_RESPONSE
    /* Wait for response with timeout */
    expected_api_id = msg->api_id;
    expected_req_id = msg->req_id;
    start_time = hal_get_ostick();
    uint32_t timeout_ticks = hal_ms_to_ticks(MPP_REMOTE_API_TIMEOUT_MS);

    while (1)
    {
        /* Try to receive response (non-blocking) */
        rpmsg_ret = rpmsg_queue_recv(ctx->rpmsg,
                                     ctx->queue_send,
                                     &remote_addr,
                                     (char *)&ctx->last_resp,
                                     sizeof(mpp_remote_api_msg_t),
                                     &msg_size,
                                     RL_DONT_BLOCK);

        if (rpmsg_ret == RL_SUCCESS)
        {
            if (ctx->last_resp.api_id == MPP_API_RESPONSE &&
                ctx->last_resp.payload.response.req_api_id == expected_api_id &&
                ctx->last_resp.payload.response.req_id == expected_req_id)
            {
                /* Copy response to caller's buffer if provided */
                if (response)
                    *response = ctx->last_resp;
                MPP_LOGD("Received response:\r\n\treturn code %s\r\n\treq_api_id %s\r\n\treq_id %d\r\n", 
                          mpp_error_string(ctx->last_resp.payload.response.ret_code),
                          _mpp_mc_api_id_to_string(ctx->last_resp.payload.response.req_api_id), 
                          ctx->last_resp.payload.response.req_id);
                hal_mutex_unlock(ctx->mutex);
                return MPP_SUCCESS;
            }
            MPP_LOGD("Discarding mismatched response:\r\n\t(api_id=%s, req_id=%d)\r\n\texpected (api_id=%s, req_id=%d)\r\n",
                      _mpp_mc_api_id_to_string(ctx->last_resp.payload.response.req_api_id),
                      ctx->last_resp.payload.response.req_id,
                      _mpp_mc_api_id_to_string(expected_api_id),
                      expected_req_id
                    );
            continue; /* Try next message */
        }

        /* Check if timeout has elapsed */
        if ((hal_get_ostick() - start_time) >= timeout_ticks)
        {
            MPP_LOGE("Timeout waiting for remote API response\r\n");
            hal_mutex_unlock(ctx->mutex);
            return MPP_ERROR;
        }

        /* Small delay before retrying */
        hal_task_delay_ms(10);
    }
#else
    /* Non-blocking mode: return immediately after sending */
    MPP_LOGD("API call sent (non-blocking mode, not waiting for response)\r\n");
    hal_mutex_unlock(ctx->mutex);
    return MPP_SUCCESS;
#endif
}

/* Initialize multicore support */
int mpp_mc_multicore_init(mpp_api_params_t *params, int pr_task_prio)
{
    int ret;

    /* Skip initialization if no remote cores configured */
    if (params == NULL || params->multicore.n_remote_cores == 0)
    {
        return MPP_SUCCESS;
    }

    /* Initialize event database for core synchronization */
    _mpp_mc_event_database_init();
    mcmgr_core_t current_core = MCMGR_GetCurrentCore();

    /* Create queue set to multiplex messages from all remote cores */
    queueset_remote = hal_queueset_create(params->multicore.n_remote_cores);
    if (queueset_remote == NULL)
    {
        MPP_LOGE("Failed to create remote API queueset\r\n");
        return MPP_ERROR;
    }

    /* Initialize communication context for each remote core */
    for (uint32_t i = 0; i < params->multicore.n_remote_cores; i++)
    {
        /* Store basic configuration */
        remote_ctx[i].core_id = params->multicore.core[i].remote_core_id;
        remote_ctx[i].rpmsg = (struct rpmsg_lite_instance *)params->multicore.core[i].rpmsg_inst;
        remote_ctx[i].event_data = MPP_MCMGR_DATA_API;
        
        /* Calculate RPMSG addresses for bidirectional communication */
        remote_ctx[i].addr_local_recv = MPP_RPMSG_ADDR_API_RX(current_core);
        remote_ctx[i].addr_remote_recv = MPP_RPMSG_ADDR_API_RX(remote_ctx[i].core_id);
        remote_ctx[i].addr_local_send = MPP_RPMSG_ADDR_API_TX(current_core);
        remote_ctx[i].addr_remote_send = MPP_RPMSG_ADDR_API_TX(remote_ctx[i].core_id);

        /* Create mutex for serializing API calls to this remote core */
        if (hal_mutex_create(&remote_ctx[i].mutex) != MPP_SUCCESS)
        {
            MPP_LOGE("Failed to create remote API mutex\r\n");
            return MPP_ERROR;
        }

        /* Create receive queue for incoming API requests from remote core */
        remote_ctx[i].queue_recv = rpmsg_queue_create(remote_ctx[i].rpmsg);
        if (remote_ctx[i].queue_recv == NULL)
        {
            MPP_LOGE("Remote API RPMSG queue creation failed\r\n");
            return MPP_ERROR;
        }

        /* Add receive queue to the queue set for multiplexing */
        ret = hal_queueset_add_queue(queueset_remote, remote_ctx[i].queue_recv);
        if (ret != MPP_SUCCESS)
        {
            MPP_LOGE("Failed to add remote API queue to queueset\r\n");
            return MPP_ERROR;
        }

        /* Create send queue for receiving responses to our API calls */
        remote_ctx[i].queue_send = rpmsg_queue_create(remote_ctx[i].rpmsg);
        if (remote_ctx[i].queue_send == NULL)
        {
            MPP_LOGE("Remote API RPMSG queue creation failed\r\n");
            return MPP_ERROR;
        }

        /* Create receive endpoint for incoming API requests */
        remote_ctx[i].ept_recv = rpmsg_lite_create_ept(
            remote_ctx[i].rpmsg,
            remote_ctx[i].addr_local_recv,
            rpmsg_queue_rx_cb,
            remote_ctx[i].queue_recv);
        if (remote_ctx[i].ept_recv == NULL)
        {
            MPP_LOGE("Remote API RPMSG endpoint creation failed\r\n");
            rpmsg_queue_destroy(remote_ctx[i].rpmsg, remote_ctx[i].queue_recv);
            return MPP_ERROR;
        }

        /* Create send endpoint for sending API calls and receiving responses */
        remote_ctx[i].ept_send = rpmsg_lite_create_ept(
            remote_ctx[i].rpmsg,
            remote_ctx[i].addr_local_send,
            rpmsg_queue_rx_cb,
            remote_ctx[i].queue_send);
        if (remote_ctx[i].ept_send == NULL)
        {
            MPP_LOGE("Remote API RPMSG endpoint creation failed\r\n");
            rpmsg_queue_destroy(remote_ctx[i].rpmsg, remote_ctx[i].queue_send);
            return MPP_ERROR;
        }

        /* Synchronize cores: Core 0 waits, other cores signal readiness */
        MPP_LOGI("Remote API initialized on core %d\r\n", current_core);
        if (current_core == kMCMGR_Core0)
        {
            /* Core 0 waits for other cores to signal they're ready */
            uint32_t start_time = hal_get_ostick();
            uint32_t timeout_ms = 10000;
            uint16_t retrieved_event = 0;

            /* Poll event database until remote core signals readiness */
            while (_mpp_mc_event_database_retrieve(remote_ctx[i].core_id, &retrieved_event) != MPP_SUCCESS)
            {
                uint32_t elapsed = hal_get_ostick() - start_time;
                if ((elapsed * hal_get_tick_period_ms()) >= timeout_ms)
                {
                    MPP_LOGE("Timeout waiting for remote core ready for API\r\n");
                    return MPP_ERROR;
                }
                hal_task_delay_ms(10);
            }

            /* Validate received event data and mark context as ready */
            remote_ctx[i].ready = true;
            remote_ctx[i].recv_event = retrieved_event;

            if (retrieved_event != remote_ctx[i].event_data)
            {
                MPP_LOGE("Remote event data mismatch: expected %d, got %d\r\n",
                         remote_ctx[i].event_data, retrieved_event);
                return MPP_ERROR;
            }
            MPP_LOGI("Remote core ready for MPP API calls\r\n");
        }
        else
        {
            /* Non-Core0: Signal Core 0 that we're ready for API calls */
#if defined(MCMGR_REMOTE_APP_EVENT_COUNT) && (MCMGR_REMOTE_APP_EVENT_COUNT >= 3)
            mcmgr_status_t ev_ret = MCMGR_TriggerEvent(kMCMGR_Core0, kMCMGR_RemoteApplicationEvent2,
                                                        remote_ctx[i].event_data);
            if (ev_ret != kStatus_MCMGR_Success)
            {
                MPP_LOGE("Failed to trigger remote API ready event: %d\r\n", ev_ret);
                return MPP_ERROR;
            }
#endif
            MPP_LOGI("Sent ready event for MPP API to remote core\r\n");
        }
    }

    /* Create the remote API handler task to process incoming requests */
    ret = hal_task_create(_mpp_mc_remote_api_task_handler,
                          "remoteApiTask",
                          REMOTE_API_TASK_STACK_SZ,
                          NULL,
                          pr_task_prio,
                          &task_remote_api);
    if (MPP_SUCCESS != ret) {
        MPP_LOGE("Failed to create remoteApiTask\n");
        return MPP_ERROR;
    }

    return MPP_SUCCESS;
}

/* Deinitialize multicore support */
int mpp_mc_multicore_deinit(void)
{
    /* Delete the remote API handler task */
    if (task_remote_api != NULL)
    {
        hal_task_delete(task_remote_api);
        task_remote_api = NULL;
    }

    /* Tear down per-core communication resources */
    for (int i = 0; i < MPP_MAX_CORES - 1; i++)
    {
        if (remote_ctx[i].rpmsg == NULL)
            continue;

        /* Destroy receive endpoint */
        if (remote_ctx[i].ept_recv != NULL)
        {
            rpmsg_lite_destroy_ept(remote_ctx[i].rpmsg, remote_ctx[i].ept_recv);
            remote_ctx[i].ept_recv = NULL;
        }

        /* Destroy send endpoint */
        if (remote_ctx[i].ept_send != NULL)
        {
            rpmsg_lite_destroy_ept(remote_ctx[i].rpmsg, remote_ctx[i].ept_send);
            remote_ctx[i].ept_send = NULL;
        }

        /* Destroy receive queue */
        if (remote_ctx[i].queue_recv != NULL)
        {
            rpmsg_queue_destroy(remote_ctx[i].rpmsg, remote_ctx[i].queue_recv);
            remote_ctx[i].queue_recv = NULL;
        }

        /* Destroy send queue */
        if (remote_ctx[i].queue_send != NULL)
        {
            rpmsg_queue_destroy(remote_ctx[i].rpmsg, remote_ctx[i].queue_send);
            remote_ctx[i].queue_send = NULL;
        }

        /* Destroy per-core API mutex */
        if (remote_ctx[i].mutex != NULL)
        {
            hal_mutex_remove(remote_ctx[i].mutex);
            remote_ctx[i].mutex = NULL;
        }

        remote_ctx[i].rpmsg = NULL;
        remote_ctx[i].ready = false;
    }

    /* Destroy the queue set used to multiplex receive queues */
    if (queueset_remote != NULL)
    {
        hal_queueset_delete(queueset_remote);
        queueset_remote = NULL;
    }

    /* Destroy the event database mutex */
    if (event_db_mutex != NULL)
    {
        hal_mutex_remove(event_db_mutex);
        event_db_mutex = NULL;
    }

    /* Reset event database */
    event_db_init = false;
    for (int i = 0; i < MPP_EVENT_DB_SIZE; i++)
    {
        event_db[i].valid = false;
    }

    return MPP_SUCCESS;
}

#endif /* MCMGR_USED */
