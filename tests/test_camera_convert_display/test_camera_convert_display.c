/*
 * Copyright 2022-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * 2D camera -> image converter -> display
 * The camera view finder is displayed on screen.
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
 * Definitions
 ******************************************************************************/

#ifndef CAMERA_FORMAT1
#define CAMERA_FORMAT1 0
#endif
#ifndef CROP_CONFIG
#define CROP_CONFIG 0
#endif
#ifndef FLIP_CONFIG
#define FLIP_CONFIG 0
#endif
#ifndef CONFIG_RC_CYCLE_FRAMES
#define CONFIG_RC_CYCLE_FRAMES 0
#endif

typedef struct _args_t {
    char camera_name[32];
    char display_name[32];
    mpp_pixel_format_t camera_format;
    mpp_pixel_format_t display_format;
} args_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

/* define this flag to enable MPP stop and start */
#ifndef CONFIG_STOP_MPP
#define CONFIG_STOP_MPP 0
#endif
#if (CONFIG_STOP_MPP == 1)
#define MPP_STOP_COUNT_MAX 1500
#define MPP_STOP_DELAY_MS 2500
#endif

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

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

    PRINTF("****** TEST test_camera_convert_display ******\r\n");
    PRINTF("****** PARAMS: CAMERA_FORMAT1 = [%d] ******\r\n", CAMERA_FORMAT1);
    PRINTF("****** PARAMS: CROP_CONFIG = [%d] ******\r\n", CROP_CONFIG);
    PRINTF("****** PARAMS: FLIP_CONFIG = [%d] ******\r\n", FLIP_CONFIG);
    PRINTF("****** PARAMS: CONFIG_STOP_MPP = [%d] ******\r\n", CONFIG_STOP_MPP);
    PRINTF("****** PARAMS: CONFIG_RC_CYCLE_FRAMES = [%d] ******\r\n", CONFIG_RC_CYCLE_FRAMES);
    PRINTF("\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\r\n");
        goto err;
    }

#ifdef EMULATOR
    strcpy(args->display_name, "opencv0");
    if (argc == 3) {
        if (strcmp(argv[1], "rgb_sim") &&
            strcmp(argv[1], "ir_sim") &&
            strcmp(argv[1], "yuv_sim")) {
                PRINTF("Camera %s not implemented\r\n", argv[1]);
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
            PRINTF("Invalid pixel format %s\r\n", argv[2]);
            goto err;
        }
        args->display_format = args->camera_format;
    } else {
        PRINTF("Invalid argument number %d\r\n", argc);
        goto err;
    }
#else /* i.MX hardware */
    strcpy(args->display_name, APP_DISPLAY_NAME);
    strcpy(args->camera_name, APP_CAMERA_NAME);
#if (CAMERA_FORMAT1 == 1)
    args->camera_format = APP_CAMERA_FORMAT1;
#else
    args->camera_format = APP_CAMERA_FORMAT;
#endif
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
	args_t *args = (args_t *) params;

	PRINTF("[%s]\r\n", mpp_get_version());

#if( defined(CONFIG_RC_CYCLE_FRAMES) && (CONFIG_RC_CYCLE_FRAMES != 0))
	mpp_api_params_t api_param = {0};
	api_param.rc_cycle_min = CONFIG_RC_CYCLE_FRAMES * 33; /* cycle period multiple of frames @30fps */
    api_param.rc_cycle_inc = 33; /* one frame @30fps */
    ret = mpp_api_init(&api_param);
#else
	ret = mpp_api_init(NULL);
#endif
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
        PRINTF("Failed to add camera %s\r\n", args->camera_name);
        goto err;
    }

#ifndef EMULATOR
    mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    elem_params.convert.pixel_format = args->display_format;
    elem_params.convert.out_buf.width = APP_DISPLAY_WIDTH;
    elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_ROTATE;

