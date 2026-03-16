/*
 * Copyright 2026 NXP.
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

 /*
 * This is the abstraction layer for the MC sink and source elements
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"

#include "mpp_api_types.h"
#include "mpp_api_types_internal.h"
#include "hal_os.h"
#include "hal_debug.h"
#include "hal_mc.h"
#include "mpp_config.h"

#if defined(HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1)

#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "mcmgr.h"

/**
 * Maximum number of endpoints that can be tracked in the database
 */
#define HAL_MC_MAX_ENDPOINTS    5

/**
 * Memory alignment requirement for multicore communication buffers
 */
#define HAL_MC_MEM_ALIGN        4

/**
 * multicore device private data structure
 */
typedef struct {
    struct rpmsg_lite_instance *rpmsg_inst; /*!< pointer to rpmsg instance */
    mpp_mcmgr_event_data_e remote_event_data; /*!< remote event data for mcmgr */
    uint32_t local_rpmsg_addr; /*!< local rpmsg endpoint address */
    uint32_t remote_rpmsg_addr; /*!< remote rpmsg endpoint address */
    struct rpmsg_lite_endpoint *rpmsg_ept; /*!< rpmsg endpoint handle */
    rpmsg_queue_handle rpmsg_queue; /*!< rpmsg queue handle for message reception */
    volatile bool remote_ready; /*!< flag indicating if remote core is ready */
    uint32_t nb_buffers; /*!< number of input buffers */
    hal_mutex_t mutex; /*!< Mutex to lock the device during an operation */
} mc_dev_private_data_t;

typedef struct {
    volatile mpp_mcmgr_event_data_e mcmgr_event_data;
    volatile bool remote_ready;
} mc_remote_event_data_t;

typedef enum {
    MC_MSG_TYPE_BUF_DESC_REQ = 0,
    MC_MSG_TYPE_BUF_DESC_RESP = 1,
    MC_MSG_TYPE_DATA_REQ = 2,
    MC_MSG_TYPE_DATA_RESP = 3
} mc_msg_type_e;

typedef struct {
    struct {
        mc_msg_type_e msg_type;
    } header;
    union {
        struct {
            uint32_t cnt;
            mpp_memory_policy_t prev_mem_policy;
            buf_desc_t *buf_desc[MAX_OUTPUT_PORTS];
        } buff_desc_resp;
        struct {
            void *addr[MAX_OUTPUT_PORTS];
        } data_req;
        struct {
            void *addr[MAX_OUTPUT_PORTS];
            uint32_t size[MAX_OUTPUT_PORTS];
            uint32_t frame_id[MAX_OUTPUT_PORTS];
        } data_rsp;
    } payload;
} mc_message_t;

/**
 * Add a global mutex for protecting shared handle between multiple
 * sources/sinks running on the same core
 */
static hal_mutex_t mc_global_mutex = NULL;

/**
 * Database with all the initialized endpoints and remote events
 */
static mc_dev_private_data_t *mc_dev_list[HAL_MC_MAX_ENDPOINTS] = {NULL};

/**
 * Database to store endpoint ready data for synchronization
 */
static mc_remote_event_data_t mc_remote_event_data[HAL_MC_MAX_ENDPOINTS] = {MPP_MCMGR_EVENT_DATA_INVALID};

/*******************************************************************************
 * Code
 ******************************************************************************/

void HAL_DCACHE_InvalidateByRange(uint32_t addr, uint32_t size);
void HAL_DCACHE_CleanByRange(uint32_t addr, uint32_t size);

void hal_mc_rpmsg_remote_ev_handler(mcmgr_core_t coreNum, uint16_t eventData, void *context)
{
    /* Using mutexes inside this function is not required because this is called from interrupt context
     * and this is the only code that adds entries to the mc_remote_event_data database */
    bool found_empty_entry = false;

    for (int i = 0; i < HAL_MC_MAX_ENDPOINTS; i++)
    {
        if (mc_remote_event_data[i].mcmgr_event_data == MPP_MCMGR_EVENT_DATA_INVALID)
        {
            mc_remote_event_data[i].mcmgr_event_data = (mpp_mcmgr_event_data_e)eventData;
            mc_remote_event_data[i].remote_ready = true;
            found_empty_entry = true;
            break;
        }
        else if (mc_remote_event_data[i].mcmgr_event_data == eventData)
        {
            if (mc_remote_event_data[i].remote_ready)
                HAL_LOGE("Event data already allocated for event_data: 0x%04x\r\n", eventData);
            else
                HAL_LOGE("Event data ready already set for event_data: 0x%04x\r\n", eventData);
            break;
        }
    }

    if (!found_empty_entry)
    {
        HAL_LOGE("Did not found any empty entry to set ready flag for event data: 0x%04x\r\n", eventData);
    }
}

/**
 * Wait for remote core to be ready for a specific event data
 * 
 * @param event_data The event data to wait for
 * @param timeout_ms Timeout in milliseconds (0 for no timeout)
 * @return kStatus_HAL_MultiCoreSuccess if remote is ready, error otherwise
 */
