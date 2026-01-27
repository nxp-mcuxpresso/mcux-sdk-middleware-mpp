/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> image quality check -> display
 * The static image is displayed on screen after calculating
 * the image quality metrics.
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
#include "app.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "test_config.h"
#include "hal_freertos.h"
#include "hal_utils.h"
#include "hal_os.h"

#define IMAGE_WIDTH  SRC_IMAGE_WIDTH
#define IMAGE_HEIGHT SRC_IMAGE_HEIGHT
#define IMAGE_FORMAT SRC_IMAGE_FORMAT

#define TEST_CHECK_PERIOD_MS 1000

/*******************************************************************************
 * Definitions
 ******************************************************************************/
typedef struct _args_t {
    char display_name[32];
    mpp_pixel_format_t image_format;
    void *image_buffer;
} args_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

/* set this flag to 1 to process image stripe by stripe */
#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

#define STATS_PRINT_PERIOD_MS 1000

/* define this flag to enable MPP stop and start */
#ifndef CONFIG_STOP_MPP
#define CONFIG_STOP_MPP 0
#endif

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_image_quality_check_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_quality_check"
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void app_task(void *params);

/*******************************************************************************
 * Code
 ******************************************************************************/
int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {

    static const img_quality_metrics_t *img_quality_metrics;
    static bool brightness_ok = false;
    static bool contrast_ok = false;
    static bool check_done = false;
    static int count = 0;   /* frame counter, to ignore first frames */
    static int check_time = 0;  /* time of checksum */

    switch(evt) {
    case MPP_EVENT_QUALITY_CHECK_READY:
        /* cast evt_data pointer to correct structure matching the event */
        img_quality_metrics = (const img_quality_metrics_t *) evt_data;
        if (img_quality_metrics == NULL) {
            return 0;
        }

        /* if check period elapsed, test again */
        int time = hal_tick_to_ms(hal_get_ostick());
        if (time > check_time + TEST_CHECK_PERIOD_MS)
        {
            check_done = false;
            check_time = time;
        }
        /* verify checksum if needed */
        if (!check_done && count > 1)
        {
            check_done = true;
            brightness_ok = img_quality_metrics->brightness == EXPECTED_BRIGHTNESS;
            contrast_ok = img_quality_metrics->contrast == EXPECTED_CONTRAST;
            PRINTF("\r\nStart %s\r\n", TC_NAME);
            if (brightness_ok && contrast_ok)
                PRINTF("%s - PASSED\r\n", TC_NAME);
            else
            {
                PRINTF("%s - FAILED\r\n", TC_NAME);
                if (!brightness_ok)
                    PRINTF("Brightness is %d (expected %d).\r\n", img_quality_metrics->brightness, EXPECTED_BRIGHTNESS);
                if (!contrast_ok)
                    PRINTF("Contrast is %d (expected %d).\r\n", img_quality_metrics->contrast, EXPECTED_CONTRAST);
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

/*
 * @brief   Application entry point.
 */
int main(int argc, char *argv[]) {
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

	/* Init board hardware. */
    BOARD_Init();

    PRINTF("****** TEST test_image_quality_check ******\r\n");
    PRINTF("****** PARAMS: CONFIG_STOP_MPP = [%d] ******\r\n", CONFIG_STOP_MPP);
    PRINTF("\n");

	args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

    /* Set input arguments*/
    args->image_buffer = (void *) image_data;
    if (!args->image_buffer ){
    	PRINTF("Failed to get input buffer\n");
    	goto err;
    }
    args->image_format = IMAGE_FORMAT;

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

static void app_task(void *params) {
	args_t *args = (args_t *) params;
	int ret;
	bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;

    PRINTF("[%s]\r\n", mpp_get_version());

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
	if (mp == MPP_INVALID) {
		PRINTF("ERROR: mpp_create failed\r\n");
		goto err;
    }

    /*Set static image element parameters*/
	mpp_img_params_t img_params ;
	memset(&img_params, 0 , sizeof(img_params));
	img_params.height = IMAGE_HEIGHT;
	img_params.width =  IMAGE_WIDTH;
	img_params.format = args->image_format;
	img_params.stripe = stripe_mode;
	ret = mpp_static_img_add(mp, &img_params, args->image_buffer, NULL);
	if (ret) {
		PRINTF("Failed to add static image\n");
		goto err;
	}

    /* Add element for image quality check */
    static mpp_stats_t img_quality_check_stats;
    static mpp_element_params_t elem_params;
    mpp_elem_handle_t img_quality_check_h = (mpp_elem_handle_t) NULL;
    memset(&elem_params, 0 , sizeof(mpp_element_params_t));
    elem_params.stats = &img_quality_check_stats;
    ret = mpp_element_add(mp, MPP_ELEMENT_IMG_QUALITY_CHECK, &elem_params, &img_quality_check_h);
    if (ret ) {
        PRINTF("Failed to add image quality check element\n");
        goto err;
    }

    ret = mpp_nullsink_add(mp);
	if (ret) {
		PRINTF("Failed to add NULL sink\r\n");
		goto err;
	}

	mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

	ret = mpp_start(mp, 1, false);
	if (ret) {
		PRINTF("Failed to start pipeline\n");
		goto err;
	}

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = STATS_PRINT_PERIOD_MS / portTICK_PERIOD_MS;
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        xTaskDelayUntil( &xLastWakeTime, xFrequency );
        mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
        PRINTF("Element stats --------------------------\r\n");
        PRINTF("image quality check : exec_time %u (ms)\r\n", img_quality_check_stats.elem.elem_exec_time);
        mpp_stats_enable(MPP_STATS_GRP_ELEMENT);
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
	ret = mpp_start(mp, 0, false);
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
