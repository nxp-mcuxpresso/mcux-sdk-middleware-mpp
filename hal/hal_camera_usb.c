/*
 * Copyright 2025-2026 NXP.
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
 * @brief USB camera module HAL camera driver implementation.
 *
 * Supports up to USB_HOST_VIDEO_INST_NUM simultaneous USB camera streams.
 * Each HAL camera device maps to one g_Videos[] instance selected by
 * the index stored in usb_camera_private_data_t.instanceIndex.
 *
 * When n_streams > 1 in the HAL init config, additional OS tasks are
 * created (one USB host app task per extra stream) so that each camera runs
 * its own negotiation loop in parallel.
 */

#include "mpp_config.h"
#include "hal_camera_dev.h"
#include "hal_debug.h"

#if (defined HAL_ENABLE_CAMERA) && (HAL_ENABLE_CAMERA_DEV_USB == 1)
#include <stdlib.h>
#include <string.h>

#include "board.h"

#include "hal.h"
#include "hal_utils.h"
#include "hal_os.h"

#include "app.h"

/* USB includes */
#include "usb_host_config.h"
#include "usb_host.h"
#include "fsl_device_registers.h"
#include "usb_host_video.h"
#include "host_video.h"

#if ((!USB_HOST_CONFIG_KHCI) && (!USB_HOST_CONFIG_EHCI) && (!USB_HOST_CONFIG_OHCI) && (!USB_HOST_CONFIG_IP3516HS))
#error Please enable USB_HOST_CONFIG_KHCI, USB_HOST_CONFIG_EHCI, USB_HOST_CONFIG_OHCI, or USB_HOST_CONFIG_IP3516HS in file usb_host_config.
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define CAMERA_NAME "USB_cam"
#define USB_HOST_TASK_SIZE      (2500L / sizeof(portSTACK_TYPE))
#define USB_HOST_APP_TASK_SIZE  ((20000L + 5000L) / sizeof(portSTACK_TYPE))

#define USB_HOST_TASK_PRIORITY     5
#define USB_HOST_APP_TASK_PRIORITY 1

typedef struct
{
    void   **addr;            /* pointer to caller's data output variable */
    int     *compressed_size; /* pointer to caller's compressed_size output variable */
} usb_stream_dequeue_req_t;

typedef struct usb_camera_private_data
{
    uint32_t n_streams;                                           /* number of USB streams owned by this device */
    uint32_t instanceIndex[USB_HOST_VIDEO_INST_NUM];              /* g_Videos[] slot per stream */
    uint32_t crt_usb_buffer_index[USB_HOST_VIDEO_INST_NUM];       /* last received buffer index per stream */

    /* multi-stream dequeue/enqueue tracking (mirrors virtual_usb pattern) */
    uint32_t crt_dequeue_stream_idx;                              /* stream cursor for sequential dequeue calls */
    uint32_t dequeued_streams;                                    /* dequeue calls received this cycle */
    uint32_t enqueued_streams;                                    /* streams marked for enqueue this cycle */
    bool     stream_enqueued[USB_HOST_VIDEO_INST_NUM];            /* which slots are enqueued this cycle */
    usb_stream_dequeue_req_t stream_dequeue_req[USB_HOST_VIDEO_INST_NUM]; /* per-stream output pointers */

    /*
     * Task handles for the USB host task and the USB host application tasks.
     * host_task: single handle for the shared USB host controller task.
     * app_task:  one handle per stream (indexed by the same order as instanceIndex[]).
     * Both are kept alive across deinit/init cycles; unused slots are NULL.
     */
    hal_task_t host_task;                              /* handle for USB_HostTask (shared) */
    hal_task_t app_task[USB_HOST_VIDEO_INST_NUM];      /* handle for USB_HostApplicationTask per stream */

#ifdef USB_IMG_BUFFER_DYNALLOC
    /*
     * Raw (unaligned) pointers returned by hal_malloc for each dynamically
     * allocated picture buffer.  Kept separately so hal_free gets the exact
     * address that was returned by the allocator, even though pictureBuffer.bufs[]
     * holds the aligned version.
     */
    uint8_t *dynbuf_raw[USB_HOST_VIDEO_INST_NUM][USB_HOST_VIDEO_PIC_BUFFER_NUM];
#endif
} usb_camera_private_data_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/
usb_host_handle g_HostHandle;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static usb_status_t USB_HostEvent(usb_device_handle deviceHandle,
                                  usb_host_configuration_handle configurationHandle,
                                  uint32_t eventCode);
static void USB_HostApplicationInit(void);
static void USB_HostTask(void *param);
static void USB_HostApplicationTask(void *param);

extern void USB_HostClockInit(void);
extern void USB_HostIsrEnable(void);
extern void USB_HostTaskFn(void *param);

/*******************************************************************************
 * Code
 ******************************************************************************/

