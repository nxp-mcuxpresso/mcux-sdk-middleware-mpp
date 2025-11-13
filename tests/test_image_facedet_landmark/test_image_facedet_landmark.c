/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> TensorFlow Lite model SCRFD_KPS_500M.
 * The model performs face detection and landmarks
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

/* NXP includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"

#include "hal_debug.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* tflite scrfd_kps models */
#include APP_TFLITE_SCRFD_KPS_DATA
/* Model info */
#include APP_TFLITE_SCRFD_KPS_INFO

#include "internal/models/scrfd_kps_500m_full_integer_quant/scrfd_kps_output_postproc.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define STATS_PRINT_PERIOD_MS 1000

#define NUM_BOXES_MAX APP_MAX_BOXES/* max nb of boxes to filter */

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
#if defined(USE_SCRFD_320_256_MODEL)
#include "images/couple_COCO_256_320_rgb.h"
#define SRC_IMAGE_FORMAT SRC_IMAGE_COUPLE_COCO_256_320_RGB_FORMAT
#define SRC_IMAGE_CHANNELS_NUMBER SRC_IMAGE_COUPLE_COCO_256_320_RGB_CHANNELS_NUMBER
#define SRC_IMAGE_HEIGHT SRC_IMAGE_COUPLE_COCO_256_320_RGB_HEIGHT
#define SRC_IMAGE_WIDTH SRC_IMAGE_COUPLE_COCO_256_320_RGB_WIDTH
void *image_data = (void *)couple_COCO_256_320_rgb_data;
#elif defined(USE_SCRFD_256_256_MODEL)
#include "images/couple_COCO_256_256_rgb.h"
#define SRC_IMAGE_FORMAT SRC_IMAGE_COUPLE_COCO_256_256_RGB_FORMAT
#define SRC_IMAGE_CHANNELS_NUMBER SRC_IMAGE_COUPLE_COCO_256_256_RGB_CHANNELS_NUMBER
#define SRC_IMAGE_HEIGHT SRC_IMAGE_COUPLE_COCO_256_256_RGB_HEIGHT
#define SRC_IMAGE_WIDTH SRC_IMAGE_COUPLE_COCO_256_256_RGB_WIDTH
void *image_data = (void *)couple_COCO_256_256_rgb_data;
#else
#include "images/couple_COCO_128_128_rgb.h"
#define SRC_IMAGE_FORMAT SRC_IMAGE_COUPLE_COCO_128_128_RGB_FORMAT
#define SRC_IMAGE_CHANNELS_NUMBER SRC_IMAGE_COUPLE_COCO_128_128_RGB_CHANNELS_NUMBER
#define SRC_IMAGE_HEIGHT SRC_IMAGE_COUPLE_COCO_128_128_RGB_HEIGHT
#define SRC_IMAGE_WIDTH SRC_IMAGE_COUPLE_COCO_128_128_RGB_WIDTH
void *image_data = (void *)couple_COCO_128_128_rgb_data;
#endif

mpp_stats_t scrfd_kps_stats;

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

    PRINTF("****** TEST test_image_facedet_landmark ******\r\n");
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
            ret = SCRFDKPS_ProcessOutput(
                    inf_output,
                    app_priv->boxes,
                    NUM_BOXES_MAX);
            if (ret != kStatus_Success)
                PRINTF("mpp_event_listener: process output error!");

            app_priv->inference_time_ms = inf_output->inference_time_ms;

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
        PRINTF("scrfd_kps : exec_time %u (ms)\r\n", scrfd_kps_stats.elem.elem_exec_time);
        mpp_stats_enable(MPP_STATS_GRP_ELEMENT);
        if (Atomic_CompareAndSwap_u32(&user_data->accessing, 1, 0))
        {
            PRINTF("inference time %d (ms) \r\n", user_data->inference_time_ms);
            if (user_data->detected_count == 0)
            {
                PRINTF("No face detected! \r\n");
            }
            else
            {
                /* ignore rectangle of the Detection zone (user_data->boxes[0]) */
                for (int i = 0; i < user_data->detected_count; i++) {
                    PRINTF("     -----------\r\n");
                    PRINTF("     Box label: %s \r\n", "face");
                    PRINTF("     Box score: %d%% \r\n", (int)(user_data->boxes[i].score * 100.0f));
                    PRINTF("     Box coordinates: \r\n");
                    PRINTF("     Box left: %d \r\n", (int)(user_data->boxes[i].left));
                    PRINTF("     Box right: %d \r\n", (int)(user_data->boxes[i].right));
                    PRINTF("     Box top: %d \r\n", (int)(user_data->boxes[i].top));
                    PRINTF("     Box bottom: %d \r\n", (int)(user_data->boxes[i].bottom));
                    for (int j = 0; j < MODEL_NUM_LANDMARKS; j++) {
                        PRINTF("     Landmark %d X %d Y %d \r\n", j, 
                            user_data->boxes[i].landmarks[j].x,
                            user_data->boxes[i].landmarks[j].y);
                    }
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
    mpp_element_params_t scrfd_kps_params;
    memset(&scrfd_kps_params, 0 , sizeof(mpp_element_params_t));

    scrfd_kps_params.ml_inference.model_data = scrfd_kps_data;
    scrfd_kps_params.ml_inference.model_size = scrfd_kps_data_len;
    scrfd_kps_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    scrfd_kps_params.ml_inference.model_input_mean = SCRFD_KPS_INPUT_MEAN;
    scrfd_kps_params.ml_inference.model_input_std = SCRFD_KPS_INPUT_STD;
    scrfd_kps_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    scrfd_kps_params.ml_inference.inference_params.num_inputs = 1;
    scrfd_kps_params.ml_inference.inference_params.num_outputs = 9;
    scrfd_kps_params.stats = &scrfd_kps_stats;

    ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &scrfd_kps_params, NULL);
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

