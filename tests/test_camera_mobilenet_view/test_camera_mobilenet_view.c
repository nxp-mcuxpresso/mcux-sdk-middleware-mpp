/*
 * Copyright 2022-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application shows usage of MultiMedia Pipeline to build a simple graph:
 * 2D camera -> split -> image converter -> draw labeled rectangle -> display
 *                   +-> image converter -> inference engine (model: MobileNet v1)
 * The camera view finder is displayed on screen
 * The model performs classification among a list of 1000 object types
 *(see models/mobilenet_v1_0.25_128_quant_int8_cm7/mobilenetv1_labels.h),
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

#include "hal_utils.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "models/mobilenet_v1_0.25_128_quant_int8/mobilenetv1_output_postproc.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

typedef struct _user_data_t {
    int inference_frame_num;
    mpp_t mp;
    mpp_elem_handle_t elem;
    mpp_labeled_rect_t labels[1];
    mobilenet_post_proc_data_t inf_out;
    uint32_t accessing; /* boolean protecting access */
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

/* set this flag to 1 in order to replace the camera source by static image of a stopwatch */
#ifndef SOURCE_STATIC_IMAGE
#define SOURCE_STATIC_IMAGE 0
#endif

/* Use TensorFlowLite-Micro as an inference engine by default */
#if !defined(INFERENCE_ENGINE_DeepViewRT) && !defined(INFERENCE_ENGINE_GLOW)
#define INFERENCE_ENGINE_TFLM
#endif

/* Model data input (depends on inference engine) */
#include APP_TFLITE_MOBILENET_DATA
/* Model info */
#include APP_TFLITE_MOBILENET_INFO

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

/* display small & large dims */
#define DISPLAY_SMALL_DIM MIN(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT)
#define DISPLAY_LARGE_DIM MAX(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT)

#define RECT_LINE_WIDTH 2
#if (SOURCE_STATIC_IMAGE == 1)
#define TEST_MODE "AUTO"
#include "test_config.h"
#define SRC_WIDTH  SRC_IMAGE_WIDTH
#define SRC_HEIGHT SRC_IMAGE_HEIGHT
#else  /* SOURCE_STATIC_IMAGE != 1 */
#define TEST_MODE ""
#define SRC_WIDTH  APP_CAMERA_WIDTH
#define SRC_HEIGHT APP_CAMERA_HEIGHT
#endif  /* SOURCE_STATIC_IMAGE */

/* source large & small dims */
#define SRC_LARGE_DIM MAX(SRC_WIDTH,SRC_HEIGHT)
#define SRC_SMALL_DIM MIN(SRC_WIDTH,SRC_HEIGHT)

#define MODEL_ASPECT_RATIO   (1.0f * MOBILENET_WIDTH / MOBILENET_HEIGHT)
/* output is displayed in landscape mode */
#define DISPLAY_ASPECT_RATIO (1.0f * DISPLAY_LARGE_DIM / DISPLAY_SMALL_DIM)
/* camera aspect ratio */
#define CAMERA_ASPECT_RATIO  (1.0f * APP_CAMERA_WIDTH / APP_CAMERA_HEIGHT)

/* The detection zone is a rectangle that has the same shape as the model input.
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

/*
 * The detection zone offsets are defined in the following way:
 * The static image and its detection zone are placed in the top left corner of the display.
 * The camera output detection zone is placed in the middle of the display.
 *
 * */
#define DETECTION_ZONE_RECT_TOP (DISPLAY_SMALL_DIM - DETECTION_ZONE_RECT_HEIGHT)/2
/*
 * DETECTION_ZONE_RECT_LEFT = 0 if a static image is used as source
 * DETECTION_ZONE_RECT_LEFT = (DISPLAY_LARGE_DIM - DETECTION_ZONE_RECT_WIDTH)/2 if a camera is used as source
 */
#define DETECTION_ZONE_RECT_LEFT (SOURCE_STATIC_IMAGE ? 0 : ((DISPLAY_LARGE_DIM - DETECTION_ZONE_RECT_WIDTH)/2))

