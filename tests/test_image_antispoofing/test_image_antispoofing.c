/*
 * Copyright 2022-2023, 2025-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> TensorFlow Lite model Antispoofing.
 * The model performs a single face recognition */
/* FreeRTOS kernel includes. */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"
#include "atomic.h"

/* NXP includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"

/* hal includes */
#include "hal_debug.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* Model data input */
#include APP_TFLITE_ANTISPOOFING_DATA

#include "antispoofing_output_postproc_quantized.h"

/* Image data input */
#include APP_STATIC_IMAGE_NAME
#define SRC_IMAGE_FORMAT SRC_IMAGE_ANTISPOOFING_GRAY_FORMAT
#define SRC_IMAGE_CHANNELS_NUMBER SRC_IMAGE_ANTISPOOFING_GRAY_CHANNELS_NUMBER
#define SRC_IMAGE_HEIGHT SRC_IMAGE_ANTISPOOFING_GRAY_HEIGHT
#define SRC_IMAGE_WIDTH SRC_IMAGE_ANTISPOOFING_GRAY_WIDTH
void *image_data = (void *)antispoofing_gray_data;


/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define STATS_PRINT_PERIOD_MS 	1000

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_image_antispoofing_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_antispoofing"
#endif

typedef struct _user_data_t {
	int inference_frame_num;
	mpp_t mp;
	mpp_elem_handle_t elem;
	antispoofing_result liveness;
	uint32_t accessing; /* boolean protecting access to user data */
	uint32_t inference_time_ms;
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
mpp_stats_t antispoofing_stats;

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

	/* Init board hardware. */
	BOARD_Init();

	PRINTF("****** TEST test_image_Antispoofing ******\r\n");
	PRINTF("---INFERENCE ENGINE: TFLITE---\r\n");

	ret = xTaskCreate(
			app_task,
			"app_task",
			configMINIMAL_STACK_SIZE + 1000,
			(void *) NULL,
			tskIDLE_PRIORITY + 1,
			&handle);

	if (pdPASS != ret)
	{
		PRINTF("Failed to create app_task task\r\n");
		while (1);
	}

	vTaskStartScheduler();
	for (;;)
		vTaskSuspend(NULL);
	return 0;
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {
	const mpp_inference_cb_param_t *inf_output;
	antispoofing_result liveness;
	
	// user_data handle contains application private data
	user_data_t *app_priv = (user_data_t *)user_data;

	switch(evt) {
	case MPP_EVENT_INFERENCE_OUTPUT_READY:
		// cast evt_data pointer to correct structure matching the event
		inf_output = (const mpp_inference_cb_param_t *) evt_data;
		ANTISPOOFING_ProcessOutput(
				inf_output,
				&liveness);

		// check that we can modify the user data (not accessed by other task)
		if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
		{
			app_priv->inference_time_ms = inf_output->inference_time_ms;
			app_priv->inference_frame_num++;
			// copy inference output
			app_priv->liveness = liveness;
			__atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
		}
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
	uint32_t last_inf_frame_num = user_data->inference_frame_num;
	PRINTF("\r\nStart %s\r\n", TC_NAME);
	for (;;) {
		xTaskDelayUntil( &xLastWakeTime, xFrequency );
		if (Atomic_CompareAndSwap_u32(&user_data->accessing, 1, 0)) {
			if (last_inf_frame_num != user_data->inference_frame_num) {
				mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
				PRINTF("Element stats --------------------------\r\n");
				PRINTF("Antispoofing : exec_time %u (ms)\r\n", antispoofing_stats.elem.elem_exec_time);
				mpp_stats_enable(MPP_STATS_GRP_ELEMENT);
				PRINTF("inference time %u (ms) \r\n", user_data->inference_time_ms);
				uint32_t liveness_res = 0; // 0 fake face, 1 real face
				uint32_t score = 0;
				if(user_data->liveness.result[1] > SPOOFING_THRESHOLD)
				{
					liveness_res = 1;
					score = user_data->liveness.result[1];
					PRINTF("%s : Real face, confidence score %d\r\n", ANTISPOOFING_NAME, user_data->liveness.result[1]);
				}
				else
				{
					liveness_res = 0;
					score = user_data->liveness.result[0];
					PRINTF("%s : Fake face, confidence score %d\r\n", ANTISPOOFING_NAME, user_data->liveness.result[0]);
				}			
			
				if ((user_data->inference_time_ms <= EXPECTED_INF_TIME) && 
					(liveness_res == EXPECTED_LIVENESS_RES) &&
					(score >= EXPECTED_INF_SCORE))
				{
					PRINTF("%s - PASSED\r\n", TC_NAME);
				}
				else
				{
					if (user_data->inference_time_ms > EXPECTED_INF_TIME)
						PRINTF("Bad inf time %u, expected less than %u\r\n", user_data->inference_time_ms, EXPECTED_INF_TIME);
					if (liveness_res != EXPECTED_LIVENESS_RES)
						PRINTF("Bad liveness result %d, expected %d\r\n", liveness_res, EXPECTED_LIVENESS_RES);
					if (score < EXPECTED_INF_SCORE)
						PRINTF("Bad score %d, expected greater than %d\r\n", score, EXPECTED_INF_SCORE);
					PRINTF("%s - FAILED\r\n", TC_NAME);
				}
				PRINTF("%s finished\r\n", TC_NAME);
				PRINTF("\r\nStart %s\r\n", TC_NAME);
			
				last_inf_frame_num = user_data->inference_frame_num;
			}
			__atomic_store_n(&user_data->accessing, 0, __ATOMIC_SEQ_CST);
		}
	}
	return;
}

static void app_task(void *param)
{
	static user_data_t user_data = {0};
	int ret;

	PRINTF("[%s]\r\n", mpp_get_version());

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
	mpp_static_img_add(mp, &img_params, (void *)image_data, NULL);
	if (ret) {
		PRINTF("Failed to add static image\r\n");
		goto err;
	}

	/* configure inference element with model */
	mpp_element_params_t antispoofing_params;
	memset(&antispoofing_params, 0 , sizeof(mpp_element_params_t));

	antispoofing_params.ml_inference.model_data = antispoofing_data;
	antispoofing_params.ml_inference.model_size = antispoofing_data_len;
	antispoofing_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
	antispoofing_params.ml_inference.model_input_mean = ANTISPOOFING_INPUT_MEAN;
	antispoofing_params.ml_inference.model_input_std = ANTISPOOFING_INPUT_STD;
	antispoofing_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
	antispoofing_params.ml_inference.inference_params.num_inputs = 1;
	antispoofing_params.ml_inference.inference_params.num_outputs = 1;
	antispoofing_params.stats = &antispoofing_stats;

	ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &antispoofing_params, NULL);
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
		PRINTF("Failed to create stat_task task\r\n");
		goto err;
	}

	ret = mpp_start(mp, 1, false);
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