#if (FLIP_CONFIG == 1)
    elem_params.convert.flip = FLIP_HORIZONTAL;
#elif (FLIP_CONFIG == 2)
    elem_params.convert.flip = FLIP_VERTICAL;
#elif (FLIP_CONFIG == 3)
    elem_params.convert.flip = FLIP_BOTH;
#else
    elem_params.convert.flip = FLIP_NONE;
#endif

#if (CROP_CONFIG != 0)
    elem_params.convert.ops |= MPP_CONVERT_CROP;
#endif

#if (CROP_CONFIG == 1)
    /*** no rotate, crop to square image ***/
    elem_params.convert.angle = ROTATE_0;
    /* input crop position */
    int pad = (FRAME_WIDTH - FRAME_HEIGHT)/2; /* pad to crop middle of image */
    elem_params.convert.crop.top = 0;
    elem_params.convert.crop.left = pad;
    elem_params.convert.crop.bottom = FRAME_HEIGHT -1;
    elem_params.convert.crop.right = FRAME_HEIGHT -1 + pad;
#elif (CROP_CONFIG == 2)
    /*** zoom x2 ***/
    /* input crop position */
    elem_params.convert.crop.top = FRAME_WIDTH/4;
    elem_params.convert.crop.left = FRAME_HEIGHT/4;
    elem_params.convert.crop.bottom = FRAME_HEIGHT*3/4 -1;
    elem_params.convert.crop.right = FRAME_WIDTH*3/4 -1;
    /* scaling operation */
    elem_params.convert.ops |= MPP_CONVERT_SCALE;
    if ((elem_params.convert.angle == ROTATE_90) ||
        (elem_params.convert.angle == ROTATE_270)) {
        /* scaling parameters are swapped */
        elem_params.convert.scale.height = FRAME_WIDTH;
        elem_params.convert.scale.width = FRAME_HEIGHT;
    }
    else {
        /* scaling parameters are not swapped */
        elem_params.convert.scale.height = FRAME_HEIGHT;
        elem_params.convert.scale.width = FRAME_WIDTH;
    }
#elif (CROP_CONFIG == 3)
    /*** 100 pix border ***/
    const int pad = 100;
    /* input crop position */
    elem_params.convert.crop.top = pad;
    elem_params.convert.crop.left = pad;
    elem_params.convert.crop.bottom = FRAME_HEIGHT - pad - 1;
    elem_params.convert.crop.right = FRAME_WIDTH - pad - 1;
    /* output window position and no scaling */
    elem_params.convert.ops |= MPP_CONVERT_OUT_WINDOW;
    elem_params.convert.out_window.top = pad;
    elem_params.convert.out_window.left = pad;
#endif

    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    if (ret) {
        PRINTF("Failed to add element CONVERT\r\n");
        goto err;
    }
#endif

    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.width = APP_DISPLAY_WIDTH;
    disp_params.format = args->display_format;
    ret = mpp_display_add(mp, args->display_name, &disp_params);
	if (ret) {
	    PRINTF("Failed to add display %s\r\n", args->display_name);
		goto err;
    }

    ret = mpp_start(mp, 1);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }

#if (CONFIG_STOP_MPP == 1)
    const TickType_t xDelay = MPP_STOP_DELAY_MS / portTICK_PERIOD_MS;
    uint32_t stop_count = 0;
    vTaskDelay(xDelay);
    do {
        PRINTF("STOP count=%d\n", stop_count);
        mpp_stop(mp);
        vTaskDelay(xDelay);
        PRINTF("START\n");
        ret = mpp_start(mp, 0);
        if (ret) {
            PRINTF("Failed to start pipeline\r\n");
            goto err;
        }
        vTaskDelay(xDelay);
        stop_count++;
        if (stop_count > MPP_STOP_COUNT_MAX) break;
    } while (true);
    PRINTF("STOP loop exit\r\n");
#endif

    /* pause application task */
    vTaskSuspend(NULL);

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}

