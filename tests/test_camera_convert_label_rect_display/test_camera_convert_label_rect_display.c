/*
 * Copyright 2022-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * 2D camera -> image converter -> draw labeled rectangle -> display
 * The camera view finder is displayed with 3 labeled rectangles
 * The position of labeled rectangles is periodically updated.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"

#ifndef EMULATOR
/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "board_init.h"
#else
#include <stdio.h>
#define PRINTF printf
#define main app_main
#endif

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

/*******************************************************************************
 * Definitions
 ******************************************************************************/

typedef struct _args_t {
    char camera_name[32];
    char display_name[32];
    mpp_pixel_format_t camera_format;
    mpp_pixel_format_t display_format;
} args_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void app_task(void *params);

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Application entry point.
 */
int main(int argc, char *argv[])
{
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

#ifndef EMULATOR
    /* Init board hardware. */
    BOARD_Init();
#endif

    PRINTF("****** TEST test_camera_convert_label_rect_display ******\n");
    PRINTF("\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

#ifdef EMULATOR
    strcpy(args->display_name, "opencv0");
    if (argc == 3) {
        if (strcmp(argv[1], "rgb_sim") &&
                strcmp(argv[1], "ir_sim") &&
                strcmp(argv[1], "yuv_sim")) {
            PRINTF("Camera %s not implemented\n", argv[1]);
            goto err;
        }
        strcpy(args->camera_name, argv[1]);

        if (!strncmp(argv[2], "rgb",3)) {
            args->camera_format = MPP_PIXEL_RGB;
        } else if (!strncmp(argv[2], "gray",4)) {
            args->camera_format = MPP_PIXEL_GRAY;
        } else if (!strncmp(argv[2], "yuv",3)) {
            args->camera_format = MPP_PIXEL_YUYV;
        } else {
            PRINTF("Invalid pixel format %s\n", argv[2]);
            goto err;
        }
        args->display_format = args->camera_format;
    } else {
        PRINTF("Invalid argument number %d\n", argc);
        goto err;
    }
#else /* i.MX hardware */
    strcpy(args->display_name, APP_DISPLAY_NAME);
    strcpy(args->camera_name, APP_CAMERA_NAME);
    args->camera_format = APP_CAMERA_FORMAT;
    args->display_format = APP_DISPLAY_FORMAT;
#endif /* EMULATOR */

    ret = xTaskCreate(
            app_task,
            "app_task",
            configMINIMAL_STACK_SIZE + 1000,
            (void *) args,
            APP_DEFAULT_PRIO,
            &handle);

err:
    if (pdPASS != ret)
    {
        PRINTF("Failed to create app_task task");
        while (1);
    }

    vTaskStartScheduler();
    for (;;)
        vTaskSuspend(NULL);
    return 0;
}

#ifdef EMULATOR
#define FRAME_WIDTH    (640)
#define FRAME_HEIGHT   (480)
#else
#define FRAME_WIDTH    APP_CAMERA_WIDTH
#define FRAME_HEIGHT   APP_CAMERA_HEIGHT
#endif

static void app_task(void *params) {

    int width  = FRAME_WIDTH;
    int height = FRAME_HEIGHT;
    int ret;
    mpp_elem_handle_t elem = 0;
    args_t *args = (args_t *) params;

    ret = mpp_api_init(NULL);
    if (ret)
        goto err;

    mpp_t mp;
    mpp_params_t mpp_params;
    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.exec_flag = MPP_EXEC_RC;
    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
        goto err;

    mpp_camera_params_t cam_params;
    memset(&cam_params, 0 , sizeof(cam_params));
    cam_params.height = height;
    cam_params.width =  width;
    cam_params.format = args->camera_format;
    cam_params.fps    = 30;
    ret = mpp_camera_add(mp, args->camera_name, &cam_params);
    if (ret) {
        PRINTF("Failed to add camera %s\n", args->camera_name);
        goto err;
    }

    mpp_element_params_t elem_params;
#ifndef EMULATOR
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    if ((APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_90) || (APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_270)) {
        elem_params.convert.out_buf.height = APP_DISPLAY_WIDTH;
        elem_params.convert.out_buf.width  = APP_DISPLAY_HEIGHT;
    }
    else {
        elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
        elem_params.convert.out_buf.width  = APP_DISPLAY_WIDTH;
    }
    elem_params.convert.flip = FLIP_HORIZONTAL;
    elem_params.convert.pixel_format = args->display_format;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_ROTATE;

    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    if (ret) {
        PRINTF("Failed to add element CONVERT (COLOR + FLIP)\n");
        goto err;
    }
#endif

    /* add three label rectangle */
    mpp_element_params_t elem_params_rects;
    memset(&elem_params_rects, 0, sizeof(mpp_element_params_t));
    mpp_labeled_rect_t labels[3];
    memset(&labels, 0, sizeof(labels));

    /* params init */
    elem_params_rects.labels.max_count = 16;
    elem_params_rects.labels.detected_count = 3;
    elem_params_rects.labels.rectangles = labels;

    /* first */
    labels[0].top = FRAME_HEIGHT * 0.1f;
    labels[0].left = FRAME_WIDTH * 0.1f;
    labels[0].bottom = FRAME_HEIGHT * 0.4f;
    labels[0].right = FRAME_WIDTH * 0.3f;
    labels[0].line_width = 2;
    labels[0].line_color.rgb.B = 0xff;
    strcpy((char *)labels[0].label, "toto");
    /* second */
    labels[1].line_width = 2;
    labels[1].top = FRAME_HEIGHT * 0.2f;
    labels[1].left = FRAME_WIDTH * 0.2f;
    labels[1].bottom = FRAME_HEIGHT * 0.5f;
    labels[1].right = FRAME_WIDTH * 0.4f;
    labels[1].line_color.rgb.G = 0xff;
    strcpy((char *)labels[1].label, "titi");
    /* third */
    labels[2].line_width = 2;
    labels[2].top = FRAME_HEIGHT * 0.3f;
    labels[2].left = FRAME_WIDTH * 0.3f;
    labels[2].bottom = FRAME_HEIGHT * 0.6f;
    labels[2].right = FRAME_WIDTH * 0.5f;
    labels[2].line_color.rgb.R = 0xff;
    strcpy((char *)labels[2].label, "tata");

    /* retrieve the element handle while add api */
    ret = mpp_element_add(mp, MPP_ELEMENT_LABELED_RECTANGLE, &elem_params_rects, &elem);
    if (ret) {
        PRINTF("Failed to add element LABELED_RECTANGLE (0x%x)\r\n", ret);
        goto err;
    }

    /* then rotate if needed */
    if ((APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_90) || (APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_270)) {
        memset(&elem_params, 0, sizeof(elem_params));
        elem_params.convert.dev_name = NULL;
        elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
        elem_params.convert.out_buf.width  = APP_DISPLAY_WIDTH;
        elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
        elem_params.convert.ops = MPP_CONVERT_ROTATE;
        ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);

        if (ret) {
            PRINTF("Failed to add element CONVERT (ROTATE)\r\n");
            goto err;
        }
    }

    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.width = APP_DISPLAY_WIDTH;
    disp_params.format = args->display_format;
    ret = mpp_display_add(mp, args->display_name, &disp_params);
    if (ret) {
        PRINTF("Failed to add display %s\n", args->display_name);
        goto err;
    }

    ret = mpp_start(mp, 1);
    if (ret) {
        PRINTF("Failed to start pipeline\n");
        goto err;
    }

    /* update example */
    elem_params_rects.labels.detected_count = 3;
    const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
    uint32_t var = 0;
    int step0 = FRAME_WIDTH / 100, step1 = FRAME_WIDTH / 100;

    do {
        /* blink third rectangle */
        labels[2].clear = var % 5;

        /* move first and second rectangles */
        if (((int)labels[0].bottom + step0) > height) {
            step0 = -10;
        } else if ((labels[0].top + step0) < 0) {
            step0 = 10;
        }
        labels[0].top += step0;
        labels[0].bottom += step0;
        if (((int)labels[1].right + step1) > width) {
            step1 = -10;
        } else if ((labels[1].left + step1) < 0) {
            step1 = 10;
        }
        labels[1].left += step1;
        labels[1].right += step1;

        mpp_element_update(mp, elem, &elem_params_rects);
        var++;
        vTaskDelay(xDelay);

    } while (true);

    /* pause application task */
    vTaskSuspend(NULL);

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}

