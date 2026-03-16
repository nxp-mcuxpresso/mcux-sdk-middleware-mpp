/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr-generic Camera HAL Implementation
 * Hardware-agnostic bridge between MPP and Zephyr Video API
 */

#include <zephyr/drivers/video-controls.h>
#include <zephyr/drivers/video.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "mpp_config.h"
#include "hal_camera_dev.h"
#include "hal_utils.h"
#include "mpp_api.h"

#if (defined HAL_ENABLE_CAMERA) && (HAL_ENABLE_CAMERA_DEV_ZEPHYR == 1)

/* Debug control - set to 1 to enable debug prints for this file */
#define HAL_CAMERA_DEBUG 0
#if HAL_CAMERA_DEBUG
#define DEBUG_PRINT(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)                                                  \
    do {                                                                       \
    } while (0)
#endif
#define HAL_CAM_ZEPHYR_BUFF_ALIGN 64

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define CAMERA_BUFFER_COUNT 2 /* Number of buffers to allocate */
#define CAMERA_FRAME_THREAD_STACK_SIZE 2048
#define CAMERA_FRAME_THREAD_PRIORITY K_PRIO_COOP(5)

/* Camera operation mode selection */
#ifndef CONFIG_CAMERA_MODE_EVENT_DRIVEN
#define CONFIG_CAMERA_MODE_EVENT_DRIVEN                                        \
    0 /* 0 = POLLING (default), 1 = EVENT_DRIVEN */
#endif

/* Camera operation mode */
typedef enum {
    CAMERA_MODE_POLLING,     /* Polling mode - dequeue blocks */
    CAMERA_MODE_EVENT_DRIVEN /* Event-driven - signal on frame ready */
} camera_mode_t;

/* Private data structure for Zephyr camera device */
typedef struct {
    const struct device *video_dev;
    struct video_buffer *vbufs[CAMERA_BUFFER_COUNT];
    struct video_buffer *current_vbuf;
    uint32_t buffer_size;
    uint8_t num_buffers;
    bool streaming;

    /* Event-driven mode support */
    camera_mode_t mode;
#if CONFIG_CAMERA_MODE_EVENT_DRIVEN
    struct k_poll_signal frame_signal;
    struct k_thread frame_thread;
    k_thread_stack_t *frame_stack;
#endif

    /* Reference to camera device for thread callback */
    camera_dev_t *dev;
} camera_zephyr_data_t;

/*******************************************************************************
 * Zephyr Video API to MPP Format Conversion (Hardware Agnostic)
 ******************************************************************************/

/* Convert MPP pixel format to Zephyr video format */
static uint32_t mpp_to_zephyr_pixfmt(mpp_pixel_format_t mpp_fmt)
{
    switch (mpp_fmt) {
    case MPP_PIXEL_YUV1P444:
        return VIDEO_PIX_FMT_XYUV32;
    case MPP_PIXEL_VYUY1P422:
        return VIDEO_PIX_FMT_VYUY;
    case MPP_PIXEL_UYVY1P422:
        return VIDEO_PIX_FMT_UYVY;
    case MPP_PIXEL_YUYV:
        return VIDEO_PIX_FMT_YUYV;
    case MPP_PIXEL_RGB565:
        return VIDEO_PIX_FMT_RGB565;
    case MPP_PIXEL_RGB:
        return VIDEO_PIX_FMT_RGB24;
    case MPP_PIXEL_BGR:
        return VIDEO_PIX_FMT_BGR24;
    case MPP_PIXEL_ARGB:
        return VIDEO_PIX_FMT_ARGB32;
    case MPP_PIXEL_BGRA:
        return VIDEO_PIX_FMT_BGRA32;
    case MPP_PIXEL_RGBA:
        return VIDEO_PIX_FMT_RGBA32;
    case MPP_PIXEL_BGRX:
        return VIDEO_PIX_FMT_BGRA32;
    case MPP_PIXEL_RGBX:
        return VIDEO_PIX_FMT_RGBA32;
    case MPP_PIXEL_GRAY:
        return VIDEO_PIX_FMT_GREY;
    default:
        DEBUG_PRINT("Warning: Unknown MPP format %d, defaulting to YUYV\n",
                    mpp_fmt);
    }
    return VIDEO_PIX_FMT_YUYV;
}

