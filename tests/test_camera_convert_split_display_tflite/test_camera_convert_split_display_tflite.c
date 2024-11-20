/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * 2D camera -> split -> image converter -> display
 *                   +-> image converter -> TensorFlow Lite model MobileNet v1
 * The camera view finder is displayed on screen
 * The model performs classification among a list of 1000 object types (see model_data.h),
 * the model output is displayed on UART console by application */

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
typedef struct _user_data_t {
    int inference_frame_num;
    mobilenet_post_proc_data_t inf_out;
    uint32_t accessing; /* boolean protecting access */
} user_data_t;

/* following mpp states are used for testing MPP stop API */
typedef enum {
    TEST_MPP_FULL_RUNNING = 0, /* all mpp branches are running */
    TEST_MPP_MAIN_STOPPED,     /* main branch is stopped => only split branch is running */
    TEST_MPP_FULL_STOPPED,     /* main and split branches are stopped => no branch is running */
    TEST_MPP_SPLIT_STOPPED,    /* split branch is stopped => only main branch is running */
} test_mpp_state_e;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

/* define this flag to enable MPP stop and start */
#ifndef CONFIG_STOP_MPP
#define CONFIG_STOP_MPP 0
#endif
#if (CONFIG_STOP_MPP == 1)
/* MPP stop frequency factor with the print frequency */
#define MPP_STOP_FREQ_FACTOR 4
#endif

