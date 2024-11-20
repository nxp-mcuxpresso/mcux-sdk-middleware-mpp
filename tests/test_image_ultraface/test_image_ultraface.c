/*
 * Copyright 2023-2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> TensorFlow Lite model Ultraface.
 * The model performs multiple face detections,
 * the model output is displayed on UART console by application.
 * The goal of this test application is to check if multiple faces detection is possible on MCX-N.
 */

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

#include "hal_debug.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* utility functions */
#include "models/utils.h"

/* tflite ultraface models */
#include APP_TFLITE_ULTRAFACE_DATA
/* Model info */
#include APP_TFLITE_ULTRAFACE_INFO

#include "models/ultraface_slim_quant_int8/ultraface_output_postproc.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define STATS_PRINT_PERIOD_MS 1000

typedef struct _args_t {
    void *image_buffer;
} args_t;

#define NUM_BOXES_MAX       MIN(APP_MAX_BOXES, ULTRAFACE_MAX_POINTS) /* max nb of boxes to filter */

typedef struct _user_data_t {
    int inference_frame_num;
    mpp_t mp;
    mpp_elem_handle_t elem;
    box_data boxes[NUM_BOXES_MAX];
    uint32_t accessing; /* boolean protecting access */
    int detected_count;          /* number of detected boxes */
    int inference_time_ms;
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

/* test image include file */
#ifndef APP_ULTRAFACE_ULTRASLIM
#include "images/skigirl320_320_rgb.h"
#else
#include "images/couple_COCO_128_128_rgb.h"
#endif

mpp_stats_t ultraface_stats;

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

    PRINTF("****** TEST test_image_ultraface ******\r\n");
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
    status_t ret;
    mpp_inference_cb_param_t *inf_output;

    /* user_data handle contains application private data */
    user_data_t *app_priv = (user_data_t *)user_data;

    switch(evt) {
    case MPP_EVENT_INFERENCE_OUTPUT_READY:
        /* cast evt_data pointer to correct structure matching the event */
        inf_output = (mpp_inference_cb_param_t *) evt_data;
        /* process new box data from inference */
        if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS) {
            ret = ULTRAFACE_ProcessOutput(
                    inf_output,
                    app_priv->boxes,
                    NUM_BOXES_MAX);
            if (ret != kStatus_Success)
                PRINTF("mpp_event_listener: process output error!");

            app_priv->detected_count = 0;
            /* count valid results */
            for (uint32_t i = 0; i < NUM_BOXES_MAX; i++)
            {
                if (app_priv->boxes[i].score > 0)
                    app_priv->detected_count++;
                else
                    break;
            }
           /* end of modification of user data */
            __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);

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
        PRINTF("Element stats --------------------------\r\n");
        PRINTF("ultraface : exec_time %u (ms)\r\n", ultraface_stats.elem.elem_exec_time);
        mpp_stats_enable(MPP_STATS_GRP_ELEMENT);
        if (Atomic_CompareAndSwap_u32(&user_data->accessing, 1, 0))
        {
            if (user_data->detected_count == 0)
            {
                PRINTF("No face detected! \r\n");
            }
            else
            {
                /* ignore rectangle of the Detection zone (user_data->boxes[0]) */
                for (int i = 0; i < user_data->detected_count; i++) {
                    PRINTF("     -----------\r\n");
                    PRINTF("     Box id: %d \r\n", i);
                    PRINTF("Box label: %s \r\n", "face");
                    PRINTF("Box score: %d%% \r\n", (int)(user_data->boxes[i].score * 100.0f));
                    PRINTF("    Box coordinates: \r\n");
                    PRINTF("Box left: %d \r\n", (int)(user_data->boxes[i].left));
                    PRINTF("Box right: %d \r\n", (int)(user_data->boxes[i].right));
                    PRINTF("Box top: %d \r\n", (int)(user_data->boxes[i].top));
                    PRINTF("Box bottom: %d \r\n", (int)(user_data->boxes[i].bottom));
                    PRINTF("     -----------\r\n");
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
    mpp_element_params_t ultraface_params;
    memset(&ultraface_params, 0 , sizeof(mpp_element_params_t));

    ultraface_params.ml_inference.model_data = ultraface_data;
    ultraface_params.ml_inference.model_size = ultraface_data_len;
    ultraface_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    ultraface_params.ml_inference.model_input_mean = ULTRAFACE_INPUT_MEAN;
    ultraface_params.ml_inference.model_input_std = ULTRAFACE_INPUT_STD;
    ultraface_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    ultraface_params.ml_inference.inference_params.num_inputs = 1;
    ultraface_params.ml_inference.inference_params.num_outputs = 1;
    ultraface_params.stats = &ultraface_stats;

    ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &ultraface_params, NULL);
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

