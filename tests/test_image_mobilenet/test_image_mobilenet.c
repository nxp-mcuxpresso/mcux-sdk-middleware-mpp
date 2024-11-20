/*
 * Copyright 2022-2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> TensorFlow Lite model MobileNet v1
 * The model performs classification among a list of 1000 object types (see model_data.h),
 * the model output is displayed on UART console by application.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"
#include "atomic.h"

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

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* tflite mobilenet model */
#include APP_TFLITE_MOBILENET_DATA
/* Model info */
#include APP_TFLITE_MOBILENET_INFO

#include "models/mobilenet_v1_0.25_128_quant_int8/mobilenetv1_output_postproc.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/


typedef struct _args_t {
    char camera_name[32];
    char display_name[32];
    mpp_pixel_format_t display_format;
    mpp_pixel_format_t camera_format;
} args_t;

typedef struct _user_data_t {
    int inference_frame_num;
    mobilenet_post_proc_data_t inf_out;
    uint32_t accessing; /* boolean protecting access */
} user_data_t;

/* following mpp states are used for testing MPP stop API */
typedef enum {
    TEST_MPP_FULL_RUNNING = 0, /* all mpp branches are running */
    TEST_MPP_MAIN_STOPPED,     /* main branch is stopped */
} test_mpp_state_e;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

#include "images/stopwatch128_128_rgb.h"
#define CROP_TOP 0
#define CROP_LEFT 0
#define CROP_SIZE SRC_IMAGE_WIDTH

/* define this flag to enable MPP stop and start */
#ifndef CONFIG_STOP_MPP
#define CONFIG_STOP_MPP 0
#endif
#if (CONFIG_STOP_MPP == 1)
/* MPP stop frequency factor with the print frequency */
#define MPP_STOP_FREQ_FACTOR 4
#endif

#define MOBILENET_FORMAT  MPP_PIXEL_RGB

#define STATS_PRINT_PERIOD_MS 1000

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

    PRINTF("****** TEST test_image_mobilenet ******\r\n");
#if defined(INFERENCE_ENGINE_TFLM)
    PRINTF("---INFERENCE ENGINE: TFLITE---\r\n");
#elif defined(INFERENCE_ENGINE_GLOW)
    PRINTF("---INFERENCE ENGINE: GLOW---\r\n");
#elif defined(INFERENCE_ENGINE_DeepViewRT)
    PRINTF("---INFERENCE ENGINE: DeepViewRT---\r\n");

