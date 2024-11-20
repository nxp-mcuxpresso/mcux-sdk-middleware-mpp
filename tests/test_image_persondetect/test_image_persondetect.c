/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> TensorFlow Lite model persondetect.
 * The model performs person detection using TF-Lite micro inference engine
 * The goal of this test application is to check if persons detection is possible on MCX-N.
 */
/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"
#include "stdio.h"
#include "atomic.h"

/* NXP includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "board_init.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* utility functions */
#include "models/utils.h"
#include "models/persondetect/persondetect_output_postprocess.h"
#include "hal_utils.h"

/* Tensorflow lite model data input */
#include APP_TFLITE_PERSONDETECT_DATA
#include APP_TFLITE_PERSONDETECT_INFO

/* test image include file */
#include APP_STATIC_IMAGE_NAME

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define MAX_LABEL_RECTS     10
#ifndef USE_NAS_OPTIMIZED_MODEL
#define NUM_BOXES_MAX       80
#else
#define NUM_BOXES_MAX       196
#endif

#define STATS_PRINT_PERIOD_MS 1000

typedef struct _user_data_t {
	int inference_frame_num;
	mpp_t mp;
	mpp_elem_handle_t elem;
	mpp_labeled_rect_t labels[MAX_LABEL_RECTS];
	/* detected final_boxes */
	box_data  final_boxes[NUM_BOXES_MAX];
	/* detected final_boxes count */
	int detected_count;
	int inference_time_ms;
	uint32_t accessing; /* boolean protecting access */
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
mpp_stats_t persondetect_stats;

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
int main()
{
	BaseType_t ret;
	TaskHandle_t handle = NULL;

	/* Init board hardware. */
	BOARD_Init();

	PRINTF("****** TEST test_image_persondetect ******\r\n");
	PRINTF("---INFERENCE ENGINE: TFLITE---\r\n");

	ret = xTaskCreate(
			app_task,
			"app_task",
			configMINIMAL_STACK_SIZE + 1000,
			NULL,
			APP_DEFAULT_PRIO,
			&handle);

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

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data)
{
	status_t ret;
	const mpp_inference_cb_param_t *inf_output;

	/* user_data handle contains application private data */
	user_data_t *app_priv = (user_data_t *)user_data;

	switch(evt) {
	case MPP_EVENT_INFERENCE_OUTPUT_READY:
		/* cast evt_data pointer to correct structure matching the event */
		inf_output = (const mpp_inference_cb_param_t *) evt_data;

		/* check that we can modify the user data (not accessed by other task) */
		if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS) {
			ret = Persondetect_Output_postprocessing(
					inf_output,
					app_priv->final_boxes,
					NUM_BOXES_MAX);

			if (ret != kStatus_Success)
				PRINTF("mpp_event_listener: process output error!");

			app_priv->detected_count = 0;
			app_priv->inference_time_ms = inf_output->inference_time_ms;

			/* count valid results */
			for (uint32_t i = 0; i < NUM_BOXES_MAX; i++)
			{
				if (app_priv->final_boxes[i].score > 0)
					app_priv->detected_count++;
			}
			/* end of modification of user data */
			__atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
		}

		/* update labeled rectangle */
		if ( (app_priv->mp != NULL) && (app_priv->elem != 0) ) {
			mpp_element_params_t params;
			/* detected_count contains at least the detection zone box */
			params.labels.detected_count = app_priv->detected_count + 1;
			params.labels.max_count = MAX_LABEL_RECTS;
			params.labels.rectangles = app_priv->labels;
			mpp_element_update(app_priv->mp, app_priv->elem, &params);
		}

		app_priv->inference_frame_num++;
		break;
	case MPP_EVENT_INVALID:
	default:
		/* nothing to do */
		break;
	}

	return 0;
}
void stat_task(void *param)
{
	user_data_t * user_data = (user_data_t  *) param;

	TickType_t xLastWakeTime;
	const TickType_t xFrequency = STATS_PRINT_PERIOD_MS / portTICK_PERIOD_MS;
	xLastWakeTime = xTaskGetTickCount();
	for (;;) {
		xTaskDelayUntil( &xLastWakeTime, xFrequency );
		mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
		PRINTF("\nElement stats --------------------------\r\n");
		PRINTF("Persondetect : exec_time %u ms\r\n", persondetect_stats.elem.elem_exec_time);
		mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

		if (Atomic_CompareAndSwap_u32(&user_data->accessing, 1, 0))
		{
			if (user_data->detected_count == 0)
			{
				PRINTF("No person detected\n\r");
			}
			else {
				PRINTF("Number of detections : %d\n\r", user_data->detected_count);

				for (int i = 0; i < NUM_BOXES_MAX; i++)
				{
					if (user_data->final_boxes[i].area > 0 && user_data->final_boxes[i].score > 0.0)
					{
						if (user_data->final_boxes[i].area > 0) {
							printf("box %d --> score %u %%\r\n Left=%d, Top=%d, Right=%d, Bottom=%d\r\n"
									"-------------------------------------------\r\n",
									i,
									(uint8_t)((user_data->final_boxes[i].score)*100),
									user_data->final_boxes[i].left,
									user_data->final_boxes[i].top,
									user_data->final_boxes[i].right,
									user_data->final_boxes[i].bottom);
						}
					}
				}
			}
			__atomic_store_n(&user_data->accessing, 0, __ATOMIC_SEQ_CST);
		}
	}
	return;
}

