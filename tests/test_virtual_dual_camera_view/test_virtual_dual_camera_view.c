/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This application tests the virtual dual camera element using core 1 and RPMsg to send 
 * frames from core 1 to core 0, simulating an USB camera task running on core 1 */
 
/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

/* pick default backend if not specified. */
#ifndef APP_GFX_BACKEND_NAME
#define APP_GFX_BACKEND_NAME NULL
#endif

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

/** Enable stream configuration changes during runtime */
#define STREAM_CFG_CHANGE           1
/** Period in milliseconds for stream configuration changes during runtime */
#define STREAM_CFG_CHANGE_PERIOD_MS 5000 

typedef struct _args_t {
    char camera_name[32];
    char display_name[32];
    mpp_pixel_format_t src_format;
    mpp_pixel_format_t display_format;
} args_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void app_task(void *params);
static int get_random_value(int min, int max);

/*******************************************************************************
 * Variables
 ******************************************************************************/
static struct rpmsg_lite_instance *rpmsg_inst = NULL;

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

    /* Early init mcmgr - this function must be called at the start of the main function */
    mpp_mcmgr_early_init();

    /* Init board hardware. */
    BOARD_Init();
    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

    strcpy(args->display_name, APP_DISPLAY_NAME);
    strcpy(args->camera_name, APP_CAMERA_NAME);
    args->src_format = APP_CAMERA_FORMAT;
    args->display_format = APP_DISPLAY_FORMAT;

    /* Create application task */
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
        PRINTF("Failed to create app_task task");
        while (1);
    }

    /* MPP API requires RTOS scheduler to be started */
    vTaskStartScheduler();
    for (;;)
        vTaskSuspend(NULL);
    return 0;
}

/*!
 * @brief Generate a random value within a specified interval
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return Random value between min and max
 */
static int get_random_value(int min, int max)
{
    if (min > max) {
        // Swap values if min is greater than max
        int temp = min;
        min = max;
        max = temp;
    }

    // Use FreeRTOS tick count as a simple pseudo-random seed
    static uint32_t seed = 0;
    if (seed == 0) {
        seed = (uint32_t)xTaskGetTickCount();
    }

    // Simple linear congruential generator
    seed = (seed * 1103515245U + 12345U) & 0x7fffffffU;

    // Scale to the desired range
    return min + (int)(seed % (uint32_t)(max - min + 1));
}

