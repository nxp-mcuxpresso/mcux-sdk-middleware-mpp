/*
 * Copyright 2025 NXP.
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
 * @brief HAL camera driver implementation for USB camera stack running on a different core
 */

#include "mpp_config.h"
#include "hal_debug.h"
#include "hal_camera_dev.h"
#include "hal_camera_shared.h"

#if (defined HAL_ENABLE_CAMERA) && (HAL_ENABLE_VIRTUAL_CAMERA == 1)
#include <stdlib.h>

#include "board.h"

#include "hal.h"
#include "hal_utils.h"
#include "hal_os.h"

#include "app.h"

#include "rpmsg_lite.h"
#include "rpmsg_queue.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define CAMERA_NAME "Virtual_USB_cam"

/** @brief Total buffer size allocated for RGB camera data in the virtual camera system (96 KB) */
#define VIRTUAL_CAMERA_RGB_BUFFER_SIZE (96*1024)
/** @brief Total buffer size allocated for IR (Infrared) camera data in the virtual camera system (96 KB) */
#define VIRTUAL_CAMERA_IR_BUFFER_SIZE  (96*1024)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/**
 * @brief Private data structure for dual camera device management
 * 
 * This structure contains all the private data needed to manage a virtual USB camera
 * device that supports dual streams (RGB and IR) via RPMSG inter-core communication.
 */
_Static_assert(NUM_STREAMS >= 2, "NUM_STREAMS must be at least 2 for dual camera support");

typedef struct
{
    mpp_camera_stream_type stream_type;                 /*!< Type of camera stream (RGB, IR, etc.) */
    void **stream_addr;                                   /*!< Address where to store the pointer to the stream buffer */
    int *stream_size;                                     /*!< Size of each stream buffer */
} stream_dequeue_req_t;

typedef struct
{
    unsigned char *stream_addr[NUM_STREAMS];              /*!< Buffer addresses for each camera stream (RGB/IR) */
    uint32_t crt_get_buf_desc_idx;                        /*!< Current index for buffer descriptor retrieval operations */
    uint32_t crt_dequeue_stream_idx;                      /*!< Current index for stream dequeue operations */
    uint32_t dequeued_streams;                            /*!< Number of streams that have been dequeued */
    uint32_t enqueued_streams;                            /*!< Number of streams that have been enqueued */
    uint32_t messages_received;                           /*!< Number of messages received for tracking communication state */
    uint32_t messages_sent;                               /*! Number of messages sent for tracking communication state */
    bool stream_enqueued[NUM_STREAMS];                    /*!< Flag array tracking enqueue status for each stream */
    virtual_usb_cam_msg_type_e enq_msg_type[NUM_STREAMS]; /*!< Last enqueued message type for state tracking */
    stream_dequeue_req_t stream_dequeue_req[NUM_STREAMS]; /*!< Dequeue request details for each stream */
    uint32_t dequeue_stream_sizes[NUM_STREAMS];           /*!< Buffer sizes for each camera stream */
    struct rpmsg_lite_instance *rpmsg_inst;               /*!< RPMSG instance for inter-core communication */
    struct rpmsg_lite_endpoint *rpmsg_ept;                /*!< RPMSG endpoint for this camera device instance */
    rpmsg_queue_handle rpmsg_queue;                       /*!< RPMSG message queue handle for receiving messages */
    uint32_t rpmsg_remote_addr;                           /*!< Remote endpoint address of the USB camera core */
    uint32_t rpmsg_local_addr;                            /*!< Local endpoint address for this camera instance */
    hal_mutex_t mutex;                                    /*!< Mutex to lock the device during an operation */
} dual_camera_dev_private_data_t;

/**
 * @brief Handle structure for virtual camera instances
 * 
 * This structure manages shared resources and state for virtual camera instances.
 * Since multiple virtual camera elements can be instantiated, this handle ensures
 * proper coordination and resource sharing between them.
 */
typedef struct _virtual_camera_handle
{
    volatile bool init_done;                     /*!< Initialization completion flag (false=not init, true=ready) */
    struct rpmsg_lite_instance *rpmsg_instance;  /*!< RPMSG instance for inter-core communication */
    volatile uint32_t rpmsg_remote_addr;         /*!< Remote endpoint address of USB camera core */
    volatile uint32_t n_instances;               /*!< Number of active virtual camera instances */
    volatile uint16_t *mcmgr_event_data_p;       /*!< Pointer to MCMGR event data for synchronization */
} virtual_camera_handle_t;

static virtual_camera_handle_t s_virt_cam_handle = {0};

/**
 * @brief RGB camera buffer for virtual USB camera
 * 
 * This buffer is used to store RGB camera data received from the USB camera
 * running on a different core via RPMSG communication. The buffer is placed
 * in shared memory section to allow inter-core access.

 * Total  size: VIRTUAL_CAMERA_RGB_BUFFER_SIZE (96 KB)
 * Data alignment: CAMERA_DEV_BUFFER_ALIGN (64 bytes)
 * Memory section: .noinit.$cam_buff_sh_mem (shared memory, not initialized)
 */
uint8_t virtual_usb_cam_rgb_buff[VIRTUAL_CAMERA_RGB_BUFFER_SIZE] __attribute__((section(".noinit.$cam_buff_sh_mem"))) __attribute__((aligned(CAMERA_DEV_BUFFER_ALIGN)));

/**
 * @brief IR camera buffer for virtual USB camera
 * 
 * This buffer is used to store IR (Infrared) camera data received from the USB 
 * camera running on a different core via RPMSG communication. The buffer is 
 * placed in shared memory section to allow inter-core access.

 * Total structure size: VIRTUAL_CAMERA_IR_BUFFER_SIZE (96 KB)
 * Data alignment: CAMERA_DEV_BUFFER_ALIGN (64 bytes)
 * Memory section: .noinit.$cam_buff_sh_mem (shared memory, not initialized)
 */
uint8_t virtual_usb_cam_ir_buff[VIRTUAL_CAMERA_IR_BUFFER_SIZE] __attribute__((section(".noinit.$cam_buff_sh_mem"))) __attribute__((aligned(CAMERA_DEV_BUFFER_ALIGN)));

/**
 * Add a global mutex for protecting shared handle
 */
static hal_mutex_t s_global_mutex = NULL;

/*******************************************************************************
 * Code
 ******************************************************************************/

// Initialize global mutex (call this during system init)
static hal_camera_status_t init_global_resources(void)
{
    hal_atomic_enter();
    if (s_global_mutex == NULL)
    {
        hal_mutex_create(&s_global_mutex);
        if (s_global_mutex == NULL)
        {
            hal_atomic_exit();
            return kStatus_HAL_CameraError;
        }
    }
    hal_atomic_exit();
    return kStatus_HAL_CameraSuccess;
}