/*
 *  The computation of the crop size(width and height) and the crop top/left depends on the detection
 *  zone dims and offsets and on the source-display scaling factor SF which is calculated differently
 *  depending on 2 constraints:
 *           * Constraint 1: display aspect ratio compared to the source aspect ratio.
 *           * Constraint 2: SWAP_DIMS value.
 * if the display_aspect_ratio < source_aspect_ratio :
 *            - SWAP_DIMS = 0: SF = APP_DISPLAY_WIDTH / SRC_WIDTH
 *            - SWAP_DIMS = 1: SF = APP_DISPLAY_HEIGHT / SRC_HEIGHT
 * if the display_aspect_ratio >= source_aspect_ratio:
 *            - SWAP_DIMS = 0: SF = APP_DISPLAY_HEIGHT / SRC_HEIGHT
 *            - SWAP_DIMS = 1: SF = APP_DISPLAY_WIDTH / SRC_WIDTH
 * the crop dims and offsets are calculated in the following way:
 * CROP_SIZE_TOP = DETECTION_ZONE_RECT_HEIGHT / SF
 * CROP_SIZE_LEFT = DETECTION_ZONE_RECT_WIDTH / SF
 * CROP_TOP = DETECTION_ZONE_RECT_HEIGHT / SF
 * CROP_LEFT = DETECTION_ZONE_RECT_LEFT / SF
 * */
#if ((DISPLAY_LARGE_DIM * SRC_HEIGHT) < (DISPLAY_SMALL_DIM * SRC_WIDTH))
#define CROP_SIZE_TOP   ((DETECTION_ZONE_RECT_HEIGHT * SRC_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#define CROP_SIZE_LEFT  ((DETECTION_ZONE_RECT_WIDTH * SRC_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))

#define CROP_TOP  ((DETECTION_ZONE_RECT_TOP * SRC_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#define CROP_LEFT ((DETECTION_ZONE_RECT_LEFT * SRC_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#else   /* DISPLAY_ASPECT_RATIO() >= SOURCE_ASPECT_RATIO() */
#define CROP_SIZE_TOP   ((DETECTION_ZONE_RECT_HEIGHT * SRC_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#define CROP_SIZE_LEFT  ((DETECTION_ZONE_RECT_WIDTH * SRC_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))

#define CROP_TOP  ((DETECTION_ZONE_RECT_TOP * SRC_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#define CROP_LEFT ((DETECTION_ZONE_RECT_LEFT * SRC_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#endif  /* DISPLAY_ASPECT_RATIO() < SOURCE_ASPECT_RATIO() */

static const char s_display_name[] = APP_DISPLAY_NAME;
#if (SOURCE_STATIC_IMAGE != 1)
static const char s_camera_name[] = APP_CAMERA_NAME;
#endif

#define STATS_PRINT_PERIOD_MS 1000

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void app_task(void *params);

static int check_mobilenet_inference_output(mobilenet_post_proc_data_t* out_data);
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

    PRINTF("****** %s TEST test_camera_mobilenet_view ******\r\n", TEST_MODE);
    PRINTF("****** PARAMS: INPUT_SOURCE = [%s] ******\r\n",
            (SOURCE_STATIC_IMAGE)?"STATIC_IMAGE":"CAMERA");
#if defined(INFERENCE_ENGINE_TFLM)
    PRINTF("---INFERENCE ENGINE: TFLITE---\r\n");
#elif defined(INFERENCE_ENGINE_GLOW)
    PRINTF("---INFERENCE ENGINE: GLOW---\r\n");
#elif defined(INFERENCE_ENGINE_DeepViewRT)
    PRINTF("---INFERENCE ENGINE: DeepViewRT---\r\n");
#endif
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

/* output :
 *  1: test passed
 * -1: test failed
 *  0: cannot state
 */
