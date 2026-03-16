/*
 * Copyright 2024-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This example application shows usage of MultiMedia Pipeline to switch model:
 * 2D camera or static image -> split -> image converter -> draw labeled rectangles -> display
 *                   +-> image converter -> inference engine (model: persondetect/ultraface)
 * The camera view finder or static image is displayed on screen
 * The model performs person or face detection using TF-Lite micro inference engine
 * the model output is displayed on UART console by application */

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
#include "app.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* utility functions */
#include "models/utils.h"

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

/* Tensorflow lite model data input */
#include APP_TFLITE_PERSONDETECT_INFO
#include APP_TFLITE_PERSONDETECT_DATA
#include APP_TFLITE_ULTRAFACE_DATA
#include APP_TFLITE_ULTRAFACE_INFO

/* test image include file */
#include APP_STATIC_IMAGE_NAME
#ifdef SRC_IMAGE_COUPLE_COCO_128_128_RGB_HEIGHT
#define SRC_IMAGE_FORMAT SRC_IMAGE_COUPLE_COCO_128_128_RGB_FORMAT
#define SRC_IMAGE_CHANNELS_NUMBER SRC_IMAGE_COUPLE_COCO_128_128_RGB_CHANNELS_NUMBER
#define SRC_IMAGE_HEIGHT SRC_IMAGE_COUPLE_COCO_128_128_RGB_HEIGHT
#define SRC_IMAGE_WIDTH SRC_IMAGE_COUPLE_COCO_128_128_RGB_WIDTH
void *image_data = (void *)couple_COCO_128_128_rgb_data;
#else
#define SRC_IMAGE_FORMAT SRC_IMAGE_COUPLE_COCO_220_220_RGB_FORMAT
#define SRC_IMAGE_CHANNELS_NUMBER SRC_IMAGE_COUPLE_COCO_220_220_RGB_CHANNELS_NUMBER
#define SRC_IMAGE_HEIGHT SRC_IMAGE_COUPLE_COCO_220_220_RGB_HEIGHT
#define SRC_IMAGE_WIDTH SRC_IMAGE_COUPLE_COCO_220_220_RGB_WIDTH
void *image_data = (void *)couple_COCO_220_220_rgb_data;
#endif

#include "models/persondetect/persondetect_output_postprocess.h"
#include "models/ultraface_slim_quant_int8/ultraface_output_postproc.h"

#define OUTPUT_PRINT_PERIOD_MS 1000

#define PERSONDETECT_DETECTION_LABEL "person"
#define ULTRAFACE_DETECTION_LABEL "face"

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_image_switch_model_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_switch_model"
#endif

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define MAX_LABEL_RECTS     10
#define NUM_BOXES_MAX       80

typedef struct _user_data_t {
    int inference_frame_num;
    mpp_t mp;
    mpp_elem_handle_t labrect_elem;
    mpp_elem_handle_t infer_elem;
    mpp_labeled_rect_t labels[MAX_LABEL_RECTS];
    box_data final_boxes[NUM_BOXES_MAX];
    uint32_t accessing; /* boolean protecting access */
    int detected_count; /* number of detected boxes */
    int inference_time_ms;
} user_data_t;

typedef enum _e_cur_model {
    MODEL_ULTRAFACE,
    MODEL_PERSONDET
} e_cur_model;

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

static e_cur_model g_cur_model = MODEL_PERSONDET;
static uint32_t g_cur_model_run_cnt = 0;
static char *g_model_name = PERSONDETECT_NAME;
static char *g_label = PERSONDETECT_DETECTION_LABEL;