static hal_mc_status_t wait_for_remote_ready(mpp_mcmgr_event_data_e event_data, uint32_t timeout_ms)
{
    uint32_t start_time = hal_get_ostick();
    bool remote_ready = false;
    int event_index = -1;

    HAL_LOGD("Waiting for remote ready with event data: 0x%04x\r\n", event_data);

    while (1)
    {
        // Search for the event data in the database
        for (int i = 0; i < HAL_MC_MAX_ENDPOINTS; i++)
        {
            if (mc_remote_event_data[i].mcmgr_event_data == event_data)
            {
                if (mc_remote_event_data[i].remote_ready)
                {
                    remote_ready = true;
                    event_index = i;
                }
                break;
            }
        }

        if (remote_ready)
        {
            HAL_LOGD("Remote ready flag set for event data: 0x%04x at index %d\r\n", event_data, event_index);
            return kStatus_HAL_MultiCoreSuccess;
        }

        // Check timeout
        if (timeout_ms > 0)
        {
            uint32_t elapsed = hal_get_ostick() - start_time;
            if ((elapsed * hal_get_tick_period_ms()) >= timeout_ms)
            {
                HAL_LOGE("Timeout waiting for remote ready (event data: 0x%04x)\r\n", event_data);
                return kStatus_HAL_MultiCoreError;
            }
        }

        // Small delay to avoid busy waiting
        hal_task_delay(pdMS_TO_TICKS(10));
    }
}

static hal_mc_status_t send_remote_ready_event(mpp_mcmgr_event_data_e event_data)
{
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    if (event_data == MPP_MCMGR_EVENT_DATA_INVALID)
    {
        HAL_LOGE("Invalid event data: 0x%04x\r\n", event_data);
        return kStatus_HAL_MultiCoreError;
    }

    mcmgr_core_t remote_core = MCMGR_GetCurrentCore() == kMCMGR_Core0 ? kMCMGR_Core1 : kMCMGR_Core0;

#if defined(MCMGR_REMOTE_APP_EVENT_COUNT) && (MCMGR_REMOTE_APP_EVENT_COUNT >= 2)
    mcmgr_status_t ev_ret = MCMGR_TriggerEvent(remote_core, kMCMGR_RemoteApplicationEvent1, event_data);
#else
    mcmgr_status_t ev_ret = kStatus_MCMGR_Error;
    #error "MCMGR_REMOTE_APP_EVENT_COUNT is less than 2. Cannot initialize multicore resources."
#endif

    if (ev_ret != kStatus_MCMGR_Success)
    {
        HAL_LOGE("MCMGR_TriggerEvent failed with status: %d\r\n", ev_ret);
        return kStatus_HAL_MultiCoreError;
    }

    HAL_LOGD("Remote ready event sent with event data: 0x%04x\r\n", event_data);
    return ret;
}

// Initialize global mutex (call this during system init)
static hal_mc_status_t init_mc_global_resources(void)
{
    hal_mutex_t new_mutex = NULL;

    if (mc_global_mutex != NULL)
        return kStatus_HAL_MultiCoreSuccess;

    if (hal_mutex_create(&new_mutex) != MPP_SUCCESS || new_mutex == NULL)
    {
        HAL_LOGE("Failed to create global mutex\n");
        return kStatus_HAL_MultiCoreError;
    }

    hal_ctx_t ctx;

    hal_atomic_enter(&ctx);
    // make sure only one thread initializes the mutex
    if (mc_global_mutex == NULL)
    {
        mc_global_mutex = new_mutex;
        new_mutex = NULL;
    }
    hal_atomic_exit(&ctx);

    if (new_mutex != NULL)
    {
        hal_mutex_remove(new_mutex);
    }
    return kStatus_HAL_MultiCoreSuccess;
}