/*******************************************************************************
 * Event-Driven Mode Support
 ******************************************************************************/

#if CONFIG_CAMERA_MODE_EVENT_DRIVEN
/**
 * @brief Thread that waits for frame ready signals from the video driver
 *
 * This thread mimics interrupt-driven behavior by waiting on signals from
 * the video driver and calling the MPP callback when frames are available.
 */
static void camera_frame_thread(void *p1, void *p2, void *p3)
{
    camera_zephyr_data_t *priv_data = (camera_zephyr_data_t *) p1;
    camera_dev_t *dev = priv_data->dev;
    struct k_poll_event events[1];
    int result;

    DEBUG_PRINT("Camera frame thread started\n");

    k_poll_event_init(&events[0], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
                      &priv_data->frame_signal);

    while (priv_data->streaming) {
        /* Wait for frame ready signal from driver */
        result = k_poll(events, 1, K_MSEC(1000));

        if (result == 0 && events[0].state == K_POLL_STATE_SIGNALED) {
            /* Frame is ready - notify MPP callback if registered */
            if (dev->cap.callback != NULL) {
                uint8_t fromISR = 0;
                dev->cap.callback(dev, kCameraEvent_SendFrame, dev->cap.param,
                                  fromISR);
            } else {
                DEBUG_PRINT("DEBUG: Frame ready but no callback registered\n");
            }

            /* Reset signal for next frame */
            k_poll_signal_reset(&priv_data->frame_signal);
            events[0].state = K_POLL_STATE_NOT_READY;
        } else if (result == -EAGAIN) {
            /* Timeout - continue waiting */
            continue;
        } else if (result != 0) {
            DEBUG_PRINT("WARNING: k_poll failed with error: %d\n", result);
        }
    }

    DEBUG_PRINT("Camera frame thread exiting\n");
}
#endif /* CONFIG_CAMERA_MODE_EVENT_DRIVEN */

/**
 * @brief Detect camera operation mode and configure accordingly
 *
 * This function queries the video driver to determine if it supports
 * event-driven mode (signals) or requires polling mode.
 *
 * @param dev Camera device structure
 * @param priv_data Private data structure
 * @return 0 on success, negative error code on failure
 */
static int camera_detect_and_configure_mode(camera_dev_t *dev,
                                            camera_zephyr_data_t *priv_data)
{
#if CONFIG_CAMERA_MODE_EVENT_DRIVEN
    int ret;

    DEBUG_PRINT("Detecting camera operation mode...\n");

    /* Initialize signal structure */
    k_poll_signal_init(&priv_data->frame_signal);

    /* Try to register signal with video driver */
    ret = video_set_signal(priv_data->video_dev, &priv_data->frame_signal);

    if (ret == 0) {
        /* Driver supports event-driven mode */
        priv_data->mode = CAMERA_MODE_EVENT_DRIVEN;
        DEBUG_PRINT("Camera mode: EVENT-DRIVEN (driver supports signals)\n");
        DEBUG_PRINT(
            "  Frame notifications will be handled via k_poll events\n");
        return 0;

    } else if (ret == -ENOSYS) {
        /* Driver does not implement video_set_signal() */
        DEBUG_PRINT("WARNING: CONFIG_CAMERA_MODE_EVENT_DRIVEN=1 but driver "
                    "does not support signals\n");
        DEBUG_PRINT("         Falling back to POLLING mode\n");
        priv_data->mode = CAMERA_MODE_POLLING;
        return 0;

    } else {
        /* Unexpected error - fall back to polling */
        DEBUG_PRINT("WARNING: video_set_signal() failed with error %d\n", ret);
        DEBUG_PRINT("         Falling back to POLLING mode\n");
        priv_data->mode = CAMERA_MODE_POLLING;
        return 0;
    }
#else
    /* Polling mode explicitly selected */
    priv_data->mode = CAMERA_MODE_POLLING;
    DEBUG_PRINT("Camera mode: POLLING\n");
    DEBUG_PRINT("  Frame dequeue will use blocking calls with timeout\n");
    return 0;
#endif
}