/* verify minimum confidence level and number of detections for each model */
/* test is passed if both models verify the expected values */
static void check_model_switch_output(user_data_t *app_priv)
{
    static bool test_fail = false;
    static int face_pass = false;
    static int person_pass = false;

    /* verify confidence level */
    for (uint32_t i = 0; i < NUM_BOXES_MAX; i++)
    {
        if (app_priv->final_boxes[i].score > 0)
        {
            if ((g_cur_model == MODEL_PERSONDET)
                    && ((app_priv->final_boxes[i].score * 100.0f) < EXPECTED_PERSON_CONFIDENCE_MIN))
            {
                test_fail = true;
                PRINTF("MODEL_PERSONDET confidence below expected min\n\r");
            }

            if ((g_cur_model == MODEL_ULTRAFACE)
                    && ((app_priv->final_boxes[i].score * 100.0f) < EXPECTED_FACE_CONFIDENCE_MIN))
            {
                test_fail = true;
                PRINTF("MODEL_ULTRAFACE confidence below expected min\n\r");
            }
        }
    }

    /* verify number of detections */
    if (g_cur_model == MODEL_PERSONDET)
    {
        if (app_priv->detected_count == EXPECTED_NUM_DETECTED_PERSONS)
        {
            person_pass = true;
        }
        else
        {
            PRINTF("MODEL_PERSONDET unexpected number of detections \n\r");
            test_fail = true;
        }
        if (app_priv->inference_time_ms >= EXPECTED_PERSON_MAX_INF_TIME)
        {
            PRINTF("MODEL_PERSONDET unexpected infernece time \n\r");
            test_fail = true;
        }
    }
    else /* (g_cur_model == MODEL_ULTRAFACE) */
    {
        if(app_priv->detected_count == EXPECTED_NUM_DETECTED_FACES)
        {
            face_pass = true;
        }
        else
        {
            PRINTF("MODEL_ULTRAFACE unexpected number of detections \n\r");
            test_fail = true;
        }
        if (app_priv->inference_time_ms >= EXPECTED_ULTRAFACE_MAX_INF_TIME)
        {
            PRINTF("MODEL_ULTRAFACE unexpected infernece time \n\r");
            test_fail = true;
        }
    }

    if (test_fail)
    {
        PRINTF("%s - FAILED\r\n", TC_NAME);
        PRINTF("%s finished\r\n", TC_NAME);
        PRINTF("\r\nStart %s\r\n", TC_NAME);
        test_fail = false;
        face_pass = false;
        person_pass = false;
    }
    else if (face_pass && person_pass)
    {
        face_pass = false;
        person_pass = false;
        PRINTF("%s - PASSED\r\n", TC_NAME);
        PRINTF("%s finished\r\n", TC_NAME);
        PRINTF("\r\nStart %s\r\n", TC_NAME);
    }
}

