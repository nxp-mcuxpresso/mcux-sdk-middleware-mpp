/*
* Copyright 2025-2026 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/* @brief This application tests the mobilefacenet accuracy on multiple images
* Static image -> split -> image converter -> draw labeled rectangle -> display
*                   +-> image converter -> inference engine (model: mobilefacenet )
* The view finder is displayed on screen
* The model outputs embeddings describing the input face
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
#include "mpp_api_types_internal.h"

/* Test includes */
#include "test_config.h"

/* Model data input */
#include APP_TFLITE_MOBILEFACENET_DATA

/* Persons database */
#include APP_DATABASE_NAME

/* Model output post-processing */
#include "mobilefacenet_output_postproc_quantized.h"

/* database utils */
#include "models/database_utils.h"

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
/* label rect line width */
#define RECT_LINE_WIDTH 2

/* pick default backend if not specified */
#ifndef APP_GFX_BACKEND_NAME
#define APP_GFX_BACKEND_NAME NULL
#endif

/* pick CPU for image decoding by default */
#ifndef IMG_DECODE_DEV_NAME
#define IMG_DECODE_DEV_NAME "jpeg_CPU"
#endif

/*
 * SWAP_DIMS = 1 if source/display dims are reversed
 * SWAP_DIMS = 0 if source/display have the same orientation
 */
#define SWAP_DIMS (((APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_90) || (APP_DISPLAY_LANDSCAPE_ROTATE == ROTATE_270)) ? 1 : 0)

/* display small and large dims */
#define DISPLAY_SMALL_DIM MIN(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT)
#define DISPLAY_LARGE_DIM MAX(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT)

/*
 * The detection zone is a rectangle that has the same shape as the model input.
 * The rectangle dimensions are calculated based on the display small dim and respecting the model aspect ratio
 * The detection zone width and height depend on the display_aspect_ratio compared to the model aspect_ratio:
 * if the display_aspect_ratio >= model_aspect_ratio then :
 *                  (width, height) = (display_small_dim * model_aspect_ratio, display_small_dim)
 * if the display_aspect_ratio < model_aspect_ratio then :
 *                  (width, height) = (display_small_dim, display_small_dim / model_aspect_ratio)
 *
 * */
#define DETECTION_ZONE_RECT_HEIGHT (((DISPLAY_LARGE_DIM * MOBILEFACENET_HEIGHT) >= (DISPLAY_SMALL_DIM * MOBILEFACENET_WIDTH)) ? \
                (DISPLAY_SMALL_DIM - RECT_LINE_WIDTH) : ((DISPLAY_SMALL_DIM - RECT_LINE_WIDTH) * MOBILEFACENET_HEIGHT / MOBILEFACENET_WIDTH))
#define DETECTION_ZONE_RECT_WIDTH  (((DISPLAY_LARGE_DIM * MOBILEFACENET_HEIGHT) >= (DISPLAY_SMALL_DIM * MOBILEFACENET_WIDTH)) ? \
                ((DISPLAY_SMALL_DIM - RECT_LINE_WIDTH) * MOBILEFACENET_WIDTH / MOBILEFACENET_HEIGHT) : (DISPLAY_SMALL_DIM - RECT_LINE_WIDTH))

/* detection zone top/left offsets */
#define DETECTION_ZONE_RECT_TOP  (DISPLAY_SMALL_DIM - DETECTION_ZONE_RECT_HEIGHT)/2
#define DETECTION_ZONE_RECT_LEFT 0

/*
 *  The computation of the crop size(width and height) and the crop top/left depends on the detection
 *  zone dims and offsets and on the source-display scaling factor SF which is calculated differently
 *  depending on 2 constraints:
 *           * Constraint 1: display aspect ratio compared to the source aspect ratio.
 *           * Constraint 2: SWAP_DIMS value.
 * if the display_aspect_ratio < source_aspect_ratio :
 *            - SWAP_DIMS = 0: SF = APP_DISPLAY_WIDTH / SRC_IMAGE_WIDTH
 *            - SWAP_DIMS = 1: SF = APP_DISPLAY_HEIGHT / SRC_IMAGE_HEIGHT
 * if the display_aspect_ratio >= source_aspect_ratio:
 *            - SWAP_DIMS = 0: SF = APP_DISPLAY_HEIGHT / SRC_IMAGE_HEIGHT
 *            - SWAP_DIMS = 1: SF = APP_DISPLAY_WIDTH / SRC_IMAGE_WIDTH
 * the crop dims and offsets are calculated in the following way:
 * CROP_SIZE_TOP = DETECTION_ZONE_RECT_HEIGHT / SF
 * CROP_SIZE_LEFT = DETECTION_ZONE_RECT_WIDTH / SF
 * CROP_TOP = DETECTION_ZONE_RECT_HEIGHT / SF
 * CROP_LEFT = DETECTION_ZONE_RECT_LEFT / SF
 * */