static hal_mc_status_t add_mc_dev_to_list(mc_dev_private_data_t *dev_priv)
{
    hal_mc_status_t ret = kStatus_HAL_MultiCoreError;
    int empty_slot = -1;

    hal_mutex_lock(mc_global_mutex);

    // First pass: check for duplicates and find first empty slot
    for (int i = 0; i < HAL_MC_MAX_ENDPOINTS; i++)
    {
        if (mc_dev_list[i] == NULL)
        {
            if (empty_slot == -1)
                empty_slot = i;
        }
        else
        {
            mc_dev_private_data_t *existing = mc_dev_list[i];

            // Check if another device has the same rpmsg parameters
            if (existing->local_rpmsg_addr == dev_priv->local_rpmsg_addr ||
                existing->remote_rpmsg_addr == dev_priv->remote_rpmsg_addr ||
                existing->remote_event_data == dev_priv->remote_event_data)
            {
                HAL_LOGE("Device with same rpmsg parameters already exists in list\r\n");
                hal_mutex_unlock(mc_global_mutex);
                return kStatus_HAL_MultiCoreError;
            }
        }
    }

    // Add to first empty slot if found
    if (empty_slot != -1)
    {
        mc_dev_list[empty_slot] = dev_priv;
        ret = kStatus_HAL_MultiCoreSuccess;
    }
    else
    {
        HAL_LOGE("Failed to add device to list: database full\r\n");
    }

    hal_mutex_unlock(mc_global_mutex);

    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Init(multicore_dev_t *dev, mpp_mc_params_t *config, hal_mc_dev_type_t type)
{
    HAL_LOGD("++HAL_MultiCoreDev_Init\r\n");
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    mcmgr_core_t current_core = MCMGR_GetCurrentCore();

    if (current_core == kMCMGR_Core0)
    {
        if (config->local_rpmsg_addr < MPP_RPMSG_EPT_ADDR_CORE0_START ||
            config->local_rpmsg_addr > MPP_RPMSG_EPT_ADDR_CORE0_STOP ||
            config->remote_rpmsg_addr < MPP_RPMSG_EPT_ADDR_CORE1_START ||
            config->remote_rpmsg_addr > MPP_RPMSG_EPT_ADDR_CORE1_STOP ||
            config->remote_event_data < MPP_MCMGR_EVENT_DATA_START ||
            config->remote_event_data > MPP_MCMGR_EVENT_DATA_STOP)
        {
            HAL_LOGE("Invalid rpmsg endpoint address or mcmgr event data for Core0\r\n");
            return kStatus_HAL_MultiCoreError;
        }
    }
    else if (current_core == kMCMGR_Core1)
    {
        if (config->local_rpmsg_addr < MPP_RPMSG_EPT_ADDR_CORE1_START ||
            config->local_rpmsg_addr > MPP_RPMSG_EPT_ADDR_CORE1_STOP ||
            config->remote_rpmsg_addr < MPP_RPMSG_EPT_ADDR_CORE0_START ||
            config->remote_rpmsg_addr > MPP_RPMSG_EPT_ADDR_CORE0_STOP ||
            config->remote_event_data < MPP_MCMGR_EVENT_DATA_START ||
            config->remote_event_data > MPP_MCMGR_EVENT_DATA_STOP)
        {
            HAL_LOGE("Invalid rpmsg endpoint address or mcmgr event data for Core1\r\n");
            return kStatus_HAL_MultiCoreError;
        }
    }
    else
    {
        HAL_LOGE("Unknown core ID\r\n");
        return kStatus_HAL_MultiCoreError;
    }

    /* Initialize dev static config data */
    dev->config.dev_type = type;
    dev->config.crt_req_cnt = 0;
    dev->config.initial_enqueue_done = false;
    /* min_req_cnt and req_exec_type are always computed by the pipeline execution at runtime */
    dev->config.min_req_cnt = 0;
    dev->config.req_exec_type = MPP_EXEC_RC;
    memset(&dev->config.requested, 0, sizeof(dev->config.requested));
    memset(&dev->config.buff_desc, 0, sizeof(dev->config.buff_desc));

    /* Allocate memory for the multicore device private data structure.
     * Returns error status if allocation fails. */
    mc_dev_private_data_t *dev_priv = hal_malloc(sizeof(mc_dev_private_data_t));
    if (dev_priv == NULL)
    {
        HAL_LOGE("Failed to allocate memory for multicore device private data\r\n");
        return kStatus_HAL_MultiCoreError;
    }
    memset(dev_priv, 0, sizeof(mc_dev_private_data_t));

    dev->data = (void *)dev_priv;

    /* Initialize global resources */
    ret = init_mc_global_resources();
    if (ret != kStatus_HAL_MultiCoreSuccess)
    {
        HAL_LOGE("Failed to initialize global multicore resources\r\n");
        dev->data = NULL;
        hal_free(dev_priv);
        return kStatus_HAL_MultiCoreError;
    }

    dev_priv->rpmsg_inst = (struct rpmsg_lite_instance *) config->rpmsg_inst;
    dev_priv->remote_event_data = config->remote_event_data;
    dev_priv->local_rpmsg_addr = config->local_rpmsg_addr;
    dev_priv->remote_rpmsg_addr = config->remote_rpmsg_addr;
    dev_priv->remote_ready = false;
    dev_priv->nb_buffers = 0;

    if (hal_mutex_create(&dev_priv->mutex) != MPP_SUCCESS)
    {
        HAL_LOGE("Failed to create device mutex\r\n");
        dev->data = NULL;
        hal_free(dev_priv);
        return kStatus_HAL_MultiCoreError;
    }

    // Add to the device list
    ret = add_mc_dev_to_list(dev_priv);
    if (ret != kStatus_HAL_MultiCoreSuccess)
    {
        hal_free(dev_priv);
        return ret;
    }

    /* Create RPMSG queue for receiving messages from the remote core.
     * The queue is used to buffer incoming RPMSG messages. */
    dev_priv->rpmsg_queue = rpmsg_queue_create(dev_priv->rpmsg_inst);
    if (dev_priv->rpmsg_queue == NULL)
    {
        HAL_LOGE("RPMSG queue creation failed\r\n");
        hal_free(dev_priv);
        return kStatus_HAL_MultiCoreError;
    }

    dev_priv->rpmsg_ept = rpmsg_lite_create_ept(dev_priv->rpmsg_inst,
                                                dev_priv->local_rpmsg_addr,
                                                rpmsg_queue_rx_cb,
                                                dev_priv->rpmsg_queue);

    if (dev_priv->rpmsg_ept == NULL)
    {
        HAL_LOGE("RPMSG endpoint creation failed\r\n");
        rpmsg_queue_destroy(dev_priv->rpmsg_inst, dev_priv->rpmsg_queue);
        hal_free(dev_priv);
        return kStatus_HAL_MultiCoreError;
    }

    if (type == kHAL_MultiCoreDevTypeSource)
    {
        /* Wait until the secondary core endpoint is ready. */
        ret = wait_for_remote_ready(dev_priv->remote_event_data, 10000);
        if (ret != kStatus_HAL_MultiCoreSuccess)
        {
            HAL_LOGE("Failed to wait for remote core ready\r\n");
            return kStatus_HAL_MultiCoreError;
        }
        HAL_LOGI("Remote RPMSG endpoint is up\r\n");
        dev_priv->remote_ready = true;
    }
    else
    {
        /* Send ready event to the remote core */
        send_remote_ready_event(dev_priv->remote_event_data);
        HAL_LOGI("Sent ready event to remote core\r\n");
    }

    HAL_LOGD("--HAL_MultiCoreDev_Init\r\n");
    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Getbufdesc(const multicore_dev_t *dev, void *io_desc, mpp_memory_policy_t prev_mem_policy)
{
    HAL_LOGD("++HAL_MultiCoreDev_Getbufdesc\r\n");
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;
    mc_message_t send_msg, recv_msg;
    uint32_t msg_size;
    int32_t rpmsg_ret;
    uint32_t remote_addr;
    io_desc_t *io = (io_desc_t *)io_desc;

    mc_dev_private_data_t *dev_priv = (mc_dev_private_data_t *) dev->data;

    if (dev_priv == NULL)
    {
        HAL_LOGE("Multicore device private data is NULL\r\n");
        return kStatus_HAL_MultiCoreError;
    }

    if (dev->config.dev_type == kHAL_MultiCoreDevTypeSource && dev_priv->remote_ready == false)
    {
        HAL_LOGE("Remote core is not ready\r\n");
        return kStatus_HAL_MultiCoreError;
    }

    if (dev->config.dev_type == kHAL_MultiCoreDevTypeSource)
    {
        /* Source device - request buffer descriptor from remote core */
        send_msg.header.msg_type = MC_MSG_TYPE_BUF_DESC_REQ;
        rpmsg_ret = rpmsg_lite_send(dev_priv->rpmsg_inst,
                                dev_priv->rpmsg_ept,
                                dev_priv->remote_rpmsg_addr,
                                (char *)&send_msg,
                                sizeof(mc_message_t),
                                RL_DONT_BLOCK);

        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Got error %d while trying to send MC_MSG_TYPE_BUF_DESC_REQ: %d\r\n", rpmsg_ret);
            return kStatus_HAL_MultiCoreError;
        }

        /* Receive buffer descriptor response from remote core */
        rpmsg_ret = rpmsg_queue_recv(dev_priv->rpmsg_inst,
                                dev_priv->rpmsg_queue,
                                &remote_addr,
                                (char *)&recv_msg,
                                sizeof(mc_message_t),
                                &msg_size,
                                RL_BLOCK);

        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Failed to receive buffer descriptor response: %d\r\n", rpmsg_ret);
            return kStatus_HAL_MultiCoreError;
        }

        if (remote_addr != dev_priv->remote_rpmsg_addr)
        {
            HAL_LOGE("Received message from unexpected remote address %d\r\n", remote_addr);
            return kStatus_HAL_MultiCoreError;
        }

        if (recv_msg.header.msg_type != MC_MSG_TYPE_BUF_DESC_RESP)
        {
            HAL_LOGE("Received unexpected message type: %d\r\n", recv_msg.header.msg_type);
            return kStatus_HAL_MultiCoreError;
        }

        if ((recv_msg.payload.buff_desc_resp.cnt == 0) || (recv_msg.payload.buff_desc_resp.cnt > MAX_OUTPUT_PORTS))
        {
            HAL_LOGE("Wrong number of buffer descriptors received %d\r\n", recv_msg.payload.buff_desc_resp.cnt);
            return kStatus_HAL_MultiCoreError;
        }

        /* Debug prints for received buffer descriptor response */
        HAL_LOGD("Received buffer descriptor response:\r\n");
        HAL_LOGD("\tBuffer count: %d\r\n", recv_msg.payload.buff_desc_resp.cnt);
        HAL_LOGD("\tPrevious memory policy: %d\r\n", recv_msg.payload.buff_desc_resp.prev_mem_policy);
        for (uint32_t i = 0; i < recv_msg.payload.buff_desc_resp.cnt; i++)
        {
            HAL_LOGD("\tBuffer[%d]:\r\n", i);
            HAL_LOGD("\t\tformat: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->format);
            HAL_LOGD("\t\twidth: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->width);
            HAL_LOGD("\t\theight: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->height);
            HAL_LOGD("\t\tstripe_num: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->stripe_num);
            HAL_LOGD("\t\tcompressed_size: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->compressed_size);
            HAL_LOGD("\t\thw_req_cons.stride: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->hw_req_cons.stride);
            HAL_LOGD("\t\thw_req_cons.nb_lines: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->hw_req_cons.nb_lines);
            HAL_LOGD("\t\thw_req_cons.alignment: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->hw_req_cons.alignment);
            HAL_LOGD("\t\thw_req_cons.cacheable: %d\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->hw_req_cons.cacheable);
            HAL_LOGD("\t\thw_req_cons.heap_p: %p\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->hw_req_cons.heap_p);
            HAL_LOGD("\t\thw_req_cons.addr: %p\r\n", recv_msg.payload.buff_desc_resp.buf_desc[i]->hw_req_cons.addr);
        }

        /* set memory policy */
        mpp_memory_policy_t remote_prev_mem_policy  = recv_msg.payload.buff_desc_resp.prev_mem_policy;
        if ((remote_prev_mem_policy == HAL_MEM_ALLOC_INPUT) || (remote_prev_mem_policy == HAL_MEM_ALLOC_NONE))
        {
            /* The element before the mc sink pair did not allocate output buffers 
             * in this case set the policy to ALLOC_NONE on our side to let the framework 
             * allocate the output buffer of mc source based on next element reuqirements 
             * In this case, the buf_desc_t selected will be sent by source device to the sink device 
             * at every data request message */
            io->mem_policy = HAL_MEM_ALLOC_NONE;
        }
        else
        {
            /* The element before the pari MC SINK has allocated the output buffers so we need to 
             * set our mem policy to ALLOC_OUTPUT to indicate that buffers are pre-allocated */
            io->mem_policy = HAL_MEM_ALLOC_OUTPUT;
        }
        HAL_LOGD("Memory policy for MC SRC set to: %d\r\n", io->mem_policy);
        io->nb_out_buf = recv_msg.payload.buff_desc_resp.cnt;
        dev_priv->nb_buffers = recv_msg.payload.buff_desc_resp.cnt;
        buf_desc_t *buf;
        for (uint32_t i = 0; i < recv_msg.payload.buff_desc_resp.cnt; i++)
        {
            buf = (buf_desc_t *) hal_malloc(sizeof(buf_desc_t));
            if (buf == NULL)
            {
                HAL_LOGE("Failed to allocate memory for buffer descriptor\r\n");
                // Free previously allocated buffers
                for (uint32_t j = 0; j < i; j++)
                {
                    if (io->out_buf[j]) hal_free(io->out_buf[j]);
                    io->out_buf[j] = NULL;
                }
                return kStatus_HAL_MultiCoreError;
            }
            memset(buf, 0, sizeof(buf_desc_t));
            /* Set output buffer parameters */
            buf->format = recv_msg.payload.buff_desc_resp.buf_desc[i]->format;
            buf->width = recv_msg.payload.buff_desc_resp.buf_desc[i]->width;
            buf->height = recv_msg.payload.buff_desc_resp.buf_desc[i]->height;
            if (recv_msg.payload.buff_desc_resp.buf_desc[i]->stripe_num != 0)
            {
                HAL_LOGE("Stripe mode is not supported\r\n");
                hal_free(buf);
                return kStatus_HAL_MultiCoreError;
            }
            buf->stripe_num = 0;
            buf->compressed_size =recv_msg.payload.buff_desc_resp.buf_desc[i]->compressed_size;

            /* Set producer requirements */
            memcpy((void *)&buf->hw_req_prod, (void *)&recv_msg.payload.buff_desc_resp.buf_desc[i]->hw_req_cons, sizeof(hw_buf_desc_t));

            /* Store buffer descriptor in output buffer array */
            io->out_buf[i] = buf;
        }
    }
    else
    {
        /* Sink device - wait for buffer descriptor request from remote core */
        rpmsg_ret = rpmsg_queue_recv(dev_priv->rpmsg_inst,
                        dev_priv->rpmsg_queue,
                        &remote_addr,
                        (char *)&recv_msg,
                        sizeof(mc_message_t),
                        &msg_size,
                        RL_BLOCK);

        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Failed to receive buffer descriptor request: %d\r\n", rpmsg_ret);
            return kStatus_HAL_MultiCoreError;
        }

        if (remote_addr != dev_priv->remote_rpmsg_addr)
        {
            HAL_LOGE("Received message from unexpected remote address %d\r\n", remote_addr);
            return kStatus_HAL_MultiCoreError;
        }

        if (recv_msg.header.msg_type != MC_MSG_TYPE_BUF_DESC_REQ)
        {
            HAL_LOGE("Received unexpected message type: %d\r\n", recv_msg.header.msg_type);
            return kStatus_HAL_MultiCoreError;
        }

        /* Prepare buffer descriptor response */
        if ((prev_mem_policy == HAL_MEM_ALLOC_OUTPUT) || (prev_mem_policy == HAL_MEM_ALLOC_BOTH))
        {
            /* Set the memory policy to none to use the buffers allocated by previous element */
            io->mem_policy = HAL_MEM_ALLOC_NONE;
        }
        else
        {
            /* Set the memory policy to ALLOC_INPUT to prevent the framework to allocate a new buffer 
             * We will use the buffer allocated on mc source side */
            io->mem_policy = HAL_MEM_ALLOC_INPUT;
        }
        HAL_LOGD("Memory policy for MC SINK set to: %d\r\n", io->mem_policy);
        send_msg.header.msg_type = MC_MSG_TYPE_BUF_DESC_RESP;
        if (io->nb_in_buf == 0 || io->nb_in_buf > MAX_OUTPUT_PORTS)
        {
            HAL_LOGE("Invalid number of input buffers: %d\r\n", io->nb_in_buf);
            HAL_LOGE("The number of input buffers must not be greater than \r\n\tmax suported output buffers for pair mc source elem\r\n");
            return kStatus_HAL_MultiCoreError;
        }
        send_msg.payload.buff_desc_resp.cnt = io->nb_in_buf;
        send_msg.payload.buff_desc_resp.prev_mem_policy = prev_mem_policy;
        dev_priv->nb_buffers = io->nb_in_buf;
        for (uint32_t i = 0; i < io->nb_in_buf; i++)
        {
            if (io->in_buf[i] == NULL)
            {
                HAL_LOGE("Input buffer %d is not allocated\r\n", i);
                return kStatus_HAL_MultiCoreError;
            }
            io->in_buf[i]->hw_req_cons.stride = dev->config.buf_config[i].pitch;
            io->in_buf[i]->hw_req_cons.nb_lines = dev->config.buf_config[i].height;
            io->in_buf[i]->hw_req_cons.alignment = HAL_MC_MEM_ALIGN;
            io->in_buf[i]->hw_req_cons.cacheable = true;
            /* Do not allocate here even though we are setting the 
             * HAL_MEM_ALLOC_INPUT policy; we will get the actuall 
             * address from the source element side at every data req */
            io->in_buf[i]->hw_req_cons.heap_p = NULL;
            io->in_buf[i]->hw_req_cons.addr = NULL;
            send_msg.payload.buff_desc_resp.buf_desc[i] = io->in_buf[i];
        }

        /* Send buffer descriptor response */
        rpmsg_ret = rpmsg_lite_send(dev_priv->rpmsg_inst,
                                dev_priv->rpmsg_ept,
                                dev_priv->remote_rpmsg_addr,
                                (char *)&send_msg,
                                sizeof(mc_message_t),
                                RL_DONT_BLOCK);

        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Got error %d while trying to send MC_MSG_TYPE_BUF_DESC_RESP: %d\r\n", rpmsg_ret);
            return kStatus_HAL_MultiCoreError;
        }
    }

    HAL_LOGD("--HAL_MultiCoreDev_Getbufdesc\r\n");
    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Deinit(multicore_dev_t *dev)
{
    HAL_LOGD("++HAL_MultiCoreDev_Deinit\r\n");
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    if (dev == NULL)
    {
        HAL_LOGE("Device is NULL\r\n");
        return kStatus_HAL_MultiCoreError;
    }

    mc_dev_private_data_t *dev_priv = (mc_dev_private_data_t *)dev->data;
    if (dev_priv == NULL)
    {
        HAL_LOGE("Multicore device private data is NULL\r\n");
        return kStatus_HAL_MultiCoreError;
    }

    /* Destroy RPMSG endpoint */
    if (dev_priv->rpmsg_ept != NULL)
    {
        int32_t rpmsg_ret = rpmsg_lite_destroy_ept(dev_priv->rpmsg_inst, dev_priv->rpmsg_ept);
        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Failed to destroy RPMSG endpoint: %d\r\n", rpmsg_ret);
            ret = kStatus_HAL_MultiCoreError;
        }
        dev_priv->rpmsg_ept = NULL;
    }

    /* Destroy RPMSG queue */
    if (dev_priv->rpmsg_queue != NULL)
    {
        int32_t rpmsg_ret = rpmsg_queue_destroy(dev_priv->rpmsg_inst, dev_priv->rpmsg_queue);
        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Failed to destroy RPMSG queue: %d\r\n", rpmsg_ret);
            ret = kStatus_HAL_MultiCoreError;
        }
        dev_priv->rpmsg_queue = NULL;
    }

    /* Destroy mutex if it exists */
    if (dev_priv->mutex != NULL)
        hal_mutex_remove(dev_priv->mutex);

    /* Remove device from the global device list */
    hal_mutex_lock(mc_global_mutex);
    for (int i = 0; i < HAL_MC_MAX_ENDPOINTS; i++)
    {
        if (mc_dev_list[i] == dev_priv)
        {
            mc_dev_list[i] = NULL;
            break;
        }
    }
    hal_mutex_unlock(mc_global_mutex);

    /* Remove entry from remote event data database */
    for (int i = 0; i < HAL_MC_MAX_ENDPOINTS; i++)
    {
        if (mc_remote_event_data[i].mcmgr_event_data == dev_priv->remote_event_data)
        {
            mc_remote_event_data[i].mcmgr_event_data = MPP_MCMGR_EVENT_DATA_INVALID;
            mc_remote_event_data[i].remote_ready = false;
            break;
        }
    }

    /* Free private data structure */
    hal_free(dev_priv);
    dev->data = NULL;

    HAL_LOGD("--HAL_MultiCoreDev_Deinit\r\n");
    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Start(multicore_dev_t *dev)
{
    HAL_LOGD("++HAL_MultiCoreDev_Start\r\n");
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    /* Clear internal counters */
    dev->config.crt_req_cnt = 0;
    dev->config.initial_enqueue_done = false;
    memset(dev->config.requested, 0, sizeof(dev->config.requested));
    memset(dev->config.enqueued, 0, sizeof(dev->config.enqueued));
    memset(dev->config.buff_desc, 0, sizeof(dev->config.buff_desc));

    HAL_LOGD("--HAL_MultiCoreDev_Start\r\n");
    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Stop(multicore_dev_t *dev)
{
    HAL_LOGD("++HAL_MultiCoreDev_Stop\r\n");
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    mc_dev_private_data_t *dev_priv = (mc_dev_private_data_t *)dev->data;

    /* Flush message queue */
    if (dev_priv != NULL && dev_priv->rpmsg_queue != NULL)
    {
        mc_message_t flush_msg;
        uint32_t msg_size;
        uint32_t remote_addr;
        int32_t rpmsg_ret;

        /* Drain all pending messages from the queue */
        do {
            rpmsg_ret = rpmsg_queue_recv(dev_priv->rpmsg_inst,
                                        dev_priv->rpmsg_queue,
                                        &remote_addr,
                                        (char *)&flush_msg,
                                        sizeof(mc_message_t),
                                        &msg_size,
                                        RL_DONT_BLOCK);
            if (rpmsg_ret == RL_SUCCESS)
            {
                HAL_LOGD("Flushed message type %d from queue\r\n", flush_msg.header.msg_type);
            }
        } while (rpmsg_ret == RL_SUCCESS);
    }

    /* Clear internal counters */
    dev->config.crt_req_cnt = 0;
    dev->config.initial_enqueue_done = false;
    memset(dev->config.requested, 0, sizeof(dev->config.requested));
    memset(dev->config.enqueued, 0, sizeof(dev->config.enqueued));
    memset(dev->config.buff_desc, 0, sizeof(dev->config.buff_desc));

    HAL_LOGD("--HAL_MultiCoreDev_Stop\r\n");
    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Lock(const multicore_dev_t *dev)
{
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    mc_dev_private_data_t *dev_priv = (mc_dev_private_data_t *)dev->data;

    /* Lock the mc device mutex to ensure thread-safe access */
    int status = hal_mutex_lock(dev_priv->mutex);
    if (status != MPP_SUCCESS)
    {
        HAL_LOGE("Failed to lock mc device mutex (error %d)\r\n", status);
        return kStatus_HAL_MultiCoreError;
    }

    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Unlock(const multicore_dev_t *dev)
{
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;

    mc_dev_private_data_t *dev_priv = (mc_dev_private_data_t *)dev->data;

    /* Lock the mc device mutex to ensure thread-safe access */
    int status = hal_mutex_unlock(dev_priv->mutex);
    if (status != MPP_SUCCESS)
    {
        HAL_LOGE("Failed to unlock mc device mutex (error %d)\r\n", status);
        return kStatus_HAL_MultiCoreError;
    }

    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Dequeue(const multicore_dev_t *dev, void **data, uint32_t *compressed_size, uint32_t *stripe, uint32_t *frame_id)
{
    HAL_LOGD("++HAL_MultiCoreDev_Dequeue\r\n");
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;
    mc_message_t recv_msg;
    uint32_t msg_size;
    int32_t rpmsg_ret;
    uint32_t remote_addr;

    mc_dev_private_data_t *dev_priv = (mc_dev_private_data_t *)dev->data;

    if (dev->config.dev_type ==  kHAL_MultiCoreDevTypeSink)
    {
        /* Dequeue message from RPMSG queue (wait for data request message) */
        rpmsg_ret = rpmsg_queue_recv(dev_priv->rpmsg_inst,
                        dev_priv->rpmsg_queue,
                        &remote_addr,
                        (char *)&recv_msg,
                        sizeof(mc_message_t),
                        &msg_size,
                        RL_BLOCK);

        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Failed to receive buffer descriptor request: %d\r\n", rpmsg_ret);
            return kStatus_HAL_MultiCoreError;
        }

        if (remote_addr != dev_priv->remote_rpmsg_addr)
        {
            HAL_LOGE("Received message from unexpected remote address %d\r\n", remote_addr);
            return kStatus_HAL_MultiCoreError;
        }

        if (recv_msg.header.msg_type != MC_MSG_TYPE_DATA_REQ)
        {
            HAL_LOGE("Received unexpected message type: %d\r\n", recv_msg.header.msg_type);
            return kStatus_HAL_MultiCoreError;
        }

        /* Debug prints for received data request message */
        HAL_LOGD("Received data request message:\r\n");
        for (int i = 0; i < dev_priv->nb_buffers; i++)
        {
            HAL_LOGD("\tBuffer[%d]:\r\n", i);
            HAL_LOGD("\t\taddr: %p\r\n", recv_msg.payload.data_req.addr[i]);
        }

        for (int i = 0; i < dev_priv->nb_buffers; i++)
        {
            data[i] = recv_msg.payload.data_req.addr[i];
            HAL_DCACHE_InvalidateByRange((uint32_t) recv_msg.payload.data_req.addr[i], sizeof(hw_buf_desc_t));
        }
    }
    else
    {
        /* Dequeue message from RPMSG queue */
        rpmsg_ret = rpmsg_queue_recv(dev_priv->rpmsg_inst,
                        dev_priv->rpmsg_queue,
                        &remote_addr,
                        (char *)&recv_msg,
                        sizeof(mc_message_t),
                        &msg_size,
                        RL_BLOCK);

        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Failed to receive buffer descriptor request: %d\r\n", rpmsg_ret);
            return kStatus_HAL_MultiCoreError;
        }

        if (remote_addr != dev_priv->remote_rpmsg_addr)
        {
            HAL_LOGE("Received message from unexpected remote address %d\r\n", remote_addr);
            return kStatus_HAL_MultiCoreError;
        }

        if (recv_msg.header.msg_type != MC_MSG_TYPE_DATA_RESP)
        {
            HAL_LOGE("Received unexpected message type: %d\r\n", recv_msg.header.msg_type);
            return kStatus_HAL_MultiCoreError;
        }

        /* Debug prints for received data response message */
        HAL_LOGD("Received data response message:\r\n");
        for (int i = 0; i < dev_priv->nb_buffers; i++)
        {
            HAL_LOGD("\tBuffer[%d]:\r\n", i);
            HAL_LOGD("\t\taddr: %p\r\n", recv_msg.payload.data_rsp.addr[i]);
            HAL_LOGD("\t\tsize: %d\r\n", recv_msg.payload.data_rsp.size[i]);
            HAL_LOGD("\t\tframe_id: %d\r\n", recv_msg.payload.data_rsp.frame_id[i]);
        }

        for (int i = 0; i < dev_priv->nb_buffers; i++)
        {
            if (dev->config.enqueued[i] != true)
                continue;

            if (recv_msg.payload.data_rsp.addr[i] == NULL)
            {
                HAL_LOGE("Received message with NULL data address for output buffer %d\r\n", i);
                return kStatus_HAL_MultiCoreError;
            }

            if (recv_msg.payload.data_rsp.addr[i] != dev->config.buff_desc[i])
            {
                HAL_LOGE("Received message with mismatched data address for output buffer %d\r\n", i);
                return kStatus_HAL_MultiCoreError;
            }

            stripe[i] = 0;
            data[i] = recv_msg.payload.data_rsp.addr[i];
            frame_id[i] = recv_msg.payload.data_rsp.frame_id[i];
            if (dev->config.buf_config[i].format == MPP_PIXEL_JPEG)
            {
                if (recv_msg.payload.data_rsp.size[i] <= 0)
                {
                    HAL_LOGE("Received message with invalid compressed size: %d\r\n", recv_msg.payload.data_rsp.size[i]);
                    return kStatus_HAL_MultiCoreError;
                }
                compressed_size[i] = recv_msg.payload.data_rsp.size[i];
            }
            else
                compressed_size[i] = 0;
        }
    }

    HAL_LOGD("--HAL_MultiCoreDev_Dequeue\r\n");
    return ret;
}

hal_mc_status_t HAL_MultiCoreDev_Enqueue(const multicore_dev_t *dev, void **data, uint32_t *compressed_size, uint32_t *stripe, uint32_t *frame_id)
{
    HAL_LOGD("++HAL_MultiCoreDev_Enqueue\r\n");
    hal_mc_status_t ret = kStatus_HAL_MultiCoreSuccess;
    mc_message_t send_msg;
    int32_t rpmsg_ret;

    mc_dev_private_data_t *dev_priv = (mc_dev_private_data_t *)dev->data;

    if (dev->config.dev_type ==  kHAL_MultiCoreDevTypeSource)
    {
        /* Send data requuest message to remote core */
        uint32_t req_cnt = 0;
        send_msg.header.msg_type = MC_MSG_TYPE_DATA_REQ;
        for (int i = 0; i < dev_priv->nb_buffers; i++)
        {
            if (dev->config.requested[i] == true)
            {
                req_cnt++;
                send_msg.payload.data_req.addr[i] = dev->config.buff_desc[i];
                HAL_DCACHE_CleanByRange((uint32_t) dev->config.buff_desc[i], sizeof(hw_buf_desc_t));
            }
            else
            {
                send_msg.payload.data_req.addr[i] = NULL;
            }
        }
        if (req_cnt == 0)
        {
            HAL_LOGE("No buffers requested, but enqueue called. This is not supported\r\n");
            return kStatus_HAL_MultiCoreError;
        }

        /* Debug prints for data request message to be sent */
        HAL_LOGD("Sending data request message:\r\n");
        for (int i = 0; i < dev_priv->nb_buffers; i++)
        {
            HAL_LOGD("\tBuffer[%d]:\r\n", i);
            HAL_LOGD("\t\trequested: %d\r\n", dev->config.requested[i]);
            HAL_LOGD("\t\taddr: %p\r\n", dev->config.requested[i] ? dev->config.buff_desc[i] : NULL);
        }

        rpmsg_ret = rpmsg_lite_send(dev_priv->rpmsg_inst,
                            dev_priv->rpmsg_ept,
                            dev_priv->remote_rpmsg_addr,
                            (char *)&send_msg,
                            sizeof(mc_message_t),
                            RL_DONT_BLOCK);

        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Got error %d while trying to send MC_MSG_TYPE_DATA_REQ: %d\r\n", rpmsg_ret);
            return kStatus_HAL_MultiCoreError;
        }
    }
    else
    {
        /* Prepare and send data response message */
        send_msg.header.msg_type = MC_MSG_TYPE_DATA_RESP;
        for (int i = 0; i < dev_priv->nb_buffers; i++)
        {
            if (data[i] == NULL)
            {
                HAL_LOGD("Data pointer is NULL for buffer %d (buffer not req)\r\n", i);
                continue;
            }

            if (stripe[i] != 0)
            {
                HAL_LOGE("Stripe mode not supported\r\n");
                return kStatus_HAL_MultiCoreError;
            }

            if (dev->config.buff_desc[i] == NULL)
            {
                HAL_LOGE("Buffer descriptor is NULL for buffer %d\r\n", i);
                return kStatus_HAL_MultiCoreError;
            }

            if (data[i] != dev->config.buff_desc[i])
            {
                /* We need to copy the data to the right address before sending the message to remote core */
                /* Copy it line by line to handle potential stride differences */
                void *src_addr = ((hw_buf_desc_t *)data[i])->addr;
                void *dst_addr = ((hw_buf_desc_t *)dev->config.buff_desc[i])->addr;
                uint32_t src_stride = ((hw_buf_desc_t *)data[i])->stride;
                uint32_t dst_stride = ((hw_buf_desc_t *)dev->config.buff_desc[i])->stride;
                if (((hw_buf_desc_t *)data[i])->nb_lines == 0)
                {
                    HAL_LOGE("nb of lines is not set for buffer %d\r\n", i);
                    return kStatus_HAL_MultiCoreError;
                }
                if (compressed_size[i] != 0)
                {
                    /* Full copy of compressed data */
                    memcpy(dst_addr, src_addr, compressed_size[i]);
                }
                else
                {
                    for (int j = 0; j < ((hw_buf_desc_t *)data[i])->nb_lines; j++)
                    {
                        memcpy((uint8_t *)dst_addr + (j * dst_stride),
                            (uint8_t *)src_addr + (j * src_stride),
                            src_stride);
                    }
                }
            }

            send_msg.payload.data_rsp.addr[i] = dev->config.buff_desc[i];
            send_msg.payload.data_rsp.size[i] = compressed_size[i];
            send_msg.payload.data_rsp.frame_id[i] = frame_id[i];
        }

         /* Debug prints for data response message to be sent */
        HAL_LOGD("Sending data response message:\r\n");
        for (int i = 0; i < dev_priv->nb_buffers; i++)
        {
            HAL_LOGD("\tBuffer[%d]:\r\n", i);
            HAL_LOGD("\t\taddr: %p\r\n", send_msg.payload.data_rsp.addr[i]);
            HAL_LOGD("\t\tsize: %d\r\n", send_msg.payload.data_rsp.size[i]);
            HAL_LOGD("\t\tframe_id: %d\r\n", send_msg.payload.data_rsp.frame_id[i]);
        }

        rpmsg_ret = rpmsg_lite_send(dev_priv->rpmsg_inst,
                                dev_priv->rpmsg_ept,
                                dev_priv->remote_rpmsg_addr,
                                (char *)&send_msg,
                                sizeof(mc_message_t),
                                RL_DONT_BLOCK);

        if (rpmsg_ret != RL_SUCCESS)
        {
            HAL_LOGE("Got error %d while trying to send MC_MSG_TYPE_DATA_RESP\r\n", rpmsg_ret);
            return kStatus_HAL_MultiCoreError;
        }
    }

    HAL_LOGD("--HAL_MultiCoreDev_Enqueue\r\n");
    return ret;
}

const static mc_dev_operator_t multi_core_dev_ops = {
    .init         = HAL_MultiCoreDev_Init,
    .deinit       = HAL_MultiCoreDev_Deinit,
    .start        = HAL_MultiCoreDev_Start,
    .stop         = HAL_MultiCoreDev_Stop,
    .enqueue      = HAL_MultiCoreDev_Enqueue,
    .dequeue      = HAL_MultiCoreDev_Dequeue,
    .lock         = HAL_MultiCoreDev_Lock,
    .unlock       = HAL_MultiCoreDev_Unlock,
    .get_buf_desc = HAL_MultiCoreDev_Getbufdesc
};

int HAL_MultiCoreDev_setup(const char *name, multicore_dev_t *dev)
{
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';
    dev->ops = &multi_core_dev_ops;

    return 0;
}

#else /* (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1) */

int HAL_MultiCoreDev_setup(const char *name, multicore_dev_t *dev)
{
    HAL_LOGE("HAL multicore not enabled\r\n");
    return -1;
}

#endif /* (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1) */