/**
 * @brief Enqueues camera stream requests to the remote USB camera core
 * 
 * This function prepares and sends stream capture requests to the USB camera running
 * on a remote core via RPMSG inter-core communication. It supports both single stream
 * (RGB or IR) and dual stream (RGB+IR) capture modes.
 * 
 * The function performs the following operations:
 * 1. Validates the stream configuration and device state
 * 2. Constructs the appropriate RPMSG message based on active streams
 * 3. Sends the request to the remote USB camera core
 * 4. Updates internal tracking state for message and stream management
 * 
 * @param dev Pointer to the camera device structure containing configuration
 * @param stream_active_cfg Array indicating which streams are active for this request
 * 
 * @return kStatus_HAL_CameraSuccess on successful enqueue operation
 * @return kStatus_HAL_CameraError on validation failure or communication error
 * 
 * @note This function is thread-safe when called with proper device locking
 * @note The remote core must be initialized and ready to receive messages
 * @note Buffer addresses are shared memory locations accessible by both cores
 */
static hal_camera_status_t camera_dev_enqueue(const camera_dev_t *dev, bool *stream_active_cfg)
{
    int error = kStatus_HAL_CameraSuccess;
    uint32_t req_cnt = 0;
    virtual_usb_cam_msg_t msg;
    int32_t rpmsg_ret;

    if (dev == NULL)
    {
        HAL_LOGE("Camera dev pointer is NULL\r\n");
        return kStatus_HAL_CameraError;
    }

    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    if (stream_active_cfg == NULL)
    {
        HAL_LOGE("Stream active configuration pointer is NULL\r\n");
        return kStatus_HAL_CameraError;
    }

    /* Count the number of active streams requested for this enqueue operation */
    for (int i = 0; i < dev->config.n_streams; i++)
    {
        if (stream_active_cfg[i] == true)
            req_cnt++;
    }

    /* Validate that at least one stream is requested */
    if (!req_cnt)
    {
        HAL_LOGE("Enqueue called, but no stream previously requested\r\n");
        return kStatus_HAL_CameraError;
    }

    /* Prevent exceeding the maximum number of streams that can be enqueued simultaneously */
    if (dev_data->enqueued_streams + req_cnt > dev->config.n_streams)
    {
        HAL_LOGE("Too many streams requested for enqueue: %d (already enqueued %d)\r\n", req_cnt, dev_data->enqueued_streams);
        return kStatus_HAL_CameraError;
    }

    /* Enforce message limit to prevent overwhelming the remote core */
    if (dev_data->messages_sent >= dev->config.n_streams)
    {
        HAL_LOGE("Maximum message limit (%d) reached\r\n", dev->config.n_streams);
        return kStatus_HAL_CameraError;
    }

    /* Initialize the RPMSG message structure to ensure clean state */
    memset(&msg, 0, sizeof(virtual_usb_cam_msg_t));

    HAL_LOGD("Enqueuing %d streams\r\n", req_cnt);

    /* Handle single stream request - determine specific stream type and configure message */
    if (req_cnt == 1)
    {
        /* Find the active stream and configure the message accordingly */
        for (int i = 0; i < dev->config.n_streams; i++)
        {
            if (stream_active_cfg[i] == true)
            {
                /* Configure message based on the specific stream type */
                switch (dev->config.stream[i].type)
                {
                case RGB_STREAM:
                    /* Request RGB stream capture with shared memory buffer */
                    msg.msg_type = VIRT_USB_CAM_REQRGB;
                    msg.msg_payload.req.rgb_frame_addr = (uint32_t)virtual_usb_cam_rgb_buff;
                    msg.msg_payload.req.rgb_max_frame_size = VIRTUAL_CAMERA_RGB_BUFFER_SIZE;
                    break;

                case IR_STREAM:
                    /* Request IR stream capture with shared memory buffer */
                    msg.msg_type = VIRT_USB_CAM_REQIR;
                    msg.msg_payload.req.ir_frame_addr = (uint32_t)virtual_usb_cam_ir_buff;
                    msg.msg_payload.req.ir_max_frame_size = VIRTUAL_CAMERA_IR_BUFFER_SIZE;
                    break;

                default:
                    HAL_LOGE("Unsupportead camera stream type (%d)\r\n", dev->config.stream[i].type);
                    return kStatus_HAL_CameraError;
                    break;
                }
                break; /* Exit loop once active stream is found and configured */
            }
        }
    }
    else
    {
        /* Handle multiple stream request - configure for simultaneous RGB+IR capture */
        msg.msg_type = VIRT_USB_CAM_REQRGBIR;
        /* Set RGB buffer parameters for dual stream capture */
        msg.msg_payload.req.rgb_frame_addr = (uint32_t)virtual_usb_cam_rgb_buff;
        msg.msg_payload.req.rgb_max_frame_size = VIRTUAL_CAMERA_RGB_BUFFER_SIZE;
        /* Set IR buffer parameters for dual stream capture */
        msg.msg_payload.req.ir_frame_addr = (uint32_t)virtual_usb_cam_ir_buff;
        msg.msg_payload.req.ir_max_frame_size = VIRTUAL_CAMERA_IR_BUFFER_SIZE;
    }

    /* Set the user ID for message routing and identification */
    msg.user_id = MPP_USER_ID;

    /* Send the configured message to the remote USB camera core via RPMSG */
    rpmsg_ret = rpmsg_lite_send(dev_data->rpmsg_inst,
                                dev_data->rpmsg_ept,
                                dev_data->rpmsg_remote_addr,
                                (char *)&msg,
                                sizeof(virtual_usb_cam_msg_t),
                                RL_DONT_BLOCK);
    if (rpmsg_ret != RL_SUCCESS)
    {
        HAL_LOGE("Got error %d while trying to send msg to USB camera core\r\n", rpmsg_ret);
        return kStatus_HAL_CameraError;
    }

    /* Store the enqueued message type for tracking and validation during dequeue */
    dev_data->enq_msg_type[dev_data->messages_sent++] = msg.msg_type;

    /* Update internal state tracking for each newly enqueued stream */
    for (int i = 0; i < dev->config.n_streams; i++)
    {
        if (stream_active_cfg[i] == true)
        {
            /* Mark stream as enqueued and increment total enqueued count */
            dev_data->stream_enqueued[i] = true;
            dev_data->enqueued_streams++;
        }
    }

    return error;
}

/**
 * @brief Finds the next enqueued stream index in the camera device configuration
 * 
 * This function iterates through the camera streams starting from the current index
 * to find the next stream that has been enqueued for processing. It checks the
 * stream_enqueued flag array to determine which streams are currently queued.
 * 
 * @param dev Pointer to the camera device structure containing stream configuration
 * @param crt_active_stream_idx Pointer to current stream index (input/output parameter)
 *                              - Input: Starting index for search
 *                              - Output: Index of next enqueued stream if found
 * 
 * @return kStatus_HAL_CameraSuccess if an enqueued stream is found
 * @return kStatus_HAL_CameraError if no enqueued streams are found
 * 
 * @note The function modifies the crt_active_stream_idx parameter to point to the
 *       next enqueued stream index, or to n_streams if none found
 * @note This function is used during dequeue operations to process streams in order
 */
