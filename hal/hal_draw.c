/*
 * Copyright 2019-2026 NXP
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
 * this file is copied from sdk 2.11.0 folder:
 *  - middleware/eiq/common/gprintf/chgui.c
 * and modified to fit mpp needs
 */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

#include "fsl_common.h"
#include "font.h"
#include "hal_draw.h"
#include "hal_debug.h"
#include "mpp_api_types.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @brief Draws in X,Y configuration. */

#define INCREMENT(X, Y) ((X) + (Y))
#define DECREMENT(X, Y) ((X) - (Y))

#define GET_MSB(a, bits, ofs)  ((((a) >> (8 - (bits))) & ((1 << (bits)) - 1)) << (ofs))

#define BLACK_RGB565    (0x0000)
#define WHITE_RGB565    (0xFFFF)

/*!
 * @brief GUI font configuration structure
 */
typedef struct {
    char *name;
    uint8_t  x_size;
    uint8_t  y_size;
    const char *data;
} chgui_font_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief Draws character to point in LCD buffer.
 *
 * @param c input character for drawing
 * @param x drawing position on X axe
 * @param y drawing position on Y axe
 */
static void GUI_DispChar(uint16_t *lcd_buf, char c, int x, int y, uint16_t bcolor, uint16_t fcolor,
        uint32_t width, int stripe_top, int stripe_bottom);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/* Allocated GUI font configuration structure. */
static chgui_font_t _gFontTbl[] =
{{ "Font", FONT_XSize, FONT_YSize, consolas_12ptBitmaps }, };


/*!
 * @brief Draws text stored in local text buffer to LCD buffer.
 *
 * This function copy content of data from local text buffer
 * to the LCD. This function should be called after
 * GUI_PrintfToBuffer.
 *
 * @param lcd_buf LCD buffer address destination for drawing text

 */
void hal_draw_text565(uint16_t *lcd_buf, uint16_t fcolor, uint16_t bcolor, uint32_t width,
                   int x, int y, const char *label, int stripe_top, int stripe_bottom)
{
    int idx = 0;
    
    /* INT32-C: Validate parameters to prevent overflow in position calculation */
    assert(x >= 0 && x <= (INT_MAX / (int)_gFontTbl[0].x_size));

    while ((label[idx] != 0) && (idx < GUI_PRINTF_BUF_SIZE)) {
        /* INT32-C: Prevent signed integer overflow */
        assert(idx <= (INT_MAX / (int)_gFontTbl[0].x_size));
        assert(x <= (INT_MAX - (idx * (int)_gFontTbl[0].x_size)));
        GUI_DispChar(lcd_buf, label[idx],
                     INCREMENT(x, idx * _gFontTbl[0].x_size), y,
                     fcolor, bcolor, width, stripe_top, stripe_bottom);
        idx++;
    }
}

/*!
 * @brief Draw character to point in LCD buffer.
 *
 * @param c input character for drawing.
 * @param x drawing position on X axe.
 * @param y drawing position on Y axe.
 * @param pdata address to character bitmaps for console.
 * @param font_xsize size of font in X axe.
 * @param font_ysize size of font in Y axe.
 * @param fcolor foreground RGB565 color.
 * @param bcolor background RGB565 color.
 */
static inline void _GUI_DispChar(uint16_t *lcd_buf, char c, int x, int y, const char *pdata,
        int font_xsize, int font_ysize, uint16_t fcolor, uint16_t bcolor, uint16_t width, int stripe_top, int stripe_bottom)
{
    uint8_t j, t;
    int pos;
    uint8_t temp;
    uint8_t XNum;
    uint32_t base;
    
    /* INT31-C & INT32-C: Validate parameters at function entry */
    assert(x >= 0 && y >= 0);
    assert(font_xsize > 0 && font_xsize <= 255);
    assert(font_ysize > 0 && font_ysize <= 255);
    int xnum_calc = (font_xsize / 8) + 1;
    assert(xnum_calc >= 0 && xnum_calc <= 255);
    XNum = (uint8_t)xnum_calc;
    if (font_ysize % 8 == 0)
    {
        /* INT30-C: Prevent unsigned integer underflow */
        assert(XNum > 0U);
        XNum--;
    }
    if (c < ' ')
    {
        return;
    }
    c = c - ' ';
    /* INT32-C: Prevent signed integer overflow in base calculation */
    assert(c >= 0 && c <= (INT_MAX / ((int)XNum * font_ysize)));
    /* INT31-C: Safe conversion to uint32_t */
    base = (c * XNum * font_ysize);

    int ystart = MAX(stripe_top, y);
    int yend = MIN(stripe_bottom, y + font_ysize - 1);

    if ((ystart <= stripe_bottom) && (yend >= stripe_top))
    {   /* part of font is in stripe */
        for (j = 0; j < XNum; j++)
        {
            for (pos = ystart; pos < yend; pos++)
            {
                /* INT30-C: Prevent unsigned integer overflow in array index */
                uint32_t idx_calc = (uint32_t)(pos - y) + (uint32_t)j * (uint32_t)font_ysize;
                assert(base <= (UINT32_MAX - idx_calc));
                temp = (uint8_t) pdata[base + idx_calc];
                for (t = 0; t < font_xsize; t++)
                {
                    /* INT32-C: Prevent signed integer overflow in pixel calculation */
                    assert(x <= (INT_MAX - font_xsize));
                    assert((x + font_xsize) >= (int)t);
                    int pixel_x = (x + font_xsize) - (int)t;
                    assert(pixel_x >= 0);
                    if ((temp >> t) & 0x01) {
                        hal_draw_pixel565(lcd_buf, (uint32_t)pixel_x, (pos - stripe_top), fcolor, width);
                    } else {
                        hal_draw_pixel565(lcd_buf, (uint32_t)pixel_x, (pos - stripe_top), bcolor, width);
                    }
                }
            }
            /* INT32-C: Prevent signed integer overflow */
            assert(x <= (INT_MAX - 8));
            x = x + 8;
        }
    }
}