static usb_status_t USB_HostEvent(usb_device_handle deviceHandle,
                                  usb_host_configuration_handle configurationHandle,
                                  uint32_t eventCode)
{
    usb_status_t status = kStatus_USB_Success;

    switch (eventCode & 0x0000FFFFU)
    {
        case kUSB_HostEventAttach:
            status = USB_HostVideoEvent(deviceHandle, configurationHandle, eventCode);
            break;

        case kUSB_HostEventNotSupported:
            HAL_LOGE("device not supported.\r\n");
            break;

        case kUSB_HostEventEnumerationDone:
            status = USB_HostVideoEvent(deviceHandle, configurationHandle, eventCode);
            break;

        case kUSB_HostEventDetach:
            status = USB_HostVideoEvent(deviceHandle, configurationHandle, eventCode);
            break;

        case kUSB_HostEventEnumerationFail:
            HAL_LOGE("enumeration failed\r\n");
            break;

        default:
            break;
    }
    return status;
}

static void USB_HostApplicationInit(void)
{
    usb_status_t status = kStatus_USB_Success;

    USB_HostClockInit();

#if ((defined FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT))
    SYSMPU_Enable(SYSMPU, 0);
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

    status = USB_HostInit(CONTROLLER_ID, &g_HostHandle, USB_HostEvent);
    if (status != kStatus_USB_Success)
    {
        HAL_LOGE("host init error\r\n");
        return;
    }
    USB_HostIsrEnable();

    HAL_LOGD("host init done\r\n");
}

static void USB_HostTask(void *param)
{
    while (1)
    {
        USB_HostTaskFn(param);
    }
}

static void USB_HostApplicationTask(void *param)
{
    USB_HostVideoAppSDcardInit();
    while (1)
    {
        USB_HostVideoTask(param);
    }
}

/*
 * Reset per-cycle dequeue/enqueue state. Called after a full dequeue cycle completes
 * or at the end of an enqueue (mirrors reset_enqueue_dequeue_state in virtual_usb).
 */
static void usb_reset_dequeue_enqueue_state(usb_camera_private_data_t *dev_priv)
{
    dev_priv->crt_dequeue_stream_idx = 0U;
    dev_priv->dequeued_streams       = 0U;
    dev_priv->enqueued_streams       = 0U;
    for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
    {
        dev_priv->stream_enqueued[i]          = false;
        dev_priv->stream_dequeue_req[i].addr  = NULL;
        dev_priv->stream_dequeue_req[i].compressed_size = NULL;
    }
}

/*
 * Mark streams as enqueued for the current cycle.
 * stream_active_cfg[i] true means stream i should be enqueued.
 * Mirrors camera_dev_enqueue() in virtual_usb.
 */
static hal_camera_status_t usb_mark_streams_enqueued(const camera_dev_t *dev,
                                                       usb_camera_private_data_t *dev_priv,
                                                       const bool *stream_active_cfg)
{
    uint32_t req_cnt = 0U;
    for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
    {
        if (stream_active_cfg[i])
            req_cnt++;
    }

    if (dev_priv->enqueued_streams + req_cnt > dev_priv->n_streams)
    {
        HAL_LOGE("Too many streams requested for enqueue: %u (already enqueued %u)\r\n",
                 (unsigned)req_cnt, (unsigned)dev_priv->enqueued_streams);
        return kStatus_HAL_CameraError;
    }

    for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
    {
        if (stream_active_cfg[i])
        {
            dev_priv->stream_enqueued[i] = true;
            dev_priv->enqueued_streams++;
        }
    }
    return kStatus_HAL_CameraSuccess;
}

/*
 * Advance crt_dequeue_stream_idx to the next enqueued stream.
 * Mirrors get_next_enqueued_stream_idx() in virtual_usb.
 */
static hal_camera_status_t usb_get_next_enqueued_stream(const usb_camera_private_data_t *dev_priv,
                                                          uint32_t *idx)
{
    while ((*idx < dev_priv->n_streams) && (!dev_priv->stream_enqueued[*idx]))
        (*idx)++;

    if (*idx >= dev_priv->n_streams)
    {
        HAL_LOGE("No enqueued stream found from index %u\r\n", (unsigned)*idx);
        return kStatus_HAL_CameraError;
    }
    return kStatus_HAL_CameraSuccess;
}

/*
 * Helper: roll back all queues and app tasks for slots [0, count).
 * Used in error paths inside HAL_CameraDev_USB_Init.
 */
static void usb_rollback_slots(usb_camera_private_data_t *dev_priv, uint32_t count)
{
    for (uint32_t j = 0U; j < count; j++)
    {
        uint32_t prev = dev_priv->instanceIndex[j];
        hal_queue_delete(g_Videos[prev].mppQueue);
        g_Videos[prev].mppQueue = NULL;
        if (dev_priv->app_task[j] != NULL)
        {
            hal_task_delete(dev_priv->app_task[j]);
            dev_priv->app_task[j] = NULL;
        }
    }
}

