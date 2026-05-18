/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application tests the following pipeline:
 * static image (32x32 RGB) -> ExecuTorch inference (CIFAR-10) -> null sink
 *
 * The CIFAR-10 model performs classification among 10 object types:
 * airplane, automobile, bird, cat, deer, dog, frog, horse, ship, truck.
 * The model output is displayed on UART console.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
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
#include "app.h"
#else
#define main app_main
#endif

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/* ExecuTorch CIFAR-10 model data */
#include APP_EXECUTORCH_CIFAR_DATA
/* Model info (dimensions, mean, std, expected results) */
#include APP_EXECUTORCH_CIFAR_INFO

/* Model output post-processing */
#include "models/cifarnet10_executorch/cifar10_output_postproc.h"

/* Input image */
#include "images/cat_32_32_rgb.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define SRC_IMAGE_FORMAT          SRC_IMAGE_CAT_32_32_RGB_FORMAT
#define SRC_IMAGE_HEIGHT          SRC_IMAGE_CAT_32_32_RGB_HEIGHT
#define SRC_IMAGE_WIDTH           SRC_IMAGE_CAT_32_32_RGB_WIDTH

#define STATS_PRINT_PERIOD_MS     1000

/* define this flag to enable MPP stop and start */
#ifndef CONFIG_STOP_MPP
#define CONFIG_STOP_MPP 0
#endif
#if (CONFIG_STOP_MPP == 1)
/* MPP stop frequency factor with the print frequency */
#define MPP_STOP_FREQ_FACTOR 4
#endif

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_static_image_executorch_cifar10_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_static_image_executorch_cifar10"
#endif

/** Default priority for application tasks */
#define APP_DEFAULT_PRIO        1

typedef struct _user_data_t {
    int inference_frame_num;
    cifar_post_proc_data_t inf_out;
    int inference_time_ms;
    uint32_t accessing; /* boolean protecting concurrent access */
} user_data_t;

/* following mpp states are used for testing MPP stop API */
typedef enum {
    TEST_MPP_FULL_RUNNING = 0,
    TEST_MPP_MAIN_STOPPED,
} test_mpp_state_e;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void app_task(void *params);

/*******************************************************************************
 * Code
 ******************************************************************************/

int main(int argc, char *argv[])
{
    BaseType_t ret;
    TaskHandle_t handle = NULL;

    /* Init board hardware. */
    BOARD_Init();

    PRINTF("****** TEST %s ******\r\n", TC_NAME);
    PRINTF("---INFERENCE ENGINE: EXECUTORCH---\r\n");
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

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data)
{
    status_t ret;
    const mpp_inference_cb_param_t *inf_output;
    cifar_post_proc_data_t out_data;

    user_data_t *app_priv = (user_data_t *)user_data;

    switch(evt) {
    case MPP_EVENT_INFERENCE_OUTPUT_READY:
        inf_output = (const mpp_inference_cb_param_t *) evt_data;

        ret = CIFAR10_ProcessOutput(inf_output, NULL, 0, NULL, &out_data);
        if (ret != kStatus_Success)
            PRINTF("mpp_event_listener: process output error!\r\n");

        if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            app_priv->inference_time_ms = inf_output->inference_time_ms;
            app_priv->inf_out = out_data;
            __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
        }
        app_priv->inference_frame_num++;
        break;
    case MPP_EVENT_INVALID:
    default:
        break;
    }

    return 0;
}

