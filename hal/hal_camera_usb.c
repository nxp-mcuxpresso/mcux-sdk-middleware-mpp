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
 */

#include "mpp_config.h"
#include "hal_camera_dev.h"
#include "hal_debug.h"

#if (defined HAL_ENABLE_CAMERA) && (HAL_ENABLE_CAMERA_DEV_USB == 1)
#include <stdlib.h>

#include "board.h"

#include "hal.h"
#include "hal_utils.h"
#include "hal_os.h"

#include "app.h"

/* 	USB includes */
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
#define USB_HOST_TASK_SIZE      2500L / sizeof(portSTACK_TYPE)
#define USB_HOST_APP_TASK_SIZE  (20000L+5000L) / sizeof(portSTACK_TYPE)

#define USB_HOST_TASK_PRIORITY     5
#define USB_HOST_APP_TASK_PRIORITY 1

typedef struct usb_camera_private_data {
    uint32_t crt_usb_buffer_index;
} usb_camera_private_data_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

usb_host_handle g_HostHandle;
extern usb_host_video_camera_instance_t g_Video;

/*!
 * @brief host callback function.
 *
 * device attach/detach callback function.
 *
 * @param deviceHandle          device handle.
 * @param configurationHandle   attached device's configuration descriptor information.
 * @param eventCode             callback event code, please reference to enumeration host_event_t.
 *
 * @retval kStatus_USB_Success              The host is initialized successfully.
 * @retval kStatus_USB_NotSupported         The application doesn't support the configuration.
 */
static usb_status_t USB_HostEvent(usb_device_handle deviceHandle,
                                  usb_host_configuration_handle configurationHandle,
                                  uint32_t eventCode);

/*!
 * @brief application initialization.
 */
static void USB_HostApplicationInit(void);

/*!
 * @brief host freertos task function.
 *
 * @param g_HostHandle   host handle
 */
static void USB_HostTask(void *param);

/*!
 * @brief host video freertos task function.
 *
 * @param param   the host video instance pointer.
 */
static void USB_HostApplicationTask(void *param);

extern void USB_HostClockInit(void);
extern void USB_HostIsrEnable(void);
extern void USB_HostTaskFn(void *param);

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief host callback function.
 *
 * device attach/detach callback function.
 *
 * @param deviceHandle           device handle.
 * @param configurationHandle attached device's configuration descriptor information.
 * @param eventCode           callback event code, please reference to enumeration host_event_t.
 *
 * @retval kStatus_USB_Success              The host is initialized successfully.
 * @retval kStatus_USB_NotSupported         The application don't support the configuration.
 */
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
            usb_echo("device not supported.\r\n");
            break;

        case kUSB_HostEventEnumerationDone:
            status = USB_HostVideoEvent(deviceHandle, configurationHandle, eventCode);
            break;

        case kUSB_HostEventDetach:
            status = USB_HostVideoEvent(deviceHandle, configurationHandle, eventCode);
            break;

        case kUSB_HostEventEnumerationFail:
            usb_echo("enumeration failed\r\n");
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
        usb_echo("host init error\r\n");
        return;
    }
    USB_HostIsrEnable();

    usb_echo("host init done\r\n");
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

hal_camera_status_t HAL_CameraDev_USB_Init(
    camera_dev_t *dev, mpp_camera_params_t *config, camera_dev_callback_t callback, void *param)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_USB_Init( param[%p])\r\n", param);
    
    if ((config->width > CAMERA_USB_MAX_WIDTH) || (config->height > CAMERA_USB_MAX_HEIGHT) ||
    		(config->width <= 0) || (config->height <= 0))
    {
        HAL_LOGE("Camera resolution unsupported\r\n");
        return kStatus_HAL_CameraError;
    }

    if ((config->format != MPP_PIXEL_YUYV) && (config->format != MPP_PIXEL_JPEG))
    {
        HAL_LOGE("Camera format %d unsupported\r\n", config->format);
        return kStatus_HAL_CameraError;
    }

    if (config->stripe != false)
    {
        HAL_LOGE("Camera stripe unsupported\r\n");
        return kStatus_HAL_CameraError;
    }


    USB_HostApplicationInit();

    g_Video.mppQueue = xQueueCreate(2, sizeof(usb_frame_ready_msg_t));

    if (g_Video.mppQueue == NULL)
    {
        HAL_LOGE("Failed to create frame ready receive queue\r\n");
        return kStatus_HAL_CameraError;
    }


    if (xTaskCreate(USB_HostTask, "usb host task", USB_HOST_TASK_SIZE, g_HostHandle, USB_HOST_TASK_PRIORITY, NULL) != pdPASS)
    {
    	usb_echo("create host task error\r\n");
    }

    if (xTaskCreate(USB_HostApplicationTask, "usb app task", USB_HOST_APP_TASK_SIZE, &g_Video, USB_HOST_APP_TASK_PRIORITY, NULL) != pdPASS)
    {
    	usb_echo("create video task error\r\n");
    }

    dev->data = hal_malloc(sizeof(usb_camera_private_data_t));
    if (dev->data == NULL)
    {
        HAL_LOGE("Failed to allocate memory for USB camera private data\r\n");
        return kStatus_HAL_CameraError;
    }

    /* save config */
    dev->config.width = config->width;
    dev->config.height = config->height;
    dev->config.framerate = config->fps;
    dev->config.format = config->format;
    dev->config.stripe = config->stripe;
    dev->cap.callback = callback;
    dev->cap.param    = param;
    dev->config.pitch = config->width * get_bitpp(config->format) / 8;
    dev->config.stripe_size = 0;
    dev->config.n_streams = config->n_streams;
    dev->config.in_advance_enqueue = config->in_advance_enqueue;

    strncpy(dev->name, CAMERA_NAME, HAL_DEVICE_NAME_MAX_LENGTH);

    HAL_LOGD("--HAL_CameraDev_USB_Init\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Getbufdesc(const camera_dev_t *dev, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_USB_Getbufdesc(out_buf=[%p])\r\n", out_buf);

    if ((out_buf == NULL) || (policy == NULL))
    {
        HAL_LOGE("NULL pointer to buffer descriptor\r\n");
        return kStatus_HAL_CameraError;
    }
    
    /* set memory policy */
    *policy = HAL_MEM_ALLOC_OUTPUT;
    out_buf->alignment = CAMERA_DEV_BUFFER_ALIGN;
