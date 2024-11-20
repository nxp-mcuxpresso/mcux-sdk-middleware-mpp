/*
 * Copyright  2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> TensorFlow Lite model Mobilefacenet.
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
#include "board_init.h"

/* hal includes */
#include "hal_debug.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* Model data input */
#include APP_TFLITE_MOBILEFACENET_DATA

/* Persons database */
#include APP_DATABASE_NAME

/* Model output post-processing */
#include "models/mobilefacenet/mobilefacenet_output_postproc_quantized.h"

/* Input image */
#include APP_STATIC_IMAGE_NAME


/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define STATS_PRINT_PERIOD_MS 	1000

typedef struct _user_data_t {
	int inference_frame_num;
	mpp_t mp;
	mpp_elem_handle_t elem;
	mpp_labeled_rect_t labels[1];
	recognition_result result;
	uint32_t accessing; /* boolean protecting access to user data */
	int inference_time_ms;
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
mpp_stats_t mobilefacenet_stats;

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

	/* Init board hardware. */
	BOARD_Init();

	PRINTF("****** TEST test_image_Mobilefacenet ******\r\n");
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
	recognition_result result;

	/* user_data handle contains application private data */
	user_data_t *app_priv = (user_data_t *)user_data;

	switch(evt) {
	case MPP_EVENT_INFERENCE_OUTPUT_READY:
		/* cast evt_data pointer to correct structure matching the event */
		inf_output = (const mpp_inference_cb_param_t *) evt_data;
		MOBILEFACENET_ProcessOutput(
                inf_output,
                Embedding_database,
                DATABASE_MAX_PEOPLE,
                &result);
		/* check that we can modify the user data (not accessed by other task) */
		if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
		{
			app_priv->inference_time_ms = inf_output->inference_time_ms;
			/* copy recognition results */
			app_priv->result = result;
			__atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
		}

		mpp_element_params_t params;
		uint8_t label_size = sizeof(params.labels.rectangles[0].label);

		const char* label = "\0";
		// Update the label in the first rectangle
		params.labels.detected_count = 1;
		params.labels.max_count = 1;
		params.labels.rectangles = app_priv->labels;
		strncpy((char *)params.labels.rectangles[0].label, label, label_size);
		params.labels.rectangles[0].label[label_size - 1] = '\0';
        if ( (app_priv->elem != 0) && ( app_priv->mp != NULL ) )
        {
            mpp_element_update(app_priv->mp, app_priv->elem, &params);
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
	for (;;) {
		xTaskDelayUntil( &xLastWakeTime, xFrequency );
		mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
		PRINTF("Element stats --------------------------\r\n");
		PRINTF("Mobilefacenet : exec_time %u (ms)\r\n", mobilefacenet_stats.elem.elem_exec_time);
		mpp_stats_enable(MPP_STATS_GRP_ELEMENT);
		if (Atomic_CompareAndSwap_u32(&user_data->accessing, 1, 0))
		{
			PRINTF("inference time %d (ms) \r\n", user_data->inference_time_ms);
			if (user_data->result.recognized_name[0]=='\0')
			{
				PRINTF("face not recognized! \r\n");
			}
			else
			{
				PRINTF("Recognized face: %s with similarity percentage: %d%%\r\n", user_data->result.recognized_name, user_data->result.similarity_percentage);			}
			__atomic_store_n(&user_data->accessing, 0, __ATOMIC_SEQ_CST);
		}
	}
	return;
}

static void app_task(void *param)
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
	mpp_element_params_t mobilefacenet_params;
	memset(&mobilefacenet_params, 0 , sizeof(mpp_element_params_t));

	mobilefacenet_params.ml_inference.model_data = mobilefacenet_data;
	mobilefacenet_params.ml_inference.model_size = mobilefacenet_data_len;
	mobilefacenet_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
	mobilefacenet_params.ml_inference.model_input_mean = MOBILEFACENET_INPUT_MEAN;
	mobilefacenet_params.ml_inference.model_input_std = MOBILEFACENET_INPUT_STD;
	mobilefacenet_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
	mobilefacenet_params.ml_inference.inference_params.num_inputs = 1;
	mobilefacenet_params.ml_inference.inference_params.num_outputs = 1;
	mobilefacenet_params.stats = &mobilefacenet_stats;

	ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &mobilefacenet_params, NULL);
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