static void print_result(mpp_stats_t *cifar_stats, user_data_t *user_data)
{
    mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
    PRINTF("Element stats --------------------------\r\n");
    PRINTF("cifar10 : exec_time %u (ms)\r\n", cifar_stats->elem.elem_exec_time);
    cifar_stats->elem.elem_exec_time = 0;
    mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

    if (Atomic_CompareAndSwap_u32(&user_data->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
    {
        PRINTF("inference time %d (ms)\r\n", user_data->inference_time_ms);
        PRINTF("cifar10 : %s (%d%%)\r\n", user_data->inf_out.label, user_data->inf_out.score);

        if ((user_data->inference_time_ms <= EXPECTED_INF_TIME) &&
            (strcmp(user_data->inf_out.label, EXPECTED_LABEL) == 0) &&
            (user_data->inf_out.score >= EXPECTED_INF_SCORE))
        {
            PRINTF("%s - PASSED\r\n", TC_NAME);
        }
        else
        {
            if (user_data->inference_time_ms > EXPECTED_INF_TIME)
                PRINTF("Bad inf time %d, expected less than %d\r\n",
                       user_data->inference_time_ms, EXPECTED_INF_TIME);
            if (strcmp(user_data->inf_out.label, EXPECTED_LABEL))
                PRINTF("Bad label %s, expected %s\r\n",
                       user_data->inf_out.label, EXPECTED_LABEL);
            if (user_data->inf_out.score < EXPECTED_INF_SCORE)
                PRINTF("Bad score %d, expected greater than %d\r\n",
                       user_data->inf_out.score, EXPECTED_INF_SCORE);
            PRINTF("%s - FAILED\r\n", TC_NAME);
        }

        PRINTF("%s finished\r\n", TC_NAME);
        PRINTF("\r\nStart %s\r\n", TC_NAME);

        user_data->inf_out.label = "No label detected";
        user_data->inf_out.score = 0;
        __atomic_store_n(&user_data->accessing, 0, __ATOMIC_SEQ_CST);
    }
}

static void app_task(void *params)
{
    user_data_t user_data = {0};
    int ret;

    PRINTF("[%s]\r\n", mpp_get_version());

    ret = mpp_api_init(NULL);
    if (ret)
        goto err;

    /* Create pipeline */
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

    /* Add static image source (32x32 RGB cat image) */
    mpp_img_params_t img_params;
    memset(&img_params, 0, sizeof(mpp_img_params_t));
    img_params.format = SRC_IMAGE_FORMAT;
    img_params.width  = SRC_IMAGE_WIDTH;
    img_params.height = SRC_IMAGE_HEIGHT;

    ret = mpp_static_img_add(mp, &img_params, (void *)cat_32_32_rgb_data, NULL);
    if (ret) {
        PRINTF("Failed to add static image\r\n");
        goto err;
    }

    /* Configure ExecuTorch inference element for CIFAR-10 */
    mpp_element_params_t cifar_params;
    static mpp_stats_t cifar_stats;
    memset(&cifar_params, 0, sizeof(mpp_element_params_t));

    cifar_params.ml_inference.model_data      = cifar_data;
    cifar_params.ml_inference.model_size       = cifar_data_len;
    cifar_params.ml_inference.model_input_mean = CIFAR_INPUT_MEAN;
    cifar_params.ml_inference.model_input_std  = CIFAR_INPUT_STD;
    cifar_params.ml_inference.type             = MPP_INFERENCE_TYPE_EXECUTORCH;
    cifar_params.ml_inference.tensor_order     = MPP_TENSOR_ORDER_NCHW;
    cifar_params.ml_inference.inference_params.num_inputs  = 1;
    cifar_params.ml_inference.inference_params.num_outputs = 1;
    cifar_params.stats = &cifar_stats;

    ret = mpp_element_add(mp, MPP_ELEMENT_INFERENCE, &cifar_params, NULL);
    if (ret) {
        PRINTF("Failed to add element INFERENCE (ExecuTorch)\r\n");
        goto err;
    }

    /* Close pipeline with null sink */
    ret = mpp_nullsink_add(mp);
    if (ret) {
        PRINTF("Failed to add NULL sink\r\n");
        goto err;
    }

    mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

    /* Start pipeline */
    ret = mpp_start(mp, 1, false);
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
    uint32_t last_inf_frame_num = user_data.inference_frame_num;

    PRINTF("\r\nStart %s\r\n", TC_NAME);

    for (;;) {
        xTaskDelayUntil(&xLastWakeTime, xFrequency);

        if (last_inf_frame_num != user_data.inference_frame_num) {
            print_result(&cifar_stats, &user_data);
            last_inf_frame_num = user_data.inference_frame_num;
        }

#if (CONFIG_STOP_MPP == 1)
        print_count++;
        if (print_count == MPP_STOP_FREQ_FACTOR) {
            switch (mpp_state) {
            case TEST_MPP_FULL_RUNNING:
                PRINTF("MPP STOP\r\n");
                mpp_stop(mp);
                mpp_state = TEST_MPP_MAIN_STOPPED;
                break;
            case TEST_MPP_MAIN_STOPPED:
                PRINTF("MPP START\r\n");
                ret = mpp_start(mp, 0, false);
                if (ret) {
                    PRINTF("Failed to start pipeline\r\n");
                    goto err;
                }
                mpp_state = TEST_MPP_FULL_RUNNING;
                break;
            default:
                PRINTF("STOP state is invalid: %d\r\n", mpp_state);
                goto err;
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