static hal_camera_status_t get_next_enqueued_stream_idx(const camera_dev_t *dev, uint32_t *crt_active_stream_idx)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    /* Iterate through streams starting from current index to find next enqueued stream */
    while ((*crt_active_stream_idx < dev->config.n_streams) && (dev_data->stream_enqueued[*crt_active_stream_idx] != true))
    {
        /* Move to next stream index if current stream is not enqueued */
        (*crt_active_stream_idx)++;
    }

    /* Check if we reached the end without finding an enqueued stream */
    if (*crt_active_stream_idx == dev->config.n_streams)
    {
        HAL_LOGE("No active stream found in configuration\r\n");
        return kStatus_HAL_CameraError;
    }

    /* Successfully found an enqueued stream at crt_active_stream_idx */
    return ret;
}

/**
 * @brief Finds the next active stream index in the camera device configuration
 * 
 * This function searches through the camera streams starting from the current index
 * to locate the next stream that is marked as active in the device configuration.
 * It examines the stream.active flag to determine stream availability.
 * 
 * @param dev Pointer to the camera device structure containing stream configuration
 * @param crt_active_stream_idx Pointer to current stream index (input/output parameter)
 *                              - Input: Starting index for search
 *                              - Output: Index of next active stream if found
 * 
 * @return kStatus_HAL_CameraSuccess if an active stream is found
 * @return kStatus_HAL_CameraError if no active streams are found
 * 
 * @note The function modifies the crt_active_stream_idx parameter to point to the
 *       next active stream index, or to n_streams if none found
 * @note This function is used during buffer descriptor operations and initialization
 * @note Active streams are those configured and enabled in the device configuration
 */
static hal_camera_status_t get_next_active_stream_idx(const camera_dev_t *dev, uint32_t *crt_active_stream_idx)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;

    /* Iterate through streams starting from current index to find next active stream */
    while ((*crt_active_stream_idx < dev->config.n_streams) && (dev->config.stream[*crt_active_stream_idx].active != true))
    {
        /* Move to next stream index if current stream is not active */
        (*crt_active_stream_idx)++;
    }

    /* Check if we reached the end without finding an active stream */
    if (*crt_active_stream_idx == dev->config.n_streams)
    {
        HAL_LOGE("No active stream found in configuration\r\n");
        return kStatus_HAL_CameraError;
    }

    /* Successfully found an active stream at crt_active_stream_idx */
    return ret;
}

/**
 * @brief Resets the enqueue/dequeue state for all camera streams
 * 
 * This function reinitializes all stream management state variables to their
 * default values, effectively clearing any pending operations and resetting
 * the camera device to a clean state. This is typically called during
 * initialization or when recovering from error conditions.
 * 
 * @param dev_data Pointer to the dual camera device private data structure
 * 
 * @note This function resets counters, clears enqueue flags, invalidates
 *       message types, and zeros out all stream-specific data structures
 */
static void reset_enqueue_dequeue_state(dual_camera_dev_private_data_t *dev_data)
{
    /* Reset the current dequeue stream index to start from the beginning */
    dev_data->crt_dequeue_stream_idx = 0;
    
    /* Clear the count of streams that have been dequeued */
    dev_data->dequeued_streams = 0;
    
    /* Clear the count of streams that have been enqueued */
    dev_data->enqueued_streams = 0;
    
    /* Reset the count of messages sent to the remote core */
    dev_data->messages_sent = 0;
    
    /* Reset the count of messages received from the remote core */
    dev_data->messages_received = 0;
    
    /* Loop through all available streams to reset per-stream state */
    for (int i = 0; i < NUM_STREAMS; i++)
    {
        /* Mark stream as not enqueued (clear enqueue flag) */
        dev_data->stream_enqueued[i] = false;
        
        /* Reset the enqueued message type to indicate no message */
        dev_data->enq_msg_type[i] = VIRT_USB_CAM_NOMSG;
        
        /* Clear the stored dequeue stream size */
        dev_data->dequeue_stream_sizes[i] = 0;
        
        /* Reset stream type to invalid value (NUM_STREAMS acts as invalid marker) */
        dev_data->stream_dequeue_req[i].stream_type = NUM_STREAMS;
        
        /* Clear the stream buffer address pointer */
        dev_data->stream_dequeue_req[i].stream_addr = NULL;
        
        /* Clear the stream size pointer */
        dev_data->stream_dequeue_req[i].stream_size = NULL;
    }
}

 /**
 * @brief Validates that a frame size does not exceed the allocated buffer capacity
 * 
 * This function performs bounds checking to ensure that incoming frame data from the
 * remote USB camera core will fit within the locally allocated buffer. This prevents
 * potential buffer overflows when copying frame data.
 * 
 * @param frame_size The size of the incoming frame data in bytes
 * @param max_size The maximum capacity of the allocated buffer in bytes
 * @param stream_name Human-readable name of the stream (e.g., "RGB", "IR") for error reporting
 * 
 * @return kStatus_HAL_CameraSuccess if frame size is within bounds
 * @return kStatus_HAL_CameraError if frame size exceeds buffer capacity
 * 
 * @note This validation is critical for preventing memory corruption when receiving
 *       variable-sized compressed data (e.g., JPEG frames) from the remote core
 */
static hal_camera_status_t validate_frame_size(uint32_t frame_size, uint32_t max_size, const char* stream_name)
{
    if (frame_size > max_size)
    {
        HAL_LOGE("%s frame size (%d) exceeds buffer capacity (%d)\r\n", stream_name, frame_size, max_size);
        return kStatus_HAL_CameraError;
    }
    return kStatus_HAL_CameraSuccess;
}

  /**
 * @brief Flushes all pending enqueued messages from the remote USB camera core
 * 
 * This function receives and discards all outstanding response messages from the remote
 * USB camera core that correspond to previously sent enqueue requests. It's used during
 * cleanup operations (like camera stop) to ensure the message queue is cleared and both
 * cores are synchronized before resetting the device state.
 * 
 * The function performs the following operations:
 * 1. Validates that there are pending messages to flush
 * 2. Iterates through all unprocessed sent messages
 * 3. Receives corresponding response messages from the remote core
 * 4. Validates message types and source addresses
 * 5. Resets the device state after successful flush
 * 
 * @param dev_data Pointer to the dual camera device private data structure
 * 
 * @return kStatus_HAL_CameraSuccess if all messages were successfully flushed
 * @return kStatus_HAL_CameraError if validation fails or communication error occurs
 * 
 * @note This function blocks until all pending messages are received
 * @note The device state is always reset at the end, regardless of success/failure
 * @note Used primarily during camera stop operations to prevent message queue overflow
 */
