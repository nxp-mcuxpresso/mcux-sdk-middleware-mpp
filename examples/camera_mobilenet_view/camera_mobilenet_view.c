/*
 * Copyright 2022-2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This example application shows usage of MultiMedia Pipeline to build a simple graph with 2 branches:
 * 2D camera -> split +-> image converter -> inference engine (model: MobileNet v1)
 *                    +-> image converter(*) -> draw labeled rectangle -> display
 * The camera view finder branch:
 * displays captured content on screen;
 * (*) image conversion may be skipped for cameras supporting same format as display;
 * the pipeline may deal with stripes of images instead of full-frame to reduce memory footprint;
 * this stripe mode can only be enabled when camera and display devices support it.
 * The inference engine branch:
 * performs a classification among a list of 1000 object types
 *(see models/mobilenet_v1_0.25_128_quant_int8_cm7/mobilenetv1_labels.h),
 * the pipeline is configured to run inference engine in a background task.
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
#include "app.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "mobilenetv1_output_postproc.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

typedef struct _user_data_t {
    int inference_frame_num;
    mpp_t mp;
    mpp_elem_handle_t elem;
    mpp_labeled_rect_t labels[1];
    mobilenet_post_proc_data_t inf_out;
    uint32_t accessing; /* boolean protecting access to user data */
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

/* Model data input */
#include APP_TFLITE_MOBILENET_DATA
/* Model info */
#include APP_TFLITE_MOBILENET_INFO

/*
 * SWAP_DIMS = 1 if source/display dims are reversed
 * SWAP_DIMS = 0 if source/display have the same orientation
 */
#ifdef APP_SKIP_CONVERT_FOR_DISPLAY
#define SWAP_DIMS 0
#else
#define SWAP_DIMS (((APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_90) || (APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_270)) ? 1 : 0)
#endif

/* display small and large dims */
#define DISPLAY_SMALL_DIM MIN(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT)
#define DISPLAY_LARGE_DIM MAX(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT)

#define RECT_LINE_WIDTH 2

#define MODEL_ASPECT_RATIO   (1.0f * MOBILENET_WIDTH / MOBILENET_HEIGHT)
/* output is displayed in landscape mode */
#define DISPLAY_ASPECT_RATIO (1.0f * DISPLAY_LARGE_DIM / DISPLAY_SMALL_DIM)
/* camera aspect ratio */
#define CAMERA_ASPECT_RATIO  (1.0f * APP_CAMERA_WIDTH / APP_CAMERA_HEIGHT)

/*
 * The detection zone is a rectangle centered on the display. It has the same shape as the model input.
 * The rectangle dimensions are calculated based on the display small dim and respecting the model aspect ratio
 * The detection zone width and height depend on the display_aspect_ratio compared to the model aspect_ratio:
 * if the display_aspect_ratio >= model_aspect_ratio then :
 *                  (width, height) = (display_small_dim * model_aspect_ratio, display_small_dim)
 * if the display_aspect_ratio < model_aspect_ratio then :
 *                  (width, height) = (display_small_dim, display_small_dim / model_aspect_ratio)
 *
 * */
#define DETECTION_ZONE_RECT_HEIGHT ((DISPLAY_ASPECT_RATIO >= MODEL_ASPECT_RATIO) ? \
		DISPLAY_SMALL_DIM : (DISPLAY_SMALL_DIM / MODEL_ASPECT_RATIO))
#define DETECTION_ZONE_RECT_WIDTH  ((DISPLAY_ASPECT_RATIO >= MODEL_ASPECT_RATIO) ? \
		(DISPLAY_SMALL_DIM * MODEL_ASPECT_RATIO) : DISPLAY_SMALL_DIM)

/* detection zone top/left offsets */
#define DETECTION_ZONE_RECT_TOP  (DISPLAY_SMALL_DIM - DETECTION_ZONE_RECT_HEIGHT)/2
#define DETECTION_ZONE_RECT_LEFT (DISPLAY_LARGE_DIM - DETECTION_ZONE_RECT_WIDTH)/2