int main()
{
    BaseType_t ret;
    TaskHandle_t handle = NULL;

    /* Init board hardware. */
    BOARD_Init();

    ret = xTaskCreate(
            app_task,
            "app_task",
            configMINIMAL_STACK_SIZE + 2400,
            NULL,
            APP_DEFAULT_PRIO,
            &handle);

    if (pdPASS != ret) {
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

            if (g_cur_model == MODEL_PERSONDET)
            {
                ret = Persondetect_Output_postprocessing(
                        inf_output,
                        app_priv->final_boxes,
                        NUM_BOXES_MAX);
            }
            else
            {
                ret = ULTRAFACE_ProcessOutput(
                        inf_output,
                        app_priv->final_boxes,
                        NUM_BOXES_MAX);
            }
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

            g_cur_model_run_cnt++;
            /* check expected behavior */
            if (g_cur_model_run_cnt >= 2)
                check_model_switch_output(app_priv);

            app_priv->inference_frame_num++;

            /* end of modification of user data */
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

static void app_task(void *params)
{
    static user_data_t user_data = {0};
    int ret = 0;

    PRINTF("[%s]\r\n", mpp_get_version());

    PRINTF("Inference Engine: TensorFlow-Lite Micro \r\n");

    /* init API */
    mpp_api_params_t api_param = {0};
#if ((APP_STRIPE_MODE == 1) && (defined APP_RC_CYCLE_INC) && (defined APP_RC_CYCLE_MIN))
    /* fine-tune RC cycle for stripe mode */
    api_param.rc_cycle_inc = APP_RC_CYCLE_INC;
    api_param.rc_cycle_min = APP_RC_CYCLE_MIN;
#endif
    ret = mpp_api_init(&api_param);
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

    /* First do crop + resize + color convert */
    static mpp_element_params_t infer_conv_params;
    memset(&infer_conv_params, 0, sizeof(infer_conv_params));
    /* pick default device from the first listed and supported by Hw */
    infer_conv_params.convert.dev_name = APP_GFX_BACKEND_NAME;
    /* set output buffer dims */
    infer_conv_params.convert.out_buf.width = PERSONDETECT_WIDTH;
    infer_conv_params.convert.out_buf.height = PERSONDETECT_HEIGHT;
    /* resize: scaling parameters */
    infer_conv_params.convert.scale.width = PERSONDETECT_WIDTH;
    infer_conv_params.convert.scale.height = PERSONDETECT_HEIGHT;
    infer_conv_params.convert.ops = MPP_CONVERT_SCALE;
    infer_conv_params.convert.stripe_in = APP_STRIPE_MODE == 1 ? true : false;
    infer_conv_params.convert.stripe_out = false; /* model takes full frames */

    static mpp_elem_handle_t infer_conv_h;
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &infer_conv_params, &infer_conv_h);
    if (ret ) {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

    /* prepare the persondetect model params */
    mpp_element_params_t persondetect_params;
    memset(&persondetect_params, 0 , sizeof(mpp_element_params_t));

    persondetect_params.ml_inference.model_data = persondetect_data;
    persondetect_params.ml_inference.model_size = persondetect_data_len;
    persondetect_params.ml_inference.model_input_mean = PERSONDETECT_INPUT_MEAN;
    persondetect_params.ml_inference.model_input_std = PERSONDETECT_INPUT_STD;
    persondetect_params.ml_inference.inference_params.num_inputs = 1;
    persondetect_params.ml_inference.inference_params.num_outputs = 1;
    persondetect_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    persondetect_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;

    /* prepare the ultraface model params */
    mpp_element_params_t ultraface_params;
    memset(&ultraface_params, 0 , sizeof(mpp_element_params_t));
    ultraface_params.ml_inference.model_data = ultraface_data;
    ultraface_params.ml_inference.model_size = ultraface_data_len;
    ultraface_params.ml_inference.model_input_mean = ULTRAFACE_INPUT_MEAN;
    ultraface_params.ml_inference.model_input_std = ULTRAFACE_INPUT_STD;
    ultraface_params.ml_inference.inference_params.num_inputs = 1;
    ultraface_params.ml_inference.inference_params.num_outputs = 1;
    ultraface_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    ultraface_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;

    /* configure TFlite element with model */
    if (g_cur_model == MODEL_PERSONDET)
    {
        ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &persondetect_params, &user_data.infer_elem);
    }
    else
    {
        ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &ultraface_params, &user_data.infer_elem);
    }

    if (ret) {
        PRINTF("Failed to add element MPP_ELEMENT_INFERENCE");
        goto err;
    }
    /* close the pipeline with a null sink */
    ret = mpp_nullsink_add(mp);
    if (ret) {
        PRINTF("Failed to add NULL sink\n");
        goto err;
    }

    /* start main pipeline branch */
    ret = mpp_start(mp, 1, false);
    if (ret) {
        PRINTF("Failed to start main pipeline branch");
        goto err;
    }

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = OUTPUT_PRINT_PERIOD_MS / portTICK_PERIOD_MS;
    xLastWakeTime = xTaskGetTickCount();
    uint32_t last_inf_frame_num = user_data.inference_frame_num;
    for (;;) {
        xTaskDelayUntil( &xLastWakeTime, xFrequency );

        if (Atomic_CompareAndSwap_u32(&user_data.accessing, 1, 0))
        {
            if (last_inf_frame_num <= (user_data.inference_frame_num - 2))
            {
                PRINTF("inference time %d ms \r\n", user_data.inference_time_ms);
                if (user_data.detected_count <= 0)
                {
                    PRINTF("%s : no detection\r\n", g_model_name);
                }
                else
                {
                    for (int i = 0; i < NUM_BOXES_MAX; i++)
                    {
                        if (user_data.final_boxes[i].area > 0)
                        {
                            PRINTF("%s : box %d label %s score %d(%%)\r\n", g_model_name, i,
                                    g_label, (int)(user_data.final_boxes[i].score * 100.0f));
                        }
                    }
                }

                mpp_stop(mp);
                if (g_cur_model == MODEL_PERSONDET)
                {
                    /* update convert params for ultraface */
                    infer_conv_params.convert.out_buf.width = ULTRAFACE_WIDTH;
                    infer_conv_params.convert.out_buf.height = ULTRAFACE_HEIGHT;
                    infer_conv_params.convert.scale.width = ULTRAFACE_WIDTH;
                    infer_conv_params.convert.scale.height = ULTRAFACE_HEIGHT;
                    ret = mpp_element_update(mp, infer_conv_h, &infer_conv_params, true);
                    if (ret) {
                        PRINTF("Failed to update element convert for ultraface");
                        goto err;
                    }

                    /* switch to ULTRAFACE */
                    ret = mpp_element_update(mp, user_data.infer_elem, &ultraface_params, true);
                    if (ret) {
                        PRINTF("Failed to update element inference for ultraface");
                        goto err;
                    }

                    g_cur_model = MODEL_ULTRAFACE;
                    g_model_name = ULTRAFACE_NAME;
                    g_label = ULTRAFACE_DETECTION_LABEL;
                } else {
                    /* update convert params for person detect */
                    infer_conv_params.convert.out_buf.width = PERSONDETECT_WIDTH;
                    infer_conv_params.convert.out_buf.height = PERSONDETECT_HEIGHT;
                    infer_conv_params.convert.scale.width = PERSONDETECT_WIDTH;
                    infer_conv_params.convert.scale.height = PERSONDETECT_HEIGHT;
                    ret = mpp_element_update(mp, infer_conv_h, &infer_conv_params, true);
                    if (ret) {
                        PRINTF("Failed to update element convert for persondetect");
                        goto err;
                    }

                    /* switch to PERSONDET */
                    ret = mpp_element_update(mp, user_data.infer_elem, &persondetect_params, true);
                    if (ret) {
                        PRINTF("Failed to update element inference for persondetect");
                        goto err;
                    }

                    g_cur_model = MODEL_PERSONDET;
                    g_model_name = PERSONDETECT_NAME;
                    g_label = PERSONDETECT_DETECTION_LABEL;
                }
                last_inf_frame_num = user_data.inference_frame_num;
                mpp_start(mp, 0, false);
            }

            __atomic_store_n(&user_data.accessing, 0, __ATOMIC_SEQ_CST);
        }
    }

    /* pause application task */
    vTaskSuspend(NULL);

    err:
    for (;;) {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}
