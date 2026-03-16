/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> jpeg decoder -> image converter -> image display
 * The static image is displayed on screen.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"

/* NXP includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"

#include "hal_debug.h"
#include "hal_os.h"
#include "hal_utils.h"
#include "hal_os.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "test_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define TEST_CHECK_PERIOD_MS 1000

#ifdef TEST_AUTO_DISABLE
#define TEST_MODE ""
#else
#define TEST_MODE "AUTO"
#endif

/* pick CPU for image conversion by default */
#ifndef IMG_CONVERT_DEV_NAME
#define IMG_CONVERT_DEV_NAME "gfx_CPU"
#endif

/* pick CPU for image decoding by default */
#ifndef IMG_DECODE_DEV_NAME
#define IMG_DECODE_DEV_NAME "jpeg_CPU"
#endif

/* set this flag to 1 to perform image scaling to display dimensions */
#ifndef IMG_FULL_SCREEN
#define IMG_FULL_SCREEN 0
#endif

#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

/* in case of Remote-FB display (and not full-screen mode): update only the image region on screen */
#if ( defined(APP_DISPLAY_REMOTE_FB) && (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1))
#if ( SRC_IMAGE_WIDTH > APP_DISPLAY_WIDTH)
    #define DISP_BUF_WIDTH APP_DISPLAY_WIDTH;
#else
    #define DISP_BUF_WIDTH SRC_IMAGE_WIDTH;
#endif
#if ( SRC_IMAGE_HEIGHT > APP_DISPLAY_HEIGHT)
    #define DISP_BUF_HEIGHT APP_DISPLAY_HEIGHT;
#else
    #define DISP_BUF_HEIGHT SRC_IMAGE_HEIGHT;
#endif
#else   /* (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1)) */
    #define DISP_BUF_WIDTH APP_DISPLAY_WIDTH;
    #define DISP_BUF_HEIGHT APP_DISPLAY_HEIGHT;
#endif  /* (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1)) */

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_image_jpeg_convert_display_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_jpeg_convert_display"
#endif

typedef struct _args_t
{
    char display_name[32];
    mpp_pixel_format_t display_format;
    mpp_pixel_format_t source_format;
} args_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
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

/* Set params for image convert element */
static void set_img_convert_params(mpp_element_params_t *elem_params)
{
    memset(elem_params, 0, sizeof(mpp_element_params_t));

    elem_params->convert.dev_name = IMG_CONVERT_DEV_NAME;
    elem_params->convert.out_buf.height = DISP_BUF_HEIGHT;
    elem_params->convert.out_buf.width  = DISP_BUF_WIDTH;
    /* pixel format */
    elem_params->convert.pixel_format = APP_DISPLAY_FORMAT;
    /* scaling parameters */
    elem_params->convert.scale.width  = DISP_BUF_WIDTH;
    elem_params->convert.scale.height = DISP_BUF_HEIGHT;
    /* rotate */
    elem_params->convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    elem_params->convert.flip = FLIP_NONE;

    elem_params->convert.ops = MPP_CONVERT_COLOR;
    elem_params->convert.ops |= MPP_CONVERT_ROTATE;
    elem_params->convert.ops |= MPP_CONVERT_SCALE;

#if (IMG_FULL_SCREEN != 0)
    /* scaling parameters */
    elem_params->convert.scale.width  = APP_DISPLAY_WIDTH;
    elem_params->convert.scale.height = APP_DISPLAY_HEIGHT;
#endif
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data)
{
    checksum_data_t *chksm;
    static bool chksm_ok = false;
    static bool chksm_done = false;
    static int count = 0;   /* frame counter, to ignore first frames */
    static int chksm_time = 0;  /* time of checksum */

    switch(evt) 
    {
    case MPP_EVENT_INTERNAL_TEST_RESERVED:
        chksm = (checksum_data_t *)evt_data;
        if (chksm == NULL)
        {
            return 0;
        }
        if (chksm->type != CHECKSUM_TYPE_CRC_ELCDIF)
        {
            PRINTF("ERROR: checksum calculated should be using CRC LCDIF\n");
            return 0;
        }
        /* if check period elapsed, test again */
        int time = hal_tick_to_ms(hal_get_ostick());
        if (time > chksm_time + TEST_CHECK_PERIOD_MS)
        {
            chksm_done = false;
            chksm_time = time;
        }
        /* verify checksum if needed */
        if (!chksm_done && count > 1)
        {
            PRINTF("\r\nStart %s\r\n", TC_NAME);
            chksm_done = true;
            chksm_ok = ((chksm->value == EXPECTED_CHECKSUM) || (APP_STRIPE_MODE > 0));  /* ignore checksum for stripes */
            if (chksm_ok)
                PRINTF("%s - PASSED\r\n", TC_NAME);
            else
            {
                PRINTF("Bad checksum 0x%08x\r\n", chksm->value);
                PRINTF("%s - FAILED\r\n", TC_NAME);
            }
            PRINTF("%s finished\r\n", TC_NAME);
        }
        count++;
        break;

    case MPP_EVENT_INVALID:
    default:
        /* nothing to do */
        break;
    }

    return 0;
}

