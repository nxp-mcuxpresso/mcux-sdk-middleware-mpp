/*
 * Copyright 2024-2026 NXP.
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
 * @brief Display HAL driver for LCDIF + MIPI DSI + Waveshare 5inch DSI LCD (800x480).
 *
 * This driver follows the same structure as hal_display_lcdifv2_rk055.c.
 * It is selected by defining:
 *   #define HAL_ENABLE_DISPLAY
 *   #define HAL_ENABLE_DISPLAY_DEV_LcdifWaveshare5Inch  1
 * in mpp_config.h.
 */

#include "mpp_config.h"
#include "mpp_api_types.h"
#include "hal_debug.h"
#include "hal_display_dev.h"
#include "hal_utils.h"

#if (defined HAL_ENABLE_DISPLAY) && (HAL_ENABLE_DISPLAY_DEV_LcdifWaveshare5Inch == 1)
#include <FreeRTOS.h>
#include <queue.h>

#include "fsl_common.h"
#include "display_support.h"

#if defined(__cplusplus)
extern "C" {
#endif

int HAL_DisplayDev_LcdifWaveshare5Inch_setup(display_dev_t *dev);

#if defined(__cplusplus)
}
#endif

/*******************************************************************************
 * Constants
 ******************************************************************************/

#define DISPLAY_NAME                              "LcdifWaveshare5Inch"

/* Waveshare 5inch DSI LCD: 800x480 landscape */
#define DISPLAY_DEV_LcdifWaveshare5Inch_WIDTH     800
#define DISPLAY_DEV_LcdifWaveshare5Inch_HEIGHT    480
#define DISPLAY_DEV_LcdifWaveshare5Inch_LEFT      0
#define DISPLAY_DEV_LcdifWaveshare5Inch_TOP       0
#define DISPLAY_DEV_LcdifWaveshare5Inch_RIGHT     (DISPLAY_DEV_LcdifWaveshare5Inch_WIDTH - 1)
#define DISPLAY_DEV_LcdifWaveshare5Inch_BOTTOM    (DISPLAY_DEV_LcdifWaveshare5Inch_HEIGHT - 1)
#define DISPLAY_DEV_LcdifWaveshare5Inch_ROTATE    ROTATE_0
#define DISPLAY_DEV_LcdifWaveshare5Inch_FORMAT    MPP_PIXEL_RGB565
#define DISPLAY_DEV_LcdifWaveshare5Inch_BUFFER_COUNT 1

#ifndef HAL_DISPLAY_MAX_BPP
#define HAL_DISPLAY_MAX_BPP 2   /* RGB565 default */
#elif ((HAL_DISPLAY_MAX_BPP < 2) || (HAL_DISPLAY_MAX_BPP > 4))
#error "HAL: DisplayDev: LcdifWaveshare5Inch: HAL_DISPLAY_MAX_BPP value not supported"
#endif

#define DISPLAY_DEV_LcdifWaveshare5Inch_IMG_SIZE \
    (DISPLAY_DEV_LcdifWaveshare5Inch_WIDTH * DISPLAY_DEV_LcdifWaveshare5Inch_HEIGHT * HAL_DISPLAY_MAX_BPP)

/*******************************************************************************
 * Declarations
 ******************************************************************************/

hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Init(
    display_dev_t *dev, mpp_display_params_t *config, mpp_callback_t callback, void *param);
hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Deinit(const display_dev_t *dev);
hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Start(display_dev_t *dev);
hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Stop(display_dev_t *dev);
hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Blit(const display_dev_t *dev, void *frame, int stripe);
hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Getbufdesc(
    const display_dev_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy);

/*******************************************************************************
 * Static variables
 ******************************************************************************/

AT_NONCACHEABLE_SECTION_ALIGN(
    static uint8_t s_LcdBuffer[DISPLAY_DEV_LcdifWaveshare5Inch_BUFFER_COUNT]
                               [DISPLAY_DEV_LcdifWaveshare5Inch_HEIGHT]
                               [DISPLAY_DEV_LcdifWaveshare5Inch_WIDTH * HAL_DISPLAY_MAX_BPP],
    FRAME_BUFFER_ALIGN);