static hal_camera_status_t flush_enqueued_messages(dual_camera_dev_private_data_t *dev_data)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    virtual_usb_cam_msg_type_e reply_msg;
    uint32_t len;
    virtual_usb_cam_msg_t msg;
    uint32_t remote_addr = 0;
    int32_t rpmsg_ret;

    /* Check if there are any pending messages that need to be flushed */
    if ((!dev_data->messages_sent) || (dev_data->messages_received >= dev_data->messages_sent))
    {
        HAL_LOGI("No messages previously sent\r\n");
        goto stop_cleanup;
    }

    /* Iterate through all unprocessed messages (from messages_received to messages_sent) */
    for (int i = dev_data->messages_received; i < dev_data->messages_sent; i++)
    {
        /* Get the expected reply message type based on what was originally sent */
        reply_msg = dev_data->enq_msg_type[i];

        /* Validate that the stored message type is one of the expected enqueue request types */
        if ((reply_msg != VIRT_USB_CAM_REQRGB) && (reply_msg != VIRT_USB_CAM_REQIR) && (reply_msg != VIRT_USB_CAM_REQRGBIR))
        {
            HAL_LOGE("Found unexpeted enqueued message type %d\r\n", dev_data->enq_msg_type);
            ret = kStatus_HAL_CameraError;
            goto stop_cleanup;
        }

        /* Receive the response message from the remote USB camera core */
        rpmsg_ret = rpmsg_queue_recv(dev_data->rpmsg_inst,
                        dev_data->rpmsg_queue,
                        (uint32_t *)&remote_addr, 
                        (char *)&msg, 
                        sizeof(virtual_usb_cam_msg_t), 
                        &len,
                        RL_BLOCK);

        /* Process the received message if communication was successful */
        if (rpmsg_ret == RL_SUCCESS)
        {
            /* Check if received message type matches what we expected (log warning if not) */
            if (msg.msg_type != reply_msg)
            {
                HAL_LOGI("Received unexpected message %d from USB camera core\r\n", msg.msg_type);
            }

            /* Validate that the message came from the expected remote core address */
            if (remote_addr != dev_data->rpmsg_remote_addr)
            {
                HAL_LOGE("Received message from an unexpected remote address: %d\r\n", remote_addr);
                HAL_LOGE("Expecting message from remote address: %d\r\n", dev_data->rpmsg_remote_addr);
                ret = kStatus_HAL_CameraError;
                goto stop_cleanup;
            }

            /* Log successful message reception for debugging */
            HAL_LOGD("Received message from USB camera core 1: %d\r\n", msg.msg_type);
        }
        else
        {
            /* Handle communication error during message reception */
            HAL_LOGE("Got error %d while trying to deque buffer from USB camera core\r\n", rpmsg_ret);
            ret = kStatus_HAL_CameraError;
            goto stop_cleanup;
        }
    }

