/*
 * Copyright 2022-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This example application shows usage of MultiMedia Pipeline to build a simple graph:
 * 2D camera -> split -> display 
                     +-> image converter -> GLOW model MobileNet v1
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
    mpp_t mp;
    mobilenet_post_proc_data_t inf_out[2];
    uint32_t accessing[2]; /* boolean protecting access */
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

#define STATS_PRINT_PERIOD_MS 1000

/* set this flag to 1 in order to replace the camera source by static image of a stopwatch */
#ifndef SOURCE_STATIC_IMAGE
#define SOURCE_STATIC_IMAGE 0
#endif

/*
 * SWAP_DIMS = 1 if source/display dims are reversed
 * SWAP_DIMS = 0 if source/display have the same orientation
 */
#define SWAP_DIMS (((APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_90) || (APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_270)) ? 1 : 0)

/*
 * SRC_DISPLAY_FLIP = FLIP_NONE if a static image is used as source
 * SRC_DISPLAY_FLIP = FLIP_HORIZONTAL if a camera is used as source
 */
#define SRC_DISPLAY_FLIP (SOURCE_STATIC_IMAGE ? FLIP_NONE : FLIP_HORIZONTAL)

#if (SOURCE_STATIC_IMAGE == 1)
#include "images/stopwatch168_208_vuyx.h"
#define SRC_WIDTH  SRC_IMAGE_WIDTH
#define SRC_HEIGHT SRC_IMAGE_HEIGHT
#define CROP_TOP 1
#define CROP_LEFT 0
#define CROP_SIZE SRC_IMAGE_WIDTH
#else  /* SOURCE_STATIC_IMAGE */
#define SRC_WIDTH  APP_CAMERA_WIDTH
#define SRC_HEIGHT APP_CAMERA_HEIGHT
#define CROP_TOP 0
#define CROP_LEFT (APP_CAMERA_WIDTH - APP_CAMERA_HEIGHT)/2
#define CROP_SIZE APP_CAMERA_HEIGHT
#endif  /* SOURCE_STATIC_IMAGE */

#define DISPLAY_SMALL_DIM MIN(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT)
#define DISPLAY_LARGE_DIM MAX(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT)