static volatile bool s_newFrameShown = false;
static dc_fb_info_t  s_fbInfo;
static volatile uint8_t s_lcdActiveFbIdx;

static const display_dev_operator_t s_DisplayDev_LcdifWaveshare5InchOps = {
    .init        = HAL_DisplayDev_LcdifWaveshare5Inch_Init,
    .deinit      = HAL_DisplayDev_LcdifWaveshare5Inch_Deinit,
    .start       = HAL_DisplayDev_LcdifWaveshare5Inch_Start,
    .stop        = HAL_DisplayDev_LcdifWaveshare5Inch_Stop,
    .blit        = HAL_DisplayDev_LcdifWaveshare5Inch_Blit,
    .get_buf_desc = HAL_DisplayDev_LcdifWaveshare5Inch_Getbufdesc,
};

static display_dev_t s_DisplayDev_LcdifWaveshare5Inch = {
    .id   = 0,
    .name = DISPLAY_NAME,
    .ops  = &s_DisplayDev_LcdifWaveshare5InchOps,
    .cap  = {
        .width        = DISPLAY_DEV_LcdifWaveshare5Inch_WIDTH,
        .height       = DISPLAY_DEV_LcdifWaveshare5Inch_HEIGHT,
        .pitch        = DISPLAY_DEV_LcdifWaveshare5Inch_WIDTH * HAL_DISPLAY_MAX_BPP,
        .left         = DISPLAY_DEV_LcdifWaveshare5Inch_LEFT,
        .top          = DISPLAY_DEV_LcdifWaveshare5Inch_TOP,
        .right        = DISPLAY_DEV_LcdifWaveshare5Inch_RIGHT,
        .bottom       = DISPLAY_DEV_LcdifWaveshare5Inch_BOTTOM,
        .rotate       = DISPLAY_DEV_LcdifWaveshare5Inch_ROTATE,
        .format       = DISPLAY_DEV_LcdifWaveshare5Inch_FORMAT,
        .nbFrameBuffer = DISPLAY_DEV_LcdifWaveshare5Inch_BUFFER_COUNT,
        .frameBuffers = NULL,
        .callback     = NULL,
        .user_data    = NULL,
    }
};

/*******************************************************************************
 * Private functions
 ******************************************************************************/

static void DISPLAY_BufferSwitchOffCallback(void *param, void *switchOffBuffer)
{
    s_newFrameShown = true;
    s_lcdActiveFbIdx ^= 1;
}

static hal_display_status_t DISPLAY_InitDisplay(display_dev_private_capability_t *cap)
{
    status_t status;

    do {
        if (s_newFrameShown)
        {
            /* Already initialized — re-enable layer only */
            break;
        }

        BOARD_PrepareDisplayController();

        status = g_dc.ops->init(&g_dc);
        if (kStatus_Success != status)
        {
            HAL_LOGE("Display initialization failed\n");
            return kStatus_HAL_DisplayError;
        }

        g_dc.ops->getLayerDefaultConfig(&g_dc, 0, &s_fbInfo);

        switch (cap->format)
        {
            case MPP_PIXEL_RGB565:
                s_fbInfo.pixelFormat = kVIDEO_PixelFormatRGB565;
                break;
            case MPP_PIXEL_ARGB:
                s_fbInfo.pixelFormat = kVIDEO_PixelFormatXRGB8888;
                break;
            case MPP_PIXEL_RGB:
                s_fbInfo.pixelFormat = kVIDEO_PixelFormatRGB888;
                break;
            default:
                HAL_LOGE("DISPLAY_InitDisplay: invalid pixel format\n");
                return kStatus_HAL_DisplayError;
        }

        s_fbInfo.width       = DISPLAY_DEV_LcdifWaveshare5Inch_WIDTH;
        s_fbInfo.height      = DISPLAY_DEV_LcdifWaveshare5Inch_HEIGHT;
        s_fbInfo.startX      = DISPLAY_DEV_LcdifWaveshare5Inch_LEFT;
        s_fbInfo.startY      = DISPLAY_DEV_LcdifWaveshare5Inch_TOP;
        s_fbInfo.strideBytes = cap->pitch;
        g_dc.ops->setLayerConfig(&g_dc, 0, &s_fbInfo);

        g_dc.ops->setCallback(&g_dc, 0, DISPLAY_BufferSwitchOffCallback, cap);

        s_lcdActiveFbIdx = 0;
        s_newFrameShown  = false;
        g_dc.ops->setFrameBuffer(&g_dc, 0, s_LcdBuffer[s_lcdActiveFbIdx]);

        if ((g_dc.ops->getProperty(&g_dc) & kDC_FB_ReserveFrameBuffer) == 0)
        {
            while (s_newFrameShown == false) {}
        }

        s_newFrameShown = true;
    } while (false);

    g_dc.ops->enableLayer(&g_dc, 0);
    return kStatus_HAL_DisplaySuccess;
}

