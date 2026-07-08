/*
 * Copyright 2021-2026 NXP.
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

/*! \addtogroup HAL_TYPES
 *  @{
 */

/**
 * @brief hal graphics (gfx) device declaration. Graphics processing devices can be used to perform conversion from one
 * image format to another, resize images and compose images on top of one another.
 * Examples of graphics devices include the PXP (pixel pipeline) found on many i.MXRT series MCUs.
 */

#ifndef _HAL_GRAPHICS_DEV_H_
#define _HAL_GRAPHICS_DEV_H_

#include "mpp_api_types.h"
#include "hal_types.h"

/** Name of the graphic device using CPU operations **/
#define HAL_GFX_DEV_CPU_NAME "gfx_CPU"

/** Gfx surface parameters */
typedef struct _gfx_surface
{
    int height;                /*!< buffer height */
    int width;                 /*!< buffer width */
    int pitch;                 /*!< buffer pitch */
    int pitch_uv;              /*!< buffer pitch for chroma planes */
    int left;                  /*!< left position */
    int top;                   /*!< top position */
    int right;                 /*!< right position */
    int bottom;                /*!< bottom position */
    int swapByte;              /*!< swap byte per two bytes */
    mpp_pixel_format_t format; /*!< pixel format */
    void *buf;                 /*!< buffer */
    void *buf_u;               /*!< buffer for chroma plane U */
    void *buf_v;               /*!< buffer for chroma plane V */
    void *lock;                /*!< the structure is determined by hal and set to null if not use in hal*/
} gfx_surface_t;

/** gfx rotate target*/
typedef enum _gfx_rotate_target
{
    kGFXRotateTarget_None = 0,
    kGFXRotate_SRCSurface,
    kGFXRotate_DSTSurface,
} gfx_rotate_target_t;

/** gfx rotate configuration*/
typedef struct _gfx_rotate_config
{
    gfx_rotate_target_t target;
    mpp_rotate_degree_t degree;
    float custom_angle;
} gfx_rotate_config_t;

typedef struct _gfx_dev gfx_dev_t;

/** @} */

/*! \addtogroup HAL_OPERATIONS
*  @{
*/

/** @brief Operation that needs to be implemented by gfx device */

typedef struct
{
    int (*init)(gfx_dev_t *dev, void *param);                               /*!< initialize the dev */
    int (*deinit)(gfx_dev_t *dev);                                          /*!< deinitialize the dev */
    int (*get_buf_desc)(const gfx_dev_t *dev,
            hw_buf_desc_t *in_buf, hw_buf_desc_t *out_buf, mpp_memory_policy_t *policy); /*!< get buffer descriptors and policy */
    int (*blit)(
        const gfx_dev_t *dev, const gfx_surface_t *pSrc, const gfx_surface_t *pDst, const gfx_rotate_config_t *pRotate, mpp_flip_mode_t flip); /*!< blit source to destination surface */
    int (*drawRect)(const gfx_dev_t *dev, gfx_surface_t *pOverlay, int x, int y, int w, int h, int color); /*!< draw rectangle in overlay surface */
    int (*drawPicture)(
        const gfx_dev_t *dev, gfx_surface_t *pOverlay, int x, int y, int w, int h, int alpha, const char *pIcon); /*!< draw picture in overlay surface */
    int (*drawText)(const gfx_dev_t *dev,
                    gfx_surface_t *pOverlay,
                    int x,
                    int y,
                    int textColor,
                    int bgColor,
                    int type,
                    const char *pText);                                      /*!< draw text in overlay surface */
    int (*compose)(const gfx_dev_t *dev,
                   gfx_surface_t *pSrc,
                   gfx_surface_t *pOverlay,
                   gfx_surface_t *pDst,
                   gfx_rotate_config_t *pRotate,
                   mpp_flip_mode_t flip);                                    /*!< blit source with overlay to destination surface */
    int (*finish)(gfx_dev_t *dev);                                          /*!< finalize rendering into destination surface */
} gfx_dev_operator_t;


/*! @brief The mpp callback function prototype */
typedef int (*mpp_callback_t)(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data);

/** @} */

/*! \addtogroup HAL_TYPES
 *  @{
 */

/** @brief Attributes of a graphics device. */
struct _gfx_dev
{
    int id;                             /*!< unique id */
    const gfx_dev_operator_t *ops;      /*!< operations */
    gfx_surface_t src;                  /*!< graphic source surface */
    gfx_surface_t dst;                  /*!< graphic destination surface */
    mpp_callback_t callback;            /*!< callback */
    void *cb_user_data;                 /*!< param for the callback */
    void *user_data;                    /*!< stores the vglite context pointer */
    mpp_t mpp;                          /*!< mpp handle to be passed to the callback */
};

/*!
 * @brief Register the graphic device with the CPU operations
 *
 * @param[in] dev graphic device to register
 * @return error code (0: success, otherwise: failure)
 *
 */
int HAL_GfxDev_CPU_Register(gfx_dev_t *dev);

/*!
 * @brief Register the graphic device with the GPU operations
 *
 * @param[in] dev graphic device to register
 * @return error code (0: success, otherwise: failure)
 *
 */
int HAL_GfxDev_GPU_Register(gfx_dev_t *dev);

/** @} */

#endif /* _HAL_GRAPHICS_DEV_H_ */