/*
 *  The computation of the crop size(width and height) and the crop top/left depends on the detection
 *  zone dims and offsets and on the camera-display scaling factor SF which is calculated differently
 *  depending on 2 constraints:
 *           * Constraint 1: display aspect ratio compared to the camera aspect ratio.
 *           * Constraint 2: SWAP_DIMS value.
 * if the display_aspect_ratio < camera_aspect_ratio :
 *            - SWAP_DIMS = 0: SF = APP_DISPLAY_WIDTH / APP_CAMERA_WIDTH
 *            - SWAP_DIMS = 1: SF = APP_DISPLAY_HEIGHT / APP_CAMERA_HEIGHT
 * if the display_aspect_ratio >= camera_aspect_ratio:
 *            - SWAP_DIMS = 0: SF = APP_DISPLAY_HEIGHT / APP_CAMERA_HEIGHT
 *            - SWAP_DIMS = 1: SF = APP_DISPLAY_WIDTH / APP_CAMERA_WIDTH
 * the crop dims and offsets are calculated in the following way:
 * CROP_SIZE_TOP = DETECTION_ZONE_RECT_HEIGHT / SF
 * CROP_SIZE_LEFT = DETECTION_ZONE_RECT_WIDTH / SF
 * CROP_TOP = DETECTION_ZONE_RECT_HEIGHT / SF
 * CROP_LEFT = DETECTION_ZONE_RECT_LEFT / SF
 * */
#if ((DISPLAY_LARGE_DIM * APP_CAMERA_HEIGHT) < (DISPLAY_SMALL_DIM * APP_CAMERA_WIDTH))
#define CROP_SIZE_TOP   ((DETECTION_ZONE_RECT_HEIGHT * APP_CAMERA_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#define CROP_SIZE_LEFT  ((DETECTION_ZONE_RECT_WIDTH * APP_CAMERA_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))

#define CROP_TOP  ((DETECTION_ZONE_RECT_TOP * APP_CAMERA_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#define CROP_LEFT ((DETECTION_ZONE_RECT_LEFT * APP_CAMERA_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#else   /* DISPLAY_ASPECT_RATIO() >= CAMERA_ASPECT_RATIO() */
#define CROP_SIZE_TOP   ((DETECTION_ZONE_RECT_HEIGHT * APP_CAMERA_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#define CROP_SIZE_LEFT  ((DETECTION_ZONE_RECT_WIDTH * APP_CAMERA_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))

