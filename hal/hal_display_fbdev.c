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
 * @brief display dev HAL driver implementation for fbdev.
 */

#include "mpp_config.h"
#include "mpp_api_types.h"
#include "hal_debug.h"
#include "hal_display_dev.h"
#include "hal_utils.h"

#if (defined HAL_ENABLE_DISPLAY) && (HAL_ENABLE_DISPLAY_DEV_Fbdev == 1)
#include <FreeRTOS.h>
#include <queue.h>

#include "fsl_common.h"
#include "fsl_fbdev.h"
#include "display_support.h"

#if defined(__cplusplus)
extern "C" {
#endif

int s_DisplayDev_Fbdev_register();

#if defined(__cplusplus)
}
#endif

/**** constants ****/

/* fixed values */
#define DISPLAY_NAME "FBdev"
#define DISPLAY_DEV_FBDEV_HEIGHT 1280
#define DISPLAY_DEV_FBDEV_WIDTH 720
#define DISPLAY_DEV_FBDEV_LEFT 0
#define DISPLAY_DEV_FBDEV_TOP 0
#define DISPLAY_DEV_FBDEV_RIGHT 719
#define DISPLAY_DEV_FBDEV_BOTTOM 1279
#define DISPLAY_DEV_FBDEV_ROTATE ROTATE_0
/* configurable default values */
#define DISPLAY_DEV_FBDEV_FORMAT MPP_PIXEL_RGB565
#define DISPLAY_DEV_FBDEV_BUFFER_COUNT FBDEV_DEFAULT_FRAME_BUFFER
#ifndef HAL_DISPLAY_MAX_BPP
#define HAL_DISPLAY_MAX_BPP 2   /* RGB565 assumed by default */
#elif ( (HAL_DISPLAY_MAX_BPP < 2) || (HAL_DISPLAY_MAX_BPP > 4) )
#error "HAL: DisplayDev: Fbdev: HAL_DISPLAY_MAX_BPP value not supported"
#endif
#define DISPLAY_DEV_FBDEV_IMG_SIZE (DISPLAY_DEV_FBDEV_WIDTH * DISPLAY_DEV_FBDEV_HEIGHT * HAL_DISPLAY_MAX_BPP)

/**** declarations ****/

hal_display_status_t HAL_DisplayDev_Fbdev_Init(
    display_dev_t *dev, mpp_display_params_t *config, mpp_callback_t callback, void *param);
hal_display_status_t HAL_DisplayDev_Fbdev_Deinit(const display_dev_t *dev);
hal_display_status_t HAL_DisplayDev_Fbdev_Start(display_dev_t *dev);
hal_display_status_t HAL_DisplayDev_Fbdev_Stop(display_dev_t *dev);
hal_display_status_t HAL_DisplayDev_Fbdev_Blit(const display_dev_t *dev, void *frame, int stripe);
hal_display_status_t HAL_DisplayDev_Fbdev_Getbufdesc(const display_dev_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy);

/**** static variables ****/

#if (DEMO_BUFFER_FIXED_ADDRESS == 1)
#if (DISPLAY_DEV_FBDEV_BUFFER_COUNT == 1)
    static uint8_t *s_FbdevBuffer[DISPLAY_DEV_FBDEV_BUFFER_COUNT] = { (uint8_t *) DEMO_BUFFER0_ADDR};
#elif (DISPLAY_DEV_FBDEV_BUFFER_COUNT == 2)
    static uint8_t *s_FbdevBuffer[DISPLAY_DEV_FBDEV_BUFFER_COUNT] = { (uint8_t *) DEMO_BUFFER0_ADDR, (uint8_t *) DEMO_BUFFER1_ADDR };
#elif (DISPLAY_DEV_FBDEV_BUFFER_COUNT == 3)
    static uint8_t *s_FbdevBuffer[DISPLAY_DEV_FBDEV_BUFFER_COUNT] = { (uint8_t *) DEMO_BUFFER0_ADDR, (uint8_t *) DEMO_BUFFER1_ADDR, (uint8_t *) DEMO_BUFFER2_ADDR };
