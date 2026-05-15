/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application shows usage of MultiMedia Pipeline for hand detection with landmarks:
 * Static Image -> split -> image converter -> draw labeled rectangles with landmarks -> display
 *                       +-> inference engine (model: BLAZE_DETECTOR)
 * The model performs hand detection and landmarks using TF-Lite inference engine
 * with hand rotation angle and landmark coordinates displayed on UART console
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

/* tflite blaze_detector models */
#include APP_TFLITE_BLAZE_DETECTOR_PTQ_DATA
/* Model info */
#include APP_TFLITE_BLAZE_DETECTOR_PTQ_INFO

#include "blaze_detector_ptq_output_postproc.h"
#include "models/utils.h"
#include "app_constants.h"

void *image_data = (void *)hand_192_192_rgb_data;
#define LANDMARK_POINT_SIZE 1

/*******************************************************************************
 * Definitions
 ******************************************************************************/

typedef struct _user_data_t {
    int inference_frame_num;
    mpp_t mp;
    mpp_elem_handle_t elem;
    mpp_elem_handle_t labrect_elem;
    box_data boxes[NUM_BOXES_MAX];
    uint32_t accessing; /* boolean protecting access */
    int detected_count;          /* number of detected boxes */
    uint32_t inference_time_ms;
    mpp_labeled_rect_t labels[MAX_LABEL_RECTS];
    mpp_stats_t *api_stats;
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

mpp_stats_t blaze_detector_stats;
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

    PRINTF("****** TEST test_image_blaze_detector_view ******\r\n");
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
 * @param landmark_type: Type of landmark
 */
void set_hand_landmark_color(mpp_landmark_t* landmark, uint32_t landmark_type) {
    switch (landmark_type) {
        case 0:
            landmark->color.rgb.R = 0xff;
            landmark->color.rgb.G = 0x00;
            landmark->color.rgb.B = 0x00;
            break;
        case 1:
            landmark->color.rgb.R = 0x00;
            landmark->color.rgb.G = 0xff;
            landmark->color.rgb.B = 0x00;
            break;
        case 2:
            landmark->color.rgb.R = 0x00;
            landmark->color.rgb.G = 0x00;
            landmark->color.rgb.B = 0xff;
            break;
        case 3:
            landmark->color.rgb.R = 0xff;
            landmark->color.rgb.G = 0xff;
            landmark->color.rgb.B = 0x00;
            break;
        case 4:
            landmark->color.rgb.R = 0xff;
            landmark->color.rgb.G = 0x00;
            landmark->color.rgb.B = 0xff;
            break;
        case 5:
            landmark->color.rgb.R = 0x00;
            landmark->color.rgb.G = 0xff;
            landmark->color.rgb.B = 0xff;
            break;
        case 6:
            landmark->color.rgb.R = 0xff;
            landmark->color.rgb.G = 0xff;
            landmark->color.rgb.B = 0xff;
            break;
        default:
            landmark->color.rgb.R = 0x00;
            landmark->color.rgb.G = 0x00;
            landmark->color.rgb.B = 0x00;
            break;
    }
}

/**
 * Convert landmark coordinates from detection space to view dimensions
 * @param src_landmark: Source landmark coordinates from detection
 * @param dst_landmark: Destination landmark structure for display
 * @param hand_idx: Index of the hand this landmark belongs to
 * @param landmark_idx: Index of the landmark within the hand
 */
void convert_landmark_coordinates(const coord_t* src_landmark, mpp_landmark_t* dst_landmark,
                                uint32_t hand_idx, uint32_t landmark_idx) {
    /* Set basic landmark properties */
    dst_landmark->clear = 0; /* don't clear landmark */
    dst_landmark->width = LANDMARK_POINT_SIZE; /* landmark point size */
    dst_landmark->stripe = false;

    /* Convert coordinates from detection space to view dimensions */
    dst_landmark->x = (src_landmark->x * DETECTION_ZONE_RECT_WIDTH / BLAZE_DETECTOR_WIDTH) + BOXES_OFFSET_LEFT;
    dst_landmark->y = (src_landmark->y * DETECTION_ZONE_RECT_HEIGHT / BLAZE_DETECTOR_HEIGHT) + BOXES_OFFSET_TOP;

    /* Encode hand and landmark index in tag */
    dst_landmark->tag = (hand_idx << 8) | landmark_idx;

    /* Set color based on landmark type */
    set_hand_landmark_color(dst_landmark, landmark_idx);
}

/* Translate boxes into labeled rectangles using display characteristics */
void boxes_to_rects(box_data boxes[], uint32_t num_boxes, uint32_t max_boxes, mpp_labeled_rect_t *rects)
{
    uint32_t box_counter = 1;

    /* other rectangles show detected objects */
    for (uint32_t i = 0; i < num_boxes && box_counter < max_boxes; i++) {
        if (boxes[i].area == 0)
            continue;
        /* input tensor preview is scaled and moved to fit on screen, and so its bounding boxes */
        rects[box_counter].left = (int)((boxes[i].left * DETECTION_ZONE_RECT_WIDTH)/ BLAZE_DETECTOR_WIDTH) + BOXES_OFFSET_LEFT;
        rects[box_counter].right = (int)((boxes[i].right * DETECTION_ZONE_RECT_WIDTH)/ BLAZE_DETECTOR_WIDTH) + BOXES_OFFSET_LEFT;
        rects[box_counter].bottom = (int)((boxes[i].bottom * DETECTION_ZONE_RECT_HEIGHT)/BLAZE_DETECTOR_HEIGHT) + BOXES_OFFSET_TOP;
        rects[box_counter].top = (int)((boxes[i].top * DETECTION_ZONE_RECT_HEIGHT)/BLAZE_DETECTOR_HEIGHT) + BOXES_OFFSET_TOP;
        rects[box_counter].line_width = RECT_LINE_WIDTH;
        rects[box_counter].line_color.rgb.R = 0xff;
        rects[box_counter].line_color.rgb.B = 0xff;

        box_counter++;
    }
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
        /* process new box data from inference */
        if (Atomic_CompareAndSwap_u32(&app_priv->accessing, 1, 0) == ATOMIC_COMPARE_AND_SWAP_SUCCESS) {
            ret = BlazeDetectorPtq_ProcessOutput(
                    inf_output,
                    app_priv->boxes,
                    NUM_BOXES_MAX,
                    false);
            if (ret != kStatus_Success)
                PRINTF("mpp_event_listener: process output error!");

            app_priv->inference_time_ms = inf_output->inference_time_ms;

            app_priv->detected_count = 0;
            /* count valid results */
            for (uint32_t i = 0; i < NUM_BOXES_MAX; i++)
            {
                if (app_priv->boxes[i].score > 0)
                {
                    app_priv->detected_count++;
                }
            }

            /* Update labeled rectangle element with detected hands and landmarks */
            if (app_priv->labrect_elem != 0) {
                mpp_element_params_t elem_params;
                memset(&elem_params, 0, sizeof(elem_params));

                /* Set rectangle parameters */
                elem_params.labels.max_rect = MAX_LABEL_RECTS;
                elem_params.labels.detected_rect = app_priv->detected_count + 1; /* +1 for detection zone */
                elem_params.labels.rectangles = app_priv->labels;

                boxes_to_rects(app_priv->boxes, NUM_BOXES_MAX, MAX_LABEL_RECTS, elem_params.labels.rectangles);

                /* Allocate landmarks array if not already done */
                static mpp_landmark_t landmarks[MAX_LABEL_RECTS * MODEL_NUM_LANDMARKS];
                memset(landmarks, 0, sizeof(landmarks));

                /* params init */
                elem_params.labels.max_landmk = MAX_LABEL_RECTS * MODEL_NUM_LANDMARKS;
                elem_params.labels.detected_landmk = 0;
                elem_params.labels.landmarks = landmarks;

                /* Fill landmarks for each detected hand */
                uint32_t landmark_idx = 0;
                for (uint32_t hand_idx = 0; hand_idx < app_priv->detected_count && hand_idx < MAX_LABEL_RECTS; hand_idx++) {
                    if (app_priv->boxes[hand_idx].score > 0) {
                        /* Add all landmarks for this hand */
                        for (uint32_t lm_idx = 0; lm_idx < MODEL_NUM_LANDMARKS && landmark_idx < (NUM_BOXES_MAX * MODEL_NUM_LANDMARKS); lm_idx++) {
                            convert_landmark_coordinates(&app_priv->boxes[hand_idx].landmarks[lm_idx], &landmarks[landmark_idx],
                                                            hand_idx, lm_idx);
                            landmark_idx++;
                            elem_params.labels.detected_landmk++;
                        }
                    }
                }

                /* Update the element with both rectangles and landmarks */
                mpp_element_update(app_priv->mp, app_priv->labrect_elem, &elem_params, true);
            }

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

void print_results(user_data_t *user_data)
{
    PRINTF("inference time %u (ms) \r\n", user_data->inference_time_ms);
    if (user_data->detected_count == 0)
    {
        PRINTF("No hand detected! \r\n");
    }
    else
    {
        PRINTF("detected hands: %d \r\n", user_data->detected_count);
        for (int i = 0; i < user_data->detected_count; i++) {
            PRINTF("--------------------------\r\n");
            PRINTF("Hand %d: \r\n", i);
            PRINTF("     Box score: %d%% \r\n", (int)(user_data->boxes[i].score * 100.0f));
            PRINTF("     Box coordinates: (%d,%d) to (%d,%d) \r\n",
                    (int)(user_data->boxes[i].left), (int)(user_data->boxes[i].top),
                    (int)(user_data->boxes[i].right), (int)(user_data->boxes[i].bottom));
            PRINTF("     Rotation angle: %f degrees \r\n", user_data->boxes[i].rotation);
            for (int j = 0; j < MODEL_NUM_LANDMARKS; j++) {
                PRINTF("     Landmark %d: (%d, %d)\r\n", j,
                        (int)(user_data->boxes[i].landmarks[j].x),
                        (int)(user_data->boxes[i].landmarks[j].y));
            }
            PRINTF("--------------------------\r\n");
        }
    }

    mpp_stats_disable(MPP_STATS_GRP_API);
    PRINTF("CPU Load: %u(%%)\n\r", user_data->api_stats->api.cpu_load);
    mpp_stats_enable(MPP_STATS_GRP_API);
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
        PRINTF("Failed to split pipeline\n");
        goto err;
    }

    /* configure inference element with BLAZE DETECTOR model */
    mpp_element_params_t blaze_detector_params;
    memset(&blaze_detector_params, 0, sizeof(mpp_element_params_t));
    blaze_detector_params.ml_inference.model_data = blaze_detector_data;
    blaze_detector_params.ml_inference.model_size = blaze_detector_data_len;
    blaze_detector_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    blaze_detector_params.ml_inference.model_input_mean = BLAZE_DETECTOR_INPUT_MEAN;
    blaze_detector_params.ml_inference.model_input_std = BLAZE_DETECTOR_INPUT_STD;
    blaze_detector_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    blaze_detector_params.ml_inference.inference_params.num_inputs = 1;
    blaze_detector_params.ml_inference.inference_params.num_outputs = 2;
    blaze_detector_params.stats = &blaze_detector_stats;

    ret = mpp_element_add(mp_split, MPP_ELEMENT_INFERENCE, &blaze_detector_params, NULL);
    if (ret)
    {
        PRINTF("Failed to add element VALGO_TFLite");
        goto err;
    }

    // close the pipeline with a null sink
    ret = mpp_nullsink_add(mp_split);
    if (ret)
    {
        PRINTF("Failed to add NULL sink\n");
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
        PRINTF("Failed to add element CONVERT for display\n");
        goto err;
    }

    /* Add labeled rectangle element for hand detection visualization */
    memset(&elem_params, 0, sizeof(elem_params));
    memset(&user_data.labels, 0, sizeof(user_data.labels));

    /* params init */
    elem_params.labels.max_rect = MAX_LABEL_RECTS;
    elem_params.labels.detected_rect = 1;
    elem_params.labels.rectangles = user_data.labels;
    elem_params.labels.max_landmk = MAX_LABEL_RECTS * MODEL_NUM_LANDMARKS;

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
        PRINTF("Failed to start secondary pipeline branch");
        goto err;
    }

    /* start main pipeline branch (display) */
    ret = mpp_start(mp, 1, false);
    if (ret) {
        PRINTF("Failed to start main pipeline branch");
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