static const char s_display_name[] = APP_DISPLAY_NAME;
#if (SOURCE_STATIC_IMAGE != 1)
static const char s_camera_name[] = APP_CAMERA_NAME;
#endif

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

    ret = xTaskCreate(
          app_task,
          "app_task",
          configMINIMAL_STACK_SIZE + 1000,
          NULL,
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
    const mpp_inference_cb_param_t *inf_output;
    mobilenet_post_proc_data_t out_data;

    /* user_data handle contains application private data */
    user_data_t *app_priv = (user_data_t *)user_data;

    switch(evt) {
    case MPP_EVENT_INFERENCE_OUTPUT_READY:
        /* cast evt_data pointer to correct structure matching the event */
        inf_output = (const mpp_inference_cb_param_t *) evt_data;
        int inf_idx = 0;
        if (inf_output->inference_type == MPP_INFERENCE_TYPE_TFLITE)
            inf_idx = 0;
        else if (inf_output->inference_type == MPP_INFERENCE_TYPE_GLOW)
            inf_idx = 1;
        else
            PRINTF("Inference Engine: Unknown!!!\r\n");

        ret = MOBILENETv1_ProcessOutput(
                inf_output,
                app_priv->mp,
                0,
                NULL,
                &out_data);
        if (ret != kStatus_Success)
            PRINTF("mpp_event_listener: process output error!\r\n");
        /* check that we can modify the stats buffer (not accessed by other task) */
        if (Atomic_CompareAndSwap_u32(&app_priv->accessing[inf_idx], 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            /* copy inference results */
            app_priv->inf_out[inf_idx] = out_data;
            /* end of modification of user data */
            __atomic_store_n(&app_priv->accessing[inf_idx], 0, __ATOMIC_SEQ_CST);
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

static void app_task(void *params)
{
    user_data_t user_data = {0};
    mpp_t mp_inf[2];
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

    user_data.mp = mp;

#if (SOURCE_STATIC_IMAGE == 1)
    mpp_img_params_t img_params;
    memset(&img_params, 0, sizeof (mpp_img_params_t));
    img_params.format = SRC_IMAGE_FORMAT;
    img_params.width = SRC_IMAGE_WIDTH;
    img_params.height = SRC_IMAGE_HEIGHT;
    mpp_static_img_add(mp, &img_params, (void *)image_data);
#else
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
#endif

    /* split the pipeline into 2 branches */
    mpp_params_t split_params[2];
    split_params[0] = mpp_params; split_params[1] = mpp_params;
    ret = mpp_split(mp, 2 , split_params, mp_inf);
    if (ret) {
        PRINTF("Failed to split pipeline\r\n");
        goto err;
    }

    /* On the preempt-able branch run the ML Inference (using a mobilenet_v1 TF-Lite model) */
    /* First do crop + resize + color convert */
    mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    /* set output buffer dims */
    elem_params.convert.out_buf.width = MOBILENET_WIDTH;
    elem_params.convert.out_buf.height = MOBILENET_HEIGHT;
    /* color convert */
    elem_params.convert.pixel_format = MPP_PIXEL_RGB;
    elem_params.convert.ops = MPP_CONVERT_COLOR;
    /* crop center of image */
    elem_params.convert.crop.top = CROP_TOP;
    elem_params.convert.crop.bottom = CROP_TOP + CROP_SIZE - 1;
    elem_params.convert.crop.left = CROP_LEFT;
    elem_params.convert.crop.right = CROP_LEFT + CROP_SIZE - 1;
    elem_params.convert.ops |= MPP_CONVERT_CROP;
    /* resize: scaling parameters */
    elem_params.convert.scale.width = MOBILENET_WIDTH;
    elem_params.convert.scale.height = MOBILENET_HEIGHT;
    elem_params.convert.ops |= MPP_CONVERT_SCALE;

    ret = mpp_element_add(mp_inf[0], MPP_ELEMENT_CONVERT, &elem_params, NULL);
    if (ret ) {
        PRINTF("Failed to add element CONVERT\r\n");
        goto err;
    }


    mpp_element_params_t mobilenet_params;
    static mpp_stats_t tflite_stats, glow_stats;
    /* configure TFlite element with model */
    memset(&mobilenet_params, 0 , sizeof(mpp_element_params_t));
    mobilenet_params.ml_inference.model_data = mobilenet_data;
    mobilenet_params.ml_inference.model_size = mobilenet_data_len;
    mobilenet_params.ml_inference.model_input_mean = MOBILENET_INPUT_MEAN;
    mobilenet_params.ml_inference.model_input_std = MOBILENET_INPUT_STD;
    mobilenet_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    mobilenet_params.ml_inference.inference_params.num_inputs = 1;
    mobilenet_params.ml_inference.inference_params.num_outputs = 1;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    mobilenet_params.stats = &tflite_stats;
    ret = mpp_element_add(mp_inf[0], MPP_ELEMENT_INFERENCE, &mobilenet_params, NULL);
    if (ret) {
        PRINTF("Failed to add element VALGO_TFLite\r\n");
        goto err;
    }
    /* close the pipeline with a null sink */
    ret = mpp_nullsink_add(mp_inf[0]);
    if (ret) {
        PRINTF("Failed to add NULL sink\r\n");
        goto err;
    }

    /* add conversion for GLOW */
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    /* set output buffer dims */
    elem_params.convert.out_buf.width = MOBILENET_WIDTH;
    elem_params.convert.out_buf.height = MOBILENET_HEIGHT;
    /* color convert */
    elem_params.convert.pixel_format = MPP_PIXEL_RGB;
    elem_params.convert.ops = MPP_CONVERT_COLOR;
    /* crop center of image */
    elem_params.convert.crop.top = CROP_TOP;
    elem_params.convert.crop.bottom = CROP_TOP + CROP_SIZE - 1;
    elem_params.convert.crop.left = CROP_LEFT;
    elem_params.convert.crop.right = CROP_LEFT + CROP_SIZE - 1;
    elem_params.convert.ops |= MPP_CONVERT_CROP;
    /* resize: scaling parameters */
    elem_params.convert.scale.width = MOBILENET_WIDTH;
    elem_params.convert.scale.height = MOBILENET_HEIGHT;
    elem_params.convert.ops |= MPP_CONVERT_SCALE ;

    ret = mpp_element_add(mp_inf[1], MPP_ELEMENT_CONVERT, &elem_params, NULL);
    if (ret ) {
        PRINTF("Failed to add element CONVERT\r\n");
        goto err;
    }

    /* Set ML filter parameters */
    mobilenet_params.ml_inference.model_data = mobilenet_v1_weights_bin;
    mobilenet_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    mobilenet_params.ml_inference.inference_params.constant_weight_MemSize = MOBILENET_V1_CONSTANT_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.mutable_weight_MemSize = MOBILENET_V1_MUTABLE_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.activations_MemSize = MOBILENET_V1_ACTIVATIONS_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.num_inputs = 1;
    mobilenet_params.ml_inference.inference_params.num_outputs = 1;
    mobilenet_params.ml_inference.inference_params.inputs_offsets[0] = MOBILENET_V1_input;
    mobilenet_params.ml_inference.inference_params.outputs_offsets[0] = MOBILENET_V1_MobilenetV1_Predictions_Reshape_1;
    mobilenet_params.ml_inference.inference_params.model_input_tensors_type = MPP_TENSOR_TYPE_INT8;
    mobilenet_params.ml_inference.inference_params.model_entry_point = &mobilenet_v1;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_GLOW ;
    mobilenet_params.stats = &glow_stats;
    ret = mpp_element_add(mp_inf[1], MPP_ELEMENT_INFERENCE, &mobilenet_params, NULL);
    if (ret) {
        PRINTF("Failed to start ML inference\r\n");
        goto err;
    }

    /* close the pipeline with a null sink */
    ret = mpp_nullsink_add(mp_inf[1]);
    if (ret) {
        PRINTF("Failed to add NULL sink\r\n");
        goto err;
    }

    /* On the main branch of the pipeline, send the frame to the display */
    /* First do color-convert + rotate */
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    /* set output buffer dims */
    elem_params.convert.out_buf.width = APP_DISPLAY_WIDTH;
    elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
    elem_params.convert.pixel_format = APP_DISPLAY_FORMAT;
    elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    /* scaling parameters */
    if ((DISPLAY_LARGE_DIM * SRC_HEIGHT) < (DISPLAY_SMALL_DIM * SRC_WIDTH)) {
        elem_params.convert.scale.width =  (SWAP_DIMS ? (APP_DISPLAY_WIDTH * SRC_HEIGHT / SRC_WIDTH) : APP_DISPLAY_HEIGHT);
        elem_params.convert.scale.height = (SWAP_DIMS ? APP_DISPLAY_WIDTH : (APP_DISPLAY_HEIGHT * SRC_HEIGHT / SRC_WIDTH));
    } else {
        elem_params.convert.scale.width = (SWAP_DIMS ? APP_DISPLAY_WIDTH : (APP_DISPLAY_HEIGHT * SRC_WIDTH / SRC_HEIGHT));
        elem_params.convert.scale.height  = (SWAP_DIMS ? (APP_DISPLAY_WIDTH * SRC_WIDTH / SRC_HEIGHT) : APP_DISPLAY_HEIGHT);
    }
    elem_params.convert.flip = SRC_DISPLAY_FLIP;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_ROTATE | MPP_CONVERT_SCALE;

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

    /* start preempt-able pipeline branch */
    ret = mpp_start(mp_inf[0], 0);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }
    
    ret = mpp_start(mp_inf[1], 0);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }
    

    /* start main pipeline branch */
    ret = mpp_start(mp, 1);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = STATS_PRINT_PERIOD_MS / portTICK_PERIOD_MS;
    xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        xTaskDelayUntil( &xLastWakeTime, xFrequency );
        PRINTF("Element stats --------------------------\r\n");
        mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
        PRINTF("\r\nInference Engine: TensorFlowLite Micro\r\n");
        PRINTF("mobilenet : exec_time %u (ms)\r\n", tflite_stats.elem.elem_exec_time);
        if (Atomic_CompareAndSwap_u32(&user_data.accessing[0], 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            PRINTF("mobilenet : %s (%d%%)\r\n", user_data.inf_out[0].label, user_data.inf_out[0].score);
            __atomic_store_n(&user_data.accessing[0], 0, __ATOMIC_SEQ_CST);
        }
        PRINTF("\r\nInference Engine: Glow\r\n");
        PRINTF("mobilenet : exec_time %u (ms)\r\n", glow_stats.elem.elem_exec_time);
        if (Atomic_CompareAndSwap_u32(&user_data.accessing[1], 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            PRINTF("mobilenet : %s (%d%%)\r\n", user_data.inf_out[1].label, user_data.inf_out[1].score);
            __atomic_store_n(&user_data.accessing[1], 0, __ATOMIC_SEQ_CST);
        }
        mpp_stats_enable(MPP_STATS_GRP_ELEMENT);
    }

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}