#define CROP_CENTER_OFFSET (APP_CAMERA_WIDTH - APP_CAMERA_HEIGHT)/2
#define MODEL_WIDTH  128
#define MODEL_HEIGHT 128
static const char s_display_name[] = APP_DISPLAY_NAME;
static const char s_camera_name[] = APP_CAMERA_NAME;

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
int main()
{
    BaseType_t ret;
    TaskHandle_t handle = NULL;

    /* Init board hardware. */
    BOARD_Init();

    PRINTF("****** TEST test_camera_convert_split_display_tflite ******\r\n");
    PRINTF("****** PARAMS: CONFIG_STOP_MPP = [%d] ******\r\n", CONFIG_STOP_MPP);
    PRINTF("\r\n");

    ret = xTaskCreate(
          app_task,
          "app_task",
          configMINIMAL_STACK_SIZE + 1000,
          NULL,
          APP_DEFAULT_PRIO,
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
    mobilenet_post_proc_data_t out_data;

    /* user_data handle contains application private data */
    user_data_t *app_priv = (user_data_t *)user_data;

    switch(evt) {
    case MPP_EVENT_INFERENCE_OUTPUT_READY:
        /* cast evt_data pointer to correct structure matching the event */
        inf_output = (mpp_inference_cb_param_t *) evt_data;
        ret = MOBILENETv1_ProcessOutput(inf_output, NULL, 0, NULL, &out_data);
        if (ret != kStatus_Success)
            PRINTF("mpp_event_listener: process output error!\r\n");
        /* check that we can modify the user data (not accessed by other task) */
        if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            /* copy inference results */
            app_priv->inf_out = out_data;
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

    mpp_camera_params_t cam_params;
    memset(&cam_params, 0 , sizeof(cam_params));
    cam_params.height = APP_CAMERA_HEIGHT;
    cam_params.width =  APP_CAMERA_WIDTH;
    cam_params.format = APP_CAMERA_FORMAT;
    cam_params.fps    = 30;
    ret = mpp_camera_add(mp, s_camera_name, &cam_params);
    if (ret) {
        PRINTF("Failed to add camera %s\r\n", s_camera_name);
        goto err;
    }

    /* split the pipeline into 2 branches */
    mpp_t mp_split;
    ret = mpp_split(mp, 1 , &mpp_params, &mp_split);
    if (ret) {
        PRINTF("Failed to split pipeline\r\n");
        goto err;
    }

    /* On one branch run the ML Inference (using a mobilenet_v1 TF-Lite model) */
    /* First do crop + resize + color convert */
    mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    /* set output buffer dims */
    elem_params.convert.out_buf.width = MODEL_WIDTH;
    elem_params.convert.out_buf.height = MODEL_HEIGHT;
    /* color convert */
    elem_params.convert.pixel_format = MPP_PIXEL_RGB;
    elem_params.convert.ops = MPP_CONVERT_COLOR;
    /* crop center of image */
    elem_params.convert.crop.top = 0;
    elem_params.convert.crop.bottom = APP_CAMERA_HEIGHT - 1;
    elem_params.convert.crop.left = CROP_CENTER_OFFSET;
    elem_params.convert.crop.right = CROP_CENTER_OFFSET + APP_CAMERA_HEIGHT - 1;
    elem_params.convert.ops |= MPP_CONVERT_CROP;
    /* resize: scaling parameters */
    elem_params.convert.scale.width = MODEL_WIDTH;
    elem_params.convert.scale.height = MODEL_HEIGHT;
    elem_params.convert.ops |= MPP_CONVERT_SCALE;

    ret = mpp_element_add(mp_split, MPP_ELEMENT_CONVERT, &elem_params, NULL);
    if (ret ) {
        PRINTF("Failed to add element CONVERT\r\n");
        goto err;
    }

    /* configure TFlite element with model */
    mpp_element_params_t mobilenet_params;
    static mpp_stats_t mobilenet_stats;
    memset(&mobilenet_params, 0 , sizeof(mpp_element_params_t));
    mobilenet_params.ml_inference.model_data = mobilenet_data;
    mobilenet_params.ml_inference.model_size = mobilenet_data_len;
    mobilenet_params.ml_inference.model_input_mean = MOBILENET_INPUT_MEAN;
    mobilenet_params.ml_inference.model_input_std = MOBILENET_INPUT_STD;
    mobilenet_params.ml_inference.inference_params.num_inputs = 1;
    mobilenet_params.ml_inference.inference_params.num_outputs = 1;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    mobilenet_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    mobilenet_params.stats = & mobilenet_stats;
    ret = mpp_element_add(mp_split, MPP_ELEMENT_INFERENCE, &mobilenet_params, NULL);
    if (ret) {
        PRINTF("Failed to add element VALGO_TFLite\r\n");
        goto err;
    }
    /* close the pipeline with a null sink */
    ret = mpp_nullsink_add(mp_split);
    if (ret) {
        PRINTF("Failed to add NULL sink\r\n");
        goto err;
    }


    /* On the other branch of the pipeline, send the frame to the display */
    /* First do color-convert + rotate */
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    /* set output buffer dims */
    elem_params.convert.out_buf.width = APP_DISPLAY_WIDTH;
    elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
    elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    elem_params.convert.flip = FLIP_HORIZONTAL;
    elem_params.convert.pixel_format = APP_DISPLAY_FORMAT;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_ROTATE;
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);
    if (ret) {
        PRINTF("Failed to add element CONVERT\r\n");
        goto err;
    }

    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.format = APP_DISPLAY_FORMAT;
    disp_params.width  = APP_DISPLAY_WIDTH;
    disp_params.height = APP_DISPLAY_HEIGHT;
    ret = mpp_display_add(mp, s_display_name, &disp_params);
    if (ret) {
        PRINTF("Failed to add display %s\r\n", s_display_name);
        goto err;
    }

    mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

    /* start 1st pipeline branch */
    ret = mpp_start(mp_split, 0);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }
    /* start 2nd pipeline branch */
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
                PRINTF("MPP STOP main branch\r\n");
                mpp_stop(mp);
                mpp_state = TEST_MPP_MAIN_STOPPED;
                break;
            case TEST_MPP_MAIN_STOPPED:
                PRINTF("MPP STOP split branch\r\n");
                mpp_stop(mp_split);
                mpp_state = TEST_MPP_FULL_STOPPED;
                break;
            case TEST_MPP_FULL_STOPPED:
                PRINTF("MPP START main branch\r\n");
                ret = mpp_start(mp, 0);
                if (ret) {
                    PRINTF("Failed to start main branch\r\n");
                    goto err;
                }
                mpp_state = TEST_MPP_SPLIT_STOPPED;
                break;
            case TEST_MPP_SPLIT_STOPPED:
                PRINTF("MPP START split branch\r\n");
                ret = mpp_start(mp_split, 0);
                if (ret) {
                    PRINTF("Failed to start split branch\r\n");
                    goto err;
                }
                mpp_state = TEST_MPP_FULL_RUNNING;
                break;
            default:
                PRINTF("TEST STOP state is invalid: %d\r\n", mpp_state);
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
