/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image (switch between two images) -> image converter -> image display
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
#include "app.h"
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
#define TEST_CHECK_PERIOD_MS 2000

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

/* set this flag to 1 to perform image scaling to display dimensions */
#ifndef IMG_FULL_SCREEN
#define IMG_FULL_SCREEN 0
#endif

/* set this flag to 1 to process image stripe by stripe */
#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

// #if ( defined(APP_DISPLAY_REMOTE_FB) && (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1))
//     #define DISP_BUF_WIDTH SRC_IMAGE_WIDTH;
//     #define DISP_BUF_HEIGHT SRC_IMAGE_HEIGHT;
// #else
//     #define DISP_BUF_WIDTH APP_DISPLAY_WIDTH;
//     #define DISP_BUF_HEIGHT APP_DISPLAY_HEIGHT;
// #endif  /* (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1)) */

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_image_switch_convert_display_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_switch_convert_display"
#endif

typedef struct _args_t {
    char display_name[32];
    mpp_pixel_format_t display_format;
} args_t;

typedef struct _user_data_t {
    mpp_t mp;
    mpp_elem_handle_t static_img;
    mpp_elem_handle_t convert;
    /* current image params */
    mpp_img_params_t crt_image_params;
    /* current image data */
    void* crt_image_data;
    /* test fail status */
    uint32_t test_failed;
} user_data_t;

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
static void set_img_convert_params(mpp_element_params_t *elem_params, uint32_t image_height, uint32_t image_width)
{
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;

    memset(elem_params, 0, sizeof(mpp_element_params_t));

    elem_params->convert.dev_name = IMG_CONVERT_DEV_NAME;
    elem_params->convert.out_buf.height = APP_DISPLAY_HEIGHT;
    elem_params->convert.out_buf.width  = APP_DISPLAY_WIDTH;
    /* pixel format */
    elem_params->convert.pixel_format = APP_DISPLAY_FORMAT;
    /* scaling parameters */
    elem_params->convert.scale.width  = image_width;
    elem_params->convert.scale.height = image_height;
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
    unsigned int crop_width = image_width - crop_pad;
    unsigned int crop_height = image_height - crop_pad;
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
    int ret;

    /* user_data handle contains application private data */
    user_data_t *app_priv = (user_data_t *)user_data;

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
            if (app_priv->crt_image_data == image1_data) {
                chksm_ok = ((chksm->value == SRC1_EXPECTED_CHECKSUM) || (APP_STRIPE_MODE > 0));  /* ignore checksum for stripes */
                PRINTF("\r\nStart %s\r\n", TC_NAME);
                if (chksm_ok) {
                    PRINTF("image 1 checksum passed\r\n");
                    app_priv->test_failed = 0;
                } else {
                    PRINTF("Bad checksum 0x%08x\r\n", chksm->value);
                    PRINTF("image 1 checksum failed\r\n");
                    app_priv->test_failed = 1;
                }

                /* Set params for next image */
                PRINTF("Change image to %s\r\n", SRC2_IMAGE_NAME);
                app_priv->crt_image_params.format = SRC2_IMAGE_FORMAT;
                app_priv->crt_image_params.height = SRC2_IMAGE_HEIGHT;
                app_priv->crt_image_params.width = SRC2_IMAGE_WIDTH;
                app_priv->crt_image_data = image2_data;
            } else {
                chksm_ok = ((chksm->value == SRC2_EXPECTED_CHECKSUM) || (APP_STRIPE_MODE > 0));  /* ignore checksum for stripes */
                if (chksm_ok) {
                    PRINTF("image 2 checksum passed\r\n");
                    app_priv->test_failed |= 0;
                } else {
                    PRINTF("Bad checksum 0x%08x\r\n", chksm->value);
                    PRINTF("image 2 checksum failed\r\n");
                    app_priv->test_failed |= 1;
                }
                if (app_priv->test_failed)
                    PRINTF("%s - FAILED\r\n", TC_NAME);
                else
                    PRINTF("%s - PASSED\r\n", TC_NAME);

                /* Set params for next image */
                PRINTF("Change image to %s\r\n", SRC1_IMAGE_NAME);
                app_priv->crt_image_params.format = SRC1_IMAGE_FORMAT;
                app_priv->crt_image_params.height = SRC1_IMAGE_HEIGHT;
                app_priv->crt_image_params.width = SRC1_IMAGE_WIDTH;
                app_priv->crt_image_data = image1_data;

                PRINTF("%s finished\r\n", TC_NAME);
            }

            mpp_element_params_t img_params;
            memset(&img_params, 0, sizeof(img_params));
            /* Switch image */
            img_params.static_image.img_params.format = app_priv->crt_image_params.format;
            img_params.static_image.img_params.width  = app_priv->crt_image_params.width;
            img_params.static_image.img_params.height = app_priv->crt_image_params.height;
            img_params.static_image.img_buffer = app_priv->crt_image_data;
            ret = mpp_element_update(app_priv->mp, app_priv->static_img, &img_params, true);

            if (MPP_SUCCESS == ret) {
                /* Update convert element */
                mpp_element_params_t convert_params;
                set_img_convert_params(&convert_params, app_priv->crt_image_params.height, app_priv->crt_image_params.width);
                ret = mpp_element_update(app_priv->mp, app_priv->convert, &convert_params, true);
                if (MPP_SUCCESS != ret)
                    PRINTF("ERR: Failed to update element CONVERT\r\n");
            } else {
                PRINTF("ERR: Failed to update element STATIC IMAGE\r\n");
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

    PRINTF("****** %s TEST test_image_switch_convert_display ******\r\n", TEST_MODE);
    PRINTF("****** PARAMS: IMG_CONVERT_CPU = [%d] ******\r\n", IMG_CONVERT_CPU);
    PRINTF("****** PARAMS: IMG_CONVERT_GPU = [%d] ******\r\n", IMG_CONVERT_GPU);
    PRINTF("****** PARAMS: IMAGE1_NAME = [%s] ******\r\n", SRC1_IMAGE_NAME);
    PRINTF("****** PARAMS: IMAGE2_NAME = [%s] ******\r\n", SRC2_IMAGE_NAME);
    PRINTF("****** PARAMS: IMG_ROTATE = [%d] ******\r\n", IMG_ROTATE);
    PRINTF("****** PARAMS: IMG_FLIP = [%d] ******\r\n", IMG_FLIP);
    PRINTF("****** PARAMS: IMG_CROP = [%d] ******\r\n", IMG_CROP);
    PRINTF("****** PARAMS: IMG_FULL_SCREEN = [%d] ******\r\n", IMG_FULL_SCREEN);
    PRINTF("\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

    strcpy(args->display_name, APP_DISPLAY_NAME);
    args->display_format = APP_DISPLAY_FORMAT;

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
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;
    int ret = 0;
    static user_data_t user_data = {0};
    args_t *args = (args_t *) params;

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
    mpp_params.cb_userdata = &user_data;
    mpp_params.mask = MPP_EVENT_ALL;
    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
        goto err;

    user_data.mp = mp;

    /* add image static */
    /* Set static image element parameters */
    memset(&user_data.crt_image_params, 0 , sizeof(user_data.crt_image_params));
    user_data.crt_image_params.height = SRC1_IMAGE_HEIGHT;
    user_data.crt_image_params.width =  SRC1_IMAGE_WIDTH;
    user_data.crt_image_params.format = SRC1_IMAGE_FORMAT;
    user_data.crt_image_params.stripe = stripe_mode;
    user_data.crt_image_data = image1_data;
    ret = mpp_static_img_add(mp, &user_data.crt_image_params, user_data.crt_image_data, &user_data.static_img);
    if (ret) {
        PRINTF("Failed to add static image");
        goto err;
    }

    /* Add element convert */
    mpp_element_params_t elem_params;
    set_img_convert_params(&elem_params, user_data.crt_image_params.height, user_data.crt_image_params.width);
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, &user_data.convert);
    if (ret ) {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

    /* Set display's parameters */
    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.format = APP_DISPLAY_FORMAT;
    disp_params.width  = APP_DISPLAY_WIDTH;
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.stripe = stripe_mode;
    ret = mpp_display_add(mp, args->display_name, &disp_params);
    if (ret) {
	    PRINTF("Failed to add display %s\r\n", args->display_name);
		goto err;
    }

    ret = mpp_start(mp, 1, false);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }

#if (ENABLE_FB_CHEKSUM == 0)
    /* If checksum not enabled, switch image periodically */
    TickType_t xLastWakeTime;
    const uint32_t image_change_freq = TEST_CHECK_PERIOD_MS / portTICK_PERIOD_MS;
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        xTaskDelayUntil( &xLastWakeTime, image_change_freq );
        /* Set params for next image */
        if (user_data.crt_image_data == image1_data) {
            PRINTF("Change image to %s\r\n", SRC2_IMAGE_NAME);
            user_data.crt_image_params.format = SRC2_IMAGE_FORMAT;
            user_data.crt_image_params.height = SRC2_IMAGE_HEIGHT;
            user_data.crt_image_params.width = SRC2_IMAGE_WIDTH;
            user_data.crt_image_data = image2_data;
        } else {
            PRINTF("Change image to %s\r\n", SRC1_IMAGE_NAME);
            user_data.crt_image_params.format = SRC1_IMAGE_FORMAT;
            user_data.crt_image_params.height = SRC1_IMAGE_HEIGHT;
            user_data.crt_image_params.width = SRC1_IMAGE_WIDTH;
            user_data.crt_image_data = image1_data;
        }

        mpp_element_params_t img_params;
        memset(&img_params, 0, sizeof(img_params));
        /* Switch image */
        img_params.static_image.img_params.format = user_data.crt_image_params.format;
        img_params.static_image.img_params.width  = user_data.crt_image_params.width;
        img_params.static_image.img_params.height = user_data.crt_image_params.height;
        img_params.static_image.img_buffer = user_data.crt_image_data;
        ret = mpp_element_update(user_data.mp, user_data.static_img, &img_params, true);

        if (MPP_SUCCESS == ret) {
            /* Update convert element */
            mpp_element_params_t convert_params;
            set_img_convert_params(&convert_params, user_data.crt_image_params.height, user_data.crt_image_params.width);
            ret = mpp_element_update(user_data.mp, user_data.convert, &convert_params, true);
            if (MPP_SUCCESS != ret)
                PRINTF("ERR: Failed to update element CONVERT\r\n");
        } else {
            PRINTF("ERR: Failed to update element STATIC IMAGE\r\n");
        }
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