hal_camera_status_t HAL_CameraDev_USB_Init(
    camera_dev_t *dev, mpp_camera_params_t *config, camera_dev_callback_t callback, void *param)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_USB_Init(param[%p])\r\n", param);

    if ((config->width > CAMERA_USB_MAX_WIDTH) || (config->height > CAMERA_USB_MAX_HEIGHT) ||
        (config->width <= 0) || (config->height <= 0))
    {
        HAL_LOGE("Camera resolution unsupported\r\n");
        return kStatus_HAL_CameraError;
    }

    if ((config->format != MPP_PIXEL_YUYV) &&
        (config->format != MPP_PIXEL_YUYV1P422) &&
        (config->format != MPP_PIXEL_JPEG))
    {
        HAL_LOGE("Camera format %d unsupported\r\n", config->format);
        return kStatus_HAL_CameraError;
    }

    if (config->stripe != false)
    {
        HAL_LOGE("Camera stripe unsupported\r\n");
        return kStatus_HAL_CameraError;
    }

    int n_streams = (config->n_streams > 0) ? config->n_streams : 1;

    if (n_streams > USB_HOST_VIDEO_INST_NUM)
    {
        HAL_LOGE("Requested %d streams but USB_HOST_VIDEO_INST_NUM=%d\r\n",
                 n_streams, USB_HOST_VIDEO_INST_NUM);
        return kStatus_HAL_CameraError;
    }

    /* Allocate private data for this HAL device */
    usb_camera_private_data_t *dev_priv =
        (usb_camera_private_data_t *)hal_malloc(sizeof(usb_camera_private_data_t));
    if (dev_priv == NULL)
    {
        HAL_LOGE("Failed to allocate USB camera private data\r\n");
        return kStatus_HAL_CameraError;
    }
    memset(dev_priv, 0, sizeof(usb_camera_private_data_t));
    dev_priv->n_streams = (uint32_t)n_streams;

    /* Initialise USB host stack once (idempotent: g_HostHandle is NULL before first call) */
    if (g_HostHandle == NULL)
    {
        USB_HostApplicationInit();

        if (hal_task_create(USB_HostTask, "usb host task",
                        USB_HOST_TASK_SIZE, g_HostHandle,
                        USB_HOST_TASK_PRIORITY,
                        &dev_priv->host_task) != MPP_SUCCESS)
        {
            HAL_LOGE("create host task error\r\n");
            hal_free(dev_priv);
            return kStatus_HAL_CameraError;
        }
    }
    /*
     * When the host is already running (re-init after deinit), host_task stays
     * NULL in dev_priv; the task is kept alive across cycles and is not owned
     * by the new private data instance.
     */

    /*
     * Allocate one g_Videos[] slot per stream.
     *
     * If config->stream[s].usb_camera_index >= 0 the caller has requested a
     * specific g_Videos[] index (e.g. 0 = RGB, 1 = IR on a dual-sensor device).
     * In that case we pin to that exact slot instead of taking the first free one.
     * Setting usb_camera_index to -1 (or leaving it zero-initialised) falls back
     * to the original first-free behaviour.
     *
     * Slots that are not assigned to any stream do NOT get a queue or task created,
     * so their USB enumeration/buffering still happens (host_video.c starts tasks
     * only when mppQueue != NULL) but frames are simply discarded.
     */
    uint32_t assigned = 0U;
    for (uint32_t s = 0U; s < (uint32_t)n_streams; s++)
    {
        int preferred = (config->n_streams > 0) ? config->stream[s].usb_camera_index : -1;
        uint32_t chosen = (uint32_t)USB_HOST_VIDEO_INST_NUM; /* sentinel: not found */

        if (preferred >= 0)
        {
            /* Pinned mode: use the requested slot if it is free */
            uint32_t req = (uint32_t)preferred;
            if (req >= (uint32_t)USB_HOST_VIDEO_INST_NUM)
            {
                HAL_LOGE("usb_camera_index %d out of range (USB_HOST_VIDEO_INST_NUM=%d)\r\n",
                         preferred, USB_HOST_VIDEO_INST_NUM);
                usb_rollback_slots(dev_priv, assigned);
                hal_free(dev_priv);
                return kStatus_HAL_CameraError;
            }
            if (g_Videos[req].mppQueue != NULL)
            {
                HAL_LOGE("usb_camera_index %d is already in use\r\n", preferred);
                usb_rollback_slots(dev_priv, assigned);
                hal_free(dev_priv);
                return kStatus_HAL_CameraError;
            }
            chosen = req;
        }
        else
        {
            /* Auto mode: pick first free slot */
            for (uint32_t i = 0U; i < (uint32_t)USB_HOST_VIDEO_INST_NUM; i++)
            {
                if (g_Videos[i].mppQueue == NULL)
                {
                    chosen = i;
                    break;
                }
            }
        }

        if (chosen >= (uint32_t)USB_HOST_VIDEO_INST_NUM)
        {
            HAL_LOGE("No free USB video slot for stream %u\r\n", (unsigned)s);
            usb_rollback_slots(dev_priv, assigned);
            hal_free(dev_priv);
            return kStatus_HAL_CameraError;
        }

        /* Store the target resolution in the instance for format negotiation */
        g_Videos[chosen].targetWidth  = (uint32_t)config->width;
        g_Videos[chosen].targetHeight = (uint32_t)config->height;

        /* Create the per-stream queue that passes completed frames to Dequeue */
        g_Videos[chosen].mppQueue = hal_queue_create(2, sizeof(usb_frame_ready_msg_t));
        if (g_Videos[chosen].mppQueue == NULL)
        {
            HAL_LOGE("Failed to create frame-ready queue for slot %u\r\n", (unsigned)chosen);
            usb_rollback_slots(dev_priv, assigned);
            hal_free(dev_priv);
            return kStatus_HAL_CameraError;
        }

        /* Create one USB application task per assigned stream */
        char taskName[32];
        snprintf(taskName, sizeof(taskName), "usb app task %u", (unsigned)chosen);
        if (hal_task_create(USB_HostApplicationTask, taskName,
                        USB_HOST_APP_TASK_SIZE, &g_Videos[chosen],
                        USB_HOST_APP_TASK_PRIORITY,
                        &dev_priv->app_task[assigned]) != MPP_SUCCESS)
        {
            HAL_LOGE("create video task error (slot %u)\r\n", (unsigned)chosen);
            /* Roll back the queue created for this slot and all earlier slots */
            hal_queue_delete(g_Videos[chosen].mppQueue);
            g_Videos[chosen].mppQueue = NULL;
            usb_rollback_slots(dev_priv, assigned);
            hal_free(dev_priv);
            return kStatus_HAL_CameraError;
        }

        dev_priv->instanceIndex[assigned]        = chosen;
        /* No buffer held yet; use the invalid sentinel so Stop/Dequeue know
         * there is nothing to release before the first frame is received. */
        dev_priv->crt_usb_buffer_index[assigned] = USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX;
        assigned++;
    }

    if (assigned < (uint32_t)n_streams)
    {
        HAL_LOGE("Only %u of %d requested USB video slots are free\r\n",
                 (unsigned)assigned, n_streams);
        usb_rollback_slots(dev_priv, assigned);
        hal_free(dev_priv);
        return kStatus_HAL_CameraError;
    }