#if ((DISPLAY_LARGE_DIM * SRC_IMAGE_HEIGHT) < (DISPLAY_SMALL_DIM * SRC_IMAGE_WIDTH))
#define CROP_SIZE_TOP   ((DETECTION_ZONE_RECT_HEIGHT * SRC_IMAGE_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#define CROP_SIZE_LEFT  ((DETECTION_ZONE_RECT_WIDTH * SRC_IMAGE_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))

#define CROP_TOP  ((DETECTION_ZONE_RECT_TOP * SRC_IMAGE_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#define CROP_LEFT ((DETECTION_ZONE_RECT_LEFT * SRC_IMAGE_WIDTH) / (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH))
#else   /* DISPLAY_ASPECT_RATIO() >= SOURCE_ASPECT_RATIO() */
#define CROP_SIZE_TOP   ((DETECTION_ZONE_RECT_HEIGHT * SRC_IMAGE_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#define CROP_SIZE_LEFT  ((DETECTION_ZONE_RECT_WIDTH * SRC_IMAGE_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))

#define CROP_TOP  ((DETECTION_ZONE_RECT_TOP * SRC_IMAGE_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#define CROP_LEFT ((DETECTION_ZONE_RECT_LEFT * SRC_IMAGE_HEIGHT) / (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT))
#endif  /* DISPLAY_ASPECT_RATIO() < SOURCE_ASPECT_RATIO() */

/* Detected boxes offsets */
#define BOXES_OFFSET_LEFT DETECTION_ZONE_RECT_LEFT
#define BOXES_OFFSET_TOP  DETECTION_ZONE_RECT_TOP

#define OUTPUT_PRINT_PERIOD_MS 900

/* Frequency of changing source image in ms */
#define IMG_CHANGE_FREQ 2000

static const char s_display_name[] = APP_DISPLAY_NAME;

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_multi_image_mobilefacenet_view_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_multi_image_mobilefacenet_view"
#endif

#if PERSON_REGISTRATION == 1
#define PERSON_RECOGNIZED_THRESHOLD     99
#else
#define PERSON_RECOGNIZED_THRESHOLD     88
#endif

/* Set how many test iterations to run. Set to 0 to run continuously */
#ifndef TEST_ITERATIONS
#define TEST_ITERATIONS     0
#endif

/*******************************************************************************
* Definitions
******************************************************************************/
typedef enum _test_state_e {
    REGISTRATION_STATE,
    RECOGNITION_STATE
} test_state_e;

typedef struct _user_data_t {
    int inference_frame_num;
    mpp_t mp;
    mpp_elem_handle_t elem;
    mpp_labeled_rect_t labels[1];
    recognition_result result;
    uint32_t accessing; /* boolean protecting access to user data */
    int inference_time_ms;
    struct _test_data_t {
        test_state_e state;
        bool result;
        bool finished;
        uint32_t iterations;
        bool session_result;
    } test;
} user_data_t;

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

