/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr-generic Display HAL Implementation
 * Hardware-agnostic bridge between MPP and Zephyr Display API
 */

#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/video.h>

#include "mpp_config.h"
#include "hal_display_dev.h"
#include "hal_utils.h"
#include "mpp_api.h"

#if (defined HAL_ENABLE_DISPLAY) && (HAL_ENABLE_DISPLAY_DEV_ZEPHYR == 1)

#define HAL_DISP_ZEPHYR_BUFF_ALIGN 64 /* Should cover usual display requirements */

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Private data structure for Zephyr display device */
typedef struct {
    const struct device *display_dev;
} display_zephyr_data_t;

/*******************************************************************************
 * Zephyr Display API to MPP Format Conversion (Hardware Agnostic)
 ******************************************************************************/

/* Convert MPP pixel format to Zephyr video format */
static uint32_t zephyr_to_mpp_pixfmt(enum display_pixel_format zephyr_fmt)
{
    switch (zephyr_fmt) {
	case PIXEL_FORMAT_RGB_888:
        return MPP_PIXEL_RGB;
	case PIXEL_FORMAT_MONO01:
        return MPP_PIXEL_INVALID;
	case PIXEL_FORMAT_MONO10:
        return MPP_PIXEL_INVALID;
	case PIXEL_FORMAT_ARGB_8888:
        return MPP_PIXEL_ARGB;
	case PIXEL_FORMAT_RGB_565:
        return MPP_PIXEL_RGB565;
	case PIXEL_FORMAT_BGR_565:
        return MPP_PIXEL_INVALID;
	case PIXEL_FORMAT_L_8:
        return MPP_PIXEL_GRAY;
	case PIXEL_FORMAT_AL_88:
        return MPP_PIXEL_INVALID;
    default:
        printk("Warning: Unknown Zephyr format %d, defaulting to YUYV\n",
               zephyr_fmt);
    }
    return MPP_PIXEL_RGB565;
}

/*******************************************************************************
 * HAL Display Device Operations
 ******************************************************************************/

static hal_display_status_t
HAL_DisplayDev_Zephyr_Init(display_dev_t *dev, mpp_display_params_t *config,
                           mpp_callback_t callback, void *user_data)
{
    const struct device *display_dev;
    display_zephyr_data_t *priv_data;
    struct display_capabilities caps;

    if (!dev || !config) {
        return kStatus_HAL_DisplayError;
    }

    /* Get display device from devicetree */
    display_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_display));
    if (!display_dev) {
        printk("ERROR: Display device not found in devicetree\n");
        printk("       Make sure 'zephyr,display' is chosen in DTS\n");
        return kStatus_HAL_DisplayError;
    }

    if (!device_is_ready(display_dev)) {
        printk("ERROR: Display device '%s' not ready\n", display_dev->name);
        return kStatus_HAL_DisplayError;
    }

    /* Allocate private data */
    priv_data = k_malloc(sizeof(display_zephyr_data_t));
    if (!priv_data) {
        printk("ERROR: Failed to allocate display private data\n");
        return kStatus_HAL_DisplayError;
    }

    priv_data->display_dev = display_dev;
    dev->data = priv_data;

    strncpy(dev->name, display_dev->name, HAL_DEVICE_NAME_MAX_LENGTH - 1);

    /* Store callback */
    dev->cap.callback = callback;
    dev->cap.user_data = user_data;

    /* Get display capabilities */
    display_get_capabilities(display_dev, &caps);

    /* Update device capabilities */
    dev->cap.width = config->width ? config->width : caps.x_resolution;
    dev->cap.height = config->height ? config->height : caps.y_resolution;
    dev->cap.pitch = config->pitch ? config->pitch : dev->cap.width;
    dev->cap.format = zephyr_to_mpp_pixfmt(caps.current_pixel_format);
    dev->cap.rotate = config->rotate;
    dev->cap.stripe = config->stripe;
    dev->cap.nbFrameBuffer = 0;
    dev->cap.frameBuffers = NULL;

    /* Set active area */
    dev->cap.left = config->left;
    dev->cap.top = config->top;
    dev->cap.right = config->right ? config->right : dev->cap.width - 1;
    dev->cap.bottom = config->bottom ? config->bottom : dev->cap.height - 1;

    printk("Display HAL: Device '%s' initialized\n", display_dev->name);
    printk("Display capabilities: %dx%d, format: %d\n", caps.x_resolution,
           caps.y_resolution, caps.current_pixel_format);

    /* Turn off blanking to enable display */
    display_blanking_off(display_dev);

    return kStatus_HAL_DisplaySuccess;
}

