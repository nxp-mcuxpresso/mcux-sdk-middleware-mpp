/*
 * Copyright 2022-2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> image converter -> image display
 * The static image is displayed on screen.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"

#ifndef EMULATOR
/* NXP includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "board_init.h"
#else
#define main app_main
#endif

#include "hal_debug.h"
#include "hal_freertos.h"
#include "hal_utils.h"
#include "hal_os.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "test_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define CROP_RATIO_MAX  4
#define CROP_RATIO_INIT 3
#define CROP_RATIO_MIN  2

#define SPEEDX 2
#define SPEEDY 1
#define ZOOMINC 1

#define TEST_CHECK_PERIOD_MS 1000

#ifdef TEST_AUTO_DISABLE
#define TEST_MODE ""
#else
#define TEST_MODE "AUTO"
#endif

/* set IMG_CONVERT_CPU or IMG_CONVERT_GPU flag to 1 to perform image conversion using CPU or GPU
 * (otherwise first gfx device in the list is used by default) */
#if !defined(IMG_CONVERT_CPU) && !defined(IMG_CONVERT_GPU)
#define IMG_CONVERT_CPU 0
#define IMG_CONVERT_GPU 0
#elif defined(IMG_CONVERT_CPU)
#define IMG_CONVERT_GPU 0
#elif defined(IMG_CONVERT_GPU)
#define IMG_CONVERT_CPU 0
#endif

#if (IMG_CONVERT_CPU == 1)
#define IMG_CONVERT_DEV_NAME "gfx_CPU"
#elif (IMG_CONVERT_GPU == 1)
#define IMG_CONVERT_DEV_NAME "gfx_GPU"
#else
/* pick default device from the first listed and supported by Hw */
#define IMG_CONVERT_DEV_NAME NULL
#endif

/* set this flag to perform an image rotation (1: 90°, 2: 180°, 3: 270°) */
#ifndef IMG_ROTATE
#define IMG_ROTATE 0
#endif

/* set this flag to perform an image flip operation (1: horizontal, 2: vertical, 3: both) */
#ifndef IMG_FLIP
#define IMG_FLIP 0
#endif

/* set this flag to perform image crop and output window */
#ifndef IMG_CROP
#define IMG_CROP 0
#endif

/* set this flag to perform dynamic image crop window */
#ifndef IMG_DYN_CROP
#define IMG_DYN_CROP 0
#endif

/* set this flag to 1 to perform image scaling to display dimensions */
#ifndef IMG_FULL_SCREEN
#define IMG_FULL_SCREEN 0
#endif

/* set this flag to 1 to process image stripe by stripe */
#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

#if ( defined(APP_DISPLAY_REMOTE_FB) && (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1))
    #define DISP_BUF_WIDTH SRC_IMAGE_WIDTH;
    #define DISP_BUF_HEIGHT SRC_IMAGE_HEIGHT;
#else
    #define DISP_BUF_WIDTH APP_DISPLAY_WIDTH;
    #define DISP_BUF_HEIGHT APP_DISPLAY_HEIGHT;
#endif  /* (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1)) */