#ifdef USB_IMG_BUFFER_DYNALLOC
    /*
     * Dynamically allocate ping-pong picture buffers for each assigned slot.
     * hal_malloc does not guarantee CAMERA_DEV_BUFFER_ALIGN alignment, so we
     * over-allocate by (CAMERA_DEV_BUFFER_ALIGN - 1) bytes and align up manually.
     * The raw pointer is saved in dynbuf_raw[][] so it can be freed on deinit.
     */
    for (uint32_t s = 0U; s < (uint32_t)n_streams; s++)
    {
        uint32_t slot = dev_priv->instanceIndex[s];
        for (uint32_t j = 0U; j < USB_HOST_VIDEO_PIC_BUFFER_NUM; j++)
        {
            uint8_t *raw = (uint8_t *)hal_malloc(CAMERA_USB_MAX_BUFF_SIZE + CAMERA_DEV_BUFFER_ALIGN - 1U);
            if (raw == NULL)
            {
                HAL_LOGE("Failed to alloc picture buffer (slot %u buf %u)\r\n",
                         (unsigned)slot, (unsigned)j);
                /* Free buffers allocated so far for this and earlier slots */
                for (uint32_t rs = 0U; rs <= s; rs++)
                {
                    uint32_t rslot = dev_priv->instanceIndex[rs];
                    uint32_t jlim  = (rs < s) ? USB_HOST_VIDEO_PIC_BUFFER_NUM : j;
                    for (uint32_t rj = 0U; rj < jlim; rj++)
                    {
                        hal_free(dev_priv->dynbuf_raw[rs][rj]);
                        dev_priv->dynbuf_raw[rs][rj] = NULL;
                        g_Videos[rslot].pictureBuffer.bufs[rj].buffer = NULL;
                    }
                }
                /* Free queues, app tasks and private data */
                usb_rollback_slots(dev_priv, (uint32_t)n_streams);
                hal_free(dev_priv);
                return kStatus_HAL_CameraError;
            }
            /* Align the usable pointer up to CAMERA_DEV_BUFFER_ALIGN */
            uintptr_t aligned = ((uintptr_t)raw + (CAMERA_DEV_BUFFER_ALIGN - 1U))
                                & ~((uintptr_t)(CAMERA_DEV_BUFFER_ALIGN - 1U));
            dev_priv->dynbuf_raw[s][j]                      = raw;
            g_Videos[slot].pictureBuffer.bufs[j].buffer     = (uint8_t *)aligned;
            g_Videos[slot].pictureBuffer.bufs[j].size       = CAMERA_USB_MAX_BUFF_SIZE;
        }
    }