static void app_task(void *params)
{
	static user_data_t user_data = {0};
	int ret;

	ret = mpp_api_init(NULL);
	if (ret)
		goto err;

	mpp_t mp;
	mpp_params_t mpp_params;
	memset(&mpp_params, 0, sizeof(mpp_params));
	mpp_params.evt_callback_f = &mpp_event_listener;
	mpp_params.mask = MPP_EVENT_ALL;
	mpp_params.cb_userdata = &user_data;
	mpp_params.exec_flag = MPP_EXEC_RC;
	mp = mpp_create(&mpp_params, &ret);
	if (mp == MPP_INVALID)
		goto err;

	mpp_img_params_t img_params;
	memset(&img_params, 0, sizeof (mpp_img_params_t));
	img_params.format = SRC_IMAGE_FORMAT;
	img_params.width = SRC_IMAGE_WIDTH;
	img_params.height = SRC_IMAGE_HEIGHT;
	mpp_static_img_add(mp, &img_params, (void *)image_data);
	if (ret) {
		PRINTF("Failed to add static image\r\n");
		goto err;
	}

	/* configure inference element with model */
	mpp_element_params_t persondetect_params;
	memset(&persondetect_params, 0 , sizeof(mpp_element_params_t));

	persondetect_params.ml_inference.model_data = persondetect_data;
	persondetect_params.ml_inference.model_size = persondetect_data_len;
	persondetect_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
	persondetect_params.ml_inference.model_input_mean = PERSONDETECT_INPUT_MEAN;
	persondetect_params.ml_inference.model_input_std = PERSONDETECT_INPUT_STD;
	persondetect_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
	persondetect_params.ml_inference.inference_params.num_inputs = 1;
	persondetect_params.ml_inference.inference_params.num_outputs = 1;
	persondetect_params.stats = &persondetect_stats;

	ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &persondetect_params, NULL);
	if (ret) {
		PRINTF("Failed to add element VALGO_TFLite\r\n");
		goto err;
	}

	ret = mpp_nullsink_add(mp);
	if (ret) {
		PRINTF("Failed to add NULL sink\r\n");
		goto err;
	}

	mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

	TaskHandle_t handle = NULL;
	ret = xTaskCreate(
			stat_task,
			"stat_task",
			configMINIMAL_STACK_SIZE + 1000,
			(void *) &user_data,
			tskIDLE_PRIORITY + 5,
			&handle);

	if (pdPASS != ret)
	{
		PRINTF("Failed to create app_task task\r\n");
		goto err;
	}

	ret = mpp_start(mp, 1);
	if (ret) {
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
