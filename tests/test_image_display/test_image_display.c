/*
 * Copyright 2022-2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> display
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
/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board_init.h"
#include "board.h"
#else
#include <stdio.h>
#define PRINTF printf
#define main app_main
#endif

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "test_config.h"
#include "hal_freertos.h"
#include "hal_utils.h"
#include "hal_os.h"

/* test image include file */
#ifdef EMULATOR
#define IMAGE_WIDTH  640
#define IMAGE_HEIGHT  480
#include "test_image_640_480_rgb24.h" /*WIDTH: 640 HEIGHT: 480 IMAGE FORMAT:RGB*/
#else   /* i.MX RT1170 EVK */
#define IMAGE_WIDTH  SRC_IMAGE_WIDTH
#define IMAGE_HEIGHT SRC_IMAGE_HEIGHT
#define IMAGE_FORMAT SRC_IMAGE_FORMAT
#endif /* EMULATOR */

#define TEST_CHECK_PERIOD_MS 1000

/*******************************************************************************
 * Definitions
 ******************************************************************************/
typedef struct _args_t {
    char display_name[32];
    mpp_pixel_format_t display_format;
    mpp_pixel_format_t image_format;
    void *image_buffer;
} args_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

/* define this flag to enable MPP stop and start */
#ifndef CONFIG_STOP_MPP
#define CONFIG_STOP_MPP 0
#endif

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void main_task(void *params);

/*******************************************************************************
 * Code
 ******************************************************************************/

/*
 * @brief   Application entry point.
 */