#else   /* DISPLAY_DEV_FBDEV_BUFFER_COUNT != 1,2,3 */
#error "HAL: DisplayDev: Fbdev: DISPLAY_DEV_FBDEV_BUFFER_COUNT value not supported."
#endif
#else /* (DEMO_BUFFER_FIXED_ADDRESS != 1) */
AT_NONCACHEABLE_SECTION_ALIGN(
    static uint8_t s_FbdevBuffer[DISPLAY_DEV_FBDEV_BUFFER_COUNT][DISPLAY_DEV_FBDEV_HEIGHT][DISPLAY_DEV_FBDEV_WIDTH * HAL_DISPLAY_MAX_BPP],
    FRAME_BUFFER_ALIGN);
#endif

static fbdev_t s_fbdev;
static fbdev_fb_info_t s_fbInfo;
static volatile bool s_fbdevInitialized = false;

const static display_dev_operator_t s_DisplayDev_FbdevOps = {
    .init        = HAL_DisplayDev_Fbdev_Init,
    .deinit      = HAL_DisplayDev_Fbdev_Deinit,
    .start       = HAL_DisplayDev_Fbdev_Start,
    .stop        = HAL_DisplayDev_Fbdev_Stop,
    .blit        = HAL_DisplayDev_Fbdev_Blit,
    .get_buf_desc    = HAL_DisplayDev_Fbdev_Getbufdesc,
};

static display_dev_t s_DisplayDev_Fbdev = {.id   = 0,
                                           .name = DISPLAY_NAME,
                                           .ops  = &s_DisplayDev_FbdevOps,
                                           .cap  = {.width       = DISPLAY_DEV_FBDEV_WIDTH,
                                                   .height      = DISPLAY_DEV_FBDEV_HEIGHT,
                                                   .pitch       = DISPLAY_DEV_FBDEV_WIDTH * HAL_DISPLAY_MAX_BPP,
                                                   .left        = DISPLAY_DEV_FBDEV_LEFT,
                                                   .top         = DISPLAY_DEV_FBDEV_TOP,
                                                   .right       = DISPLAY_DEV_FBDEV_RIGHT,
                                                   .bottom      = DISPLAY_DEV_FBDEV_BOTTOM,
                                                   .rotate      = DISPLAY_DEV_FBDEV_ROTATE,
                                                   .format      = DISPLAY_DEV_FBDEV_FORMAT,
                                                   .nbFrameBuffer = DISPLAY_DEV_FBDEV_BUFFER_COUNT,
                                                   .frameBuffers = NULL,
                                                   .callback    = NULL,
                                                   .user_data   = NULL,
                                                   .handle      = NULL,
                                                   .p_in_buf_addr = NULL}};

/**** definitions ****/

static video_pixel_format_t mpp_to_video_format(mpp_pixel_format_t format)
{
    switch(format) {
        case MPP_PIXEL_RGB565:
            return kVIDEO_PixelFormatRGB565;
        case MPP_PIXEL_ARGB:
            return kVIDEO_PixelFormatXRGB8888;
        case MPP_PIXEL_RGB:
            return kVIDEO_PixelFormatRGB888;
        case MPP_PIXEL_BGR:
            return kVIDEO_PixelFormatBGR888;
        default:
            HAL_LOGE("Unsupported pixel format: %d\n", format);
            return kVIDEO_PixelFormatRGB565; /* fallback */
    }
}