#endif
    PRINTF("****** PARAMS: CONFIG_STOP_MPP = [%d] ******\r\n", CONFIG_STOP_MPP);
    PRINTF("\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\r\n");
        goto err;
    }

    strcpy(args->display_name, "Lcdifv2Rk055ah");
    strcpy(args->camera_name, "MipiOv5640");
    args->camera_format = MPP_PIXEL_YUV1P444;
    args->display_format = MPP_PIXEL_RGB565;

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
    mobilenet_post_proc_data_t out_data;

    /* user_data handle contains application private data */
    user_data_t *app_priv = (user_data_t *)user_data;
    /* statistics to be printed from application task*/

    switch(evt) {
    case MPP_EVENT_INFERENCE_OUTPUT_READY:
        /* cast evt_data pointer to correct structure matching the event */
        inf_output = (mpp_inference_cb_param_t *) evt_data;
        ret = MOBILENETv1_ProcessOutput(inf_output, NULL, 0, NULL, &out_data);
        if (ret != kStatus_Success)
            PRINTF("mpp_event_listener: process output error!\r\n");
        /* check that we can modify the stats buffer (not accessed by other task) */
        if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            /* copy inference output */
            app_priv->inf_out = out_data;
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

void print_result(mpp_stats_t *mobilenet_stats, user_data_t *user_data) {

    mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
    PRINTF("Element stats --------------------------\r\n");
    PRINTF("mobilenet : exec_time %u (ms)\r\n", mobilenet_stats->elem.elem_exec_time);
    /* after reading, stats should be cleared */
    mobilenet_stats->elem.elem_exec_time = 0;
    mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

    if (Atomic_CompareAndSwap_u32(&user_data->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
    {
        PRINTF("mobilenet : %s (%d%%)\r\n", user_data->inf_out.label, user_data->inf_out.score);
        /* after reading, inference output should be cleared */
        user_data->inf_out.label = "No label detected";
        user_data->inf_out.score = 0;
        __atomic_store_n(&user_data->accessing, 0, __ATOMIC_SEQ_CST);
    }
}

static void app_task(void *params)
{
    user_data_t user_data = {0};
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
    mpp_element_params_t mobilenet_params;
    static mpp_stats_t mobilenet_stats;
    memset(&mobilenet_params, 0 , sizeof(mpp_element_params_t));

#if defined(INFERENCE_ENGINE_TFLM)             /* TFlite */
    mobilenet_params.ml_inference.model_data = mobilenet_data;
    mobilenet_params.ml_inference.model_size = mobilenet_data_len;
    mobilenet_params.ml_inference.model_input_mean = MOBILENET_INPUT_MEAN;
    mobilenet_params.ml_inference.model_input_std = MOBILENET_INPUT_STD;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
#elif defined(INFERENCE_ENGINE_GLOW)           /* GLOW */
    mobilenet_params.ml_inference.model_data = mobilenet_v1_weights_bin;
    mobilenet_params.ml_inference.inference_params.constant_weight_MemSize = MOBILENET_V1_CONSTANT_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.mutable_weight_MemSize = MOBILENET_V1_MUTABLE_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.activations_MemSize = MOBILENET_V1_ACTIVATIONS_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.inputs_offsets[0] = MOBILENET_V1_input;
    mobilenet_params.ml_inference.inference_params.outputs_offsets[0] = MOBILENET_V1_MobilenetV1_Predictions_Reshape_1;
    mobilenet_params.ml_inference.inference_params.model_input_tensors_type = MPP_TENSOR_TYPE_INT8;
    mobilenet_params.ml_inference.inference_params.model_entry_point = &mobilenet_v1;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_GLOW;
#elif defined(INFERENCE_ENGINE_DeepViewRT)    /* DeepViewRT */
    mobilenet_params.ml_inference.model_data = mobilenet_data;
    mobilenet_params.ml_inference.model_size = mobilenet_data_len;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_DEEPVIEWRT ;
#endif

    mobilenet_params.ml_inference.inference_params.num_inputs = 1;
    mobilenet_params.ml_inference.inference_params.num_outputs = 1;
    mobilenet_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    mobilenet_params.stats = &mobilenet_stats;

    ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &mobilenet_params, NULL);
    if (ret) {
        PRINTF("Failed to add element VALGO_TFLite\r\n");
        goto err;
    }

    mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

    ret = mpp_nullsink_add(mp);
    if (ret) {
        PRINTF("Failed to add NULL sink\r\n");
        goto err;
    }

    ret = mpp_start(mp, 1);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }

#if (CONFIG_STOP_MPP == 1)
    uint32_t print_count = 0;
    test_mpp_state_e mpp_state = TEST_MPP_FULL_RUNNING;
#endif

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = STATS_PRINT_PERIOD_MS / portTICK_PERIOD_MS;
    for (;;) {
        xTaskDelayUntil(&xLastWakeTime, xFrequency);
        print_result(&mobilenet_stats, &user_data);
#if (CONFIG_STOP_MPP == 1)
        print_count ++;
        if (print_count == MPP_STOP_FREQ_FACTOR) {
            switch (mpp_state) {
            case TEST_MPP_FULL_RUNNING:
                PRINTF("MPP STOP\r\n");
                mpp_stop(mp);
                mpp_state = TEST_MPP_MAIN_STOPPED;
                break;
            case TEST_MPP_MAIN_STOPPED:
                PRINTF("MPP START\r\n");
                ret = mpp_start(mp, 0);
                if (ret) {
                    PRINTF("Failed to start pipeline\r\n");
                    goto err;
                }
                mpp_state = TEST_MPP_FULL_RUNNING;
                break;
            default:
                PRINTF("STOP state is invalid: %d\r\n", mpp_state);
                goto err;
                break;
            }
            print_count = 0;
        }
#endif
    }

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}