typedef struct _args_t {
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
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;

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
#if (IMG_ROTATE == 1)
    elem_params->convert.angle = ROTATE_90;
#elif (IMG_ROTATE == 2)
    elem_params->convert.angle = ROTATE_180;
#elif (IMG_ROTATE == 3)
    elem_params->convert.angle = ROTATE_270;
#endif
    /* flip */
#if (IMG_FLIP == 1)
    elem_params->convert.flip = FLIP_HORIZONTAL;
#elif (IMG_FLIP == 2)
    elem_params->convert.flip = FLIP_VERTICAL;
#elif (IMG_FLIP == 3)
    elem_params->convert.flip = FLIP_BOTH;
#else
    elem_params->convert.flip = FLIP_NONE;
#endif


    elem_params->convert.ops = MPP_CONVERT_COLOR;
#if (IMG_ROTATE != 0) || (IMG_FLIP != 0)
    elem_params->convert.ops |= MPP_CONVERT_ROTATE;
#endif
#if (IMG_FULL_SCREEN != 0)
    /* scaling parameters */
    elem_params->convert.scale.width  = APP_DISPLAY_WIDTH;
    elem_params->convert.scale.height = APP_DISPLAY_HEIGHT;
    elem_params->convert.ops |= MPP_CONVERT_SCALE;
#endif

    /* crop and output window */
#if (IMG_CROP == 1)
    const int crop_pad = 30; /* pad to crop part of the image */
    int out_window_pad = 30; /* pad to output buffer */
    unsigned int crop_width = SRC_IMAGE_WIDTH - crop_pad;
    unsigned int crop_height = SRC_IMAGE_HEIGHT - crop_pad;
    /* input crop position */
    elem_params->convert.crop.top = crop_pad;
    elem_params->convert.crop.left = crop_pad;
    elem_params->convert.crop.bottom = crop_height + crop_pad - 1;
    elem_params->convert.crop.right = crop_width + crop_pad - 1;
    elem_params->convert.ops |= MPP_CONVERT_CROP;
    /* output window position */
    elem_params->convert.out_window.top = out_window_pad;
    elem_params->convert.out_window.left = out_window_pad;
    elem_params->convert.ops |= MPP_CONVERT_OUT_WINDOW;
#endif
    elem_params->convert.stripe_in = stripe_mode;
    elem_params->convert.stripe_out = stripe_mode;
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {
    checksum_data_t *chksm;
    static bool chksm_ok = false;
    static bool chksm_done = false;
    static int count = 0;   /* frame counter, to ignore first frames */
    static int chksm_time = 0;  /* time of checksum */

    switch(evt) {
    case MPP_EVENT_INTERNAL_TEST_RESERVED:
        chksm = (checksum_data_t *)evt_data;
        if (chksm == NULL) {
            return 0;
        }
        if (chksm->type != CHECKSUM_TYPE_CRC_ELCDIF) {
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
            chksm_done = true;
            chksm_ok = ((chksm->value == EXPECTED_CHECKSUM) || (APP_STRIPE_MODE > 0));  /* ignore checksum for stripes */
            if (chksm_ok)
                PRINTF("\r\nTEST PASS\r\n");
            else
            {
                PRINTF("\r\nBad checksum 0x%08x\r\n", chksm->value);
                PRINTF("\r\nTEST FAIL\r\n");
            }
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

#ifndef EMULATOR
    /* Init board hardware. */
    BOARD_Init();
#endif

    PRINTF("****** %s TEST test_image_convert_display ******\n", TEST_MODE);
    PRINTF("****** PARAMS: IMG_CONVERT_CPU = [%d] ******\n", IMG_CONVERT_CPU);
    PRINTF("****** PARAMS: IMG_CONVERT_GPU = [%d] ******\n", IMG_CONVERT_GPU);
    PRINTF("****** PARAMS: IMAGE_NAME = [%s] ******\n", IMAGE_NAME);
    PRINTF("****** PARAMS: IMG_ROTATE = [%d] ******\n", IMG_ROTATE);
    PRINTF("****** PARAMS: IMG_FLIP = [%d] ******\n", IMG_FLIP);
    PRINTF("****** PARAMS: IMG_CROP = [%d] ******\n", IMG_CROP);
    PRINTF("****** PARAMS: IMG_FULL_SCREEN = [%d] ******\n", IMG_FULL_SCREEN);
    PRINTF("\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
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

static int dyn_crop_loop(mpp_t mp, mpp_elem_handle_t convert_h, mpp_element_params_t * pelem_params)
{
    int ret = 0;
    int crop_height = SRC_IMAGE_HEIGHT / CROP_RATIO_INIT;
    int crop_width = SRC_IMAGE_WIDTH / CROP_RATIO_INIT;
    int crop_x = SRC_IMAGE_WIDTH / CROP_RATIO_INIT, crop_y = 0;
    int speedx = SPEEDX, speedy = SPEEDY, zoominc = ZOOMINC;
    for (;;)
    {
        vTaskDelay(20);
        /* compute new position */
        crop_x += speedx;
        crop_y += speedy;
        crop_height += zoominc;
        crop_width += zoominc;
        /* inverse speed when reaching border */
        if (crop_height + crop_y >= SRC_IMAGE_HEIGHT)
        {
            speedy = 0 - SPEEDY;
            crop_y += speedy;
        }
        else if (crop_y <= 0)
        {
            speedy = SPEEDY;
            crop_y += speedy;
        }
        if (crop_width + crop_x >= SRC_IMAGE_WIDTH)
        {
            speedx = 0 - SPEEDX;
            crop_x += speedx;
        }
        else if (crop_x <= 0)
        {
            speedx = SPEEDX;
            crop_x += speedx;
        }
        /* inverse zoom when reaching limits */
        if ((crop_width > SRC_IMAGE_WIDTH / CROP_RATIO_MIN) || (crop_width < SRC_IMAGE_WIDTH / CROP_RATIO_MAX))
        {
            zoominc = 0 - zoominc;
        }
        pelem_params->convert.crop.top = crop_y;
        pelem_params->convert.crop.left = crop_x;
        pelem_params->convert.crop.bottom = crop_height + crop_y - 1;
        pelem_params->convert.crop.right = crop_width + crop_x - 1;
        pelem_params->convert.ops |= MPP_CONVERT_CROP;
        ret = mpp_element_update(mp, convert_h, pelem_params);
        if (ret) {
            PRINTF("Failed mpp_element_update\r\n");
            break;
        }
    }
    return ret;
}

static void app_task(void *params)
{
    int src_width = SRC_IMAGE_WIDTH;
    int src_height = SRC_IMAGE_HEIGHT;
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;
    int ret = 0;
    args_t *args = (args_t *) params;
    mpp_elem_handle_t convert_h = (mpp_elem_handle_t) NULL;

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
    img_params.stripe = stripe_mode;
    ret = mpp_static_img_add(mp, &img_params, (void *)image_data);
    if (ret) {
        PRINTF("Failed to add static image");
        goto err;
    }

    /* Add element convert */
    mpp_element_params_t elem_params;
    set_img_convert_params(&elem_params);
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, &convert_h);
    if (ret ) {
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
    if (ret) {
	    PRINTF("Failed to add display %s\r\n", args->display_name);
		goto err;
    }

    ret = mpp_start(mp, 1);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }

#if (IMG_DYN_CROP == 1)
    ret = dyn_crop_loop(mp, convert_h, &elem_params);
    if (ret) {
        mpp_stop(mp);
        goto err;
    }
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