static inline void GUI_DispChar(uint16_t *lcd_buf, char c, int x, int y, uint16_t fcolor, uint16_t bcolor,
        uint32_t width, int stripe_top, int stripe_bottom)
{
    _GUI_DispChar(lcd_buf, c, x, y, _gFontTbl[0].data, (int)_gFontTbl[0].x_size,
            (int)_gFontTbl[0].y_size, fcolor, bcolor, width, stripe_top, stripe_bottom);
}

/*!
 * @brief Converts image from RGB888 to RGB565.
 *
 * @param r 0-255 red color value
 * @param g 0-255 green color value
 * @param b 0-255 blue color value
 * @return color in RGB565
 */
static inline uint16_t ConvRgb888Rgb565(mpp_color_t col)
{
    uint16_t b = GET_MSB(col.rgb.B, 5, 0);
    uint16_t g = GET_MSB(col.rgb.G, 6, 5);
    uint16_t r = GET_MSB(col.rgb.R, 5, 11);

    return (uint16_t)(r | g | b);
}

static inline void hal_draw_rect565(uint16_t *lcd_buf, hal_rect_t rect,
                  mpp_color_t rgb, uint32_t width,
                  int stripe_top, int stripe_bottom)
{
    /* INT31-C: Safe conversion through ConvRgb888Rgb565 */
    uint16_t color16 = ConvRgb888Rgb565(rgb);

    /* horizontal top bar */
    if ((rect.top >= stripe_top) && (rect.top <= stripe_bottom))
    {
        /* INT31-C: Validate rect coordinates are non-negative before conversion */
        assert(rect.left >= 0 && rect.right >= 0);
        assert(rect.left <= rect.right);
        for (int i = rect.left; i < rect.right; i++) {
            assert(i >= 0);
            hal_draw_pixel565(lcd_buf, i, rect.top - stripe_top, color16, width);
        }
    }
    /* horizontal bottom bar */
    if ((rect.bottom >= stripe_top) && (rect.bottom <= stripe_bottom))
    {
        for (int i = rect.left; i < rect.right; i++)
        {
            hal_draw_pixel565(lcd_buf, i, rect.bottom - stripe_top, color16, width);
        }
    }

    /* verticals */
    int ystart = MAX(stripe_top, rect.top);
    int yend = MIN(stripe_bottom, rect.bottom);
    if ((ystart <= stripe_bottom) && (yend >= stripe_top))
    {
        for (int i = ystart; i <= yend; i++)
        {
            hal_draw_pixel565(lcd_buf, rect.left, i - stripe_top, color16, width);
            hal_draw_pixel565(lcd_buf, rect.right, i - stripe_top, color16, width);
        }
    }
}

static inline void hal_draw_pixel565(uint16_t *pDst, uint32_t x, uint32_t y, uint16_t color, uint32_t lcd_w)
{
    /* INT30-C: Prevent unsigned integer overflow in array index */
    assert(y <= (UINT32_MAX / lcd_w) && (y * lcd_w) <= (UINT32_MAX - x));
    /* INT31-C: Validate width fits in uint16_t for pixel operations (Line 195) */
    assert(lcd_w <= UINT16_MAX);
    pDst[y * (lcd_w) + x] = color;
}

