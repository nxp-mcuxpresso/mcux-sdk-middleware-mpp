/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application shows usage of MultiMedia Pipeline for hand landmark detection:
 * Static image -> split -> image converter -> draw labeled rectangles -> display
 *                       +-> inference engine (model: hand_landmark)
 * The model performs hand landmark detection using TF-Lite micro inference engine
 * with hand detection and landmark coordinates displayed on UART console
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"
#include "atomic.h"
#include "math.h"

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

/* tflite hand_landmark models */
#include APP_TFLITE_HAND_LANDMARK_DATA
#include APP_TFLITE_GESTURE_EMBEDDER_DATA
#include APP_TFLITE_GESTURE_CLASSIFIER_DATA
/* Model info */
#include APP_TFLITE_HAND_LANDMARK_INFO
#include APP_TFLITE_GESTURE_EMBEDDER_INFO
#include APP_TFLITE_GESTURE_CLASSIFIER_INFO

#include "hand_landmark_quant_output_postproc.h"
#include "canned_gesture_classifier_output_postproc.h"
#include "models/utils.h"
#include "app_constants.h"

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_image_hand_landmark_view_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_hand_landmark_view"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

typedef struct _user_data_t {
    int inference_frame_num;
    mpp_t mp;
    mpp_elem_handle_t hand_ldmk_elem;
    mpp_elem_handle_t gesture_emb_elem;
    mpp_elem_handle_t gesture_cls_elem;
    mpp_elem_handle_t labrect_elem;
    hand_data hand_data;
    uint32_t accessing; /* boolean protecting access */
    uint32_t hand_ldmk_inference_time_ms;
    uint32_t gesture_emb_inference_time_ms;
    uint32_t gesture_cls_inference_time_ms;
    gesture_data gesture_data;
    mpp_labeled_rect_t labels[MAX_LABEL_RECTS];
    mpp_stats_t *api_stats;
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

mpp_stats_t hand_landmark_stats;
mpp_stats_t gesture_embedder_stats;
mpp_stats_t gesture_classifier_stats;
static user_data_t user_data = {0};
static const char s_display_name[] = APP_DISPLAY_NAME;

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

    PRINTF("****** TEST test_image_hand_landmark_view ******\r\n");
    PRINTF("---INFERENCE ENGINE: TFLITE---\r\n");

    ret = xTaskCreate(
          app_task,
          "app_task",
          configMINIMAL_STACK_SIZE + 2400,
          (void *) NULL,
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

/**
 * Set hand landmark color based on landmark type
 * @param landmark: Pointer to landmark structure to set color for
 */
void set_hand_landmark_color(mpp_landmark_t* landmark) {
    landmark->color.rgb.R = 0x00;
    landmark->color.rgb.G = 0x00;
    landmark->color.rgb.B = 0xFF;
}

/**
 * Convert landmark coordinates from detection space to view dimensions
 * @param src_landmark: Source landmark coordinates from detection
 * @param dst_landmark: Destination landmark structure for display
 * @param landmark_idx: Index of the landmark within the hand
 */
void convert_landmark_coordinates(const coord_3d_t* src_landmark, mpp_landmark_t* dst_landmark, uint32_t landmark_idx) {
    /* Set basic landmark properties */
    dst_landmark->clear = 0; /* don't clear landmark */
    dst_landmark->width = LANDMARK_POINT_SIZE; /* landmark point size */
    dst_landmark->stripe = false;

    /* Convert coordinates from detection space to view dimensions */
    dst_landmark->x = (((int16_t)src_landmark->x) * DETECTION_ZONE_RECT_WIDTH / HAND_LANDMARK_WIDTH) + BOXES_OFFSET_LEFT;
    dst_landmark->y = (((int16_t)src_landmark->y) * DETECTION_ZONE_RECT_HEIGHT / HAND_LANDMARK_HEIGHT) + BOXES_OFFSET_TOP;

    /* Encode hand and landmark index in tag */
    dst_landmark->tag = landmark_idx;

    /* Set color based on landmark type */
    set_hand_landmark_color(dst_landmark);
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data_ptr) {
    status_t ret;
    mpp_inference_cb_param_t *inf_output;

    /* user_data handle contains application private data */
    user_data_t *app_priv = (user_data_t *)user_data_ptr;

    switch(evt) {
    case MPP_EVENT_INFERENCE_OUTPUT_READY:
        /* cast evt_data pointer to correct structure matching the event */
        inf_output = (mpp_inference_cb_param_t *) evt_data;

        switch (inf_output->model_id) {
        case HANDLANDMARK_MODEL_ID:
            /* process new hand data from inference */
            if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS) {
                ret = HandLandmark_ProcessOutput(inf_output, &app_priv->hand_data);
                if (ret != kStatus_Success)
                    PRINTF("mpp_event_listener: process output error for hand landmark model!\r\n");

                app_priv->hand_ldmk_inference_time_ms = inf_output->inference_time_ms;

                /* Update labeled rectangle element with detected faces and landmarks */
                if (app_priv->labrect_elem != 0) {
                    mpp_element_params_t elem_params;
                    memset(&elem_params, 0, sizeof(elem_params));

                    /* Set rectangle parameters */
                    elem_params.labels.max_rect = MAX_LABEL_RECTS;
                    elem_params.labels.detected_rect = 1; /* 1 for detection zone */
                    elem_params.labels.rectangles = app_priv->labels;

                    /* Allocate landmarks array if not already done */
                    static mpp_landmark_t landmarks[MAX_LABEL_RECTS * MODEL_NUM_3D_LANDMARKS];
                    memset(landmarks, 0, sizeof(landmarks));

                    /* params init */
                    elem_params.labels.max_landmk = MAX_LABEL_RECTS * MODEL_NUM_3D_LANDMARKS;
                    elem_params.labels.detected_landmk = 0;
                    elem_params.labels.landmarks = landmarks;

                    /* Fill landmarks for each detected face */
                    if (app_priv->hand_data.has_hand)
                    {
                        uint32_t landmark_idx = 0;
                        for (uint32_t lm_idx = 0; lm_idx < MODEL_NUM_3D_LANDMARKS && landmark_idx < (MAX_LABEL_RECTS * MODEL_NUM_3D_LANDMARKS); lm_idx++) {
                            convert_landmark_coordinates(&app_priv->hand_data.landmarks[lm_idx], &landmarks[landmark_idx], lm_idx);
                            landmark_idx++;
                            elem_params.labels.detected_landmk++;
                        }
                    }

                    /* prepare the output for the next model */
                    float *gest_emb_inp = (float *) mpp_get_input_buff_address(app_priv->gesture_emb_elem, 0);
                    if (gest_emb_inp == NULL)
                    {
                        PRINTF("mpp_event_listener: Failed to get gesture embedder input buffer\r\n");
                        __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
                        break;
                    }
                    for (int i = 0; i < MODEL_NUM_3D_LANDMARKS; i++) {
                        gest_emb_inp[i * 3] = app_priv->hand_data.landmarks[i].x;
                        gest_emb_inp[i * 3 + 1] = app_priv->hand_data.landmarks[i].y;
                        gest_emb_inp[i * 3 + 2] = app_priv->hand_data.landmarks[i].z;
                    }

                    /* Update the element with both rectangles and landmarks */
                    mpp_element_update(app_priv->mp, app_priv->labrect_elem, &elem_params, true);
                }

                app_priv->inference_frame_num++;

                /* end of modification of user data */
                __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
            }
            break;

        case GESTURE_EMBEDDER_MODEL_ID:
            if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS) {

                if (!app_priv->hand_data.has_hand) {
                    __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
                    break;
                }

                app_priv->gesture_emb_inference_time_ms = inf_output->inference_time_ms;

                /* Prepare gesture embedder output for gesture classifier input */
                float *gest_emb_output = (float *) inf_output->out_tensors[0]->data;
                float *gest_cls_inp = (float *) mpp_get_input_buff_address(app_priv->gesture_cls_elem, 0);

                for (int i = 0; i < GESTURE_EMBEDDER_OUTPUT_SIZE; i++) {
                    gest_cls_inp[i] = gest_emb_output[i];
                }

                /* end of modification of user data */
                __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
            }
            break;

        case GESTURE_CLASSIFIER_MODEL_ID:
            if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS) {

                if (!app_priv->hand_data.has_hand) {
                    __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
                    break;
                }
                
                app_priv->gesture_cls_inference_time_ms = inf_output->inference_time_ms;

                /* Process gesture classifier output */
                ret = GestureClassifier_ProcessOutput(inf_output, &app_priv->gesture_data);
                if (ret != kStatus_Success)
                    PRINTF("mpp_event_listener: process output error for gesture classifier model!\r\n");

                /* end of modification of user data */
                __atomic_store_n(&app_priv->accessing, 0, __ATOMIC_SEQ_CST);
            }
            break;

        default:
            PRINTF("mpp_event_listener: unknown model_id %d\r\n", inf_output->model_id);
            break;
        }
        break;
    case MPP_EVENT_INVALID:
    default:
        /* nothing to do */
        break;
    }

    return 0;
}
void print_results(user_data_t *user_data)
{
    bool test_passed = true;
    PRINTF("\r\n========================================================\r\n");
    PRINTF("Start %s\r\n", TC_NAME);
    PRINTF("hand landmark inference time %u (ms) \r\n", user_data->hand_ldmk_inference_time_ms);
    if (user_data->hand_data.has_hand == false)
    {
        PRINTF("Hand detected: no\r\n");
        test_passed = false;
    }
    else
    {
        PRINTF("gesture embedder inference time %u (ms) \r\n", user_data->gesture_emb_inference_time_ms);
        PRINTF("gesture classifier inference time %u (ms) \r\n", user_data->gesture_cls_inference_time_ms);
        PRINTF("Hand detected: yes\r\n");
        PRINTF("Hand score: %d%%\r\n", (int)(user_data->hand_data.score * 100.0f));
        PRINTF("Left hand: %s\r\n", user_data->hand_data.left_hand ? "yes" : "no");
        PRINTF("Gesture score: %d%%\r\n", (int)(user_data->gesture_data.score * 100.0f));
        PRINTF("Gesture label: %s \r\n", user_data->gesture_data.gesture);
#ifdef PRINT_LANDMARKS
        for (int j = 0; j < MODEL_NUM_3D_LANDMARKS; j++) {
            PRINTF("       Landmark %d: x=%d, y=%d\r\n",
                    j,
                    (int16_t) user_data->hand_data.landmarks[j].x,
                    (int16_t) user_data->hand_data.landmarks[j].y);
        }
#endif
        if (user_data->hand_ldmk_inference_time_ms > EXPECTED_HAND_LANDMARK_MAX_INF_TIME)
        {
            PRINTF("Hand landmark inference time (%d) exceeded threshold (%d)\r\n", user_data->hand_ldmk_inference_time_ms, EXPECTED_HAND_LANDMARK_MAX_INF_TIME);
            test_passed = false;
        }
        if (user_data->gesture_emb_inference_time_ms > EXPECTED_GESTURE_EMB_MAX_INF_TIME)
        {
            PRINTF("Gesture embedder inference time (%d) exceeded threshold (%d)\r\n", user_data->gesture_emb_inference_time_ms, EXPECTED_GESTURE_EMB_MAX_INF_TIME);
            test_passed = false;
        }
        if (user_data->gesture_cls_inference_time_ms > EXPECTED_GESTURE_CLS_MAX_INF_TIME)
        {
            PRINTF("Gesture classifier inference time (%d) exceeded threshold (%d)\r\n", user_data->gesture_cls_inference_time_ms, EXPECTED_GESTURE_CLS_MAX_INF_TIME);
            test_passed = false;
        }
        if ((int)(user_data->hand_data.score * 100.0f) < EXPECTED_HAND_SCORE_MIN)
        {
            PRINTF("Hand score (%d) below minimum threshold (%d)\r\n", (int)(user_data->hand_data.score * 100.0f), EXPECTED_HAND_SCORE_MIN);
            test_passed = false;
        }
        if (user_data->hand_data.left_hand != EXPECTED_LEFT_HAND_VALUE)
        {
            PRINTF("Left hand value (%d) does not match expected value (%d)\r\n", user_data->hand_data.left_hand, EXPECTED_LEFT_HAND_VALUE);
            test_passed = false;
        }
        if ((int)(user_data->gesture_data.score * 100.0f) < EXPECTED_GESTURE_SCORE_MIN)
        {
            PRINTF("Gesture score (%d) below minimum threshold (%d)\r\n", (int)(user_data->gesture_data.score * 100.0f), EXPECTED_GESTURE_SCORE_MIN);
            test_passed = false;
        }
        if (strcmp(user_data->gesture_data.gesture, EXPECTED_GESTURE) != 0)
        {
            PRINTF("Gesture (%s) does not match expected gesture (%s)\r\n", user_data->gesture_data.gesture, EXPECTED_GESTURE);
            test_passed = false;
        }
    }

    mpp_stats_disable(MPP_STATS_GRP_API);
    PRINTF("CPU Load: %u%%\n\r", user_data->api_stats->api.cpu_load);
    mpp_stats_enable(MPP_STATS_GRP_API);

    if (!test_passed)
    {
        PRINTF("%s - FAILED\r\n", TC_NAME);
    }
    else
    {
        PRINTF("%s - PASSED\r\n", TC_NAME);
    }
    PRINTF("%s finished\r\n", TC_NAME);
    PRINTF("========================================================\r\n"); 
}