#if (defined(USB_IMG_BUFFER_DYNALLOC) || (!defined(USE_UNCACHED_JPG_BUFFERS) && !defined(USE_PSRAM_JPG_BUFFERS)))
    out_buf->cacheable = true; /* TODO check cacheability */
#else
    out_buf->cacheable = false;
#endif
    out_buf->stride = dev->config.pitch;
    out_buf->nb_lines = dev->config.height;
    out_buf->addr = NULL;    /* TODO provide 1st buffer or NULL */

    HAL_LOGD("--HAL_CameraDev_USB_Getbufdesc\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Deinit(camera_dev_t *dev)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    status_t status = kStatus_Success;

    if (dev->data != NULL)
    {
        hal_free(dev->data);
        dev->data = NULL;
    }

    /* TODO destroy the queues*/

    if (status != kStatus_Success) return kStatus_HAL_CameraError;

    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Start(const camera_dev_t *dev)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_USB_Start\r\n");

    /* TODO start camera capture here */


    HAL_LOGD("--HAL_CameraDev_USB_Start\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Stop(const camera_dev_t *dev)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    HAL_LOGD("++HAL_CameraDev_USB_Stop\r\n");

    /* TODO */

    HAL_LOGD("--HAL_CameraDev_USB_Stop\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Dequeue(const camera_dev_t *dev, void **data, int *stripe, int *compressed_size)
{
    hal_camera_status_t ret = kStatus_HAL_CameraSuccess;
    usb_frame_ready_msg_t msg;
    void *mpp_buffer = NULL;
    uint32_t mpp_buffer_size = 0;

    usb_camera_private_data_t *dev_priv = (usb_camera_private_data_t *)dev->data;
    if (dev_priv == NULL)
    {
        HAL_LOGE("USB camera private data is NULL\r\n");
        return kStatus_HAL_CameraError;
    }

    HAL_LOGD("++HAL_CameraDev_USB_Dequeue\r\n");

    if( xQueueReceive(g_Video.mppQueue, &msg, portMAX_DELAY ) == pdPASS )
    {
        mpp_buffer = (void *) msg.pictureBuffer;
        mpp_buffer_size = msg.pictureLength;
        dev_priv->crt_usb_buffer_index = msg.pictureBufferIndex;
    }
    else
    {
        HAL_LOGE("++HAL_CameraDev_USB_Dequeue: Unexpected camera queue receive error\r\n");
        return kStatus_HAL_CameraError;
    }

    if ((mpp_buffer_size == 0) || (mpp_buffer_size > CAMERA_USB_MAX_BUFF_SIZE))
    {
        HAL_LOGE("Invalid buffer size\r\n");
        return kStatus_HAL_CameraError;
    }
    *data = mpp_buffer;

    if (dev->config.format == MPP_PIXEL_JPEG)
        *compressed_size = mpp_buffer_size;
    else
        *compressed_size = 0;

    *stripe = 0;

    HAL_LOGD("--HAL_CameraDev_USB_Dequeue\r\n");
    return ret;
}

hal_camera_status_t HAL_CameraDev_USB_Enqueue(const camera_dev_t *dev, void *data)
{
    int error = 0;
    HAL_LOGD("++HAL_CameraDev_USB_Enqueue\r\n");

    hal_ctx_t ctx;
    usb_camera_private_data_t *dev_priv = (usb_camera_private_data_t *)dev->data;

    if (dev_priv == NULL)
    {
        HAL_LOGE("USB camera private data is NULL\r\n");
        return kStatus_HAL_CameraError;
    }

    hal_atomic_enter(&ctx);
    g_Video.pictureBufferState[dev_priv->crt_usb_buffer_index] = 0;
    hal_atomic_exit(&ctx);

    HAL_LOGD("--HAL_CameraDev_USB_Enqueue\r\n");
    return error;
}

const static camera_dev_operator_t camera_dev_usb_ops = {
    .init        = HAL_CameraDev_USB_Init,
    .deinit      = HAL_CameraDev_USB_Deinit,
    .start       = HAL_CameraDev_USB_Start,
    .stop        = HAL_CameraDev_USB_Stop,
    .enqueue     = HAL_CameraDev_USB_Enqueue,
    .dequeue     = HAL_CameraDev_USB_Dequeue,
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