static hal_display_status_t DISPLAY_DeInitDisplay(void)
{
    status_t status;

    status = g_dc.ops->disableLayer(&g_dc, 0);
    if (kStatus_Success != status)
    {
        HAL_LOGE("Display disableLayer failed\n");
        return kStatus_HAL_DisplayError;
    }

    return kStatus_HAL_DisplaySuccess;
}

/*******************************************************************************
 * Public HAL operations
 ******************************************************************************/

hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Init(
    display_dev_t *dev, mpp_display_params_t *config, mpp_callback_t callback, void *user_data)
{
    hal_display_status_t ret = kStatus_HAL_DisplaySuccess;
    HAL_LOGD("++HAL_DisplayDev_LcdifWaveshare5Inch_Init\n");

    memset(s_LcdBuffer, 0x0, sizeof(s_LcdBuffer));

    /* Copy default capabilities */
    memcpy(&dev->cap, &s_DisplayDev_LcdifWaveshare5Inch.cap,
           sizeof(display_dev_private_capability_t));

    /* Validate pixel depth */
    if (get_bitpp(config->format) / 8 > HAL_DISPLAY_MAX_BPP)
    {
        HAL_LOGE("Pixel depth higher than HAL_DISPLAY_MAX_BPP.\n");
        return kStatus_HAL_DisplayError;
    }

    dev->cap.format = config->format;
    switch (config->format)
    {
        case MPP_PIXEL_RGB565:
            dev->cap.pitch = dev->cap.width * 2;
            break;
        case MPP_PIXEL_ARGB:
            dev->cap.pitch = dev->cap.width * 4;
            break;
        case MPP_PIXEL_RGB:
            dev->cap.pitch = dev->cap.width * 3;
            break;
        default:
            HAL_LOGE("HAL_DisplayDev_LcdifWaveshare5Inch_Init: invalid pixel format\n");
            return kStatus_HAL_DisplayError;
    }

    /* Validate resolution (fixed for this panel) */
    if (((config->width  != 0) && (config->width  != DISPLAY_DEV_LcdifWaveshare5Inch_WIDTH))  ||
        ((config->height != 0) && (config->height != DISPLAY_DEV_LcdifWaveshare5Inch_HEIGHT)) ||
        ((config->pitch  != 0) && (config->pitch  != dev->cap.pitch)))
    {
        HAL_LOGE("HAL_DisplayDev_LcdifWaveshare5Inch_Init: invalid resolution\n");
        return kStatus_HAL_DisplayError;
    }

    /* Validate display area */
    if (((config->top    != 0) && (config->top    != DISPLAY_DEV_LcdifWaveshare5Inch_TOP))    ||
        ((config->left   != 0) && (config->left   != DISPLAY_DEV_LcdifWaveshare5Inch_LEFT))   ||
        ((config->right  != 0) && (config->right  != DISPLAY_DEV_LcdifWaveshare5Inch_RIGHT))  ||
        ((config->bottom != 0) && (config->bottom != DISPLAY_DEV_LcdifWaveshare5Inch_BOTTOM)))
    {
        HAL_LOGE("HAL_DisplayDev_LcdifWaveshare5Inch_Init: invalid area\n");
        return kStatus_HAL_DisplayError;
    }

    dev->cap.frameBuffers = (void **)s_LcdBuffer;
    dev->cap.callback     = callback;
    dev->cap.user_data    = user_data;

    HAL_LOGD("--HAL_DisplayDev_LcdifWaveshare5Inch_Init\n");
    return ret;
}

hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Deinit(const display_dev_t *dev)
{
    return kStatus_HAL_DisplaySuccess;
}

hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Start(display_dev_t *dev)
{
    hal_display_status_t ret;
    HAL_LOGD("++HAL_DisplayDev_LcdifWaveshare5Inch_Start\n");
    ret = DISPLAY_InitDisplay(&dev->cap);
    HAL_LOGD("--HAL_DisplayDev_LcdifWaveshare5Inch_Start\n");
    return ret;
}

hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Stop(display_dev_t *dev)
{
    hal_display_status_t ret;
    HAL_LOGD("++HAL_DisplayDev_LcdifWaveshare5Inch_Stop\n");
    ret = DISPLAY_DeInitDisplay();
    HAL_LOGD("--HAL_DisplayDev_LcdifWaveshare5Inch_Stop\n");
    return ret;
}

hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Blit(
    const display_dev_t *dev, void *frame, int stripe)
{
    HAL_LOGD("++HAL_DisplayDev_LcdifWaveshare5Inch_Blit\n");
    g_dc.ops->setFrameBuffer(&g_dc, 0, frame);

#if (ENABLE_PISANO_CHECKSUM == 1)
    checksum_data_t checksum;
    checksum.type = CHECKSUM_TYPE_PISANO;
    if (stripe == 0)
    {
        checksum.value = calc_checksum(
            (dev->cap.width * get_bitpp(dev->cap.format) / 8) * dev->cap.height, frame);
        HAL_LOGD("CHECKSUM=0x%X\n", checksum.value);
    }
    else
    {
        checksum.value = 0;
        HAL_LOGD("Checksum not supported with stripes\n");
    }
    if (dev->cap.callback != NULL)
        dev->cap.callback(NULL, MPP_EVENT_INTERNAL_TEST_RESERVED,
                          (void *)&checksum, dev->cap.user_data);
#endif

    HAL_LOGD("--HAL_DisplayDev_LcdifWaveshare5Inch_Blit\n");
    return kStatus_HAL_DisplaySuccess;
}

hal_display_status_t HAL_DisplayDev_LcdifWaveshare5Inch_Getbufdesc(
    const display_dev_t *dev, hw_buf_desc_t *in_buf, mpp_memory_policy_t *policy)
{
    hal_display_status_t ret = kStatus_HAL_DisplaySuccess;
    HAL_LOGD("++HAL_DisplayDev_LcdifWaveshare5Inch_Getbufdesc\n");

    do {
        if ((in_buf == NULL) || (policy == NULL))
        {
            HAL_LOGE("NULL pointer to buffer descriptor\n");
            ret = kStatus_HAL_DisplayError;
            break;
        }
        *policy          = HAL_MEM_ALLOC_INPUT;
        in_buf->alignment = FRAME_BUFFER_ALIGN;
        in_buf->nb_lines  = DISPLAY_DEV_LcdifWaveshare5Inch_HEIGHT;
        in_buf->cacheable = false;
    } while (false);

    HAL_LOGD("--HAL_DisplayDev_LcdifWaveshare5Inch_Getbufdesc\n");
    return ret;
}

/*******************************************************************************
 * Registration
 ******************************************************************************/

int HAL_DisplayDev_LcdifWaveshare5Inch_setup(display_dev_t *dev)
{
    *dev = s_DisplayDev_LcdifWaveshare5Inch;
    return 0;
}

#endif /* HAL_ENABLE_DISPLAY && HAL_ENABLE_DISPLAY_DEV_LcdifWaveshare5Inch */
