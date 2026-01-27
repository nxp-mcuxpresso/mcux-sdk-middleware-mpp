/*
 * Copyright 2025-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * 2D camera -> image converter -> lvgl
 * The camera view finder is displayed on screen with an lvgl stop button.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"

/* Driver includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"

/* LVGL includes */
#include "lvgl_support.h"
#include "lvgl.h"

/* Board includes. */
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* static image data */
#ifdef STATIC_IMAGE
#include "images/dogs_COCO_320_320_bgra.h"
#define SRC_IMAGE_FORMAT SRC_IMAGE_DOGS_COCO_320_320_BGRA_FORMAT
#define SRC_IMAGE_CHANNELS_NUMBER SRC_IMAGE_DOGS_COCO_320_320_BGRA_CHANNELS_NUMBER
#define SRC_IMAGE_HEIGHT SRC_IMAGE_DOGS_COCO_320_320_BGRA_HEIGHT
#define SRC_IMAGE_WIDTH SRC_IMAGE_DOGS_COCO_320_320_BGRA_WIDTH
void *image_data = (void *)dogs_COCO_320_320_bgra_data;
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#ifndef CAMERA_FORMAT1
#define CAMERA_FORMAT1 0
#endif
#ifndef CONFIG_RC_CYCLE_FRAMES
#define CONFIG_RC_CYCLE_FRAMES 1
#endif

#define FRAME_WIDTH    APP_CAMERA_WIDTH
#define FRAME_HEIGHT   APP_CAMERA_HEIGHT

/* pick default backend if not specified. */
#ifndef APP_GFX_BACKEND_NAME
#define APP_GFX_BACKEND_NAME NULL
#endif

typedef struct _args_t 
{
    char camera_name[32];
    char display_name[32];
    mpp_pixel_format_t camera_format;
    mpp_pixel_format_t display_format;
} args_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

/** Default priority for application tasks
   LVGL Task must have higher prio than MPP.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_LVGL_PRIO       5
#define APP_MPP_MIN_PRIO    1

static volatile bool s_lvgl_initialized = false;
SemaphoreHandle_t g_button_press = NULL;
extern lv_obj_t * g_img;    /* the lvgl image object shared with mpp */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void mpp_task(void *params);
static void lvgl_task(void *params);
void lvgl_camera_scene(void);

/* callback defined in MPP HAL for lvgl */
void lv_event_refr_ready(lv_event_t * e);

void print_cb(lv_log_level_t level, const char * buf);

#if LV_USE_LOG
void print_cb(lv_log_level_t level, const char * buf)
{
    LV_UNUSED(level);

    PRINTF("\r%s\n", buf);
}
#endif

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

    /* Init board hardware. */
    BOARD_Init();

    PRINTF("****** TEST test_camera_lvgl ******\r\n");
    PRINTF("****** PARAMS: CAMERA_FORMAT1 = [%d] ******\r\n", CAMERA_FORMAT1);
    PRINTF("****** PARAMS: CONFIG_RC_CYCLE_FRAMES = [%d] ******\r\n", CONFIG_RC_CYCLE_FRAMES);
    PRINTF("\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) 
    {
        PRINTF("Allocation failed\r\n");
        goto err;
    }

    strcpy(args->display_name, APP_DISPLAY_NAME);
    strcpy(args->camera_name, APP_CAMERA_NAME);
#if (CAMERA_FORMAT1 == 1)
    args->camera_format = APP_CAMERA_FORMAT1;
#else
    args->camera_format = APP_CAMERA_FORMAT;
#endif
    args->display_format = APP_DISPLAY_FORMAT;

    /* create synchronisation semaphores */
    g_button_press = xSemaphoreCreateBinary();

    ret = xTaskCreate(
          lvgl_task,
          "lvgl_task",
          configMINIMAL_STACK_SIZE + 1000,
          (void *) args,
          APP_LVGL_PRIO,
          &handle);

    if (pdPASS != ret)
    {
        PRINTF("Failed to create lvgl_task task");
        while (1);
    }

    ret = xTaskCreate(
          mpp_task,
          "mpp_task",
          configMINIMAL_STACK_SIZE + 1000,
          (void *) args,
          APP_MPP_MIN_PRIO,
          &handle);

    if (pdPASS != ret)
    {
        PRINTF("Failed to create mpp_task task");
        while (1);
    }

err:
    vTaskStartScheduler();
    for (;;)
        vTaskSuspend(NULL);
    return 0;
}

