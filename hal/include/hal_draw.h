/*
 * Copyright 2019-2024 NXP
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
 *  */

#ifndef HAL_DRAW_H
#define HAL_DRAW_H

#include "mpp_api_types.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

/*! \addtogroup HAL_TYPES
*  @{
*/

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*! @brief Local text buffer size. */
#define GUI_PRINTF_BUF_SIZE 64

/*! @brief rectangle positions. */
typedef struct {
    int top;
    int left;
    int bottom;
    int right;
} hal_rect_t;

/** @} */

/*******************************************************************************
 * API
 ******************************************************************************/

/*! \addtogroup HAL_OPERATIONS
*  @{
*/

/*!
 * @brief Draws pixel with RGB565 color to defined point.
 *
 * @param pDst image data address of destination buffer
 * @param x drawing position on X axe
 * @param y drawing position on Y axe
 * @param color RGB565 encoded value
 * @param lcd_w lcd width
 */
static inline void hal_draw_pixel565(uint16_t *pDst, uint32_t x, uint32_t y, uint16_t color, uint32_t lcd_w);

/*!
 * @brief Draws text stored in label pointer to LCD buffer.
 *
 * This function copy content of data from label text buffer
 * to the LCD.
 *
 * @param lcd_buf LCD buffer address destination for drawing text
 * @param fcolor foreground color in rgb565 format
 * @param bcolor background color in rgb565 format
 * @param width LCD width in pixels
 * @param x drawing position on X axis
 * @param y drawing position on Y axis
 * @param label C string to draw
 * @param stripe_top top boundary of the stripe in which to draw (pixels from top)
 * @param stripe_bottom bottom boundary of the stripe in which to draw (pixels from top)
 */
static inline void hal_draw_text565(uint16_t *lcd_buf, uint16_t fcolor, uint16_t bcolor, uint32_t width,
                   int x, int y, const char *label, int stripe_top, int stripe_bottom);

/*!
 * @brief Draws rectangle
 *
 * @param[in] lcd_buf LCD buffer address destination for drawing rectangle
 * @param[in] rect rectangle position and size (top, left, bottom, right)
 * @param[in] rgb color encoded as mpp_color_t (r, g, b fields)
 * @param[in] width LCD width in pixels
 * @param[in] stripe_top top boundary of the stripe in which to draw (pixels from top)
 * @param[in] stripe_bottom bottom boundary of the stripe in which to draw (pixels from top)
 */
static inline void hal_draw_rect565(uint16_t *lcd_buf, hal_rect_t rect,
        mpp_color_t rgb, uint32_t width,
        int stripe_top, int stripe_bottom);

/** @} */

#if defined(__cplusplus)
}
#endif /* __cplusplus*/


#endif /* HAL_DRAW_H */