/*******************************************************************************
 * Helper Functions for Camera Initialization
 ******************************************************************************/

/**
 * @brief Get camera device from devicetree
 */
static const struct device *camera_get_device(void)
{
    const struct device *video_dev;

    video_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_camera));
    if (!video_dev) {
        DEBUG_PRINT("ERROR: Camera device not found in devicetree\n");
        DEBUG_PRINT("       Make sure 'zephyr,camera' is chosen in DTS\n");
        return NULL;
    }

    if (!device_is_ready(video_dev)) {
        DEBUG_PRINT("ERROR: Camera device '%s' not ready\n", video_dev->name);
        return NULL;
    }

    return video_dev;
}

/**
 * @brief Get camera capabilities from driver
 */
static int camera_get_capabilities(const struct device *video_dev,
                                   struct video_caps *caps)
{
    int ret;

    memset(caps, 0, sizeof(*caps));
    caps->type = VIDEO_BUF_TYPE_OUTPUT;

    ret = video_get_caps(video_dev, caps);
    if (ret) {
        DEBUG_PRINT("WARNING: Failed to get camera capabilities: %d\n", ret);
        /* Continue anyway - not all drivers implement this */
    } else {
        DEBUG_PRINT("Camera capabilities: min_vbuf_count=%d\n",
                    caps->min_vbuf_count);
    }

    return 0; /* Non-fatal */
}

/**
 * @brief Allocate and initialize private data structure
 */
static camera_zephyr_data_t *
camera_allocate_private_data(const struct device *video_dev, camera_dev_t *dev)
{
    camera_zephyr_data_t *priv_data;

    priv_data = k_malloc(sizeof(camera_zephyr_data_t));
    if (!priv_data) {
        DEBUG_PRINT("ERROR: Failed to allocate camera private data\n");
        return NULL;
    }

    memset(priv_data, 0, sizeof(camera_zephyr_data_t));
    priv_data->video_dev = video_dev;
    priv_data->streaming = false;
    priv_data->dev = dev;

#if CONFIG_CAMERA_MODE_EVENT_DRIVEN
    /* Allocate thread stack for event-driven mode */
    priv_data->frame_stack =
        k_thread_stack_alloc(CAMERA_FRAME_THREAD_STACK_SIZE, 0);
    if (!priv_data->frame_stack) {
        DEBUG_PRINT("ERROR: Failed to allocate camera frame thread stack\n");
        k_free(priv_data);
        return NULL;
    }
#endif

    return priv_data;
}
/**
 * @brief Configure camera video format (resolution, pixel format)
 */
static int camera_configure_format(const struct device *video_dev,
                                   mpp_camera_params_t *config,
                                   uint32_t *buffer_size)
{
    struct video_format fmt;
    uint32_t fmt_size;
    int ret;

    /* Set video format */
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = VIDEO_BUF_TYPE_OUTPUT;
    fmt.pixelformat = mpp_to_zephyr_pixfmt(config->format);
    fmt.width = config->width;
    fmt.height = config->height;
    fmt.pitch = config->width * get_bitpp(config->format) / 8;

    ret = video_set_format(video_dev, &fmt);
    if (ret) {
        DEBUG_PRINT("ERROR: Failed to set camera format: %d\n", ret);
        return ret;
    }

    /* Get the actual format set by the driver (may be adjusted) */
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = VIDEO_BUF_TYPE_OUTPUT;
    ret = video_get_format(video_dev, &fmt);

#if KERNEL_VERSION_NUMBER < 40300
    fmt_size = fmt.pitch * fmt.height;
#else
    /* fmt.size is available in Zephyr >= 4.3.0 */
    fmt_size = fmt.size;
#endif

    if (ret) {
        DEBUG_PRINT("WARNING: Failed to get camera format: %d\n", ret);
        /* Use requested size as fallback */
        *buffer_size =
            config->width * config->height * get_bitpp(config->format) / 8;
    } else {
        DEBUG_PRINT("Camera format set: %dx%d, pitch=%d, size=%d\n", fmt.width,
                    fmt.height, fmt.pitch, fmt_size);
        *buffer_size = fmt_size;
    }

    return 0;
}

