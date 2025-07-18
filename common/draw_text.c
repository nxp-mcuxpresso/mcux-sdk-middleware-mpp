/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdint.h>
#include <string.h>
#include "ascii_dmserif30_cells_rgb565.h"   // generated header
#include "draw_text.h"

static void blit_cell_RGB565_clipped(const uint16_t *cell,
                                           int dstX, int dstY,
                                           const text_context_t *ctx)
{
   if (!cell || !ctx) return;
   if (dstX >= ctx->width || dstY >= ctx->height || dstX + CELL_W <= 0 || dstY + CELL_H <= 0) return;
   int src_x = 0, src_y = 0;
   int cpy_w = CELL_W, cpy_h = CELL_H;
   if (dstX < 0) { src_x = -dstX; cpy_w -= src_x; dstX = 0; }
   if (dstY < 0) { src_y = -dstY; cpy_h -= src_y; dstY = 0; }
   if (dstX + cpy_w > ctx->width)  cpy_w = ctx->width - dstX;
   if (dstY + cpy_h > ctx->height)  cpy_h = ctx->height - dstY;
   if (cpy_w <= 0 || cpy_h <= 0) return;
   for (int y = 0; y < cpy_h; ++y) {
       const uint16_t *src = cell + (src_y + y) * CELL_W + src_x;
       uint16_t *dst = ctx->fb + (dstY + y) * ctx->fbStridePixels + dstX;
       memcpy(dst, src, (size_t)cpy_w * sizeof(uint16_t));
   }
}

void init_text_buf(text_context_t *ctx, void* buf, int width, int height, int fbStridePixels)
{
    if (ctx) {
        ctx->fb = buf;
        ctx->width = width;
        ctx->height = height;
        ctx->fbStridePixels = fbStridePixels;
    }
}

// Draw one line: x is left, y_mid is vertical center of the text row
void draw_text_line(const text_context_t *ctx, const char *s, int x, int y_mid)
{
   if (!ctx || !ctx->fb) return;
   int penX = x;
   int y = y_mid - (CELL_H / 2);          // cells are baseline-aligned internally
   for (const char *p = s; *p; ++p) {
       unsigned ch = (unsigned char)*p;
       if (ch >= 128) continue;
       if (ch == ' ') {
           penX += g_adv[' '];            // or ATLAS_SPACE_ADV
           continue;
       }
       const uint16_t *cell = g_cell_ptrs[ch];
       if (cell) {
           blit_cell_RGB565_clipped(cell, penX, y, ctx);
           int adv = (int)g_adv[ch];
           penX += (adv > 0 ? adv : CELL_W);
       } else {
           penX += g_adv[' '];
       }
       if (penX >= ctx->width) break;
   }
}

int get_font_width()
{
	return CELL_W;
}

int get_font_height()
{
	return CELL_H;
}