static hal_display_status_t DISPLAY_InitFbdev(display_dev_private_capability_t *cap)
{
    status_t status;
    
    if (s_fbdevInitialized) {
        HAL_LOGD("Fbdev already initialized\n");
        return kStatus_HAL_DisplaySuccess;
    }

    do {
        /* Open the fbdev */
        status = FBDEV_Open(&s_fbdev, &g_dc, 0); /* layer 0 */
        if (kStatus_Success != status) 
        {
            HAL_LOGE("FBDEV_Open failed: %d\n", status);
            return kStatus_HAL_DisplayError;
        }

        /* Get frame buffer info */
        FBDEV_GetFrameBufferInfo(&s_fbdev, &s_fbInfo);

        /* Configure frame buffer info */
        s_fbInfo.bufferCount = DISPLAY_DEV_FBDEV_BUFFER_COUNT;
        s_fbInfo.bufInfo.pixelFormat = mpp_to_video_format(cap->format);
        s_fbInfo.bufInfo.width = cap->width;
        s_fbInfo.bufInfo.height = cap->height;
        s_fbInfo.bufInfo.strideBytes = cap->pitch;

        /* Clear buffers */
        for(int i = 0; i < DISPLAY_DEV_FBDEV_BUFFER_COUNT; i++)
        {
            memset(s_FbdevBuffer[i], 0x0, cap->pitch * cap->height);
        }
    
        /* Set frame buffer addresses */
        for (int i = 0; i < DISPLAY_DEV_FBDEV_BUFFER_COUNT; i++) 
        {
            s_fbInfo.buffers[i] = (void *)s_FbdevBuffer[i];
        }

        /* Set frame buffer info */
        status = FBDEV_SetFrameBufferInfo(&s_fbdev, &s_fbInfo);
        if (kStatus_Success != status)
        {
            HAL_LOGE("FBDEV_SetFrameBufferInfo failed: %d\n", status);
            FBDEV_Close(&s_fbdev);
            return kStatus_HAL_DisplayError;
        }

        s_fbdevInitialized = true;
    } while (false);

    /* Get available frame buffer from fbdev */
    *cap->p_in_buf_addr = FBDEV_GetFrameBuffer(&s_fbdev, 0);    /* else kFBDEV_NoWait */
    if (*(cap->p_in_buf_addr) == NULL)
    {
        HAL_LOGE("\nNULL display buffer address\n");
        return kStatus_HAL_DisplayError;
    }

    /* Set the frame buffer for initial screen */
    status = FBDEV_SetFrameBuffer(&s_fbdev, *(cap->p_in_buf_addr), 0);
    if (kStatus_Success != status)
    {
        HAL_LOGE("FBDEV_SetFrameBuffer failed: %d\n", status);
        return kStatus_HAL_DisplayError;
    }

    return kStatus_HAL_DisplaySuccess;
}

static hal_display_status_t DISPLAY_DeInitFbdev(void)
{
    status_t status = kStatus_HAL_DisplayError;

    if (!s_fbdevInitialized) 
    {
        return kStatus_HAL_DisplaySuccess;
    }

    /* Disable the fbdev */
    status = FBDEV_Disable(&s_fbdev);
    if (kStatus_Success != status) 
    {
        HAL_LOGE("FBDEV_Disable failed: %d\n", status);
        return kStatus_HAL_DisplayError;
    }

    /* Close the fbdev */
    status = FBDEV_Close(&s_fbdev);
    if (kStatus_Success != status) 
    {
        HAL_LOGE("FBDEV_Close failed: %d\n", status);
        return kStatus_HAL_DisplayError;
    }

    s_fbdevInitialized = false;
    return kStatus_HAL_DisplaySuccess;
}