/**
 * @brief Configure camera framerate
 */
static int camera_configure_framerate(const struct device *video_dev,
                                      uint32_t fps)
{
    struct video_frmival frmival;
    int ret;

    /* Set frame interval (inverse of framerate)
     * Frame interval = 1/fps (e.g., 30 fps = 1/30 second per frame)
     */
    memset(&frmival, 0, sizeof(frmival));
    frmival.numerator = 1;
    frmival.denominator = fps;

    ret = video_set_frmival(video_dev, &frmival);
    if (ret) {
        DEBUG_PRINT("WARNING: Failed to set framerate to %d fps: %d\n", fps,
                    ret);
        DEBUG_PRINT("         Camera may be running at default framerate\n");
        /* Non-fatal - not all drivers support this */
    } else {
        DEBUG_PRINT("Camera framerate set to %d fps\n", fps);

        /* Verify the setting by reading it back */
        memset(&frmival, 0, sizeof(frmival));
        ret = video_get_frmival(video_dev, &frmival);
        if (ret == 0) {
            DEBUG_PRINT("  Actual framerate: %d fps (%d/%d)\n",
                   frmival.denominator / frmival.numerator,
                   frmival.denominator, frmival.numerator);
        }
    }

    return 0; /* Non-fatal */
}

/**
 * @brief Allocate video buffers for camera
 */
static int camera_allocate_buffers(camera_zephyr_data_t *priv_data,
                                   uint32_t buffer_size, uint8_t num_buffers)
{
    DEBUG_PRINT("Allocating %d camera buffers of size %d bytes\n", num_buffers,
                buffer_size);

    for (int i = 0; i < num_buffers; i++) {
        priv_data->vbufs[i] = video_buffer_aligned_alloc(buffer_size,
                HAL_CAM_ZEPHYR_BUFF_ALIGN, K_NO_WAIT);
        if (!priv_data->vbufs[i]) {
            DEBUG_PRINT("ERROR: Failed to allocate video buffer %d\n", i);
            /* Free previously allocated buffers */
            for (int j = 0; j < i; j++) {
                video_buffer_release(priv_data->vbufs[j]);
            }
            return -ENOMEM;
        }
        priv_data->vbufs[i]->type = VIDEO_BUF_TYPE_OUTPUT;
        DEBUG_PRINT("  Buffer %d allocated at %p (size=%d)\n", i,
                    priv_data->vbufs[i]->buffer, priv_data->vbufs[i]->size);
    }

    priv_data->num_buffers = num_buffers;
    priv_data->buffer_size = buffer_size;

    return 0;
}

/*******************************************************************************
 * HAL Camera Device Operations
 ******************************************************************************/