static void app_task(void *params)
{
    int ret;

    PRINTF("[%s]\r\n", mpp_get_version());

    /* init API */
    static mpp_api_params_t api_param = {0};
    mpp_stats_t api_stats;
    memset(&api_stats, 0, sizeof(api_stats));
    api_param.stats = &api_stats;
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

    /* Add static image element */
    static mpp_img_params_t img_params;
    memset(&img_params, 0, sizeof (mpp_img_params_t));
    img_params.format = SRC_IMAGE_FORMAT;
    img_params.width = SRC_IMAGE_WIDTH;
    img_params.height = SRC_IMAGE_HEIGHT;
    mpp_static_img_add(mp, &img_params, (void *)image_data, NULL);

    /* Split the pipeline into 2 branches:
     * - first for the conversion to model (inference branch)
     * - second for the label-rect draw & display (display branch)
     */
    static mpp_t mp_split;
    mpp_params.exec_flag = MPP_EXEC_RC;
    ret = mpp_split(mp, 1, &mpp_params, &mp_split);
    if (ret) {
        PRINTF("Failed to split pipeline\r\n");
        goto err;
    }

    /* configure inference element with hand_landmark model */
    mpp_element_params_t hand_landmark_params;
    memset(&hand_landmark_params, 0, sizeof(mpp_element_params_t));
    hand_landmark_params.ml_inference.model_data = hand_landmark_data;
    hand_landmark_params.ml_inference.model_size = hand_landmark_data_len;
    hand_landmark_params.ml_inference.model_id   = HANDLANDMARK_MODEL_ID;
    hand_landmark_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    hand_landmark_params.ml_inference.model_input_mean = HAND_LANDMARK_INPUT_MEAN;
    hand_landmark_params.ml_inference.model_input_std = HAND_LANDMARK_INPUT_STD;
    hand_landmark_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    hand_landmark_params.ml_inference.inference_params.num_inputs = 1;
    hand_landmark_params.ml_inference.inference_params.num_outputs = 4;
    hand_landmark_params.stats = &hand_landmark_stats;

    ret = mpp_element_add(mp_split, MPP_ELEMENT_INFERENCE, &hand_landmark_params, &user_data.hand_ldmk_elem);
    if (ret)
    {
        PRINTF("Failed to add element VALGO_TFLite\r\n");
        goto err;
    }

    /* configure inference element with gesture embedder model */
    mpp_element_params_t gesture_embedder_params;
    memset(&gesture_embedder_params, 0, sizeof(mpp_element_params_t));
    gesture_embedder_params.ml_inference.model_data = gesture_embedder_data;
    gesture_embedder_params.ml_inference.model_size = gesture_embedder_data_len;
    gesture_embedder_params.ml_inference.model_id   = GESTURE_EMBEDDER_MODEL_ID;
    gesture_embedder_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    gesture_embedder_params.ml_inference.model_input_mean = 0;
    gesture_embedder_params.ml_inference.model_input_std = 1;
    gesture_embedder_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    gesture_embedder_params.ml_inference.inference_params.num_inputs = 1;
    gesture_embedder_params.ml_inference.inference_params.num_outputs = 1;
    gesture_embedder_params.stats = &gesture_embedder_stats;

    ret = mpp_element_add(mp_split, MPP_ELEMENT_INFERENCE, &gesture_embedder_params, &user_data.gesture_emb_elem);
    if (ret)
    {
        PRINTF("Failed to add element VALGO_TFLite\r\n");
        goto err;
    }

    /* configure inference element with gesture embedder model */
    mpp_element_params_t gesture_classifier_params;
    memset(&gesture_classifier_params, 0, sizeof(mpp_element_params_t));
    gesture_classifier_params.ml_inference.model_data = canned_gesture_classifier_data;
    gesture_classifier_params.ml_inference.model_size = canned_gesture_classifier_data_len;
    gesture_classifier_params.ml_inference.model_id   = GESTURE_CLASSIFIER_MODEL_ID;
    gesture_classifier_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    gesture_classifier_params.ml_inference.model_input_mean = 0;
    gesture_classifier_params.ml_inference.model_input_std = 1;
    gesture_classifier_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    gesture_classifier_params.ml_inference.inference_params.num_inputs = 1;
    gesture_classifier_params.ml_inference.inference_params.num_outputs = 1;
    gesture_classifier_params.stats = &gesture_classifier_stats;

    ret = mpp_element_add(mp_split, MPP_ELEMENT_INFERENCE, &gesture_classifier_params, &user_data.gesture_cls_elem);
    if (ret)
    {
        PRINTF("Failed to add element VALGO_TFLite\r\n");
        goto err;
    }

    // close the pipeline with a null sink
    ret = mpp_nullsink_add(mp_split);
    if (ret)
    {
        PRINTF("Failed to add NULL sink\r\n");
        goto err;
    }

    /* DISPLAY BRANCH: On the main branch, send the frame to the display */
    /* First do color-convert + scale for display */
    static mpp_element_params_t elem_params;
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick GFX device */
    elem_params.convert.dev_name = APP_GFX_BACKEND_NAME;
    /* set output buffer dims */
    elem_params.convert.out_buf.width =  VIEW_WIDTH;
    elem_params.convert.out_buf.height = VIEW_HEIGHT;
    elem_params.convert.pixel_format = APP_DISPLAY_FORMAT;
    /* scaling parameters */
    elem_params.convert.scale.width =  VIEW_WIDTH;
    elem_params.convert.scale.height = VIEW_HEIGHT;
    elem_params.convert.ops = MPP_CONVERT_COLOR | MPP_CONVERT_SCALE;

    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);
    if (ret) {
        PRINTF("Failed to add element CONVERT for display\r\n");
        goto err;
    }

    /* Add labeled rectangle element for face detection visualization */
    memset(&elem_params, 0, sizeof(elem_params));
    memset(&user_data.labels, 0, sizeof(user_data.labels));

    /* params init */
    elem_params.labels.max_rect = MAX_LABEL_RECTS;
    elem_params.labels.detected_rect = 1;
    elem_params.labels.rectangles = user_data.labels;
    elem_params.labels.max_landmk = MAX_LABEL_RECTS * MODEL_NUM_3D_LANDMARKS;

    /* Add detection zone box */
    /* first add detection zone box */
    user_data.labels[0].top    = DETECTION_ZONE_RECT_TOP;
    user_data.labels[0].left   = DETECTION_ZONE_RECT_LEFT;
    user_data.labels[0].bottom = DETECTION_ZONE_RECT_TOP + DETECTION_ZONE_RECT_HEIGHT;
    user_data.labels[0].right  = DETECTION_ZONE_RECT_LEFT + DETECTION_ZONE_RECT_WIDTH;
    user_data.labels[0].line_width = RECT_LINE_WIDTH;
    user_data.labels[0].line_color.rgb.G = 0xff;
    strcpy((char *)user_data.labels[0].label, "Detection Zone");

    /* add element and retrieve its handle in user data */
    ret = mpp_element_add(mp, MPP_ELEMENT_LABELED_RECTANGLE, &elem_params, &user_data.labrect_elem);
    if (ret) {
        PRINTF("Failed to add element LABELED_RECTANGLE (0x%x)\r\n", ret);
        goto err;
    }

    /* pass the mpp of the element 'label rectangle' to callback */
    user_data.mp = mp;