stop_cleanup:
    /* Always reset the device state to ensure clean state regardless of success/failure */
    reset_enqueue_dequeue_state(dev_data);

    return ret;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Init(
    camera_dev_t *dev, mpp_camera_params_t *config, camera_dev_callback_t callback, void *param)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    int32_t rpmsg_ret;
    uint32_t recv_addr = 0;
    dual_camera_dev_private_data_t *dev_data;
    bool global_mutex_locked = false;

    RETURN_ON_ERROR(config->n_streams > NUM_STREAMS, kStatus_HAL_CameraError, 
        "Max number of streams for this camera is: %d. Configured value is: %d\n", NUM_STREAMS, config->n_streams);

    dev_data = (dual_camera_dev_private_data_t *) hal_malloc(sizeof(dual_camera_dev_private_data_t));
    RETURN_ON_ERROR(dev_data == NULL, kStatus_HAL_CameraError, "Memory allocation for camera private data failed\n");
    memset(dev_data, 0, sizeof(dual_camera_dev_private_data_t));

    hal_mutex_create(&dev_data->mutex);
    GOTO_ON_ERROR(dev_data->mutex == NULL, cleanup_resources, "Mutex creation failed\n");

    dev_data->stream_addr[RGB_STREAM] = (unsigned char *) virtual_usb_cam_rgb_buff;
    dev_data->stream_addr[IR_STREAM] = (unsigned char *) virtual_usb_cam_ir_buff;

    // Protect global state access
    GOTO_ON_ERROR(init_global_resources() != kStatus_HAL_CameraSuccess, cleanup_resources, "Global resources initialization failed\n");
    hal_mutex_lock(s_global_mutex);
    global_mutex_locked = true;

    dev_data->rpmsg_local_addr = MPP_EPT_ADDRESSS + s_virt_cam_handle.n_instances;

    /* Handle the case when two separate virtual camera elements are added */
    if (s_virt_cam_handle.init_done == false)
    {
        virtual_usb_cam_msg_t cfg_msg = {0};

        s_virt_cam_handle.rpmsg_instance = (struct rpmsg_lite_instance *)config->rpmsg_inst;
        GOTO_ON_ERROR(s_virt_cam_handle.rpmsg_instance == NULL, cleanup_resources, "RPMSG instance is NULL\n");
        s_virt_cam_handle.mcmgr_event_data_p = config->mcmgr_event_data;
        GOTO_ON_ERROR(s_virt_cam_handle.mcmgr_event_data_p == NULL, cleanup_resources, "MCMGR envent data pointer is NULL\n");

        /* Wait for RPMSG link up */
        rpmsg_lite_wait_for_link_up(s_virt_cam_handle.rpmsg_instance, RL_BLOCK);
        HAL_LOGI("RPMSG link is up!\r\n");

        dev_data->rpmsg_queue = rpmsg_queue_create(s_virt_cam_handle.rpmsg_instance);
        GOTO_ON_ERROR(dev_data->rpmsg_queue == NULL, cleanup_resources, "RPMSG queue creation failed\n");

        dev_data->rpmsg_ept = rpmsg_lite_create_ept(s_virt_cam_handle.rpmsg_instance,
                                                    dev_data->rpmsg_local_addr,
                                                    rpmsg_queue_rx_cb,
                                                    dev_data->rpmsg_queue);
        GOTO_ON_ERROR(dev_data->rpmsg_ept == NULL, cleanup_resources, "Failed to create RPMSG endpoint\n");

        /* Wait until the secondary core endpoint is ready. */
        while (APP_EP_READY_EVENT_DATA != *s_virt_cam_handle.mcmgr_event_data_p)
            hal_task_delay(1);
        HAL_LOGI("Remote RPMSG endpoint is up\r\n");

        s_virt_cam_handle.rpmsg_remote_addr = CORE1_EPT_ADDRESS;
        HAL_LOGI("Remote endpoint address is %d\r\n", s_virt_cam_handle.rpmsg_remote_addr);

        /* Send config message */
        cfg_msg.msg_type = VIRT_USB_CAM_CONFIG;
        cfg_msg.user_id = MPP_USER_ID;
        cfg_msg.msg_payload.config.camera_width = config->width;
        cfg_msg.msg_payload.config.camera_height = config->height;
        cfg_msg.msg_payload.config.color_format = VIRT_USB_CAM_JPEG;
        cfg_msg.msg_payload.config.fps = config->fps;

        rpmsg_ret = rpmsg_lite_send(s_virt_cam_handle.rpmsg_instance,
                                    dev_data->rpmsg_ept,
                                    s_virt_cam_handle.rpmsg_remote_addr,
                                    (char *)&cfg_msg,
                                    sizeof(virtual_usb_cam_msg_t),
                                    RL_DONT_BLOCK);
        GOTO_ON_ERROR(rpmsg_ret != RL_SUCCESS, cleanup_resources, 
            "Got error %d while trying to send config msg to USB camera core\r\n", rpmsg_ret);

        /* Wait for config ACK 
        * This handshake is needed for synchronizing the two endpoints and not miss messages */
        rpmsg_ret = rpmsg_queue_recv(s_virt_cam_handle.rpmsg_instance,
                                    dev_data->rpmsg_queue,
                                    (uint32_t *)&recv_addr, 
                                    (char *)&cfg_msg, 
                                    sizeof(virtual_usb_cam_msg_t), 
                                    (void *) 0,
                                    RL_BLOCK);
        if (rpmsg_ret == RL_SUCCESS)
        {
            GOTO_ON_ERROR(cfg_msg.msg_type != VIRT_USB_CAM_CONFIG_ACK, cleanup_resources, 
                "Received unexpected message from USB camera core %d\r\n", cfg_msg.msg_type);

            GOTO_ON_ERROR(s_virt_cam_handle.rpmsg_remote_addr != recv_addr, cleanup_resources,
                "Received message from an unexpected remote address: %d\r\n", recv_addr);
        }
        else
        {
            GOTO_ON_ERROR(true, cleanup_resources, "Got error %d while waiting for CONFIG ACK message from USB camera core 1\r\n", rpmsg_ret);
        }

        s_virt_cam_handle.init_done = true;
    }
    else
    {
        GOTO_ON_ERROR(s_virt_cam_handle.rpmsg_instance == NULL,  cleanup_resources, "RPMSG instance is not initialized\r\n");
        
        dev_data->rpmsg_queue = rpmsg_queue_create(s_virt_cam_handle.rpmsg_instance);
        GOTO_ON_ERROR(dev_data->rpmsg_queue == NULL, cleanup_resources, "RPMSG queue creation failed\n");

        dev_data->rpmsg_ept = rpmsg_lite_create_ept(s_virt_cam_handle.rpmsg_instance,
                                                    dev_data->rpmsg_local_addr,
                                                    rpmsg_queue_rx_cb,
                                                    dev_data->rpmsg_queue);
        GOTO_ON_ERROR(dev_data->rpmsg_ept == NULL, cleanup_resources, "Failed to create RPMSG endpoint...\n");
    }

    dev->id = s_virt_cam_handle.n_instances++;

    hal_mutex_unlock(s_global_mutex);
    global_mutex_locked = false;

    /* Init internal data */
    dev_data->crt_get_buf_desc_idx = 0;
    reset_enqueue_dequeue_state(dev_data);
    dev_data->rpmsg_inst = s_virt_cam_handle.rpmsg_instance;
    dev_data->rpmsg_remote_addr = s_virt_cam_handle.rpmsg_remote_addr;

    /* save config */
    dev->config.width = config->width;
    dev->config.height = config->height;
    dev->config.framerate = config->fps;
    dev->config.format = config->format;
    dev->config.stripe = config->stripe;
    dev->config.pitch = config->width * get_bitpp(config->format) / 8;
    dev->config.stripe_size = 0;
    dev->config.n_streams = config->n_streams;
    dev->config.min_stream_req_cnt = 0;
    dev->config.crt_stream_req_cnt = 0;
    dev->config.in_advance_enqueue = config->in_advance_enqueue;
    memcpy((void *) &dev->config.stream[0], (void *) &config->stream[0], sizeof(dev->config.stream));
    for (int i = 0; i < NUM_STREAMS; i++)
        dev->config.stream_requested[i] = !config->in_advance_enqueue;

    /* Init cap */
    dev->cap.callback = callback;
    dev->cap.param    = param;

    /* Store the camera name in the device structure */
    strncpy(dev->name, CAMERA_NAME, HAL_DEVICE_NAME_MAX_LENGTH);

    dev->data = (void *) dev_data;

    HAL_LOGD("dev id: %d, local addr: %d initialized\r\n", dev->id, dev_data->rpmsg_local_addr);

    return ret;