/*!
 * @brief Application entry point.
 */
int main(int argc, char *argv[])
{
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

    /* Init board hardware. */
    BOARD_Init();

    PRINTF("****** %s TEST test_image_jpeg_convert_display ******\r\n", TEST_MODE);
    PRINTF("****** PARAMS: IMAGE_NAME = [%s] ******\r\n", IMAGE_NAME);
    PRINTF("****** PARAMS: IMG_FULL_SCREEN = [%d] ******\r\n", IMG_FULL_SCREEN);
    PRINTF("\r\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args)
    {
        PRINTF("Allocation failed\n");
        goto err;
    }

    strcpy(args->display_name, APP_DISPLAY_NAME);
    args->display_format = APP_DISPLAY_FORMAT;
    args->source_format = SRC_IMAGE_FORMAT;

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

static void app_task(void *params)
{
    int src_width = SRC_IMAGE_WIDTH;
    int src_height = SRC_IMAGE_HEIGHT;
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;
    int ret = 0;
    args_t *args = (args_t *) params;
    mpp_elem_handle_t convert_h = (mpp_elem_handle_t) NULL;

    PRINTF("[%s]\r\n", mpp_get_version());

    /* add support for params */
    ret = mpp_api_init(NULL);
    if (ret)
        goto err;

    mpp_t mp;
    mpp_params_t mpp_params;
    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.exec_flag = MPP_EXEC_RC;
    mpp_params.evt_callback_f = &mpp_event_listener;
    mpp_params.mask = MPP_EVENT_ALL;
    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
	goto err;

    /* add image static */
    /* Set static image element parameters */
    mpp_img_params_t img_params ;
    memset(&img_params, 0 , sizeof(img_params));
    img_params.height = src_height;
    img_params.width =  src_width;
    img_params.format = args->source_format;
    img_params.compressed_size = image_data_len;
    ret = mpp_static_img_add(mp, &img_params, (void *)image_data, NULL);
    if (ret)
    {
        PRINTF("Failed to add static image");
        goto err;
    }

    /* Add element jpeg decode */
    mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(mpp_element_params_t));
    elem_params.decode.dev_name = IMG_DECODE_DEV_NAME;
    elem_params.decode.width = src_width;
    elem_params.decode.height = src_height;

    if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_CPU") == 0)
        elem_params.decode.out_format = MPP_PIXEL_BGR; /* TODO auto detect */
    else if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_HW") == 0)
        elem_params.decode.out_format = MPP_PIXEL_YUYV; /* TODO auto detect */

    ret = mpp_element_add(mp, MPP_ELEMENT_IMG_DECODE, &elem_params, NULL);
    if (ret)
    {
        PRINTF("Failed to add element DECODE\n");
        goto err;
    }

    /* Add element convert */
    set_img_convert_params(&elem_params);
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, &convert_h);
    if (ret )
    {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

    /* Set display's parameters */
    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.format = APP_DISPLAY_FORMAT;
    disp_params.width  = DISP_BUF_WIDTH;
    disp_params.height = DISP_BUF_HEIGHT;
    disp_params.stripe = stripe_mode;
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

    /* pause application task */
    vTaskSuspend(NULL);

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}