static hal_camera_status_t
HAL_CameraDev_Zephyr_Init(camera_dev_t *dev, mpp_camera_params_t *config,
                          camera_dev_callback_t callback, void *param)
{
    const struct device *video_dev;
    camera_zephyr_data_t *priv_data;
    struct video_caps caps;
    uint32_t buffer_size;
    uint8_t num_buffers;
    int ret;

    if (!dev || !config) {
        return kStatus_HAL_CameraError;
    }

    /* Get camera device from devicetree */
    video_dev = camera_get_device();
    if (!video_dev) {
        return kStatus_HAL_CameraError;
    }

    /* Get camera capabilities */
    camera_get_capabilities(video_dev, &caps);

    /* Allocate and initialize private data */
    priv_data = camera_allocate_private_data(video_dev, dev);
    if (!priv_data) {
        return kStatus_HAL_CameraError;
    }
    dev->data = priv_data;

    /* Store callback */
    dev->cap.callback = callback;
    dev->cap.param = param;

    /* Detect and configure camera operation mode */
    ret = camera_detect_and_configure_mode(dev, priv_data);
    if (ret != 0) {
        DEBUG_PRINT("ERROR: Failed to configure camera mode\n");
        k_free(priv_data);
        return kStatus_HAL_CameraError;
    }

    /* Configure video format (resolution, pixel format) */
    ret = camera_configure_format(video_dev, config, &buffer_size);
    if (ret != 0) {
        k_free(priv_data);
        return kStatus_HAL_CameraError;
    }

    /* Configure framerate */
    camera_configure_framerate(video_dev, config->fps);

    /* Determine number of buffers to allocate */
    num_buffers = CAMERA_BUFFER_COUNT;
    if (caps.min_vbuf_count > 0 && caps.min_vbuf_count < CAMERA_BUFFER_COUNT) {
        num_buffers = caps.min_vbuf_count;
    }

    /* Allocate video buffers */
    ret = camera_allocate_buffers(priv_data, buffer_size, num_buffers);
    if (ret != 0) {
        k_free(priv_data);
        return kStatus_HAL_CameraError;
    }

    /* Update device config */
    dev->config.width = config->width;
    dev->config.height = config->height;
    dev->config.format = config->format;
    dev->config.framerate = config->fps;
    dev->config.stripe = config->stripe;

    DEBUG_PRINT("Camera HAL: Device '%s' initialized successfully\n",
                video_dev->name);

    return kStatus_HAL_CameraSuccess;
}
static hal_camera_status_t HAL_CameraDev_Zephyr_Deinit(camera_dev_t *dev)
{
    camera_zephyr_data_t *priv_data;

    if (!dev || !dev->data) {
        return kStatus_HAL_CameraError;
    }

    priv_data = (camera_zephyr_data_t *) dev->data;

    /* Free video buffers */
    for (int i = 0; i < priv_data->num_buffers; i++) {
        if (priv_data->vbufs[i]) {
            video_buffer_release(priv_data->vbufs[i]);
        }
    }

#if CONFIG_CAMERA_MODE_EVENT_DRIVEN
    /* Free thread stack if allocated */
    if (priv_data->frame_stack) {
        k_thread_stack_free(priv_data->frame_stack);
    }
#endif

    k_free(priv_data);
    dev->data = NULL;

    return kStatus_HAL_CameraSuccess;
}
static hal_camera_status_t HAL_CameraDev_Zephyr_Start(const camera_dev_t *dev)
{
    camera_zephyr_data_t *priv_data;
    int ret;
    static bool first_time = true;

    if (!dev || !dev->data) {
        return kStatus_HAL_CameraError;
    }

    priv_data = (camera_zephyr_data_t *) dev->data;

    if (priv_data->streaming) {
        DEBUG_PRINT("Camera already streaming\n");
        return kStatus_HAL_CameraSuccess;
    }

    if (first_time) {
        /* Enqueue all buffers before starting */
        DEBUG_PRINT("Enqueuing %d buffers before starting stream\n",
               priv_data->num_buffers);
        for (int i = 0; i < priv_data->num_buffers; i++) {
            ret = video_enqueue(priv_data->video_dev, priv_data->vbufs[i]);
            if (ret) {
                DEBUG_PRINT("ERROR: Failed to enqueue buffer %d: %d\n", i, ret);
                return kStatus_HAL_CameraError;
            }

            DEBUG_PRINT("  Buffer %d @ %p enqueued\n", i,
                        priv_data->vbufs[i]->buffer);
        }
    }

    /* Set streaming flag before starting thread */
    priv_data->streaming = true;

    if (first_time) {
        first_time = false;
#if CONFIG_CAMERA_MODE_EVENT_DRIVEN
        /* Start event-driven thread if supported */
        if (priv_data->mode == CAMERA_MODE_EVENT_DRIVEN) {
            DEBUG_PRINT("Starting camera in EVENT-DRIVEN mode\n");
            k_thread_create(&priv_data->frame_thread, priv_data->frame_stack,
                            CAMERA_FRAME_THREAD_STACK_SIZE, camera_frame_thread,
                            priv_data, NULL, NULL, CAMERA_FRAME_THREAD_PRIORITY, 0,
                            K_NO_WAIT);
            k_thread_name_set(&priv_data->frame_thread, "camera_frames");
        } else {
            DEBUG_PRINT("Starting camera in POLLING mode\n");
        }
#endif

        /* Start streaming */
        DEBUG_PRINT("Starting camera stream...\n");
        ret = video_stream_start(priv_data->video_dev, VIDEO_BUF_TYPE_OUTPUT);
        if (ret) {
            DEBUG_PRINT("ERROR: Failed to start camera streaming: %d\n", ret);
            priv_data->streaming = false;
            return kStatus_HAL_CameraError;
        }
    }

    DEBUG_PRINT("Camera '%s' streaming started successfully\n", dev->name);
    return kStatus_HAL_CameraSuccess;
}