int main(int argc, char *argv[]) {
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

	/* Init board hardware. */
#ifndef EMULATOR
    BOARD_Init();
#endif /* i.MX RT1170 EVK */

    PRINTF("****** TEST test_image_display ******\r\n");
    PRINTF("****** PARAMS: CONFIG_STOP_MPP = [%d] ******\r\n", CONFIG_STOP_MPP);
    PRINTF("\n");

	args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

    /* Set input arguments*/
#ifndef EMULATOR
    args->image_buffer = (void *) image_data;
#else   /* i.MX RT1170 EVK */
    args->image_buffer = emulator_test_image_bin;
#endif  /* EMULATOR */
    if (!args->image_buffer ){
    	PRINTF("Failed to get input buffer\n");
    	goto err;
    }
#ifdef EMULATOR
    strcpy(args->display_name, "opencv0");
    if (argc == 3) {
        if (strcmp(argv[1], "rgb_sim") &&
            strcmp(argv[1], "ir_sim") &&
            strcmp(argv[1], "yuv_sim")) {
                goto err;
        }

        if (!strncmp(argv[2], "rgb",3)) {
            args->image_format = MPP_PIXEL_RGB;
        } else if (!strncmp(argv[2], "gray",3)) {
            args->image_format = MPP_PIXEL_GRAY;
        } else if (!strncmp(argv[2], "yuv",3)) {
            args->image_format = MPP_PIXEL_YUYV;
        } else {
            PRINTF("Invalid pixel format %s\n", argv[2]);
            goto err;
        }
        args->display_format = args->image_format;
    } else {
        PRINTF("Invalid argument number %d\n", argc);
        goto err;
    }
#else /* i.MX RT1170 EVK */
    strcpy(args->display_name, APP_DISPLAY_NAME);
    args->image_format = IMAGE_FORMAT;
#endif /* EMULATOR */

	ret = xTaskCreate(
			main_task,
			"main_task",
			configMINIMAL_STACK_SIZE + 1000,
			(void *) args,
			APP_DEFAULT_PRIO,
			&handle);

err:
	if (pdPASS != ret)
	{
		PRINTF("Failed to create main_task task");
		while (1);
	}

	vTaskStartScheduler();
	for (;;)
		vTaskSuspend(NULL);

	return 0;
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {

    checksum_data_t *chksm = (checksum_data_t *)evt_data;
    static bool test_done = false;
    static int chksm_time = 0;  /* time of checksum */
    int time;

    switch(evt) {
    case MPP_EVENT_INTERNAL_TEST_RESERVED:
        if (chksm == NULL) {
            return 0;
        }

#if defined(CHECKSUM_TYPE_EXPECTED_PISANO) && (CHECKSUM_TYPE_EXPECTED_PISANO == 1)
        if (chksm->type != CHECKSUM_TYPE_PISANO) {
            PRINTF("ERROR: checksum calculated should be using PISANO for MCXN CPUs\n");
            return 0;
        }
#else
        if (chksm->type != CHECKSUM_TYPE_CRC_ELCDIF) {
            PRINTF("ERROR: checksum calculated should be using CRC LCDIF\n");
            return 0;
        }
#endif

        /* if check period elapsed, test again */
        time = hal_tick_to_ms(hal_get_ostick());
        if (time > chksm_time + TEST_CHECK_PERIOD_MS)
        {
            test_done = false;
            chksm_time = time;
        }
        /* verify checksum if needed */
        if (chksm->value != 0 && !test_done)    /* ignore first black frame */
        {
            test_done = true;
            if ((chksm->value == EXPECTED_CHECKSUM)
                || (APP_STRIPE_MODE > 0))   /* ignore checksum for stripes */
            {
                PRINTF("\r\nTEST PASS");

            } else {
                PRINTF("\r\nBad checksum 0x%08x", chksm->value);
                PRINTF("\r\nTEST FAIL");
            }
        }
        break;
    case MPP_EVENT_INVALID:
    default:
        /* nothing to do */
        break;
    }
    return 0;
}

static void main_task(void *params) {
	args_t *args = (args_t *) params;
	int ret;
	bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;

	ret = mpp_api_init(NULL);
	if (ret)
    	  goto err;

	mpp_t mp;
	mpp_params_t mpp_params;
	memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.evt_callback_f = &mpp_event_listener;
    mpp_params.mask = MPP_EVENT_ALL;
    mpp_params.cb_userdata = NULL;
    mpp_params.exec_flag = MPP_EXEC_RC;
	mp = mpp_create(&mpp_params, &ret);
	if (mp == MPP_INVALID)
		goto err;

    /*Set static image element parameters*/
	mpp_img_params_t img_params ;
	memset(&img_params, 0 , sizeof(img_params));
	img_params.height = IMAGE_HEIGHT;
	img_params.width =  IMAGE_WIDTH;
	img_params.format = args->image_format;
	img_params.stripe = stripe_mode;
	ret = mpp_static_img_add(mp, &img_params, args->image_buffer);
	if (ret) {
		PRINTF("Failed to add static image");
		goto err;
	}

	/* Set display's parameters */
	mpp_display_params_t disp_params;
	memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.width = APP_DISPLAY_WIDTH;
	disp_params.format = args->image_format;
	disp_params.stripe = stripe_mode;

    /* if display buffer is remote, define the output position in the screen */
#if(defined(APP_DISPLAY_REMOTE_FB) && (APP_DISPLAY_REMOTE_FB == 1))
    disp_params.width = SRC_IMAGE_WIDTH;
    disp_params.height = SRC_IMAGE_HEIGHT;
    disp_params.top = OUTPUT_WINDOW_TOP;
    disp_params.left = OUTPUT_WINDOW_LEFT;
#endif

	ret = mpp_display_add(mp, args->display_name, &disp_params);
	if (ret) {
		PRINTF("Failed to add display\n");
		goto err;
	}
	ret = mpp_start(mp, 1);
	if (ret) {
		PRINTF("Failed to start pipeline\n");
		goto err;
	}

#if (CONFIG_STOP_MPP == 1)
	/* run for 3 seconds  */
	vTaskDelay(3000/portTICK_PERIOD_MS);

	/* stop the pipeline */
	ret = mpp_stop(mp);
	if (ret) {
	    PRINTF("Failed to stop pipeline\n");
	    goto err;
	}
	/* wait 6 seconds */
	vTaskDelay(6000/portTICK_PERIOD_MS);

	/* restart the pipeline */
	ret = mpp_start(mp, 0);
	if (ret) {
	    PRINTF("Failed to restart pipeline\n");
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