hal_display_status_t HAL_DisplayDev_Fbdev_Init(
    display_dev_t *dev, mpp_display_params_t *config, mpp_callback_t callback, void *user_data)
{
    hal_display_status_t ret = kStatus_HAL_DisplaySuccess;
    HAL_LOGD("++HAL_DisplayDev_Fbdev_Init\n");

    if (dev == NULL)
    {
        HAL_LOGE("\nNULL display device\n");
        return kStatus_HAL_DisplayError;
    }

    BOARD_PrepareDisplayController();

    /* set default config */
    memcpy(&dev->cap, &s_DisplayDev_Fbdev.cap, sizeof(display_dev_private_capability_t) );

    /* Apply user configuration if provided */
    if (config != NULL) 
    {
        /* check input pixel depth versus static config */
        if (get_bitpp(config->format)/8 > HAL_DISPLAY_MAX_BPP)
        {
            HAL_LOGE("Pixel depth higher than max defined in mpp_config.h.\n");
            return kStatus_HAL_DisplayError;
        }

        /* Update format and calculate pitch */
        dev->cap.format = config->format;
        switch(config->format) 
        {
        case MPP_PIXEL_RGB565:
            dev->cap.pitch = dev->cap.width * 2;
            break;
        case MPP_PIXEL_ARGB:
            dev->cap.pitch = dev->cap.width * 4;
            break;
        case MPP_PIXEL_RGB:
        case MPP_PIXEL_BGR:
            dev->cap.pitch = dev->cap.width * 3;
            break;
        default:
            HAL_LOGE("HAL_DisplayDev_Fbdev_Init: invalid pixel format parameter %d.\n", config->format);
            return kStatus_HAL_DisplayError;
        }

        /* Allow user to override resolution if provided (non-zero values) */
        if (config->width != 0) 
        {
            if (config->width != DISPLAY_DEV_FBDEV_WIDTH) 
            {
                HAL_LOGD("Warning: Changing width from %d to %d\n", DISPLAY_DEV_FBDEV_WIDTH, config->width);
            }
            dev->cap.width = config->width;
            dev->cap.right = config->width - 1;
            /* Recalculate pitch based on new width */
            dev->cap.pitch = dev->cap.width * (get_bitpp(dev->cap.format) / 8);
        }

        if (config->height != 0) 
        {
            if (config->height != DISPLAY_DEV_FBDEV_HEIGHT) 
            {
                HAL_LOGD("Warning: Changing height from %d to %d\n", DISPLAY_DEV_FBDEV_HEIGHT, config->height);
            }
            dev->cap.height = config->height;
            dev->cap.bottom = config->height - 1;
        }

        /* Allow user to override pitch if explicitly provided */
        if (config->pitch != 0) 
        {
            dev->cap.pitch = config->pitch;
        }

        /* Allow user to configure display area */
        if (config->top != 0) 
        {
            dev->cap.top = config->top;
        }

        if (config->left != 0) 
        {
            dev->cap.left = config->left;
        }

        if (config->right != 0) 
        {
            dev->cap.right = config->right;
        }

        if (config->bottom != 0) 
        {
            dev->cap.bottom = config->bottom;
        }

        /* Validate display area consistency */
        if (dev->cap.right < dev->cap.left || dev->cap.bottom < dev->cap.top) 
        {
            HAL_LOGE("HAL_DisplayDev_Fbdev_Init: invalid display area (right < left or bottom < top).\n");
            return kStatus_HAL_DisplayError;
        }

        /* Update rotation if provided */
        if (config->rotate != ROTATE_0) 
        {
            dev->cap.rotate = config->rotate;
            HAL_LOGD("Display rotation set to: %d\n", config->rotate);
        }
    }

    dev->cap.frameBuffers = (void **)s_FbdevBuffer;
    dev->cap.callback = callback;
    dev->cap.user_data = user_data;

    /* initialize FB device */
    ret = DISPLAY_InitFbdev(&dev->cap);
    if (ret != kStatus_HAL_DisplaySuccess) 
    {
        return ret;
    }

    HAL_LOGD("--HAL_DisplayDev_Fbdev_Init\n");
    return ret;
}

hal_display_status_t HAL_DisplayDev_Fbdev_Deinit(const display_dev_t *dev)
{
    hal_display_status_t ret = kStatus_HAL_DisplaySuccess;
    HAL_LOGD("++HAL_DisplayDev_Fbdev_Deinit\n");
    
    ret = DISPLAY_DeInitFbdev();
    
    HAL_LOGD("--HAL_DisplayDev_Fbdev_Deinit\n");
    return ret;
}

hal_display_status_t HAL_DisplayDev_Fbdev_Start(display_dev_t *dev)
{
    hal_display_status_t ret = kStatus_HAL_DisplaySuccess;
    status_t status;
    HAL_LOGD("++HAL_DisplayDev_Fbdev_Start\n");

    /* Enable the fbdev */
    status = FBDEV_Enable(&s_fbdev);
    if (kStatus_Success != status) 
    {
        HAL_LOGE("FBDEV_Enable failed: %d\n", status);
        return kStatus_HAL_DisplayError;
    }

    /* Get available frame buffer for the pipeline */
    *dev->cap.p_in_buf_addr = FBDEV_GetFrameBuffer(&s_fbdev, 0);
    if (*(dev->cap.p_in_buf_addr) == NULL) 
    {
        HAL_LOGE("\nNULL display buffer address\n");
        return kStatus_HAL_DisplayError;
    }

    HAL_LOGD("--HAL_DisplayDev_Fbdev_Start\n");
    return ret;
}