static hal_camera_status_t HAL_CameraDev_Zephyr_Stop(const camera_dev_t *dev)
{
    camera_zephyr_data_t *priv_data;

    if (!dev || !dev->data) {
        return kStatus_HAL_CameraError;
    }

    priv_data = (camera_zephyr_data_t *) dev->data;

    if (!priv_data->streaming) {
        DEBUG_PRINT("Camera not streaming\n");
        return kStatus_HAL_CameraSuccess;
    }

    /* Clear streaming flag */
    priv_data->streaming = false;

    /* we're not calling the actual Zephyr camera stop API */

    DEBUG_PRINT("Camera '%s' streaming stopped\n", dev->name);
    return kStatus_HAL_CameraSuccess;
}
static hal_camera_status_t HAL_CameraDev_Zephyr_Dequeue(const camera_dev_t *dev,
                                                        void **data,
                                                        int *stripe,
                                                        int *compressed_size)
{
    camera_zephyr_data_t *priv_data;
    struct video_buffer *vbuf;
    int ret;

    if (!dev || !dev->data || !data) {
        return kStatus_HAL_CameraError;
    }

    priv_data = (camera_zephyr_data_t *) dev->data;

    if (!priv_data->streaming) {
        return kStatus_HAL_CameraNoData;
    }

    /* Re-enqueue previous buffer BEFORE dequeuing new one */
    if (priv_data->current_vbuf != NULL) {

        ret = video_enqueue(priv_data->video_dev, priv_data->current_vbuf);
        if (ret) {
            DEBUG_PRINT("ERROR: Failed to re-enqueue previous buffer: %d\n",
                        ret);
            /* Don't return error - try to continue */
        } else {
            DEBUG_PRINT("DEBUG: Re-enqueued buffer %p\n",
                        priv_data->current_vbuf->buffer);
        }
        priv_data->current_vbuf = NULL;
    }

    /* Dequeue new buffer */
    ret = video_dequeue(priv_data->video_dev, &vbuf, K_MSEC(1000));
    if (ret == 0) {
        DEBUG_PRINT("DEBUG: video_dequeue returned vbuf=%p, vbuf->buffer=%p\n",
                    vbuf, vbuf->buffer);
    }
    if (ret) {
        if (ret == -EAGAIN) {
            DEBUG_PRINT("DEBUG: No frame available (timeout)\n");
            return kStatus_HAL_CameraNoData;
        }
        DEBUG_PRINT("ERROR: video_dequeue failed: %d\n", ret);
        return kStatus_HAL_CameraError;
    }

    DEBUG_PRINT("DEBUG: Dequeued new buffer %p, bytesused=%u\n", vbuf->buffer,
                vbuf->bytesused);

    /* Return buffer pointer to MPP */
    *data = vbuf->buffer;

    /* Store for next cycle */
    priv_data->current_vbuf = vbuf;

    /* Handle stripe mode if needed */
    if (stripe) {
        *stripe = dev->config.stripe ? 1 : 0;
    }

    /* Handle compressed size if needed */
    if (compressed_size) {
        *compressed_size = vbuf->bytesused;
    }

    return kStatus_HAL_CameraSuccess;
}
static hal_camera_status_t HAL_CameraDev_Zephyr_Enqueue(const camera_dev_t *dev,
                                                        void *data)
{
    camera_zephyr_data_t *priv_data;
    struct video_buffer *vbuf;
    int ret;

    if (!dev || !dev->data) {
        return kStatus_HAL_CameraError;
    }

    priv_data = (camera_zephyr_data_t *) dev->data;

    if (!priv_data->streaming) {
        return kStatus_HAL_CameraSuccess; /* Not an error if not streaming */
    }

    vbuf = priv_data->current_vbuf;
    if (!vbuf) {
        DEBUG_PRINT("ERROR: No buffer to enqueue\n");
        return kStatus_HAL_CameraError;
    }

    /* Return buffer to camera driver */
    ret = video_enqueue(priv_data->video_dev, vbuf);
    if (ret) {
        DEBUG_PRINT("ERROR: Failed to enqueue camera buffer: %d\n", ret);
        return kStatus_HAL_CameraError;
    }

    priv_data->current_vbuf = NULL;
    return kStatus_HAL_CameraSuccess;
}