/*!
 * @brief FreeRTOS tick hook.
 */
void vApplicationTickHook(void)
{
    if (s_lvgl_initialized)
    {
        lv_tick_inc(1);
    }
}

static void lvgl_task(void *param)
{
    PRINTF("lvgl camera scene started\r\n");

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

#if LV_USE_LOG
    lv_log_register_print_cb(print_cb);
#endif

    LV_LOG("lvgl camera view demo started\r\n");

    lvgl_camera_scene();
    
    lv_display_add_event_cb(lv_display_get_default(), (lv_event_cb_t) lv_event_refr_ready, LV_EVENT_REFR_READY, NULL);

    s_lvgl_initialized = true;

    for (;;)
    {
        vTaskDelay(lv_timer_handler());
    }
}

static void mpp_task(void *params) 
{

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

#ifdef STATIC_IMAGE
    mpp_img_params_t img_params;
    memset(&img_params, 0, sizeof (mpp_img_params_t));
    img_params.format = SRC_IMAGE_FORMAT;
    img_params.width = SRC_IMAGE_WIDTH;
    img_params.height = SRC_IMAGE_HEIGHT;
    mpp_static_img_add(mp, &img_params, (void *)image_data, NULL);
#else
	mpp_camera_params_t cam_params;
	memset(&cam_params, 0 , sizeof(cam_params));
	cam_params.height = height;
	cam_params.width =  width;
	cam_params.format = args->camera_format;
	cam_params.fps    = 30;
	ret = mpp_camera_add(mp, args->camera_name, &cam_params, NULL);
    if (ret) 
    {
        PRINTF("Failed to add camera %s\r\n", args->camera_name);
        goto err;
    }
#endif

    mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(elem_params));
    /* select graphics device */
    elem_params.convert.dev_name = APP_GFX_BACKEND_NAME;
    elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    elem_params.convert.pixel_format = args->display_format;
    elem_params.convert.out_buf.width = APP_DISPLAY_WIDTH;
    elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_ROTATE;
    elem_params.convert.flip = FLIP_HORIZONTAL;
#ifdef STATIC_IMAGE
    // resize: scaling parameters
    elem_params.convert.scale.width = APP_DISPLAY_WIDTH;
    elem_params.convert.scale.height = APP_DISPLAY_HEIGHT;
    elem_params.convert.ops |= MPP_CONVERT_SCALE;
#endif // STATIC_IMAGE
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    if (ret) 
    {
        PRINTF("Failed to add element CONVERT\r\n");
        goto err;
    }

    /* wait for lvgl to be initialized to get handle on image widget */
    PRINTF("Waiting for lvgl task to initialize \r\n");
    while(!s_lvgl_initialized);
    
    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.width = APP_DISPLAY_WIDTH;
    disp_params.handle = g_img;
    disp_params.format = args->display_format;
    ret = mpp_display_add(mp, args->display_name, &disp_params);
	if (ret) 
	{
	    PRINTF("Failed to add display %s\r\n", args->display_name);
		goto err;
    }

    ret = mpp_start(mp, 1, false);
    if (ret) 
    {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }

    PRINTF("Started pipeline.\r\n");
    bool started = true;
    do 
    {
        BaseType_t ret = xSemaphoreTake(g_button_press, portMAX_DELAY);
        if (ret == pdFALSE)
        {
            PRINTF("Synchronisation issue with lvgl task\r\n");
            goto err;
        }

        if (started)
        {
            PRINTF("STOP\r\n");
            ret = mpp_stop(mp);
            started = false;
        }
        else
        {
            PRINTF("START\r\n");
            ret = mpp_start(mp, 0, false);
            started = true;
        }
        if (ret) 
        {
            PRINTF("Failed to start/stop pipeline\r\n");
            goto err;
        }
    } while (true);

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}