#endif /* USB_IMG_BUFFER_DYNALLOC */

    dev->data = dev_priv;

    /* Save config */
    dev->config.width              = config->width;
    dev->config.height             = config->height;
    dev->config.framerate          = config->fps;
    dev->config.format             = config->format;
    dev->config.stripe             = config->stripe;
    dev->cap.callback              = callback;
    dev->cap.param                 = param;
    dev->config.pitch              = config->width * get_bitpp(config->format) / 8;
    dev->config.stripe_size        = 0;
    dev->config.n_streams          = config->n_streams;
    dev->config.in_advance_enqueue = config->in_advance_enqueue;
    memcpy((void *) &dev->config.stream[0], (void *) &config->stream[0], sizeof(dev->config.stream));
    /* Pre-mark streams as requested for non-advance-enqueue mode (mirrors virtual_usb pattern) */
    for (int si = 0; si < n_streams; si++)
        dev->config.stream_requested[si] = !config->in_advance_enqueue;

    strncpy(dev->name, CAMERA_NAME, HAL_DEVICE_NAME_MAX_LENGTH);

    HAL_LOGD("--HAL_CameraDev_USB_Init (%u stream(s) assigned)\r\n", (unsigned)assigned);
    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Getbufdesc(const camera_dev_t *dev,
                                                  hw_buf_desc_t *out_buf,
                                                  mpp_memory_policy_t *policy)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_USB_Getbufdesc(out_buf=[%p])\r\n", out_buf);

    if ((out_buf == NULL) || (policy == NULL))
    {
        HAL_LOGE("NULL pointer to buffer descriptor\r\n");
        return kStatus_HAL_CameraError;
    }

    *policy            = HAL_MEM_ALLOC_OUTPUT;
    out_buf->alignment = CAMERA_DEV_BUFFER_ALIGN;
#if (defined(USB_IMG_BUFFER_DYNALLOC) || (!defined(USE_UNCACHED_JPG_BUFFERS) && !defined(USE_PSRAM_JPG_BUFFERS)))
    out_buf->cacheable = true;
#else
    out_buf->cacheable = false;