uint32_t go_to_next_image(uint32_t crt_image_idx, mpp_t mp, mpp_elem_handle_t elem, char* image_name, user_data_t *user_data)
{
    uint32_t next_image_idx;
    uint32_t total_image_size = ARRAY_SIZE(img_list);
    mpp_element_params_t img_params;
    int ret;

    memset(&img_params, 0, sizeof(img_params));

    /* Set next image idx */
    next_image_idx = (crt_image_idx + 1) % total_image_size;

    /* Copy image params to static image element params */
    memcpy((void *) &img_params.static_image.img_params, &img_list[next_image_idx].img_params, sizeof(mpp_img_params_t));
    img_params.static_image.img_buffer = img_list[next_image_idx].img_data;
    strcpy(image_name, img_list[next_image_idx].img_name);

    PRINTF("Change image to %s\r\n", img_list[next_image_idx].img_name);

    ret = mpp_element_update(mp, elem, &img_params, true);

    if (ret!= MPP_SUCCESS)
        PRINTF("[ERR]: Got error while trying to update static image element\r\n");

    if (next_image_idx == 0)
    {
        /* We have looped thorugh all images, change the test state and set test.finished flag if needed */
#if PERSON_REGISTRATION == 1
        if (user_data->test.state == REGISTRATION_STATE)
        {
            user_data->test.state = RECOGNITION_STATE;
        }
        else
        {
#ifdef CLEAR_DATABASE_AFTER_EACH_ITERATION
            user_data->test.state = REGISTRATION_STATE;
#endif
            user_data->test.finished = true;
        }
#else
        user_data->test.state = RECOGNITION_STATE;
        user_data->test.finished = true;
#endif
    }

    return next_image_idx;
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {
    const mpp_inference_cb_param_t *inf_output;
    static recognition_result result;

    /* user_data handle contains application private data */
    user_data_t *app_priv = (user_data_t *)user_data;

    switch(evt) {
    case MPP_EVENT_INFERENCE_OUTPUT_READY:
        /* cast evt_data pointer to correct structure matching the event */
        inf_output = (const mpp_inference_cb_param_t *) evt_data;
        MOBILEFACENET_ProcessOutput(
                inf_output,
                g_embedding_db,
                DATABASE_MAX_SIZE,
                &result,
                0.0f);
        /* check that we can modify the user data (not accessed by other task) */
        if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
        {
            app_priv->inference_time_ms = inf_output->inference_time_ms;
            app_priv->inference_frame_num++;
            /* copy recognition results */
            app_priv->result = result;

            mpp_element_params_t params;
            memset(&params, 0, sizeof(params));
            uint8_t label_size = sizeof(params.labels.rectangles[0].label);
            // Update the label in the first rectangle
            params.labels.detected_rect = 1;
            params.labels.max_rect = 1;
            params.labels.rectangles = app_priv->labels;
            /* update recognition label */
            if (app_priv->result.similarity_percentage > 0) {
                strncpy((char *)params.labels.rectangles[0].label, app_priv->result.recognized_name, label_size);
            } else {
                strcpy(app_priv->result.recognized_name,"\0");
                strncpy((char *)params.labels.rectangles[0].label, "Face not recognized", label_size);
            }
            params.labels.rectangles[0].label[label_size - 1] = '\0';
            if ( (app_priv->elem != 0) && ( app_priv->mp != NULL ) ) {
                mpp_element_update(app_priv->mp, app_priv->elem, &params, true);
            }
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
    int ret;
    uint32_t crt_image_idx = 0;
    char crt_image_name[MAX_NAME_SIZE+1];
    mpp_elem_handle_t static_img_elem;

    PRINTF("[%s]\r\n", mpp_get_version());
    PRINTF("Inference Engine: TensorFlow-Lite Micro \r\n");

    /* fix max pipeline task priority. */
    static mpp_api_params_t api_params;
    api_params.pipeline_task_max_prio = APP_PIPELINE_TASK_MAX_PRIO;

    init_database(g_embedding_db);

    ret = mpp_api_init(&api_params);
    if (ret)
        goto err;

    static mpp_t mp;
    static mpp_params_t mpp_params;
    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.evt_callback_f = &mpp_event_listener;
    mpp_params.mask = MPP_EVENT_ALL;
    mpp_params.cb_userdata = &user_data;
    mpp_params.exec_flag = MPP_EXEC_RC;

    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
        goto err;

    user_data.mp = mp;

    static mpp_img_params_t img_params;
    memset(&img_params, 0, sizeof (mpp_img_params_t));
    img_params.format = img_list[crt_image_idx].img_params.format;
    img_params.width = img_list[crt_image_idx].img_params.width;
    img_params.height = img_list[crt_image_idx].img_params.height;
	img_params.compressed_size = img_list[crt_image_idx].img_params.compressed_size;
    strcpy(crt_image_name, img_list[crt_image_idx].img_name);
    mpp_static_img_add(mp, &img_params, img_list[crt_image_idx].img_data, &static_img_elem);

    /* Add element jpeg decode */
    mpp_element_params_t jpeg_params;
    memset(&jpeg_params, 0, sizeof(mpp_element_params_t));
    jpeg_params.decode.dev_name = IMG_DECODE_DEV_NAME;
    jpeg_params.decode.width = SRC_IMAGE_WIDTH;
    jpeg_params.decode.height = SRC_IMAGE_HEIGHT;
    jpeg_params.decode.out_format = MPP_PIXEL_BGR; /* TODO auto detect */
    ret = mpp_element_add(mp, MPP_ELEMENT_IMG_DECODE, &jpeg_params, NULL);
    if (ret)
    {
        PRINTF("Failed to add element DECODE\n");
        goto err;
    }

    // split the pipeline into 2 branches
    static mpp_t mp_split;
    mpp_params.exec_flag = MPP_EXEC_PREEMPT;
    ret = mpp_split(mp, 1, &mpp_params, &mp_split);
    if (ret) {
        PRINTF("Failed to split pipeline\n");
        goto err;
    }

    /* On the preempt-able branch run the ML Inference (using an Mobilefacenet TFLite model) */
    /* First do crop + resize + color convert */
    mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(elem_params));

    // First do color-convert
    memset(&elem_params, 0, sizeof(elem_params));
    // pick default device from the first listed and supported by Hw
    elem_params.convert.dev_name = APP_GFX_BACKEND_NAME;
    // set output buffer dims
    elem_params.convert.out_buf.width = MOBILEFACENET_WIDTH;
    elem_params.convert.out_buf.height = MOBILEFACENET_HEIGHT;
    // resize: scaling parameters
    elem_params.convert.scale.width = MOBILEFACENET_WIDTH;
    elem_params.convert.scale.height = MOBILEFACENET_HEIGHT;
    elem_params.convert.ops |= MPP_CONVERT_SCALE;

    ret = mpp_element_add(mp_split, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    if (ret) {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

    // configure TFlite element with model
    static mpp_element_params_t mobilefacenet_params;
    static mpp_stats_t mobilefacenet_stats;
    memset(&mobilefacenet_params, 0 , sizeof(mpp_element_params_t));

    mobilefacenet_params.ml_inference.model_data = mobilefacenet_data;
    mobilefacenet_params.ml_inference.model_size = mobilefacenet_data_len;
    mobilefacenet_params.ml_inference.model_input_mean = MOBILEFACENET_INPUT_MEAN;
    mobilefacenet_params.ml_inference.model_input_std = MOBILEFACENET_INPUT_STD;
    mobilefacenet_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    mobilefacenet_params.ml_inference.inference_params.num_inputs = 1;
    mobilefacenet_params.ml_inference.inference_params.num_outputs = 1;
    mobilefacenet_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    mobilefacenet_params.stats = &mobilefacenet_stats;

    ret = mpp_element_add(mp_split, MPP_ELEMENT_INFERENCE, &mobilefacenet_params, NULL);
    if (ret) {
        PRINTF("Failed to add element VALGO_TFLite");
        goto err;
    }
    // close the pipeline with a null sink
    ret = mpp_nullsink_add(mp_split);
    if (ret) {
        PRINTF("Failed to add NULL sink\n");
        goto err;
    }

    // On the main branch of the pipeline, send the frame to the display
    // First do color-convert + flip
    memset(&elem_params, 0, sizeof(elem_params));
    // pick default device from the first listed and supported by Hw.
    elem_params.convert.dev_name = APP_GFX_BACKEND_NAME;
    // set output buffer dims
    elem_params.convert.out_buf.width = (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH);
    elem_params.convert.out_buf.height = (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT);
    elem_params.convert.pixel_format = APP_DISPLAY_FORMAT;
    elem_params.convert.ops = MPP_CONVERT_COLOR;
    /* scaling parameters */
    if ((DISPLAY_LARGE_DIM * SRC_IMAGE_HEIGHT) < (DISPLAY_SMALL_DIM * SRC_IMAGE_WIDTH)) {
        elem_params.convert.scale.width =  (SWAP_DIMS ? APP_DISPLAY_HEIGHT : APP_DISPLAY_WIDTH);
        elem_params.convert.scale.height = (SWAP_DIMS ? (APP_DISPLAY_HEIGHT * SRC_IMAGE_HEIGHT / SRC_IMAGE_WIDTH) :
                (APP_DISPLAY_WIDTH * SRC_IMAGE_HEIGHT / SRC_IMAGE_WIDTH));
    } else {
        elem_params.convert.scale.height = (SWAP_DIMS ? APP_DISPLAY_WIDTH : APP_DISPLAY_HEIGHT);
        elem_params.convert.scale.width  = (SWAP_DIMS ? (APP_DISPLAY_WIDTH * SRC_IMAGE_WIDTH / SRC_IMAGE_HEIGHT) :
                (APP_DISPLAY_HEIGHT * SRC_IMAGE_WIDTH / SRC_IMAGE_HEIGHT));
    }

    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_SCALE;

    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);

    if (ret) {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

    // add one label rectangle
    memset(&elem_params, 0, sizeof(elem_params));
    memset(&user_data.labels, 0, sizeof(user_data.labels));

    // params init
    elem_params.labels.max_rect = 1;
    elem_params.labels.detected_rect = 1;
    elem_params.labels.rectangles = user_data.labels;

    // first add detection zone box
    user_data.labels[0].top    = DETECTION_ZONE_RECT_TOP;
    user_data.labels[0].left   = DETECTION_ZONE_RECT_LEFT;
    user_data.labels[0].bottom = DETECTION_ZONE_RECT_TOP + DETECTION_ZONE_RECT_HEIGHT;
    user_data.labels[0].right  = DETECTION_ZONE_RECT_LEFT + DETECTION_ZONE_RECT_WIDTH;
    user_data.labels[0].line_width = RECT_LINE_WIDTH;
    user_data.labels[0].line_color.rgb.B = 0xff;
    strcpy((char *)user_data.labels[0].label, "Face not recognized");

    // retrieve the element handle while add api
    ret = mpp_element_add(mp, MPP_ELEMENT_LABELED_RECTANGLE, &elem_params, &user_data.elem);
    if (ret) {
        PRINTF("Failed to add element LABELED_RECTANGLE (0x%x)\r\n", ret);
        goto err;
    }

    /* then rotate if needed */
    if (APP_DISPLAY_LANDSCAPE_ROTATE != ROTATE_0) {
        memset(&elem_params, 0, sizeof(elem_params));
        // pick device selected in mpp_config.
        elem_params.convert.dev_name = APP_GFX_BACKEND_NAME;
        // set output buffer dims
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

    static mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.format = APP_DISPLAY_FORMAT;
    disp_params.width  = APP_DISPLAY_WIDTH;
    disp_params.height = APP_DISPLAY_HEIGHT;
    ret = mpp_display_add(mp, s_display_name, &disp_params);
    if (ret) {
        PRINTF("Failed to add display %s\n", s_display_name);
        goto err;
    }

    mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

    // start preempt-able pipeline branch
    ret = mpp_start(mp_split, 0, false);
    if (ret) {
        PRINTF("Failed to start pipeline");
        goto err;
    }

    // start main pipeline branch
    ret = mpp_start(mp, 1, false);
    if (ret) {
        PRINTF("Failed to start pipeline");
        goto err;
    }

    TickType_t x_last_awake_time;
    const TickType_t x_frequency = OUTPUT_PRINT_PERIOD_MS / portTICK_PERIOD_MS;
    const TickType_t image_change_freq = IMG_CHANGE_FREQ / portTICK_PERIOD_MS;
    x_last_awake_time = xTaskGetTickCount();
    uint32_t last_inf_frame_num = user_data.inference_frame_num;

#if PERSON_REGISTRATION == 1
    user_data.test.state = REGISTRATION_STATE;
#else
    user_data.test.state = RECOGNITION_STATE;
#endif
    user_data.test.result = true;
    user_data.test.finished = false;
    user_data.test.session_result = true;
    user_data.test.iterations = 0;

    PRINTF("\r\nStart %s\r\n", TC_NAME);

    for (;;) {
        xTaskDelayUntil( &x_last_awake_time, x_frequency );
        if (last_inf_frame_num != user_data.inference_frame_num)
        {
            mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
            PRINTF("Element stats --------------------------\r\n");
            PRINTF("mobilefacenet : exec_time %u (ms)\r\n", mobilefacenet_stats.elem.elem_exec_time);
            mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

            if (Atomic_CompareAndSwap_u32(&user_data.accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS)
            {
                PRINTF("inference time %d (ms) \r\n", user_data.inference_time_ms);

                if (user_data.result.recognized_name[0]=='\0')
                {
                    /* Stop the pipeline to register the person */
                    mpp_stop(mp_split);
                    
                    if (user_data.test.state != REGISTRATION_STATE)
                    {
                        user_data.test.result &= false;
                        PRINTF("[ERR]: Trying to register person when not in REGISTRATION_STATE. ");
                        PRINTF("Current state is %d\r\n", user_data.test.state);
                    }
                    else 
                    {
                        PRINTF("face not recognized. Registering it ...\r\n");
                        /* Register the person */
                        set_new_face_embeddings((const float *)user_data.result.embedding);
                        if (!database_add(crt_image_name))
                        {
                            PRINTF("[ERR] - Got error while trying to add a new person to the database");
                            goto err;
                        }
                    }
                    /* after reading, inference output should be cleared */
                    strcpy(user_data.result.recognized_name,"\0");
                    user_data.result.similarity_percentage = 0;
                    /* Wait some time before changing the image */
                    xTaskDelayUntil( &x_last_awake_time, image_change_freq );
                    /* Change the image */
                    crt_image_idx = go_to_next_image(crt_image_idx, mp, static_img_elem, crt_image_name, &user_data);
                    /* Wait some time before starting the inference branch again */
                    xTaskDelayUntil( &x_last_awake_time, image_change_freq / 4 );
                    /* Start the pipeline again */
                    mpp_start(mp_split, 0, false);
                }
                else 
                {
                    /* Person is recognized. Stop the pipeline to check results and change the image */
                    mpp_stop(mp_split);
                    
                    if (user_data.test.state != RECOGNITION_STATE)
                    {
                        user_data.test.result &= false;
                        PRINTF("[ERR]: Recognize a person when not in RECOGNITION_STATE. ");
                        PRINTF("Current state is %d\r\n", user_data.test.state);
                        /* after reading, inference output should be cleared */
                    }
                    else
                    {
                        PRINTF("Recognized face: %s with similarity percentage: %d%%\r\n", user_data.result.recognized_name, user_data.result.similarity_percentage);
                        if (user_data.result.similarity_percentage < PERSON_RECOGNIZED_THRESHOLD)
                            user_data.test.result &= false;
                        /* Check if the correct person is recognized */
                        if (strcmp(crt_image_name, user_data.result.recognized_name) != 0)
                            user_data.test.result &= false;
                    }
                    /* after reading, inference output should be cleared */
                    strcpy(user_data.result.recognized_name,"\0");
                    user_data.result.similarity_percentage = 0;
                    /* Wait some time before changing the image */
                    xTaskDelayUntil( &x_last_awake_time, image_change_freq );
                    /* Change the image */
                    crt_image_idx = go_to_next_image(crt_image_idx, mp, static_img_elem, crt_image_name, &user_data);
                    /* Check here if the test is finished to clear the database 
                        before starting the inference pipeline */
                    if (user_data.test.finished)
                    {
#if PERSON_REGISTRATION == 1
#ifdef CLEAR_DATABASE_AFTER_EACH_ITERATION
                        database_delete_all();
#endif
#endif
                    }
                    /* Wait some time before starting the inference branch again */
                    xTaskDelayUntil( &x_last_awake_time, image_change_freq / 4);
                    /* Start the pipeline again */
                    mpp_start(mp_split, 0, false);
                }

                last_inf_frame_num = user_data.inference_frame_num;
            }

            /* Check if test has finished or not */
            if (user_data.test.finished)
            {
                /* Check the test result */
                if (user_data.test.result)
                    PRINTF("%s - PASSED\r\n", TC_NAME);
                else
                    PRINTF("%s - FAILED\r\n", TC_NAME);

                PRINTF("%s finished\r\n", TC_NAME);

#if TEST_ITERATIONS != 0
                user_data.test.iterations++;
                user_data.test.session_result &= user_data.test.result;

                if (user_data.test.iterations == TEST_ITERATIONS)
                {
                    /* Stop the pipeline and the test */
                    mpp_stop(mp_split);
                    mpp_stop(mp);
                    PRINTF("Test stopped after %d iterations \r\n", user_data.test.iterations);
                    if (user_data.test.session_result)
                        PRINTF("Overall test result is PASS\r\n");
                    else
                        PRINTF("Overall test result is FAIL\r\n");
                    for (;;)
                    {
                        vTaskSuspend(NULL);
                    }
                }
#endif

                /* Re-initialize test state */
                user_data.test.result = true;
                user_data.test.finished = 0;
                PRINTF("\r\nStart %s\r\n", TC_NAME);
            }
            __atomic_store_n(&user_data.accessing, 0, __ATOMIC_SEQ_CST);
        }
    }

    err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}