/* Application task function */
static void app_task(void *params) {
    int ret;
    args_t *args = (args_t *) params;
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;
    mpp_t display_branch;
    mpp_t sink_branch;

    PRINTF("[%s]\r\n", mpp_get_version());

    /* Boot secondary core */
    volatile uint16_t *mcmgr_event_data_p = mpp_boot_secondary_core();
    PRINTF("Secondary core is up\r\n");

    /* Initialize RPMSG module */
    rpmsg_inst = (struct rpmsg_lite_instance *) mpp_init_rpmsg();
    if (rpmsg_inst == NULL) {
        PRINTF("RPMSG init failed\r\n");
        goto err;
    }
    PRINTF("RPMsg intialized\r\n");

    /* init API */
    mpp_api_params_t api_param = {0};
#if ((defined APP_RC_CYCLE_INC) && (defined APP_RC_CYCLE_MIN))
    /* fine-tune RC cycle for stripe mode */
    api_param.rc_cycle_inc = APP_RC_CYCLE_INC;
    api_param.rc_cycle_min = APP_RC_CYCLE_MIN;
#endif
    ret = mpp_api_init(&api_param);
    if (ret)
        goto err;

    /* create mpp */
    mpp_t mp;
    mpp_params_t mpp_params;
    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.exec_flag = MPP_EXEC_RC;
    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
        goto err;

    /* add camera */
    mpp_camera_params_t cam_params;
    mpp_elem_handle_t cam_elem;
    memset(&cam_params, 0 , sizeof(cam_params));
    cam_params.height = APP_CAMERA_HEIGHT;
    cam_params.width  = APP_CAMERA_WIDTH;
    cam_params.format = args->src_format;
    cam_params.fps    = 30;
    cam_params.stripe = stripe_mode;
    cam_params.rpmsg_inst = rpmsg_inst;
    cam_params.mcmgr_event_data = mcmgr_event_data_p;
    cam_params.in_advance_enqueue = CAMERA_IN_ADVANCE_ENQUEUE;
#if (APP_CONFIG==2)
    cam_params.n_streams = 1;
    cam_params.stream[0].type = RGB_STREAM;
    cam_params.stream[0].active = true;
#elif (APP_CONFIG <= 5)
    cam_params.n_streams = 2;
    cam_params.stream[0].type = RGB_STREAM;
    cam_params.stream[0].active = true;
    cam_params.stream[1].type = IR_STREAM;
    cam_params.stream[1].active = true;
#else
    #pragma message "configuration APP_CONFIG value is not supported by test"
#endif

    ret = mpp_camera_add(mp, args->camera_name, &cam_params, &cam_elem);
    if (ret) {
        PRINTF("Failed to add camera %s\n", args->camera_name);
        goto err;
    }

#if (APP_CONFIG==0 || APP_CONFIG==3)
    mpp_t mp_split;
    mpp_params.exec_flag = MPP_EXEC_RC;
    ret = mpp_split(mp, 1 , &mpp_params, &mp_split);
    if (ret) {
        PRINTF("Failed to split pipeline\n");
        goto err;
    }
#elif ((APP_CONFIG==1) || (APP_CONFIG==4) || (APP_CONFIG==5))
    mpp_t mp_split;
    mpp_params.exec_flag = MPP_EXEC_PREEMPT;
    ret = mpp_split(mp, 1 , &mpp_params, &mp_split);
    if (ret) {
        PRINTF("Failed to split pipeline\n");
        goto err;
    }
#elif (APP_CONFIG==2)
    /* create another mpp */
    mpp_t mp_new;
    mpp_params_t mpp_params_new;
    memset(&mpp_params_new, 0, sizeof(mpp_params_new));
    mpp_params_new.exec_flag = MPP_EXEC_RC;
    mp_new = mpp_create(&mpp_params_new, &ret);
    if (mp_new == MPP_INVALID)
        goto err;

    /* add camera */
    mpp_camera_params_t cam_params_new;
    memset(&cam_params_new, 0 , sizeof(cam_params_new));
    cam_params_new.height = APP_CAMERA_HEIGHT;
    cam_params_new.width  = APP_CAMERA_WIDTH;
    cam_params_new.format = args->src_format;
    cam_params_new.fps    = 30;
    cam_params_new.stripe = stripe_mode;
    cam_params_new.rpmsg_inst = rpmsg_inst;
    cam_params_new.n_streams = 1;
    cam_params_new.stream[0].type = IR_STREAM;
    cam_params_new.stream[0].active = true;

    ret = mpp_camera_add(mp_new, args->camera_name, &cam_params_new, NULL);
    if (ret) {
        PRINTF("Failed to add camera %s\n", args->camera_name);
        goto err;
    }
#else
    #pragma message "configuration APP_CONFIG value is not supported by test"
#endif

    /* Assign which branch is going to be displayed and which will be terminated with NULL SINK */
#if ((APP_CONFIG==0) || (APP_CONFIG==3) || (APP_CONFIG==4) || (APP_CONFIG==5))
    display_branch = mp;
    sink_branch = mp_split;
#elif (APP_CONFIG==1)
    display_branch = mp_split;
    sink_branch = mp;
#else
    display_branch = mp;
    sink_branch = mp_new;
#endif

    if (args->src_format == MPP_PIXEL_JPEG)
    {
        /* Add element jpeg decode */
        mpp_element_params_t elem_params;
        memset(&elem_params, 0, sizeof(mpp_element_params_t));
        elem_params.decode.dev_name = IMG_DECODE_DEV_NAME;
        elem_params.decode.width = APP_CAMERA_WIDTH;
        elem_params.decode.height = APP_CAMERA_HEIGHT;

        if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_CPU") == 0)
            elem_params.decode.out_format = MPP_PIXEL_BGR; /* TODO auto detect */
        else if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_HW") == 0)
            elem_params.decode.out_format = MPP_PIXEL_YUYV; /* TODO auto detect */

        ret = mpp_element_add(display_branch, MPP_ELEMENT_IMG_DECODE, &elem_params, NULL);
        if (ret)
        {
            PRINTF("Failed to add element DECODE\n");
            goto err;
        }

#if ((APP_CONFIG==4) || (APP_CONFIG==5))
        /* Add element jpeg decode on sink branch */
        memset(&elem_params, 0, sizeof(mpp_element_params_t));
        elem_params.decode.dev_name = IMG_DECODE_DEV_NAME;
        elem_params.decode.width = APP_CAMERA_WIDTH;
        elem_params.decode.height = APP_CAMERA_HEIGHT;

        if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_CPU") == 0)
            elem_params.decode.out_format = MPP_PIXEL_BGR; /* TODO auto detect */
        else if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_HW") == 0)
            elem_params.decode.out_format = MPP_PIXEL_YUYV; /* TODO auto detect */

        ret = mpp_element_add(sink_branch, MPP_ELEMENT_IMG_DECODE, &elem_params, NULL);
        if (ret)
        {
            PRINTF("Failed to add element DECODE\n");
            goto err;
        }
#endif
    }

#ifndef APP_SKIP_CONVERT_FOR_DISPLAY
    /* add convert element for color conversion and rotation
       as required by the display */
    mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = APP_GFX_BACKEND_NAME;
    /* set output buffer dims */
    elem_params.convert.out_buf.width = APP_DISPLAY_WIDTH;
    elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
    elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    elem_params.convert.flip = APP_SRC_DISPLAY_FLIP;
    elem_params.convert.pixel_format = args->display_format;
    elem_params.convert.angle = ROTATE_90;
    elem_params.convert.scale.width = APP_DISPLAY_WIDTH;
    elem_params.convert.scale.height = APP_DISPLAY_HEIGHT;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_ROTATE | MPP_CONVERT_SCALE;
    ret = mpp_element_add(display_branch, MPP_ELEMENT_CONVERT, &elem_params, NULL);
    if (ret) {
        PRINTF("Failed to add element CONVERT - op COLOR|ROTATE\n");
        goto err;
    }
#endif /* SKIP_CONVERT */

    /* add display */
    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.format = args->display_format;
    disp_params.width  = APP_DISPLAY_WIDTH;
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.stripe = stripe_mode;
#ifdef APP_SKIP_CONVERT_FOR_DISPLAY
    disp_params.rotate = APP_DISPLAY_LANDSCAPE_ROTATE;
#endif
    ret = mpp_display_add(display_branch, args->display_name, &disp_params);
    if (ret) {
        PRINTF("Failed to add display %s\n", args->display_name);
        goto err;
    }

    /* Add null-sink elements on the sink branches */
    ret = mpp_nullsink_add(sink_branch);
    if (ret) {
        PRINTF("Failed to add NULL sink\n");
        goto err;
    }

    /* start mpp and run application pipeline */
    ret = mpp_start(sink_branch, 0, false);
    if (ret) {
        PRINTF("Failed to start pipeline\n");
        goto err;
    }
    ret = mpp_start(display_branch, 1, false);
    if (ret) {
        PRINTF("Failed to start pipeline\n");
        goto err;
    }

    PRINTF("PIPELINE STARTED\r\n");

    /* run for 3 seconds  */
    vTaskDelay(3000/portTICK_PERIOD_MS);

    /* stop the pipeline */
    ret = mpp_stop(sink_branch);
    if (ret) {
        PRINTF("Failed to stop pipeline\n");
        goto err;
    }
    ret = mpp_stop(display_branch);
    if (ret) {
        PRINTF("Failed to stop pipeline\n");
        goto err;
    }

    PRINTF("PIPELINE STOPPED\r\n");

    /* wait 3 seconds */
    vTaskDelay(3000/portTICK_PERIOD_MS);

    /* restart the pipeline */
    ret = mpp_start(sink_branch, 0, false);
    if (ret) {
        PRINTF("Failed to start pipeline\n");
        goto err;
    }
    ret = mpp_start(display_branch, 0, true);
    if (ret) {
        PRINTF("Failed to restart pipeline\n");
        goto err;
    }

    PRINTF("PIPELINE RESTARTED\r\n");

    TickType_t xLastWakeTime;
#if ((APP_CONFIG == 4) || (APP_CONFIG == 5))
    TickType_t xFrequency = get_random_value(20, 200);
#else
    const TickType_t xFrequency = STREAM_CFG_CHANGE_PERIOD_MS / portTICK_PERIOD_MS;
#endif
    xLastWakeTime = xTaskGetTickCount();
#if STREAM_CFG_CHANGE
    mpp_element_params_t update_cam_params;
    memset(&update_cam_params, 0 , sizeof(update_cam_params));
#endif
    for (;;) {
        xTaskDelayUntil(&xLastWakeTime, xFrequency);

#if STREAM_CFG_CHANGE
#if (APP_CONFIG==0 || APP_CONFIG==3)
        update_cam_params.camera.n_streams = 2;
        update_cam_params.camera.stream[0].active = true;
        update_cam_params.camera.stream[0].type = (update_cam_params.camera.stream[0].type == RGB_STREAM) ? IR_STREAM : RGB_STREAM;
        update_cam_params.camera.stream[1].active = true;
        update_cam_params.camera.stream[1].type = (update_cam_params.camera.stream[1].type == RGB_STREAM) ? IR_STREAM : RGB_STREAM;
        update_cam_params.camera.in_advance_enqueue = CAMERA_IN_ADVANCE_ENQUEUE;

        if (mpp_element_update(mp, cam_elem, &update_cam_params, true) != MPP_SUCCESS)
        {
            PRINTF("FAILED to update camera parameters\r\n");
        }
        else
        {
            PRINTF("Changing stream configuration\r\n");
        }
#endif
#endif

#if (APP_CONFIG==4)
        PRINTF("Stop sink branch\r\n");
        mpp_stop(sink_branch);

        xTaskDelayUntil(&xLastWakeTime, get_random_value(2*APP_RC_CYCLE_MIN, 2*APP_RC_CYCLE_MIN + 200));

        PRINTF("Start sink branch\r\n");
        mpp_start(sink_branch, 0, false);

        xFrequency = get_random_value(2*APP_RC_CYCLE_MIN, 2*APP_RC_CYCLE_MIN + 200);
#endif

#if (APP_CONFIG==5)
        update_cam_params.camera.n_streams = 2;
        update_cam_params.camera.stream[0].active = true;
        update_cam_params.camera.stream[0].type = RGB_STREAM;
        update_cam_params.camera.stream[1].active = false;
        update_cam_params.camera.stream[1].type = IR_STREAM;
        update_cam_params.camera.in_advance_enqueue = CAMERA_IN_ADVANCE_ENQUEUE;

        if (mpp_element_update(display_branch, cam_elem, &update_cam_params, true) != MPP_SUCCESS)
        {
            PRINTF("FAILED to update camera parameters\r\n");
        }
        else
        {
            PRINTF("Stopping the sink pipeline branch \r\n");
        }

        mpp_stop(sink_branch);

        xTaskDelayUntil( &xLastWakeTime, get_random_value(2, 20));

        mpp_start(sink_branch, 0, true);

        PRINTF("Running sink branch with input camera stream inactive\r\n");

        xTaskDelayUntil( &xLastWakeTime, get_random_value(500, 1000));

        mpp_stop(sink_branch);

        update_cam_params.camera.n_streams = 2;
        update_cam_params.camera.stream[0].active = true;
        update_cam_params.camera.stream[0].type = RGB_STREAM;
        update_cam_params.camera.stream[1].active = true;
        update_cam_params.camera.stream[1].type = IR_STREAM;
        update_cam_params.camera.in_advance_enqueue = CAMERA_IN_ADVANCE_ENQUEUE;
        if (mpp_element_update(display_branch, cam_elem, &update_cam_params, true) != MPP_SUCCESS)
        {
            PRINTF("FAILED to update camera parameters\r\n");
        }
        else
        {
            PRINTF("Restarting the sink pipeline branch \r\n");
        }

        xTaskDelayUntil( &xLastWakeTime, get_random_value(2, 10));

        mpp_start(sink_branch, 0, false);

        xFrequency = get_random_value(200, 2000);
#endif
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