int hal_landmark(uint8_t *frame, int width, int height, mpp_pixel_format_t format,
                        mpp_landmark_t *lk, int stripe, int stripe_max)
{
    /* for now only support RGB565 format */
    if (format != MPP_PIXEL_RGB565) return MPP_INVALID_PARAM;
    
    /* for now only support full frame */    
    if (stripe) return MPP_INVALID_PARAM;

    /* INT32-C: Validate landmark parameters at function entry */
    assert(lk->width > 0 && lk->width <= (INT_MAX / 2));
    assert(width > 0 && height > 0);

    uint16_t color16 = ConvRgb888Rgb565(lk->color);

    int width_times_2 = lk->width * 2;

    /* check landmark size versus image border */
    /* INT32-C: Prevent signed integer overflow in boundary checks */
    assert(height >= width_times_2);
    assert(width >= width_times_2);
    if ( (lk->y <= lk->width) || (lk->y >= (height - width_times_2))
        || (lk->x <= lk->width) || (lk->x >= (width - width_times_2)) )
    {
        HAL_LOGE("invalid landmark size versus image border x:%d y:%d thickness:%d width:%d height:%d\n", lk->x, lk->y, lk->width, width, height);
        return MPP_INVALID_PARAM;
    }
    
    /* draw a 'cross' shape */
    int x, y;
    /* draw horizontal bar */
    for (y = lk->y; y < lk->y + lk->width; y ++)
    {
        for (x = (lk->x - lk->width); x < (lk->x + (lk->width*2)); x++)
        {
            hal_draw_pixel565((uint16_t *) frame, x, y, color16, width);
        }
    }
    /* draw vertical bar */
    for (y = lk->y - lk->width; y < lk->y + (lk->width * 2); y++)
    {
        for (x = lk->x; x < (lk->x + lk->width); x++)
        {
            hal_draw_pixel565((uint16_t *) frame, x, y, color16, width);
        }
    }
    return MPP_SUCCESS;
}

int hal_label_rectangle(uint8_t *frame, int width, int height, mpp_pixel_format_t format,
                        mpp_labeled_rect_t *lr, int stripe, int stripe_max)
{
    uint32_t xsize, ysize, lw;
    hal_rect_t rect;
    int stripe_top, stripe_bottom;

    /* for now only support RGB565 format */
    if (format != MPP_PIXEL_RGB565) return MPP_INVALID_PARAM;

    /* INT32-C: Validate rectangle parameters at function entry */
    assert(lr->left >= 0 && lr->top >= 0);
    assert(lr->right >= lr->left && lr->bottom >= lr->top);

    /* INT31-C: Validate before conversion to uint32_t */
    xsize = lr->right - lr->left;
    ysize = lr->bottom - lr->top;

    /* check rectangle fits in frame */
    if (    (lr->left < 0) || (lr->top < 0)
            || (lr->right > width) || (lr->bottom > height)
            || (lr->left > lr->right) || (lr->top > lr->bottom)
    ) return MPP_INVALID_PARAM;

    /* check rectangle is large enough to draw lines inside */
    /* if not, skip the drawing to avoid crash */
    if (   ( xsize < (2 * lr->line_width) )
        || ( ysize < (2 * lr->line_width) ) )
    {
        HAL_LOGD("rectangle %dx%d too small to draw %d line width\n", xsize, ysize, lr->line_width);
        return MPP_SUCCESS;
    }

    if (stripe)
    {
        /* INT32-C: Prevent signed integer overflow (Line 355) */
        int stripe_h = height / stripe_max;
        assert(stripe > 0 && stripe_h <= (INT_MAX / stripe));
        assert((stripe - 1) <= (INT_MAX / stripe_h));
        int temp_stripe_top = (stripe - 1) * stripe_h;
        assert(stripe_h > 0 && temp_stripe_top <= (INT_MAX - stripe_h));
        stripe_top = (stripe - 1) * stripe_h;
        stripe_bottom = temp_stripe_top + stripe_h - 1;
    }
    else
    {
        stripe_top = 0;
        stripe_bottom = height - 1;
    }

    for (lw = 0; lw < lr->line_width; lw++) {
        rect.top = lr->top + lw;
        rect.left = lr->left + lw;
        rect.right = lr->right - lw;
        rect.bottom = lr->bottom - lw;
        hal_draw_rect565((uint16_t *)frame, rect, lr->line_color, width, stripe_top, stripe_bottom);
    }

    /* check label fits in frame */
    /* INT31-C: Validate string length calculation */
    size_t label_len = strlen((const char *)lr->label);
    assert(label_len <= (SIZE_MAX / FONT_XSize));
    size_t strxsize_calc = label_len * FONT_XSize;
    assert(strxsize_calc <= INT_MAX);
    int strxsize = (int)strxsize_calc;
    /* INT32-C: Prevent signed integer overflow in boundary check */
    assert(lr->left <= (width - strxsize));
    if (    (lr->left + strxsize > width)
            || (lr->top + FONT_YSize > height)
    )   return MPP_INVALID_PARAM;

    hal_draw_text565((uint16_t *)frame, WHITE_RGB565, BLACK_RGB565, width,
                 lr->left, lr->top, (char *)lr->label, stripe_top, stripe_bottom);
    return MPP_SUCCESS;
}
