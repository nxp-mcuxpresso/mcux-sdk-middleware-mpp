/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef DISPLAY_FONT_DRAW_TEXT_H_
#define DISPLAY_FONT_DRAW_TEXT_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef struct {
    uint16_t *fb;          // framebuffer
    uint16_t width;
    uint16_t height;
    uint16_t fbStridePixels;  // framebuffer stride in pixels
} text_context_t;

void init_text_buf(text_context_t *ctx, void* buf, int width, int height, int fbStridePixels);
void draw_text_line(const text_context_t *ctx, const char *s, int x, int y_mid);
int get_font_width();
int get_font_height();

#if defined(__cplusplus)
}
#endif /* __cplusplus */
#endif /* DISPLAY_FONT_DRAW_TEXT_H_ */