#define CROP_TOP  ((DETECTION_ZONE_RECT_TOP * APP_CAMERA_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#define CROP_LEFT ((DETECTION_ZONE_RECT_LEFT * APP_CAMERA_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#endif  /* DISPLAY_ASPECT_RATIO() < CAMERA_ASPECT_RATIO() */

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

    ret = xTaskCreate(
          app_task,
          "app_task",
          configMINIMAL_STACK_SIZE + 1000,
          NULL,
          APP_DEFAULT_PRIO,
          &handle);

    if (pdPASS != ret)
    {
        PRINTF("Failed to create app_task task");
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
        ret = MOBILENETv1_ProcessOutput(
                inf_output,
                app_priv->mp,
                app_priv->elem,
                app_priv->labels,
                &out_data);
        if (ret != kStatus_Success)
            PRINTF("mpp_event_listener: process output error!");
        /* check that we can modify the user data (not accessed by other task) */
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

static void app_task(void *params)
{
    static user_data_t user_data = {0};
    int ret;
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;

    PRINTF("[%s]\r\n", mpp_get_version());
    PRINTF("Inference Engine: TensorFlow-Lite Micro \r\n");

    mpp_api_params_t api_params = {0};
#if ((defined APP_RC_CYCLE_INC) && (defined APP_RC_CYCLE_MIN))
    /* fine-tune RC cycle for stripe mode */
    api_params.rc_cycle_inc = APP_RC_CYCLE_INC;
    api_params.rc_cycle_min = APP_RC_CYCLE_MIN;
#endif
    mpp_stats_t api_stats;
    memset(&api_stats, 0, sizeof(api_stats));
    api_params.stats = &api_stats;
    ret = mpp_api_init(&api_params);
    if (ret)
        goto err;

    mpp_t mp;
    mpp_stats_t mpp_stats;
    mpp_params_t mpp_params;
    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.evt_callback_f = &mpp_event_listener;
    mpp_params.mask = MPP_EVENT_ALL;
    mpp_params.cb_userdata = &user_data;
    mpp_params.exec_flag = MPP_EXEC_RC;
    mpp_params.stats = &mpp_stats;

    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
        goto err;

    mpp_camera_params_t cam_params;
    memset(&cam_params, 0 , sizeof(cam_params));
    cam_params.height = APP_CAMERA_HEIGHT;
    cam_params.width =  APP_CAMERA_WIDTH;
    cam_params.format = APP_CAMERA_FORMAT;
    cam_params.fps    = 30;
    cam_params.stripe = stripe_mode;
    ret = mpp_camera_add(mp, s_camera_name, &cam_params);
    if (ret) {
        PRINTF("Failed to add camera %s\n", s_camera_name);
        goto err;
    }

    /* split the pipeline into 2 branches */
    mpp_t mp_split;
    mpp_params.exec_flag = MPP_EXEC_INHERIT;
    ret = mpp_split(mp, 1 , &mpp_params, &mp_split);
    if (ret) {
        PRINTF("Failed to split pipeline\n");
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
    elem_params.convert.crop.bottom = CROP_TOP + CROP_SIZE_TOP - 1;
    elem_params.convert.crop.left = CROP_LEFT;
    elem_params.convert.crop.right = CROP_LEFT + CROP_SIZE_LEFT - 1;
    elem_params.convert.ops |= MPP_CONVERT_CROP;
    /* resize: scaling parameters */
    elem_params.convert.scale.width = MOBILENET_WIDTH;
    elem_params.convert.scale.height = MOBILENET_HEIGHT;
    elem_params.convert.ops |= MPP_CONVERT_SCALE;
    elem_params.convert.stripe_in = stripe_mode;
    elem_params.convert.stripe_out = false; /* model takes full frames */

    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);
    if (ret ) {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

    /* create a background mpp (preempt-able branch) for the ML Inference
     * because it may take longer than capture period.
     * Inference runs a MobilenetV1 model in TF-Lite format */
    mpp_t mp_bg;
    mpp_stats_t bg_stats;
    mpp_params.stats = &bg_stats;
    mpp_params.exec_flag = MPP_EXEC_PREEMPT;

    ret = mpp_background(mp, &mpp_params, &mp_bg);
    if (ret) {
        PRINTF("Failed to split pipeline\n");
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
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;

    mobilenet_params.ml_inference.inference_params.num_inputs = 1;
    mobilenet_params.ml_inference.inference_params.num_outputs = 1;
    mobilenet_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    mobilenet_params.stats = &mobilenet_stats;

    ret = mpp_element_add(mp_bg, MPP_ELEMENT_INFERENCE, &mobilenet_params, NULL);
    if (ret) {
        PRINTF("Failed to add element VALGO_TFLite");
        goto err;
    }
    /* close the pipeline with a null sink */
    ret = mpp_nullsink_add(mp_bg);
    if (ret) {
        PRINTF("Failed to add NULL sink\n");
        goto err;
    }

#ifndef APP_SKIP_CONVERT_FOR_DISPLAY
    /* On the secondary branch of the pipeline, send the frame to the display */
    /* First do color-convert + flip */
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    /* set output buffer dims */
    elem_params.convert.out_buf.width = APP_CAMERA_WIDTH;
    elem_params.convert.out_buf.height = APP_CAMERA_HEIGHT;
    elem_params.convert.pixel_format = APP_DISPLAY_FORMAT;
    elem_params.convert.flip = FLIP_HORIZONTAL;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_ROTATE;
    ret = mpp_element_add(mp_split, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    if (ret) {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }
#endif

    /* add one label rectangle */
    memset(&elem_params, 0, sizeof(elem_params));
    memset(&user_data.labels, 0, sizeof(user_data.labels));

    /* params init */
    elem_params.labels.max_count = 1;
    elem_params.labels.detected_count = 1;
    elem_params.labels.rectangles = user_data.labels;

    /* first add detection zone box */
    user_data.labels[0].top    = DETECTION_ZONE_RECT_TOP;
    user_data.labels[0].left   = DETECTION_ZONE_RECT_LEFT;
    user_data.labels[0].bottom = DETECTION_ZONE_RECT_TOP + DETECTION_ZONE_RECT_HEIGHT;
    user_data.labels[0].right  = DETECTION_ZONE_RECT_LEFT + DETECTION_ZONE_RECT_WIDTH;
    user_data.labels[0].line_width = RECT_LINE_WIDTH;
    user_data.labels[0].line_color.rgb.B = 0xff;
    strcpy((char *)user_data.labels[0].label, "no label");

    /* retrieve the element handle while add api */
    ret = mpp_element_add(mp_split, MPP_ELEMENT_LABELED_RECTANGLE, &elem_params, &user_data.elem);
    if (ret) {
        PRINTF("Failed to add element LABELED_RECTANGLE (0x%x)\r\n", ret);
        goto err;
    }

    /* pass the mpp of the element 'label rectangle' to callback */
    user_data.mp = mp_split;

#ifndef APP_SKIP_CONVERT_FOR_DISPLAY
    /* then rotate if needed */
    if (APP_DISPLAY_LANDSCAPE_ROTATE != ROTATE_0) {
    	memset(&elem_params, 0, sizeof(elem_params));
    	/* set output buffer dims */
    	elem_params.convert.out_buf.width = APP_DISPLAY_WIDTH;
    	elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
    	elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    	elem_params.convert.ops = MPP_CONVERT_ROTATE;
    	ret = mpp_element_add(mp_split, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    	if (ret) {
    		PRINTF("Failed to add element CONVERT\r\n");
    		goto err;
    	}
    }
#endif

    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.format = APP_DISPLAY_FORMAT;
    disp_params.width  = APP_DISPLAY_WIDTH;
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.stripe = stripe_mode;
#ifdef APP_SKIP_CONVERT_FOR_DISPLAY
    disp_params.rotate = APP_DISPLAY_LANDSCAPE_ROTATE;
#endif
    ret = mpp_display_add(mp_split, s_display_name, &disp_params);
    if (ret) {
        PRINTF("Failed to add display %s\n", s_display_name);
        goto err;
    }

    mpp_stats_enable(MPP_STATS_GRP_API);
    mpp_stats_enable(MPP_STATS_GRP_MPP);
    mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

    /* start preempt-able pipeline branch */
    ret = mpp_start(mp_bg, 0);
    if (ret) {
        PRINTF("Failed to start preempt-able pipeline branch");
        goto err;
    }
    /* start secondary pipeline branch */
    ret = mpp_start(mp_split, 0);
    if (ret) {
        PRINTF("Failed to start secondary pipeline branch");
        goto err;
    }
    /* start main pipeline branch */
    ret = mpp_start(mp, 1);
    if (ret) {
        PRINTF("Failed to start main pipeline branch");
        goto err;
    }

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = STATS_PRINT_PERIOD_MS / portTICK_PERIOD_MS;
    xLastWakeTime = xTaskGetTickCount();
#if (configGENERATE_RUN_TIME_STATS == 1)
    static char task_stats_buf[512];
#endif
    for (;;) {
        xTaskDelayUntil( &xLastWakeTime, xFrequency );
        mpp_stats_disable(MPP_STATS_GRP_API);
        mpp_stats_disable(MPP_STATS_GRP_MPP);
        mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
        PRINTF("API stats ------------------------------\r\n");
        PRINTF("rc_cycle = %u ms rc_cycle_max %u ms\r\n",
                api_stats.api.rc_cycle, api_stats.api.rc_cycle_max);
        PRINTF("pr_slot  = %u ms pr_rounds %u app_slot %u ms\r\n",
                api_stats.api.pr_slot, api_stats.api.pr_rounds, api_stats.api.app_slot);
        PRINTF("MPP stats ------------------------------\r\n");
        PRINTF("mpp %p exec_time %u ms\r\n", mpp_stats.mpp.mpp, mpp_stats.mpp.mpp_exec_time);
        PRINTF("mpp %p exec_time %u ms\r\n", bg_stats.mpp.mpp, bg_stats.mpp.mpp_exec_time);
        PRINTF("Element stats --------------------------\r\n");
        PRINTF("mobilenet : exec_time %u ms\r\n", mobilenet_stats.elem.elem_exec_time);
        if (Atomic_CompareAndSwap_u32(&user_data.accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            PRINTF("mobilenet : %s (%d%%)\r\n", user_data.inf_out.label, user_data.inf_out.score);
            __atomic_store_n(&user_data.accessing, 0, __ATOMIC_SEQ_CST);
        }
        mpp_stats_enable(MPP_STATS_GRP_MPP);
        mpp_stats_enable(MPP_STATS_GRP_API);
        mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

#if (configGENERATE_RUN_TIME_STATS == 1)
        vTaskGetRunTimeStats(task_stats_buf);
        PRINTF(task_stats_buf);
#endif
    }

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}