static hal_display_status_t
HAL_DisplayDev_Zephyr_Deinit(const display_dev_t *dev)
{
    display_zephyr_data_t *priv_data;

    if (!dev || !dev->data) {
        return kStatus_HAL_DisplayError;
    }

    priv_data = (display_zephyr_data_t *) dev->data;

    /* Blank the display */
    display_blanking_on(priv_data->display_dev);

    k_free(priv_data);

    return kStatus_HAL_DisplaySuccess;
}

static hal_display_status_t HAL_DisplayDev_Zephyr_Start(display_dev_t *dev)
{
    display_zephyr_data_t *priv_data;

    if (!dev || !dev->data) {
        return kStatus_HAL_DisplayError;
    }

    priv_data = (display_zephyr_data_t *) dev->data;

    /* Ensure display is not blanked */
    display_blanking_off(priv_data->display_dev);

    printk("Display '%s' started\n", dev->name);
    return kStatus_HAL_DisplaySuccess;
}

static hal_display_status_t HAL_DisplayDev_Zephyr_Stop(display_dev_t *dev)
{
    display_zephyr_data_t *priv_data;

    if (!dev || !dev->data) {
        return kStatus_HAL_DisplayError;
    }

    priv_data = (display_zephyr_data_t *) dev->data;

    /* Blank the display */
    display_blanking_on(priv_data->display_dev);

    printk("Display '%s' stopped\n", dev->name);
    return kStatus_HAL_DisplaySuccess;
}

static hal_display_status_t HAL_DisplayDev_Zephyr_Blit(const display_dev_t *dev,
                                                       void *frame, int stripe)
{
    display_zephyr_data_t *priv_data;
    struct display_buffer_descriptor desc;
    int ret;

    if (!dev || !dev->data || !frame) {
        return kStatus_HAL_DisplayError;
    }

    priv_data = (display_zephyr_data_t *) dev->data;

    /* Setup buffer descriptor */
    desc.buf_size =
        dev->cap.width * dev->cap.height * get_bitpp(dev->cap.format) / 8;
    desc.width = dev->cap.width;
    desc.height = dev->cap.height;
    desc.pitch = dev->cap.pitch;

    /* Write framebuffer to display at configured position */
    ret = display_write(priv_data->display_dev, dev->cap.left, dev->cap.top,
                        &desc, frame);
    if (ret) {
        printk("ERROR: Display write failed: %d\n", ret);
        return kStatus_HAL_DisplayError;
    }

    return kStatus_HAL_DisplaySuccess;
}

static hal_display_status_t
HAL_DisplayDev_Zephyr_GetBufDesc(const display_dev_t *dev,
                                 hw_buf_desc_t *in_buf,
                                 mpp_memory_policy_t *policy)
{
    if (!dev || !dev->data || !in_buf || !policy) {
        return kStatus_HAL_DisplayError;
    }

    *policy = HAL_MEM_ALLOC_NONE;
    in_buf->alignment = HAL_DISP_ZEPHYR_BUFF_ALIGN;
    // in_buf->nb_lines = dev->cap.height;
    in_buf->cacheable = false;
    // in_buf->stride = dev->cap.pitch;
    // in_buf->max_image_size = in_buf->nb_lines * in_buf->stride;
    // in_buf->addr = NULL;

    return kStatus_HAL_DisplaySuccess;
}

/*******************************************************************************
 * Display Device Operator Structure
 ******************************************************************************/

const display_dev_operator_t display_dev_zephyr_ops = {
    .init = HAL_DisplayDev_Zephyr_Init,
    .deinit = HAL_DisplayDev_Zephyr_Deinit,
    .start = HAL_DisplayDev_Zephyr_Start,
    .stop = HAL_DisplayDev_Zephyr_Stop,
    .blit = HAL_DisplayDev_Zephyr_Blit,
    .get_buf_desc = HAL_DisplayDev_Zephyr_GetBufDesc,
};

/*******************************************************************************
 * Public Setup Functions (Called by MPP HAL)
 ******************************************************************************/

/**
 * @brief Setup function for Zephyr display
 *
 * This function is called by the MPP HAL layer during display registration.
 * It initializes the display device structure with Zephyr-specific operations.
 *
 * @param dev Pointer to display device structure to initialize
 * @return 0 on success, negative error code on failure
 */
int HAL_DisplayDev_Zephyr_setup(display_dev_t *dev)
{
    if (!dev) {
        return -1;
    }

    /* Assign Zephyr display operations */
    dev->ops = &display_dev_zephyr_ops;

    /* Initialize private data pointer to NULL (will be allocated in init) */
    dev->data = NULL;

    return 0;
}

#endif /* HAL_ENABLE_DISPLAY */
