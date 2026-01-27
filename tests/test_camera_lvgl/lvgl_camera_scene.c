/*
 * Copyright 2025-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief the lvgl scene for camera viewfinder:
 * an image view + a start/stop button.
 */

#include "FreeRTOS.h"
#include "semphr.h"
#include "lvgl.h"
#include "mpp_config.h"
#include "fsl_debug_console.h"

extern SemaphoreHandle_t g_button_press;
lv_obj_t * g_img = NULL;    /* the lvgl image object shared with mpp */
lv_image_dsc_t img_dsc;

static void btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED) 
    {
        static uint8_t cnt = 0;
        cnt++;

        BaseType_t ret = xSemaphoreGive(g_button_press);
        if (ret == pdFALSE)
        {
            PRINTF("Synchronisation issue with mpp task\r\n");
        }

        /*Get the first child of the button which is the label and change its text*/
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        if (cnt%2) 
            lv_label_set_text(label, "Start");
        else
            lv_label_set_text(label, "Stop");
    }
}

/**
 * Create a full screen image, a button with a label and react on click event.
 */
void lvgl_camera_scene(void)
{
    /* get screen object and set style */
    lv_obj_t * scr = lv_screen_active();
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_color(scr, lv_palette_lighten(LV_PALETTE_GREY, 4), 0);
    lv_obj_set_style_pad_all(lv_screen_active(), 8, 0);
    lv_obj_set_style_pad_top(lv_screen_active(), 48, 0);
    lv_obj_set_style_pad_gap(lv_screen_active(), 8, 0);
    lv_obj_set_layout(scr, LV_LAYOUT_NONE);

    /* create full screen image */
    static lv_image_dsc_t img_dsc = {
        .header.magic = LV_IMAGE_HEADER_MAGIC,
        .header.w = APP_DISPLAY_WIDTH,
        .header.h = APP_DISPLAY_HEIGHT,
        .data_size = APP_DISPLAY_WIDTH * APP_DISPLAY_HEIGHT * LV_COLOR_DEPTH / 8,
        .header.cf = LV_COLOR_FORMAT_NATIVE,          /*Set the color format*/
        .data = NULL,
    };
    g_img = lv_image_create(lv_screen_active());
    lv_image_set_src(g_img, &img_dsc);

    /* create button */
    lv_obj_t * btn = lv_btn_create(scr);            /*Add a button to the current screen*/
    lv_obj_set_size(btn, 200, 100);                 /*Set its size*/
    lv_obj_set_align(btn, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL); /*Assign a callback to the button*/

    /* create button's label */
    lv_obj_t * label = lv_label_create(btn);    /*Add a label to the button*/
    lv_label_set_text(label, "Stop");           /*Set the labels text*/
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
}

