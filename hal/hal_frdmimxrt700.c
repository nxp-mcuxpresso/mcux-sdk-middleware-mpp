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

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "hal_os.h"

#include "fsl_cache.h"

#include "hal_graphics_dev.h"
#include "hal_vdec_dev.h"
#include "hal_utils.h"
#include "hal_os.h"
#include "hal_mc.h"

/* Decoder setup */
hal_img_decoder_setup_t decoder_setup[] =
{
    {"jpeg_HW", HAL_JPEG_HW_Register},
    {"jpeg_CPU", HAL_JPEG_CPU_Register},
};

int setup_vdec_dev(hal_img_decoder_setup_t decoder_setup[], int vdec_nb,
                      const char *name, vdec_dev_t *dev);
int hal_img_decoder_setup(const char *name, vdec_dev_t *dev)
{
    return setup_vdec_dev(decoder_setup, ARRAY_SIZE(decoder_setup), name, dev);
}

/* Graphics setup */
hal_graphics_setup_t gfx_setup[] =
{
    {"gfx_GPU", HAL_GfxDev_GPU_Register},
    {"gfx_CPU", HAL_GfxDev_CPU_Register},
};

int setup_graphic_dev(hal_graphics_setup_t gfx_setup[], int graphic_nb,
                      const char *name, gfx_dev_t *dev);
int hal_gfx_setup(const char *name, gfx_dev_t *dev)
{
    return setup_graphic_dev(gfx_setup, ARRAY_SIZE(gfx_setup), name, dev);
}

/* Display setup */
#if MPP_OS_FREERTOS
int HAL_DisplayDev_LcdifWaveshare5Inch_setup(display_dev_t *dev);
#elif MPP_OS_ZEPHYR
int HAL_DisplayDev_Zephyr_setup(display_dev_t *dev);
#endif
int HAL_DisplayDev_Fbdev_setup(display_dev_t *dev);

hal_display_setup_t display_setup[] =
{
#if MPP_OS_FREERTOS
    {"LcdifWaveshare5Inch", HAL_DisplayDev_LcdifWaveshare5Inch_setup},
#elif MPP_OS_ZEPHYR
    {"LcdifWaveshare5Inch", HAL_DisplayDev_Zephyr_setup},
#endif
    {"FBdev", HAL_DisplayDev_Fbdev_setup},
};

int setup_display_dev(hal_display_setup_t display_setup[], int display_nb,
                      const char *name, display_dev_t *dev);
int hal_display_setup(const char *name, display_dev_t *dev)
{
    return setup_display_dev(display_setup, ARRAY_SIZE(display_setup), name, dev);
}

/* Camera setup */
int HAL_CameraDev_EzhV_Ov7670_setup(const char *name, camera_dev_t *dev);
int HAL_CameraDev_USB_setup(const char *name, camera_dev_t *dev);
int HAL_CameraDev_VirtualUSB_setup(const char *name, camera_dev_t *dev);

hal_camera_setup_t camera_setup[] =
{
    {"EzhV_Ov7670", HAL_CameraDev_EzhV_Ov7670_setup},
    {"USB_cam",     HAL_CameraDev_USB_setup},
    {"Virtual_USB_cam", HAL_CameraDev_VirtualUSB_setup},
};

int setup_camera_dev(hal_camera_setup_t camera_setup[], int camera_nb,
                      const char *name, camera_dev_t *dev);
int hal_camera_setup(const char *name, camera_dev_t *dev)
{
    return setup_camera_dev(camera_setup, ARRAY_SIZE(camera_setup), name, dev);
}

/* multicore hal setup */
int hal_mc_dev_setup(const char *name, multicore_dev_t *dev)
{
    return HAL_MultiCoreDev_setup(name, dev);
}

#if defined(MIMXRT798S_cm33_core0_SERIES)
void HAL_DCACHE_CleanInvalidateByRange(uint32_t addr, uint32_t size)
{
    hal_ctx_t ctx;

    hal_atomic_enter(&ctx);
    XCACHE_CleanInvalidateCacheByRange(addr, size);
    hal_atomic_exit(&ctx);
    return;
}

void HAL_DCACHE_CleanByRange(uint32_t addr, uint32_t size)
{
    hal_ctx_t ctx;

    hal_atomic_enter(&ctx);
    XCACHE_CleanCacheByRange(addr, size);
    hal_atomic_exit(&ctx);
    return;
}

void HAL_DCACHE_InvalidateByRange(uint32_t addr, uint32_t size)
{
    hal_ctx_t ctx;

    hal_atomic_enter(&ctx);
    XCACHE_InvalidateCacheByRange(addr, size);
    hal_atomic_exit(&ctx);
    return;
}
#else
void HAL_DCACHE_CleanInvalidateByRange(uint32_t addr, uint32_t size)
{
    return;
}

void HAL_DCACHE_CleanByRange(uint32_t addr, uint32_t size)
{
    return;
}

void HAL_DCACHE_InvalidateByRange(uint32_t addr, uint32_t size)
{
    return;
}
#endif /* MIMXRT798S_cm33_core0_SERIES */