static int check_mobilenet_inference_output(mobilenet_post_proc_data_t* out_data)
{
    int status = 0;
#if (SOURCE_STATIC_IMAGE == 1)
    if(EXPECTED_LABEL != NULL)
    {
        if (out_data->label == NULL)
            status = 0;
        else
        {
            if(strcmp(out_data->label, EXPECTED_LABEL) != 0)
            {
                PRINTF("\r\nERROR: expected label '%s', but got label '%s'\r\n", EXPECTED_LABEL, out_data->label);
                status = -1;
            }
            else if (out_data->score < EXPECTED_CONFIDENCE_MIN)
            {
                PRINTF("\r\nERROR: expected confidence score > %d, but got %d\r\n", EXPECTED_CONFIDENCE_MIN, out_data->score);
                status = -1;
            }
            else
                status = 1;
        }
    }
#endif
    return status;
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {
    status_t ret;
    const mpp_inference_cb_param_t *inf_output;
    mobilenet_post_proc_data_t out_data;
#if (SOURCE_STATIC_IMAGE == 1)
    checksum_data_t *chksm;
#endif
    /* user_data handle contains application private data */
    user_data_t *app_priv = (user_data_t *)user_data;
    /* statistics to be printed from application task*/
    int infer_check;
    static bool chksm_ok = false;
    static bool chksm_done = false;

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
            PRINTF("mpp_event_listener: process output error!\r\n");

        infer_check = check_mobilenet_inference_output(&out_data);
        if (chksm_done)
        {
            if (infer_check == 1 && chksm_ok)
                PRINTF("\r\nTEST PASS\r\n");
            else if(infer_check == -1 || !chksm_ok)
                PRINTF("\r\nTEST FAIL\r\n");
            else /* infer_check == 0 */
            {;/* do nothing */}
        }

        /* check that we can modify the stats buffer (not accessed by other task) */
        if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            /* copy inference output */
            app_priv->inf_out = out_data;
           /* end of modification of user data */
            __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
        }
        app_priv->inference_frame_num++;
        break;
    case MPP_EVENT_INTERNAL_TEST_RESERVED:
#if (SOURCE_STATIC_IMAGE == 1)
        chksm = (checksum_data_t *)evt_data;
        if (chksm == NULL) {
            return 0;
        }
        if (chksm->type != CHECKSUM_TYPE_CRC_ELCDIF) {
            PRINTF("ERROR: checksum calculated should be using CRC LCDIF\n");
            return 0;
        }
        /* wait for 2nd inference to update label */
        if (app_priv->inference_frame_num > 1 && !chksm_done )
        {
            chksm_done = true;
            chksm_ok = (chksm->value == EXPECTED_CHECKSUM);
            if (!chksm_ok)  PRINTF("\r\nBad checksum 0x%08x\r\n", chksm->value);
        }
#endif
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
    img_params.width  = SRC_IMAGE_WIDTH;
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
    mpp_t mp_split;
    mpp_params.exec_flag = MPP_EXEC_PREEMPT;
    ret = mpp_split(mp, 1 , &mpp_params, &mp_split);
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
    elem_params.convert.crop.bottom = CROP_TOP + CROP_SIZE_TOP - 1;
    elem_params.convert.crop.left = CROP_LEFT;
    elem_params.convert.crop.right = CROP_LEFT + CROP_SIZE_LEFT - 1;
    elem_params.convert.ops |= MPP_CONVERT_CROP;
    /* resize: scaling parameters */
    elem_params.convert.scale.width = MOBILENET_WIDTH;
    elem_params.convert.scale.height = MOBILENET_HEIGHT;
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

#if defined(INFERENCE_ENGINE_TFLM)
    mobilenet_params.ml_inference.model_data = mobilenet_data;
    mobilenet_params.ml_inference.model_size = mobilenet_data_len;
    mobilenet_params.ml_inference.model_input_mean = MOBILENET_INPUT_MEAN;
    mobilenet_params.ml_inference.model_input_std = MOBILENET_INPUT_STD;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
#elif defined(INFERENCE_ENGINE_GLOW)
    mobilenet_params.ml_inference.model_data = mobilenet_v1_weights_bin;
    mobilenet_params.ml_inference.inference_params.constant_weight_MemSize = MOBILENET_V1_CONSTANT_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.mutable_weight_MemSize = MOBILENET_V1_MUTABLE_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.activations_MemSize = MOBILENET_V1_ACTIVATIONS_MEM_SIZE;
    mobilenet_params.ml_inference.inference_params.num_inputs = 1;
    mobilenet_params.ml_inference.inference_params.inputs_offsets[0] = MOBILENET_V1_input;
    mobilenet_params.ml_inference.inference_params.outputs_offsets[0] = MOBILENET_V1_MobilenetV1_Predictions_Reshape_1;
    mobilenet_params.ml_inference.inference_params.model_input_tensors_type = MPP_TENSOR_TYPE_INT8;
    mobilenet_params.ml_inference.inference_params.model_entry_point = &mobilenet_v1;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_GLOW ;
#elif defined(INFERENCE_ENGINE_DeepViewRT)
    mobilenet_params.ml_inference.model_data = mobilenet_data;
    mobilenet_params.ml_inference.model_size = mobilenet_data_len;
    mobilenet_params.ml_inference.type = MPP_INFERENCE_TYPE_DEEPVIEWRT ;
#endif

    mobilenet_params.ml_inference.inference_params.num_inputs = 1;
    mobilenet_params.ml_inference.inference_params.num_outputs = 1;
    mobilenet_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    mobilenet_params.stats = &mobilenet_stats;

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

    /* On the main branch of the pipeline, send the frame to the display */
    /* First do color-convert + flip */
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;
    /* set output buffer dims */
    elem_params.convert.out_buf.width =  (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH);
    elem_params.convert.out_buf.height = (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT);
    elem_params.convert.pixel_format = APP_DISPLAY_FORMAT;
    /* scaling parameters */
    if ((DISPLAY_LARGE_DIM * SRC_HEIGHT) < (DISPLAY_SMALL_DIM * SRC_WIDTH)) {
    	elem_params.convert.scale.width =  (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH);
    	elem_params.convert.scale.height = (SWAP_DIMS ? (APP_DISPLAY_HEIGHT * SRC_HEIGHT / SRC_WIDTH) :
    			(APP_DISPLAY_WIDTH * SRC_HEIGHT / SRC_WIDTH));
    } else {
    	elem_params.convert.scale.height = (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT);
    	elem_params.convert.scale.width  = (SWAP_DIMS ? (APP_DISPLAY_WIDTH * SRC_WIDTH / SRC_HEIGHT) :
    			(APP_DISPLAY_HEIGHT * SRC_WIDTH / SRC_HEIGHT));
    }

    elem_params.convert.flip = SRC_DISPLAY_FLIP;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_ROTATE | MPP_CONVERT_SCALE;

    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    if (ret) {
        PRINTF("Failed to add element CONVERT\r\n");
        goto err;
    }

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
    ret = mpp_element_add(mp, MPP_ELEMENT_LABELED_RECTANGLE, &elem_params, &user_data.elem);
    if (ret) {
        PRINTF("Failed to add element LABELED_RECTANGLE (0x%x)\r\n", ret);
        goto err;
    }

    /* then rotate if needed */
    if (APP_DISPLAY_LANDSCAPE_ROTATE != ROTATE_0) {
    	memset(&elem_params, 0, sizeof(elem_params));
    	/* set output buffer dims */
    	elem_params.convert.out_buf.width = APP_DISPLAY_WIDTH;
    	elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
    	elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    	elem_params.convert.ops = MPP_CONVERT_ROTATE;
    	ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    	if (ret) {
    		PRINTF("Failed to add element CONVERT\r\n");
    		goto err;
    	}
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
    ret = mpp_start(mp_split, 0);
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
#if (configGENERATE_RUN_TIME_STATS == 1)
    static char task_stats_buf[512];
#endif
    for (;;) {
        xTaskDelayUntil( &xLastWakeTime, xFrequency );
        mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
        PRINTF("Element stats --------------------------\r\n");
        PRINTF("mobilenet : exec_time %u (ms)\r\n", mobilenet_stats.elem.elem_exec_time);
        mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

        if (Atomic_CompareAndSwap_u32(&user_data.accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            PRINTF("mobilenet : %s (%d%%)\r\n", user_data.inf_out.label, user_data.inf_out.score);
            __atomic_store_n(&user_data.accessing, 0, __ATOMIC_SEQ_CST);
        }
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