static hal_camera_status_t
HAL_CameraDev_Zephyr_GetBufDesc(const camera_dev_t *dev, hw_buf_desc_t *out_buf,
                                mpp_memory_policy_t *policy)
{
    camera_zephyr_data_t *priv_data;

    if (!dev || !dev->data || !out_buf || !policy) {
        return kStatus_HAL_CameraError;
    }

    priv_data = (camera_zephyr_data_t *) dev->data;

    /* Tell MPP that buffers are managed by the driver */
    out_buf->alignment = HAL_CAM_ZEPHYR_BUFF_ALIGN;
    out_buf->cacheable = true;
    out_buf->stride = dev->config.width * get_bitpp(dev->config.format) / 8;
    out_buf->nb_lines = dev->config.height;
    out_buf->max_image_size = out_buf->nb_lines * out_buf->stride;
    out_buf->addr = NULL;

    /* Set memory policy - buffers are allocated by Zephyr video driver */
    *policy = HAL_MEM_ALLOC_OUTPUT; /* Buffers allocated from heap */

    return kStatus_HAL_CameraSuccess;
}

static hal_camera_status_t HAL_CameraDev_Zephyr_Lock(const camera_dev_t *dev)
{
    /* Not needed for Zephyr video API - thread-safe by design */
    return kStatus_HAL_CameraSuccess;
}

static hal_camera_status_t HAL_CameraDev_Zephyr_Unlock(const camera_dev_t *dev)
{
    /* Not needed for Zephyr video API - thread-safe by design */
    return kStatus_HAL_CameraSuccess;
}

/*******************************************************************************
 * Camera Device Operator Structure
 ******************************************************************************/

const camera_dev_operator_t camera_dev_zephyr_ops = {
    .init = HAL_CameraDev_Zephyr_Init,
    .deinit = HAL_CameraDev_Zephyr_Deinit,
    .start = HAL_CameraDev_Zephyr_Start,
    .stop = HAL_CameraDev_Zephyr_Stop,
    .enqueue = HAL_CameraDev_Zephyr_Enqueue,
    .dequeue = HAL_CameraDev_Zephyr_Dequeue,
    .get_buf_desc = HAL_CameraDev_Zephyr_GetBufDesc,
    .lock = HAL_CameraDev_Zephyr_Lock,
    .unlock = HAL_CameraDev_Zephyr_Unlock,
};

/*******************************************************************************
 * Public Setup Functions (Called by MPP HAL)
 ******************************************************************************/

/**
 * @brief Setup function for Zephyr camera
 *
 * This function is called by the MPP HAL layer during camera registration.
 * It initializes the camera device structure with Zephyr-specific operations.
 *
 * @param name Camera device name
 * @param dev Pointer to camera device structure to initialize
 * @return 0 on success, negative error code on failure
 */
int HAL_CameraDev_Zephyr_setup(const char *name, camera_dev_t *dev)
{
    if (!dev) {
        return -1;
    }

    /* Set device name */
    strncpy(dev->name, name, HAL_DEVICE_NAME_MAX_LENGTH - 1);
    dev->name[HAL_DEVICE_NAME_MAX_LENGTH - 1] = '\0';

    /* Assign Zephyr camera operations */
    dev->ops = &camera_dev_zephyr_ops;

    /* Initialize private data pointer to NULL (will be allocated in init) */
    dev->data = NULL;

    DEBUG_PRINT("Camera setup: '%s' registered with Zephyr HAL\n", name);

    return 0;
}

#endif /* HAL_ENABLE_CAMERA */
