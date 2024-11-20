/*
 * Copyright 2023-2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> image converter -> null sink
 * The checksum of the converted image is calculated and provided to the application
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
#include "board_init.h"

#include "hal_debug.h"
#include "hal_freertos.h"
#include "hal_utils.h"
#include "hal_os.h"

/* MPP includes */
#include "mpp_api.h"
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

/* set this flag to 0 to perform image conversion using PXP (otherwise CPU is used) */
#ifndef IMG_CONVERT_CPU
#define IMG_CONVERT_CPU 1
#endif

#if (IMG_CONVERT_CPU == 0)
/* pick default device from the first listed and supported by Hw */
#define IMG_CONVERT_DEV_NAME NULL
#else
#define IMG_CONVERT_DEV_NAME "gfx_CPU"
#endif

/* set this flag to perform image color conversion (0: RGB565, 1: RGB888, 2: BGR888) */
#ifndef IMG_COLOR_CONVERT
#define IMG_COLOR_CONVERT 0
#endif

/* set this flag to perform image scaling (1: scaling is disabled, other: scaling factor value) */
#ifndef IMG_SCALE
#define IMG_SCALE 1
#endif

typedef struct _args_t {
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
    elem_params->convert.out_buf.height = SRC_IMAGE_HEIGHT * IMG_SCALE;
    elem_params->convert.out_buf.width  = SRC_IMAGE_WIDTH * IMG_SCALE;
    /* pixel format */
#if (IMG_COLOR_CONVERT == IMG_COLOR_RGB565)
    elem_params->convert.pixel_format = MPP_PIXEL_RGB565;
#elif (IMG_COLOR_CONVERT == IMG_COLOR_RGB888)
    elem_params->convert.pixel_format = MPP_PIXEL_RGB;
#elif (IMG_COLOR_CONVERT == IMG_COLOR_BGR888)
    elem_params->convert.pixel_format = MPP_PIXEL_BGR;
#endif

#if (IMG_SCALE != 1)
    elem_params->convert.ops = MPP_CONVERT_SCALE;
    /* scaling parameters */
    elem_params->convert.scale.width = SRC_IMAGE_WIDTH * IMG_SCALE;
    elem_params->convert.scale.height = SRC_IMAGE_HEIGHT * IMG_SCALE;
#else
    /* image convert single operation */
    elem_params->convert.ops = MPP_CONVERT_COLOR;
#endif
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
        if (chksm->type != CHECKSUM_TYPE_PISANO) {
            PRINTF("ERROR: checksum calculated should be using PISANO\n");
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
            chksm_ok = (chksm->value == EXPECTED_CHECKSUM);
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

    /* Init board hardware. */
    BOARD_Init();

    PRINTF("****** %s TEST test_image_convert ******\n", TEST_MODE);
    PRINTF("****** PARAMS: IMG_CONVERT_CPU = [%d] ******\n", IMG_CONVERT_CPU);
    PRINTF("****** PARAMS: IMAGE_NAME = [%s] ******\n", IMAGE_NAME);
    PRINTF("****** PARAMS: IMG_COLOR_CONVERT = [%d] ******\n", IMG_COLOR_CONVERT);
    PRINTF("****** PARAMS: IMG_SCALE = [%d] ******\n", IMG_SCALE);
    PRINTF("\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

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

    int ret;
    args_t *args = (args_t *) params;

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
    ret = mpp_static_img_add(mp, &img_params, (void *)image_data);
    if (ret) {
        PRINTF("Failed to add static image");
        goto err;
    }

    /* Add element convert */
    mpp_element_params_t elem_params;
    set_img_convert_params(&elem_params);
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);
    if (ret ) {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

    /* close the pipeline with a null sink */
    ret = mpp_nullsink_add(mp);
    if (ret) {
        PRINTF("Failed to add NULL sink\r\n");
        goto err;
    }

    ret = mpp_start(mp, 1);
    if (ret) {
        PRINTF("Failed to start pipeline");
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