#endif
    out_buf->stride  = dev->config.pitch;
    out_buf->nb_lines = dev->config.height;
    out_buf->addr    = NULL;

    HAL_LOGD("--HAL_CameraDev_USB_Getbufdesc\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Deinit(camera_dev_t *dev)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    usb_status_t status = kStatus_USB_Success;

    HAL_LOGD("++HAL_CameraDev_USB_Deinit\r\n");

    usb_camera_private_data_t *dev_priv = (usb_camera_private_data_t *)dev->data;
    if (dev_priv != NULL)
    {
        /*
         * Step 1: mark every owned instance as no longer attached BEFORE any
         * pipe or transfer teardown.  USB_HostVideoAppStopStream triggers
         * USB_HostEhciCloseIso -> USB_HostEhciItdArrayDeinit, which fires
         * USB_HostVideoStreamDataInCallback for every cancelled ITD.  When
         * devState == kStatus_DEV_Attached that callback re-queues a new ISO
         * transfer onto the now-closing pipe, creating an infinite
         * cancel/re-queue loop.  Setting devState to kStatus_DEV_Idle here
         * makes the callback bail out immediately without re-queuing.
         */
        for (uint32_t s = 0U; s < dev_priv->n_streams; s++)
        {
            uint32_t slot = dev_priv->instanceIndex[s];
            if (slot < (uint32_t)USB_HOST_VIDEO_INST_NUM)
            {
                g_Videos[slot].devState = kStatus_DEV_Idle;
            }
        }

        /*
         * Step 2: close the ISO pipe for each owned stream by switching the
         * stream interface to alternate setting 0 (zero bandwidth).
         * devState is already Idle so ITD cancellation callbacks will not
         * re-queue new transfers.  The USB host task must still be running here.
         */
        for (uint32_t s = 0U; s < dev_priv->n_streams; s++)
        {
            uint32_t slot = dev_priv->instanceIndex[s];
            if (slot < (uint32_t)USB_HOST_VIDEO_INST_NUM)
            {
                USB_HostVideoAppStopStream(&g_Videos[slot]);
            }
        }

        /*
         * Step 3: delete the frame-ready queues.  The NULL guard in
         * USB_HostVideoStreamDataInCallback prevents a crash if the callback
         * fires during this window.
         */
        for (uint32_t s = 0U; s < dev_priv->n_streams; s++)
        {
            uint32_t slot = dev_priv->instanceIndex[s];
            if ((slot < (uint32_t)USB_HOST_VIDEO_INST_NUM) && (g_Videos[slot].mppQueue != NULL))
            {
                hal_queue_delete(g_Videos[slot].mppQueue);
                g_Videos[slot].mppQueue = NULL;
            }
        }

        /* Deinit the USB video class for each owned slot */
        for (uint32_t s = 0U; s < dev_priv->n_streams; s++)
        {
            uint32_t slot = dev_priv->instanceIndex[s];
            if ((slot < (uint32_t)USB_HOST_VIDEO_INST_NUM) && (g_Videos[slot].classHandle != NULL))
            {
                USB_HostVideoDeinit(g_Videos[slot].deviceHandle, g_Videos[slot].classHandle);
                g_Videos[slot].classHandle = NULL;
            }
        }

        /*
         * Step 4: remove each device from the host device list so that
         * USB_HostDeinit's while(deviceList != NULL) loop exits immediately
         * instead of spinning forever.
         */
        for (uint32_t s = 0U; s < dev_priv->n_streams; s++)
        {
            uint32_t slot = dev_priv->instanceIndex[s];
            if ((slot < (uint32_t)USB_HOST_VIDEO_INST_NUM) && (g_Videos[slot].deviceHandle != NULL))
            {
                status = USB_HostRemoveDevice(g_HostHandle, g_Videos[slot].deviceHandle);
                if (status != kStatus_USB_Success)
                    HAL_LOGE("Got error %d while calling USB_HostRemoveDevice\r\n", status);
                g_Videos[slot].deviceHandle = NULL;
            }
        }

        /* Delete per-stream USB host application tasks now that the ISO pipe is
         * closed and the mppQueues are deleted.  The app tasks spin on
         * USB_HostVideoTask which re-enters the USB middleware; it is safe to
         * delete them here because the pipe is already stopped above. */
        for (uint32_t j = 0U; j < (uint32_t)USB_HOST_VIDEO_INST_NUM; j++)
        {
            if (dev_priv->app_task[j] != NULL)
            {
                hal_task_delete(dev_priv->app_task[j]);
                dev_priv->app_task[j] = NULL;
            }
        }

        /* USB host controller and host task are intentionally kept alive across
         * deinit/init cycles.  g_HostHandle remains valid so that the next
         * HAL_CameraDev_USB_Init skips USB_HostInit and host task creation
         * (guarded by "if (g_HostHandle == NULL)").
         * Calling USB_HostDeinit here would trigger repeated "detach device"
         * callbacks until the cable is physically disconnected. */


#ifdef USB_IMG_BUFFER_DYNALLOC
        /* Free dynamically allocated picture buffers for each stream slot */
        for (uint32_t s = 0U; s < dev_priv->n_streams; s++)
        {
            uint32_t slot = dev_priv->instanceIndex[s];
            for (uint32_t j = 0U; j < USB_HOST_VIDEO_PIC_BUFFER_NUM; j++)
            {
                if (dev_priv->dynbuf_raw[s][j] != NULL)
                {
                    hal_free(dev_priv->dynbuf_raw[s][j]);
                    dev_priv->dynbuf_raw[s][j]                  = NULL;
                    g_Videos[slot].pictureBuffer.bufs[j].buffer = NULL;
                }
            }
        }
#endif /* USB_IMG_BUFFER_DYNALLOC */

        hal_free(dev_priv);
        dev->data = NULL;
    }

    HAL_LOGD("--HAL_CameraDev_USB_Deinit\r\n");

    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Start(const camera_dev_t *dev)
{
    HAL_LOGD("++HAL_CameraDev_USB_Start\r\n");

    if (dev->config.in_advance_enqueue)
    {
        /* Prime the first dequeue cycle by marking all active streams as enqueued */
        usb_camera_private_data_t *dev_priv = (usb_camera_private_data_t *)dev->data;
        if (dev_priv != NULL)
        {
            bool stream_active[USB_HOST_VIDEO_INST_NUM];
            memset(stream_active, 0, sizeof(stream_active));
            for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
                stream_active[i] = dev->config.stream[i].active;
            usb_mark_streams_enqueued(dev, dev_priv, stream_active);
        }
    }

    /* Streaming starts automatically once USB enumeration completes */
    HAL_LOGD("--HAL_CameraDev_USB_Start\r\n");
    return kStatus_HAL_CameraSuccess;
}

hal_camera_status_t HAL_CameraDev_USB_Stop(const camera_dev_t *dev)
{
    HAL_LOGD("++HAL_CameraDev_USB_Stop\r\n");

    usb_camera_private_data_t *dev_priv = (usb_camera_private_data_t *)dev->data;
    if (dev_priv != NULL)
    {
        hal_ctx_t ctx;
        for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
        {
            uint32_t vslot = dev_priv->instanceIndex[i];
            /* Only release if a buffer was actually received from the queue;
             * the sentinel means no frame has been dequeued yet for this stream. */
            if ((vslot < (uint32_t)USB_HOST_VIDEO_INST_NUM) &&
                (dev_priv->crt_usb_buffer_index[i] != USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX))
            {
                hal_atomic_enter(&ctx);
                USB_HostVideoReleaseBuffer(&g_Videos[vslot], dev_priv->crt_usb_buffer_index[i]);
                dev_priv->crt_usb_buffer_index[i] = USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX;
                hal_atomic_exit(&ctx);
            }
        }
        usb_reset_dequeue_enqueue_state(dev_priv);
    }

    HAL_LOGD("--HAL_CameraDev_USB_Stop\r\n");
    return kStatus_HAL_CameraSuccess;
}

hal_camera_status_t HAL_CameraDev_USB_Dequeue(const camera_dev_t *dev,
                                               void **data,
                                               int *stripe,
                                               int *compressed_size)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    hal_ctx_t ctx;

    usb_camera_private_data_t *dev_priv = (usb_camera_private_data_t *)dev->data;
    if (dev_priv == NULL)
    {
        HAL_LOGE("USB camera private data is NULL\r\n");
        return kStatus_HAL_CameraError;
    }

    HAL_LOGD("++HAL_CameraDev_USB_Dequeue (dequeued=%u enqueued=%u)\r\n",
             (unsigned)dev_priv->dequeued_streams, (unsigned)dev_priv->enqueued_streams);

    /*
     * On first dequeue call of a cycle (when in_advance_enqueue is false):
     * enqueue was not called externally, so mark all active streams as enqueued now.
     */
    if (!dev->config.in_advance_enqueue && dev_priv->enqueued_streams == 0U)
    {
        /* Count active+requested streams (mirrors virtual_usb inline-enqueue path) */
        uint32_t req_cnt = 0U;
        for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
        {
            if (dev->config.stream[i].active && dev->config.stream_requested[i])
                req_cnt++;
        }

        if (req_cnt == 0U)
        {
            HAL_LOGI("No active and requested stream found for inline enqueue\r\n");
            HAL_LOGD("--HAL_CameraDev_USB_Dequeue\r\n");
            return kStatus_HAL_CameraNoData;
        }

        bool stream_active[USB_HOST_VIDEO_INST_NUM];
        memset(stream_active, 0, sizeof(stream_active));
        for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
            stream_active[i] = dev->config.stream[i].active && dev->config.stream_requested[i];

        ret = usb_mark_streams_enqueued(dev, dev_priv, stream_active);
        if (ret != kStatus_HAL_CameraSuccess)
            return ret;
    }

    if (dev_priv->enqueued_streams == 0U)
    {
        HAL_LOGE("Dequeue called but no streams are enqueued\r\n");
        return kStatus_HAL_CameraError;
    }

    /* Guard: dequeue must not be called more times than enqueued streams */
    if (dev_priv->dequeued_streams >= dev_priv->enqueued_streams)
    {
        HAL_LOGE("Dequeue called too many times (%u >= %u)\r\n",
                 (unsigned)dev_priv->dequeued_streams, (unsigned)dev_priv->enqueued_streams);
        return kStatus_HAL_CameraError;
    }

    /* Advance cursor to the next enqueued stream */
    ret = usb_get_next_enqueued_stream(dev_priv, &dev_priv->crt_dequeue_stream_idx);
    if (ret != kStatus_HAL_CameraSuccess)
        return ret;

    uint32_t s    = dev_priv->crt_dequeue_stream_idx;
    uint32_t slot = dev_priv->instanceIndex[s];

    if (slot >= (uint32_t)USB_HOST_VIDEO_INST_NUM)
    {
        HAL_LOGE("Invalid USB video instance index %u\r\n", (unsigned)slot);
        return kStatus_HAL_CameraError;
    }

    /* Record caller's output pointers for this stream */
    dev_priv->stream_dequeue_req[dev_priv->dequeued_streams].addr            = data;
    dev_priv->stream_dequeue_req[dev_priv->dequeued_streams].compressed_size = compressed_size;
    dev_priv->dequeued_streams++;

    /* Prepare cursor for next call */
    if (dev_priv->dequeued_streams < dev_priv->enqueued_streams)
        dev_priv->crt_dequeue_stream_idx++;

    /*
     * Only on the LAST dequeue call of the cycle: release old buffers (if needed),
     * block-receive a new frame for every enqueued stream, fill caller outputs, reset state.
     */
    if (dev_priv->dequeued_streams == dev_priv->enqueued_streams)
    {
        uint32_t cursor = 0U;
        for (uint32_t si = 0U; si < dev_priv->n_streams; si++)
        {
            if (!dev_priv->stream_enqueued[si])
                continue;

            uint32_t vslot = dev_priv->instanceIndex[si];
            usb_frame_ready_msg_t msg;

            /* Release previously held buffer so USB task can reuse it.
             * When in_advance_enqueue is false, the release happens here (inline).
             * When in_advance_enqueue is true, the release has already happened in Enqueue.
             * Skip the release if no frame has been received yet (sentinel value). */
            if (!dev->config.in_advance_enqueue &&
                (dev_priv->crt_usb_buffer_index[si] != USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX))
            {
                hal_atomic_enter(&ctx);
                USB_HostVideoReleaseBuffer(&g_Videos[vslot], dev_priv->crt_usb_buffer_index[si]);
                dev_priv->crt_usb_buffer_index[si] = USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX;
                hal_atomic_exit(&ctx);
            }

            /* Wait for the next completed frame on this stream's queue */
            if (!hal_queue_receive(g_Videos[vslot].mppQueue, &msg, HAL_MAX_TIMEOUT))
            {
                HAL_LOGE("Queue receive error on slot %u\r\n", (unsigned)vslot);
                usb_reset_dequeue_enqueue_state(dev_priv);
                return kStatus_HAL_CameraError;
            }

            dev_priv->crt_usb_buffer_index[si] = msg.pictureBufferIndex;

            if ((msg.pictureLength == 0U) || (msg.pictureLength > CAMERA_USB_MAX_BUFF_SIZE))
            {
                HAL_LOGE("Invalid buffer size %u on slot %u\r\n",
                         (unsigned)msg.pictureLength, (unsigned)vslot);
                usb_reset_dequeue_enqueue_state(dev_priv);
                return kStatus_HAL_CameraError;
            }

            /* Fill caller's output variables recorded during earlier dequeue calls */
            if (dev_priv->stream_dequeue_req[cursor].addr != NULL)
                *(dev_priv->stream_dequeue_req[cursor].addr) = (void *)msg.pictureBuffer;

            if (dev_priv->stream_dequeue_req[cursor].compressed_size != NULL)
            {
                *(dev_priv->stream_dequeue_req[cursor].compressed_size) =
                    (dev->config.format == MPP_PIXEL_JPEG) ? (int)msg.pictureLength : 0;
            }

            cursor++;
        }

        usb_reset_dequeue_enqueue_state(dev_priv);
    }

    *stripe = 0;

    HAL_LOGD("--HAL_CameraDev_USB_Dequeue\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Enqueue(const camera_dev_t *dev, void *data)
{
    (void)data;
    HAL_LOGD("++HAL_CameraDev_USB_Enqueue\r\n");

    usb_camera_private_data_t *dev_priv = (usb_camera_private_data_t *)dev->data;

    if (dev_priv == NULL)
    {
        HAL_LOGE("USB camera private data is NULL\r\n");
        return kStatus_HAL_CameraError;
    }

    /*
     * When in_advance_enqueue is true, Enqueue is called after the downstream pipeline
     * has finished consuming the previously dequeued frame. Release the old buffers now
     * so the USB task can reuse them for the next frame.
     */
    if (dev->config.in_advance_enqueue)
    {
        hal_ctx_t ctx;
        for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
        {
            uint32_t vslot = dev_priv->instanceIndex[i];
            /* Only release if a frame was actually received; skip the sentinel. */
            if ((vslot < (uint32_t)USB_HOST_VIDEO_INST_NUM) &&
                (dev_priv->crt_usb_buffer_index[i] != USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX))
            {
                hal_atomic_enter(&ctx);
                USB_HostVideoReleaseBuffer(&g_Videos[vslot], dev_priv->crt_usb_buffer_index[i]);
                dev_priv->crt_usb_buffer_index[i] = USB_HOST_VIDEO_INVALID_PIC_BUFFER_IDX;
                hal_atomic_exit(&ctx);
            }
        }
    }

    /*
     * Mark all requested+active streams as enqueued so Dequeue knows which
     * slots to serve this cycle. Mirrors camera_dev_enqueue() in virtual_usb.
     */
    bool stream_active[USB_HOST_VIDEO_INST_NUM];
    memset(stream_active, 0, sizeof(stream_active));
    for (uint32_t i = 0U; i < dev_priv->n_streams; i++)
        stream_active[i] = dev->config.stream[i].active && dev->config.stream_requested[i];

    hal_camera_status_t ret = usb_mark_streams_enqueued(dev, dev_priv, stream_active);

    HAL_LOGD("--HAL_CameraDev_USB_Enqueue (enqueued=%u)\r\n", (unsigned)dev_priv->enqueued_streams);
    return ret;
}

const static camera_dev_operator_t camera_dev_usb_ops = {
    .init         = HAL_CameraDev_USB_Init,
    .deinit       = HAL_CameraDev_USB_Deinit,
    .start        = HAL_CameraDev_USB_Start,
    .stop         = HAL_CameraDev_USB_Stop,
    .enqueue      = HAL_CameraDev_USB_Enqueue,
    .dequeue      = HAL_CameraDev_USB_Dequeue,
    .get_buf_desc = HAL_CameraDev_USB_Getbufdesc,
    .lock         = NULL,
    .unlock       = NULL
};

int HAL_CameraDev_USB_setup(const char *name, camera_dev_t *dev)
{
    dev->ops = &camera_dev_usb_ops;
    return 0;
}

#else /* (defined HAL_ENABLE_CAMERA) && (HAL_ENABLE_CAMERA_DEV_USB == 1) */

int HAL_CameraDev_USB_setup(const char *name, camera_dev_t *dev, _Bool defconfig)
{
    HAL_LOGE("Camera USB not enabled\r\n");
    return -1;
}

#endif /* (defined HAL_ENABLE_CAMERA) && (HAL_ENABLE_CAMERA_DEV_USB == 1) */