cleanup_resources:
    if (dev_data->rpmsg_ept != NULL)
    {
        rpmsg_lite_destroy_ept(s_virt_cam_handle.rpmsg_instance, dev_data->rpmsg_ept);
        dev_data->rpmsg_ept = NULL;
    }
    if (dev_data->rpmsg_queue != NULL)
    {
        rpmsg_queue_destroy(s_virt_cam_handle.rpmsg_instance, dev_data->rpmsg_queue);
        dev_data->rpmsg_queue = NULL;
    }
    if ((s_global_mutex != NULL) && (global_mutex_locked == true))
    {
        hal_mutex_unlock(s_global_mutex);
        global_mutex_locked = false;
    }   
    if (dev_data != NULL)
    {
        if (dev_data->mutex != NULL)
            hal_mutex_remove(dev_data->mutex);
        hal_free(dev_data);
    }
    return kStatus_HAL_CameraError;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Getbufdesc(const camera_dev_t *dev, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_Virtual_USB_Getbufdesc\r\n");

    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    HAL_LOGD("dev id: %d, local addr: %d\r\n", dev->id, dev_data->rpmsg_local_addr);
    HAL_LOGD("out_buf=[%p])\r\n", out_buf);

    if ((out_buf == NULL) || (policy == NULL))
    {
        HAL_LOGE("NULL pointer to buffer descriptor\r\n");
        return kStatus_HAL_CameraError;
    }

    if (dev_data->crt_get_buf_desc_idx >= dev->config.n_streams)
    {
        HAL_LOGE("Current get buffer descriptor index exceeds the total number of streams\r\n");
        return kStatus_HAL_CameraError;
    }

    if (get_next_active_stream_idx(dev, &dev_data->crt_get_buf_desc_idx) != kStatus_HAL_CameraSuccess)
        return kStatus_HAL_CameraError;

    /* set memory policy */
    *policy = HAL_MEM_ALLOC_OUTPUT;
    out_buf->alignment = CAMERA_DEV_BUFFER_ALIGN;
    out_buf->cacheable = true;
    out_buf->stride = dev->config.width;
    out_buf->nb_lines = dev->config.height;
    out_buf->addr = dev_data->stream_addr[dev->config.stream[dev_data->crt_get_buf_desc_idx++].type];

    /* reset crt_get_buf_desc_idx if we processed the last stream */
    if (dev_data->crt_get_buf_desc_idx >= dev->config.n_streams)
        dev_data->crt_get_buf_desc_idx = 0;

    HAL_LOGD("--HAL_CameraDev_Virtual_USB_Getbufdesc\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Deinit(camera_dev_t *dev)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_Virtual_USB_Deinit\r\n");

    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    if (dev_data == NULL)
    {
        HAL_LOGE("device data pointer is null.. skipping deinit\n");
        return kStatus_HAL_CameraError;
    }

    HAL_LOGD("dev id: %d, local addr: %d\r\n", dev->id, dev_data->rpmsg_local_addr);

    // Lock global state
    hal_mutex_lock(s_global_mutex);

    /* Check if rpmsg instance is still configured */
    if (dev_data->rpmsg_inst == NULL)
    {
        HAL_LOGE("RPMSG instance pointer is null.. skipping deinit\n");
        hal_mutex_remove(dev_data->mutex);
        hal_free(dev_data);
        hal_mutex_unlock(s_global_mutex);
        return kStatus_HAL_CameraError;
    }

    /* Destroy endpoint */
    if (dev_data->rpmsg_ept != NULL)
    {
        (void)rpmsg_lite_destroy_ept(dev_data->rpmsg_inst, dev_data->rpmsg_ept);
        dev_data->rpmsg_ept = NULL;
    }

    /* Destroy queue */
    if (dev_data->rpmsg_queue != NULL)
    {
        (void)rpmsg_queue_destroy(dev_data->rpmsg_inst, dev_data->rpmsg_queue);
        dev_data->rpmsg_queue = NULL;
    }

    hal_mutex_remove(dev_data->mutex);

    hal_free(dev_data);

    // Only reset init_done when this is the last instance
    if (s_virt_cam_handle.n_instances > 0)
    {
        s_virt_cam_handle.n_instances--;
        
        // Only cleanup global resources when no instances remain
        if (s_virt_cam_handle.n_instances == 0)
        {
            s_virt_cam_handle.init_done = false;
            s_virt_cam_handle.rpmsg_instance = NULL;
            s_virt_cam_handle.rpmsg_remote_addr = 0;
        }
    }

    hal_mutex_unlock(s_global_mutex);

    HAL_LOGD("--HAL_CameraDev_Virtual_USB_Deinit\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Start(const camera_dev_t *dev)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_Virtual_USB_Start\r\n");

    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    if (dev->config.in_advance_enqueue != true)
    {
        HAL_LOGD("No advance enqueue, skipping initial enqueue\r\n");
        HAL_LOGD("--HAL_CameraDev_Virtual_USB_Start\n");

        return kStatus_HAL_CameraSuccess;
    }

    HAL_LOGD("dev id: %d, local addr: %d\r\n", dev->id, dev_data->rpmsg_local_addr);

    bool stream_active[NUM_STREAMS];

    for (int i = 0; i < NUM_STREAMS; i++)
        stream_active[i] = dev->config.stream[i].active;

    /* Enqueue the first request to USB camera on core 1
     * Request parameters for this enqueue are updated in the init method */
    ret = camera_dev_enqueue(dev, stream_active);

    HAL_LOGD("--HAL_CameraDev_Virtual_USB_Start\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Stop(const camera_dev_t *dev)
{
    hal_camera_status_t ret;

    HAL_LOGD("++HAL_CameraDev_Virtual_USB_Stop\r\n");

    if (dev->config.in_advance_enqueue != true)
    {
        HAL_LOGD("No advance enqueue, skipping flush of previously enqueued messages\r\n");
        HAL_LOGD("--HAL_CameraDev_Virtual_USB_Start\n");

        return kStatus_HAL_CameraSuccess;
    }

    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    HAL_LOGD("dev id: %d, local addr: %d\r\n", dev->id, dev_data->rpmsg_local_addr);

    /* Flash any previously enqueued streams */
    ret = flush_enqueued_messages(dev_data);
    HAL_LOGD("--HAL_CameraDev_Virtual_USB_Stop\r\n");

    return ret;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Lock(const camera_dev_t *dev)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    /* Lock the camera device mutex to ensure thread-safe access */
    int status = hal_mutex_lock(dev_data->mutex);
    if (status != MPP_SUCCESS)
    {
        HAL_LOGE("Failed to lock camera device mutex (error %d)\r\n", status);
        return kStatus_HAL_CameraError;
    }

    return ret;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Unlock(const camera_dev_t *dev)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;

    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    /* Lock the camera device mutex to ensure thread-safe access */
    int status = hal_mutex_unlock(dev_data->mutex);
    if (status != MPP_SUCCESS)
    {
        HAL_LOGE("Failed to unlock camera device mutex (error %d)\r\n", status);
        return kStatus_HAL_CameraError;
    }

    return ret;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Dequeue(const camera_dev_t *dev, void **data, int *stripe, int *compressed_size)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    virtual_usb_cam_msg_t msg;
    virtual_usb_cam_msg_type_e reply_msg;
    uint32_t len, remote_addr;
    int32_t rpmsg_ret;
    mpp_camera_stream_type stream_type;
    bool separate_stream_dequeue = false;

    HAL_LOGD("++HAL_CameraDev_Virtual_USB_Dequeue\r\n");

    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    HAL_LOGD("dev id: %d, local addr: %d\r\n", dev->id, dev_data->rpmsg_local_addr);

    /* Check if any stream was enqueued or not */
    if (!dev_data->enqueued_streams)
    {
        if (dev->config.in_advance_enqueue == false)
        {
            /* Attempt in-place enqueue if no streams are currently enqueued */
            uint32_t req_cnt = 0;
        
            /* Count how many streams are both active and requested */
            for (int i = 0; i < dev->config.n_streams; i++)
            {
                if (dev->config.stream[i].active == true && dev->config.stream_requested[i] == true)
                    req_cnt++;
            }
        
            /* If no streams are requested, return no data available */
            if (!req_cnt)
            {
                HAL_LOGI("No active and requested stream found for in place enqueue\r\n");
                HAL_LOGD("--HAL_CameraDev_Virtual_USB_Dequeue\r\n");
                return kStatus_HAL_CameraNoData;
            }
            else
            {
                /* Attempt to enqueue the requested streams now */
                bool stream_active[NUM_STREAMS];
                for (int i = 0; i < NUM_STREAMS; i++)
                    stream_active[i] = dev->config.stream[i].active & dev->config.stream_requested[i];

                GOTO_ON_ERROR(camera_dev_enqueue(dev, stream_active) != kStatus_HAL_CameraSuccess, dequeue_cleanup,
                            "Got error while trying to enqueue buffers to camera device\n");

                GOTO_ON_ERROR(!dev_data->enqueued_streams, dequeue_cleanup,
                            "No enqueued stream found even after in-place enqueue\r\n");
            }
        }
        else
        {
            /* Standard behavior: return no data if no streams are enqueued */
            HAL_LOGI("No enqueued stream found\r\n");
            HAL_LOGD("--HAL_CameraDev_Virtual_USB_Dequeue\r\n");
            return kStatus_HAL_CameraNoData;
        }
    }

    /* Validate that messages were actually sent for the enqueued streams */
    GOTO_ON_ERROR(dev_data->messages_sent == 0, dequeue_cleanup, 
                "No messages sent even though equeued streams is %d\r\n", dev_data->enqueued_streams);

    /* Ensure we haven't already received more messages than configured streams */
    GOTO_ON_ERROR(dev_data->messages_received >= dev->config.n_streams, dequeue_cleanup, 
                "received message already exceeding configured number of streams\r\n");

    /* Check if we've already received all sent messages */
    GOTO_ON_ERROR(dev_data->messages_received >= dev_data->messages_sent, dequeue_cleanup, 
                "Already received all messages\r\n");

    /* Validate that we haven't exceeded the maximum number of streams for dequeue */
    GOTO_ON_ERROR(dev_data->dequeued_streams >= dev->config.n_streams, dequeue_cleanup,
                "Dequeued streams (%d) already reached maximum configured streams ($d)\r\n",
                dev_data->dequeued_streams, dev->config.n_streams);

    /* Ensure the current dequeue index is within valid range */
    GOTO_ON_ERROR(dev_data->crt_dequeue_stream_idx >= dev->config.n_streams, dequeue_cleanup,
                "Current dequeue stream index (%d) exceeds the active number of streams (%d)\r\n",
                dev_data->crt_dequeue_stream_idx, dev->config.n_streams);

    /* Find the next stream that has been enqueued and is ready for dequeue */
    GOTO_ON_ERROR(get_next_enqueued_stream_idx(dev, &dev_data->crt_dequeue_stream_idx) != kStatus_HAL_CameraSuccess,
                dequeue_cleanup, "Got error while finding next enqueued stream index\r\n");

    HAL_LOGD("Number of enqueued streams is %d\r\n", dev_data->enqueued_streams);

    /* Get the stream type for the current stream being dequeued */
    stream_type = dev->config.stream[dev_data->crt_dequeue_stream_idx].type;

    /* Validate that the stream type is within expected range */
    GOTO_ON_ERROR(stream_type >= NUM_STREAMS, dequeue_cleanup, "Invalid stream type\r\n");

    /* Store dequeue request information for later processing */
    dev_data->stream_dequeue_req[dev_data->dequeued_streams].stream_type = stream_type;
    dev_data->stream_dequeue_req[dev_data->dequeued_streams].stream_addr = data;
    dev_data->stream_dequeue_req[dev_data->dequeued_streams].stream_size = compressed_size;
    *stripe = 0; /* Virtual camera doesn't use stripe mode */

    /* Determine expected reply message type based on number of enqueued streams */
    if (dev_data->enqueued_streams == 1)
    {
        /* Single stream case: expect specific response based on stream type */
        switch (stream_type)
        {
        case RGB_STREAM:
            reply_msg = VIRT_USB_CAM_RSPRGB;
            break;

        case IR_STREAM:
            reply_msg = VIRT_USB_CAM_RSPIR;
            break;

        default:
            GOTO_ON_ERROR(true, dequeue_cleanup, "Unsupported camera stream type %d\r\n", stream_type);
            break;
        }
    }
    else
    {
        /* Multiple streams case: handle different scenarios */
        if (dev_data->messages_sent == 1)
        {
            /* Single message sent for multiple streams (dual stream capture) */
            if (dev_data->dequeued_streams < (dev_data->enqueued_streams - 1))
            {
                /* Not the last stream - just update counters and return success */
                dev_data->crt_dequeue_stream_idx++;
                dev_data->dequeued_streams++;
                HAL_LOGD("--HAL_CameraDev_Virtual_USB_Dequeue\r\n");
                return kStatus_HAL_CameraSuccess;
            }
            /* Last stream - expect combined RGB+IR response */
            reply_msg = VIRT_USB_CAM_RSPRGBIR;
        }
        else
        {
            /* Multiple messages sent - handle each stream separately */
            switch (dev_data->enq_msg_type[dev_data->messages_received])
            {
                case VIRT_USB_CAM_REQRGB:
                    reply_msg = VIRT_USB_CAM_RSPRGB;
                    break;

                case VIRT_USB_CAM_REQIR:
                    reply_msg = VIRT_USB_CAM_RSPIR;
                    break;

                default:
                    GOTO_ON_ERROR(true, dequeue_cleanup, "Unsupported enqueued message type (%d)\r\n",
                                dev_data->enq_msg_type[dev_data->messages_received]);
                    break;
            }
            separate_stream_dequeue = true; /* Flag for separate stream processing */
        }
    }

    /* Receive the response message from the remote USB camera core */
    rpmsg_ret = rpmsg_queue_recv(dev_data->rpmsg_inst,
                                dev_data->rpmsg_queue,
                                (uint32_t *)&remote_addr, 
                                (char *)&msg, 
                                sizeof(virtual_usb_cam_msg_t), 
                                &len,
                                RL_BLOCK);

    /* Validate that message reception was successful */
    GOTO_ON_ERROR(rpmsg_ret != RL_SUCCESS, dequeue_cleanup,
                "Got error %d while trying to dequeue buffer from USB camera core\r\n", rpmsg_ret);

    /* Verify the message came from the expected remote address */
    GOTO_ON_ERROR(remote_addr != dev_data->rpmsg_remote_addr, dequeue_cleanup,
                "Received message from an unexpected remote address: %d\r\n", remote_addr);

    /* Check if received message type matches expected (allow flexibility for separate stream dequeue) */
    GOTO_ON_ERROR((msg.msg_type != reply_msg) && (!separate_stream_dequeue), dequeue_cleanup, 
                "Received unexpected message %d from USB camera core\r\n", msg.msg_type);

    /* Validate frame sizes and store them based on message type */
    switch (msg.msg_type)
    {
    case VIRT_USB_CAM_RSPRGB:
        /* Validate RGB frame size doesn't exceed buffer capacity */
        GOTO_ON_ERROR(validate_frame_size(msg.msg_payload.rsp.rgb_frame_size, VIRTUAL_CAMERA_RGB_BUFFER_SIZE, "RGB") != kStatus_HAL_CameraSuccess,
                    dequeue_cleanup, "RGB frame size validation failed\r\n");
        dev_data->dequeue_stream_sizes[RGB_STREAM] = msg.msg_payload.rsp.rgb_frame_size;
        break;

    case VIRT_USB_CAM_RSPIR:
        /* Validate IR frame size doesn't exceed buffer capacity */
        GOTO_ON_ERROR(validate_frame_size(msg.msg_payload.rsp.ir_frame_size, VIRTUAL_CAMERA_IR_BUFFER_SIZE, "IR") != kStatus_HAL_CameraSuccess,
                    dequeue_cleanup, "IR frame size validation failed\r\n");
        dev_data->dequeue_stream_sizes[IR_STREAM] = msg.msg_payload.rsp.ir_frame_size;
        break;

    case VIRT_USB_CAM_RSPRGBIR:
        /* Validate both RGB and IR frame sizes for dual stream response */
        GOTO_ON_ERROR(validate_frame_size(msg.msg_payload.rsp.rgb_frame_size, VIRTUAL_CAMERA_RGB_BUFFER_SIZE, "RGB") != kStatus_HAL_CameraSuccess ||
                    validate_frame_size(msg.msg_payload.rsp.ir_frame_size, VIRTUAL_CAMERA_IR_BUFFER_SIZE, "IR") != kStatus_HAL_CameraSuccess,
                    dequeue_cleanup, "RGB or IR frame size validation failed\r\n");
        dev_data->dequeue_stream_sizes[RGB_STREAM] = msg.msg_payload.rsp.rgb_frame_size;
        dev_data->dequeue_stream_sizes[IR_STREAM]  = msg.msg_payload.rsp.ir_frame_size;
        break;

    default:
        GOTO_ON_ERROR(true, dequeue_cleanup, "Unsupported message type: %d\r\n", msg.msg_type);
        break;
    }

    HAL_LOGD("Received message from USB camera core 1: %d\r\n", msg.msg_type);

    /* Update dequeue tracking counters */
    dev_data->crt_dequeue_stream_idx++;
    dev_data->dequeued_streams++;
    dev_data->messages_received++;

    /* Check if all enqueued streams have been dequeued */
    if (dev_data->dequeued_streams >= dev_data->enqueued_streams)
    {
        /* Verify that we've received all expected messages */
        GOTO_ON_ERROR(dev_data->messages_received != dev_data->messages_sent, dequeue_cleanup,
                    "Did not received all messages even though all streams are dequeued\r\n");

        /* Process all dequeued streams and set output parameters */
        for (int i = 0; i < dev_data->dequeued_streams; i++)
        {
            stream_type = dev_data->stream_dequeue_req[i].stream_type;

            /* Validate stream type is within expected range */
            GOTO_ON_ERROR(stream_type >= NUM_STREAMS, dequeue_cleanup, "Invalid stream type %d\r\n", stream_type);
        
            /* Ensure stream address pointer is valid */
            GOTO_ON_ERROR(dev_data->stream_dequeue_req[i].stream_addr == NULL, dequeue_cleanup, 
                            "Stream address is NULL for stream index %d\r\n", i);
        
            /* Ensure stream size pointer is valid */
            GOTO_ON_ERROR(dev_data->stream_dequeue_req[i].stream_size == NULL, dequeue_cleanup, 
                            "Stream size pointer is NULL for stream index %d\r\n", i);

            /* Set the output buffer address to point to the shared memory buffer */
            *(dev_data->stream_dequeue_req[i].stream_addr) = (void *) (dev_data->stream_addr[stream_type]);
        
            /* Set the compressed size based on format type */
            if (dev->config.format == MPP_PIXEL_JPEG)
            {
                /* For JPEG format, use the actual compressed size received from remote core */
                GOTO_ON_ERROR(dev_data->dequeue_stream_sizes[stream_type] == 0, dequeue_cleanup, 
                            "Error: Stream size is zero for JPEG format for stream index %d\r\n", i);

                *(dev_data->stream_dequeue_req[i].stream_size) = dev_data->dequeue_stream_sizes[stream_type];
            }
            else
            {
                /* For uncompressed formats, size is determined by resolution and format */
                *(dev_data->stream_dequeue_req[i].stream_size) = 0;
            }
        }

        /* Reset all state variables now that dequeue operation is complete */
        reset_enqueue_dequeue_state(dev_data);
    }

    HAL_LOGD("--HAL_CameraDev_Virtual_USB_Dequeue\r\n");
    return ret;

dequeue_cleanup:
    /* Error cleanup: set error status and flush any remaining messages */
    ret = kStatus_HAL_CameraError;
    /* Do not use return code of flush_enqueued_messages() to prevent masking previous error */
    flush_enqueued_messages(dev_data);

    HAL_LOGD("--HAL_CameraDev_Virtual_USB_Dequeue\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_Virtual_USB_Enqueue(const camera_dev_t *dev, void *data)
{
    int error = 0;
    (void) data;
    HAL_LOGD("++HAL_CameraDev_Virtual_USB_Enqueue\r\n");

    dual_camera_dev_private_data_t *dev_data = (dual_camera_dev_private_data_t *) dev->data;

    HAL_LOGD("dev id: %d, local addr: %d\r\n", dev->id, dev_data->rpmsg_local_addr);

    /* Array to track which camera streams are currently active/enabled */
    bool stream_active[NUM_STREAMS];

    memcpy((void *) &stream_active[0], (void *)&dev->config.stream_requested[0], sizeof(stream_active));

    error = camera_dev_enqueue(dev, stream_active);

    HAL_LOGD("--HAL_CameraDev_Virtual_USB_Enqueue\r\n");
    return error;
}

const static camera_dev_operator_t camera_dev_virt_usb_ops = {
    .init        = HAL_CameraDev_Virtual_USB_Init,
    .deinit      = HAL_CameraDev_Virtual_USB_Deinit,
    .start       = HAL_CameraDev_Virtual_USB_Start,
    .stop        = HAL_CameraDev_Virtual_USB_Stop,
    .enqueue     = HAL_CameraDev_Virtual_USB_Enqueue,
    .dequeue     = HAL_CameraDev_Virtual_USB_Dequeue,
    .get_buf_desc = HAL_CameraDev_Virtual_USB_Getbufdesc,
    .lock         = HAL_CameraDev_Virtual_USB_Lock,
    .unlock       = HAL_CameraDev_Virtual_USB_Unlock
};

int HAL_CameraDev_Virtual_USB_setup(const char *name, camera_dev_t *dev)
{
    dev->ops = &camera_dev_virt_usb_ops;

    return 0;
}
#else /* (defined HAL_ENABLE_CAMERA) && (HAL_ENABLE_VIRTUAL_CAMERA == 1) */
int HAL_CameraDev_Virtual_USB_setup(const char *name, camera_dev_t *dev)
{
    HAL_LOGE("Virtual Camera USB not enabled\r\n");
    return -1;
}
#endif /* (defined HAL_ENABLE_CAMERA) && (HAL_ENABLE_VIRTUAL_CAMERA == 1) */