#if (APP_SKIP_CONVERT_FOR_DISPLAY == 0)
    /* then rotate if needed */
    if (APP_DISPLAY_LANDSCAPE_ROTATE != ROTATE_0) {
        memset(&elem_params, 0, sizeof(elem_params));
        elem_params.convert.dev_name = APP_GFX_BACKEND_NAME;
        /* set output buffer dims */
        elem_params.convert.out_buf.width = APP_DISPLAY_WIDTH;
        elem_params.convert.out_buf.height = APP_DISPLAY_HEIGHT;
        elem_params.convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
        elem_params.convert.scale.width =  SCALED_VIEW_WIDTH;
        elem_params.convert.scale.height = SCALED_VIEW_HEIGHT;

        elem_params.convert.ops = MPP_CONVERT_ROTATE | MPP_CONVERT_SCALE;
        ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, NULL);

        if (ret) {
            PRINTF("Failed to add element CONVERT\r\n");
            goto err;
        }
    }
#endif

    /* Add display element */
    mpp_display_params_t display_params;
    memset(&display_params, 0, sizeof(mpp_display_params_t));
    display_params.height = APP_DISPLAY_HEIGHT;
    display_params.width = APP_DISPLAY_WIDTH;
    display_params.format = APP_DISPLAY_FORMAT;
    display_params.rotate = APP_DISPLAY_LANDSCAPE_ROTATE;

    ret = mpp_display_add(mp, s_display_name, &display_params);
    if (ret) {
        PRINTF("Failed to add display element\r\n");
        goto err;
    }

    mpp_stats_enable(MPP_STATS_GRP_API);

    /* start secondary pipeline branch (inference conversion) */
    ret = mpp_start(mp_split, 0, false);
    if (ret) {
        PRINTF("Failed to start secondary pipeline branch\r\n");
        goto err;
    }

    /* start main pipeline branch (display) */
    ret = mpp_start(mp, 1, false);
    if (ret) {
        PRINTF("Failed to start main pipeline branch\r\n");
        goto err;
    }

    user_data.api_stats = &api_stats;

    uint32_t last_inf_frame_num = user_data.inference_frame_num;

    for (;;) {
        /* manage periodic print */
        if (Atomic_CompareAndSwap_u32(&user_data.accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS) {
            if (last_inf_frame_num != user_data.inference_frame_num) {
                print_results(&user_data);
                last_inf_frame_num = user_data.inference_frame_num;
            }
            __atomic_store_n(&user_data.accessing, 0, __ATOMIC_SEQ_CST);
        }

        vTaskDelay(pdMS_TO_TICKS(OUTPUT_PRINT_PERIOD_MS));
    }

err:
    for (;;) {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}