hal_display_status_t HAL_DisplayDev_Fbdev_Stop(display_dev_t *dev)
{
    hal_display_status_t ret = kStatus_HAL_DisplaySuccess;
    status_t status;
    HAL_LOGD("++HAL_DisplayDev_Fbdev_Stop\n");
    
    /* give back the buffer to avoid being stuck */
    if (*(dev->cap.p_in_buf_addr) == NULL)
    {
        HAL_LOGE("\nNULL display buffer address\n");
        return kStatus_HAL_DisplayError;
    }

    status = FBDEV_SetFrameBuffer(&s_fbdev, *(dev->cap.p_in_buf_addr), 0); /* wait */
    if (kStatus_Success != status)
    {
        HAL_LOGE("FBDEV_SetFrameBuffer failed: %d\n", status);
        return kStatus_HAL_DisplayError;
    }

    /* Disable the fbdev */
    status = FBDEV_Disable(&s_fbdev);
    if (kStatus_Success != status)
    {
        HAL_LOGE("FBDEV_Disable failed: %d\n", status);
        ret = kStatus_HAL_DisplayError;
    }
    
    HAL_LOGD("--HAL_DisplayDev_Fbdev_Stop\n");
    return ret;
}

hal_display_status_t HAL_DisplayDev_Fbdev_Blit(const display_dev_t *dev, void *frame, int stripe)
{
    hal_display_status_t ret = kStatus_HAL_DisplaySuccess;
    status_t status;
    HAL_LOGD("++HAL_DisplayDev_Fbdev_Blit\n");

    if (!s_fbdevInitialized) 
    {
        HAL_LOGE("Fbdev not initialized\n");
        return kStatus_HAL_DisplayError;
    }

    /* Set the frame buffer */
    status = FBDEV_SetFrameBuffer(&s_fbdev, *(dev->cap.p_in_buf_addr), 0); /* wait */
    if (kStatus_Success != status) 
    {
        HAL_LOGE("FBDEV_SetFrameBuffer failed: %d\n", status);
        return kStatus_HAL_DisplayError;
    }

    /* Get available frame buffer from fbdev */
    *(dev->cap.p_in_buf_addr) = FBDEV_GetFrameBuffer(&s_fbdev, 0);  /* wait */
    if (*(dev->cap.p_in_buf_addr) == NULL) 
    {
        HAL_LOGE("\nNULL display buffer address\n");
        return kStatus_HAL_DisplayError;
    }

    /* Trigger callback if available */
    const display_dev_private_capability_t *cap = &(dev->cap);
    if (cap->callback != NULL) 
    {
        cap->callback(NULL, kDisplayEvent_RequestFrame, NULL, cap->user_data);
    }

    HAL_LOGD("--HAL_DisplayDev_Fbdev_Blit\n");
    return ret;
}

hal_display_status_t HAL_DisplayDev_Fbdev_Getbufdesc(const display_dev_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy)
{
    hal_display_status_t ret = kStatus_HAL_DisplaySuccess;
    HAL_LOGD("++HAL_DisplayDev_Fbdev_Getbufdesc\n");

    do
    {
        if ((in_buf == NULL) || (policy == NULL))
        {
            HAL_LOGE("\nNULL pointer to buffer descriptor\n");
            ret = kStatus_HAL_DisplayError;
            break;
        }
        /* set memory policy */
        *policy = HAL_MEM_ALLOC_INPUT;
        in_buf->alignment = FRAME_BUFFER_ALIGN;
        in_buf->nb_lines = dev->cap.height;
        in_buf->cacheable = false;
        in_buf->stride = dev->cap.pitch;
        in_buf->max_image_size = in_buf->nb_lines * in_buf->stride;

        /* store pointer to framebuffer address used by previous element */
        /* remove const qualifier to allow modification of capability structure */
        ((display_dev_t *)dev)->cap.p_in_buf_addr = (void **) &in_buf->addr;
    } while (false);

    HAL_LOGD("--HAL_DisplayDev_Fbdev_Getbufdesc\n");
    return ret;
}

int HAL_DisplayDev_Fbdev_setup(display_dev_t *dev)
{
    dev->ops = &s_DisplayDev_FbdevOps;
    return 0;
}

#else /* (defined HAL_ENABLE_DISPLAY) && (HAL_ENABLE_DISPLAY_DEV_Fbdev == 1) */
int HAL_DisplayDev_Fbdev_setup(display_dev_t *dev)
{
    HAL_LOGE("Display Fbdev not enabled\n");
    return -1;
}
#endif /* (defined HAL_ENABLE_DISPLAY) && (HAL_ENABLE_DISPLAY_DEV_Fbdev == 1) */

