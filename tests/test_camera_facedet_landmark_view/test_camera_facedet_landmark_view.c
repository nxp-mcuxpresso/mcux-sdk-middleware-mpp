/*
 * Copyright 2025-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application shows usage of MultiMedia Pipeline for face detection with landmarks:
 * Camera -> split -> image converter -> draw labeled rectangles -> display
 *                +-> image converter -> inference engine (model: SCRFD_KPS_500M)
 * The model performs face detection and landmarks using TF-Lite micro inference engine
 * with face tilt/yaw calculation displayed on UART console
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

/* tflite scrfd_kps models */
#include APP_TFLITE_SCRFD_KPS_DATA
/* Model info */
#include APP_TFLITE_SCRFD_KPS_INFO

#include "scrfd_kps_output_postproc.h"
#include "models/utils.h"
#include "app_constants.h"

#if (SOURCE_STATIC_IMAGE == 1)
#if defined(USE_SCRFD_320_256_MODEL)
void *image_data = (void *)couple_COCO_320_256_rgb_data;
#elif defined(USE_SCRFD_256_256_MODEL)
void *image_data = (void *)couple_COCO_256_256_rgb_data;
#else
void *image_data = (void *)couple_COCO_128_128_rgb_data;
#endif /* USE_SCRFD_320_256_MODEL */
#define LANDMARK_POINT_SIZE 1
#else
static const char s_camera_name[] = APP_CAMERA_NAME;
#define LANDMARK_POINT_SIZE 2
#endif
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/** Default priority for application tasks */
#define APP_DEFAULT_PRIO        1

typedef struct _user_data_t {
    int inference_frame_num;
    mpp_t mp;
    mpp_elem_handle_t elem;
    mpp_elem_handle_t labrect_elem;
    box_data boxes[NUM_BOXES_MAX];
    uint32_t accessing; /* boolean protecting access */
    int detected_count;          /* number of detected boxes */
    int inference_time_ms;
    float face_tilt[NUM_BOXES_MAX];  /* face tilt angles */
    float face_yaw[NUM_BOXES_MAX];   /* face yaw angles */
    mpp_landmark_t landmarks[NUM_BOXES_MAX * SCRFD_NUM_LANDMARKS]; /* landmarks for display */
    mpp_labeled_rect_t labels[MAX_LABEL_RECTS];
    mpp_stats_t *api_stats;
} user_data_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

mpp_stats_t scrfd_kps_stats;
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

    PRINTF("****** TEST test_camera_facedet_landmark_display ******\r\n");
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

// TODO place these functions in a shared utility file
/* Translate boxes into labeled rectangles using display characteristics */
void boxes_to_rects(box_data boxes[], uint32_t num_boxes, uint32_t max_boxes, mpp_labeled_rect_t *rects)
{
    uint32_t box_counter = 1;

    /* other rectangles show detected objects */
    for (uint32_t i = 0; i < num_boxes && box_counter < max_boxes; i++) {
        if (boxes[i].area == 0)
            continue;
        /* input tensor preview is scaled and moved to fit on screen, and so its bounding boxes */
        rects[box_counter].left = (int)((boxes[i].left * DETECTION_ZONE_RECT_WIDTH)/ SCRFD_KPS_WIDTH) + BOXES_OFFSET_LEFT;
        rects[box_counter].right = (int)((boxes[i].right * DETECTION_ZONE_RECT_WIDTH)/ SCRFD_KPS_WIDTH) + BOXES_OFFSET_LEFT;
        rects[box_counter].bottom = (int)((boxes[i].bottom * DETECTION_ZONE_RECT_HEIGHT)/SCRFD_KPS_HEIGHT) + BOXES_OFFSET_TOP;
        rects[box_counter].top = (int)((boxes[i].top * DETECTION_ZONE_RECT_HEIGHT)/SCRFD_KPS_HEIGHT) + BOXES_OFFSET_TOP;
        rects[box_counter].line_width = RECT_LINE_WIDTH;
        rects[box_counter].line_color.rgb.R = 0xff;
        rects[box_counter].line_color.rgb.B = 0xff;

        box_counter++;
    }
}

/**
 * Process face orientation calculation and logging
 * @param app_priv: Application private data structure
 * @param face_index: Index of the face being processed (0-based)
 */
void process_face_orientation(user_data_t* app_priv, int face_index) {
    /* Calculate face tilt and yaw using landmarks */
    face_data_t face_data = {0};

    /* Map landmarks from box_data to face_data_t format */
    /* Assuming landmarks order: left_eye, right_eye, nose, mouth_left, mouth_right */
    if (SCRFD_NUM_LANDMARKS >= 3) {
        face_data.leye = app_priv->boxes[face_index].landmarks[0]; /* left eye */
        face_data.reye = app_priv->boxes[face_index].landmarks[1]; /* right eye */
        face_data.nose = app_priv->boxes[face_index].landmarks[2]; /* nose */

        int tilt_yaw_ret = get_face_tilt_yaw(&face_data,
                                           &app_priv->face_tilt[face_index],
                                           &app_priv->face_yaw[face_index]);

        if (tilt_yaw_ret != FACE_TILT_YAW_SUCCESS) {
            PRINTF("Failed to calculate face tilt/yaw for face %d, error: %d\r\n",
                   face_index, tilt_yaw_ret);
            app_priv->face_tilt[face_index] = 0.0f;
            app_priv->face_yaw[face_index] = 0.0f;
        } else {
            PRINTF("Face %d centered! \r\n", face_index);
        }
    } else {
        PRINTF("Not enough landmarks for face tilt/yaw calculation\r\n");
        app_priv->face_tilt[face_index] = 0.0f;
        app_priv->face_yaw[face_index] = 0.0f;
    }
}

/**
 * Set face landmark color based on landmark type
 * @param landmark: Pointer to landmark structure to set color for
 * @param landmark_type: Type of landmark (0=left eye, 1=right eye, 2=nose, etc.)
 */
void set_face_landmark_color(mpp_landmark_t* landmark, uint32_t landmark_type) {
    switch (landmark_type) {
        case 0: /* left eye */
            landmark->color.rgb.R = 0x00;
            landmark->color.rgb.G = 0xff;
            landmark->color.rgb.B = 0x00;
            break;
        case 1: /* right eye */
            landmark->color.rgb.R = 0x00;
            landmark->color.rgb.G = 0xff;
            landmark->color.rgb.B = 0x00;
            break;
        case 2: /* nose */
            landmark->color.rgb.R = 0xff;
            landmark->color.rgb.G = 0xff;
            landmark->color.rgb.B = 0x00;
            break;
        case 3: /* mouth left */
            landmark->color.rgb.R = 0xff;
            landmark->color.rgb.G = 0x00;
            landmark->color.rgb.B = 0xff;
            break;
        case 4: /* mouth right */
            landmark->color.rgb.R = 0xff;
            landmark->color.rgb.G = 0x00;
            landmark->color.rgb.B = 0xff;
            break;
        default: /* other landmarks */
            landmark->color.rgb.R = 0x00;
            landmark->color.rgb.G = 0x00;
            landmark->color.rgb.B = 0xff;
            break;
    }
}

/**
 * Convert landmark coordinates from detection space to view dimensions
 * @param src_landmark: Source landmark coordinates from detection
 * @param dst_landmark: Destination landmark structure for display
 * @param face_idx: Index of the face this landmark belongs to
 * @param landmark_idx: Index of the landmark within the face
 */
void convert_landmark_coordinates(const coord_t* src_landmark, mpp_landmark_t* dst_landmark,
                                uint32_t face_idx, uint32_t landmark_idx) {
    /* Set basic landmark properties */
    dst_landmark->clear = 0; /* don't clear landmark */
    dst_landmark->width = LANDMARK_POINT_SIZE; /* landmark point size */
    dst_landmark->stripe = false;

    /* Convert coordinates from detection space to view dimensions */
    dst_landmark->x = (src_landmark->x * DETECTION_ZONE_RECT_WIDTH / SCRFD_KPS_WIDTH) + BOXES_OFFSET_LEFT;
    dst_landmark->y = (src_landmark->y * DETECTION_ZONE_RECT_HEIGHT / SCRFD_KPS_HEIGHT) + BOXES_OFFSET_TOP;

    /* Encode face and landmark index in tag */
    dst_landmark->tag = (face_idx << 8) | landmark_idx;

    /* Set color based on landmark type */
    set_face_landmark_color(dst_landmark, landmark_idx);
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
            ret = SCRFDKPS_ProcessOutput(
                    inf_output,
                    app_priv->boxes,
                    NUM_BOXES_MAX,
                    false);
            if (ret != kStatus_Success)
                PRINTF("mpp_event_listener: process output error!");

            app_priv->inference_time_ms = inf_output->inference_time_ms;

            app_priv->detected_count = 0;
            /* count valid results and calculate face tilt/yaw */
            for (uint32_t i = 0; i < NUM_BOXES_MAX; i++)
            {
                if (app_priv->boxes[i].score > 0)
                {
                    app_priv->detected_count++;
                    process_face_orientation(app_priv, i);
                }
            }

            /* Update labeled rectangle element with detected faces and landmarks */
            if (app_priv->labrect_elem != 0) {
                mpp_element_params_t elem_params;
                memset(&elem_params, 0, sizeof(elem_params));

                /* Set rectangle parameters */
                elem_params.labels.max_rect = MAX_LABEL_RECTS;
                elem_params.labels.detected_rect = app_priv->detected_count + 1; /* +1 for detection zone */
                elem_params.labels.rectangles = app_priv->labels;

                boxes_to_rects(app_priv->boxes, NUM_BOXES_MAX, MAX_LABEL_RECTS, elem_params.labels.rectangles);

                /* Allocate landmarks array if not already done */
                static mpp_landmark_t landmarks[MAX_LABEL_RECTS * SCRFD_NUM_LANDMARKS];
                memset(landmarks, 0, sizeof(landmarks));

                /* params init */
                elem_params.labels.max_landmk = MAX_LABEL_RECTS * SCRFD_NUM_LANDMARKS;
                elem_params.labels.detected_landmk = 0;
                elem_params.labels.landmarks = landmarks;

                /* Fill landmarks for each detected face */
                uint32_t landmark_idx = 0;
                for (uint32_t face_idx = 0; face_idx < app_priv->detected_count && face_idx < MAX_LABEL_RECTS; face_idx++) {
                    if (app_priv->boxes[face_idx].score > 0) {
                        /* Add all landmarks for this face */
                        for (uint32_t lm_idx = 0; lm_idx < SCRFD_NUM_LANDMARKS && landmark_idx < (NUM_BOXES_MAX * SCRFD_NUM_LANDMARKS); lm_idx++) {
                        	convert_landmark_coordinates(&app_priv->boxes[face_idx].landmarks[lm_idx], &landmarks[landmark_idx],
                        	                                face_idx, lm_idx);
                        	landmark_idx++;
                            elem_params.labels.detected_landmk++;
                        }
                    }
                }

                /* Update the element with both rectangles and landmarks */
                mpp_element_update(app_priv->mp, app_priv->labrect_elem, &elem_params, true);
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
void print_results(user_data_t *user_data)
{
    if (Atomic_CompareAndSwap_u32(&user_data->accessing, 1, 0))
    {
        PRINTF("inference time %d (ms) \r\n", user_data->inference_time_ms);
        if (user_data->detected_count == 0)
        {
            PRINTF("No face detected! \r\n");
        }
        else
        {
            PRINTF("detected faces: %d \r\n", user_data->detected_count);
            for (int i = 0; i < user_data->detected_count; i++) {
                PRINTF("     -----------\r\n");
                PRINTF("     Face %d: \r\n", i);
                PRINTF("     Box score: %d%% \r\n", (int)(user_data->boxes[i].score * 100.0f));
                PRINTF("     Box coordinates: (%d,%d) to (%d,%d) \r\n",
                       (int)(user_data->boxes[i].left), (int)(user_data->boxes[i].top),
                       (int)(user_data->boxes[i].right), (int)(user_data->boxes[i].bottom));
                int yaw_deg  = (int)(user_data->face_yaw[i]  * 180.0f / M_PI);
                int tilt_deg = (int)(user_data->face_tilt[i] * 180.0f / M_PI);
                PRINTF("     Face Tilt: %d (degrees) \r\n", tilt_deg);
                PRINTF("     Face Yaw: %d (degrees) \r\n", yaw_deg);
                PRINTF("     -----------\r\n");
            }
        }

        mpp_stats_disable(MPP_STATS_GRP_API);
        PRINTF("CPU Load: %u(%%)\n\r", user_data->api_stats->api.cpu_load);
        mpp_stats_enable(MPP_STATS_GRP_API);

        __atomic_store_n(&user_data->accessing, 0, __ATOMIC_SEQ_CST);
    }
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

    /* Add camera element */
#if (SOURCE_STATIC_IMAGE == 1)
	static mpp_img_params_t img_params;
    memset(&img_params, 0, sizeof (mpp_img_params_t));
    img_params.format = SRC_IMAGE_FORMAT;
    img_params.width = SRC_IMAGE_WIDTH;
    img_params.height = SRC_IMAGE_HEIGHT;
    mpp_static_img_add(mp, &img_params, (void *)image_data, NULL);
#else
    mpp_camera_params_t camera_params;
    memset(&camera_params, 0, sizeof(mpp_camera_params_t));
    camera_params.height = APP_CAMERA_HEIGHT;
    camera_params.width = APP_CAMERA_WIDTH;
    camera_params.format = APP_CAMERA_FORMAT;

    ret = mpp_camera_add(mp, s_camera_name, &camera_params, NULL);
    if (ret) {
        PRINTF("Failed to add camera element\r\n");
        goto err;
    }
#endif /* SOURCE_STATIC_IMAGE == 1 */

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

    /* INFERENCE BRANCH: First do crop + resize + color convert for inference */
    static mpp_element_params_t infer_conv_params;
    memset(&infer_conv_params, 0, sizeof(infer_conv_params));
    /* pick GFX device */
    infer_conv_params.convert.dev_name = APP_GFX_BACKEND_INFER_NAME;
    /* set output buffer dims */
    infer_conv_params.convert.out_buf.width = SCRFD_KPS_WIDTH;
    infer_conv_params.convert.out_buf.height = SCRFD_KPS_HEIGHT;
	// crop center of image
    infer_conv_params.convert.crop.top = CROP_TOP;
    infer_conv_params.convert.crop.bottom = CROP_TOP + CROP_SIZE_TOP - 1;
    infer_conv_params.convert.crop.left = CROP_LEFT;
    infer_conv_params.convert.crop.right = CROP_LEFT + CROP_SIZE_LEFT - 1;
    infer_conv_params.convert.ops = MPP_CONVERT_CROP;
    /* color convert */
    infer_conv_params.convert.pixel_format = SCRFD_KPS_PIXEL_FORMAT;
    infer_conv_params.convert.ops = MPP_CONVERT_COLOR;
    /* resize: scaling parameters */
    infer_conv_params.convert.scale.width = SCRFD_KPS_WIDTH;
    infer_conv_params.convert.scale.height = SCRFD_KPS_HEIGHT;
    infer_conv_params.convert.ops |= MPP_CONVERT_SCALE;
    infer_conv_params.convert.stripe_in = false;
    infer_conv_params.convert.stripe_out = false; /* model takes full frames */

    static mpp_elem_handle_t infer_conv_h;
    ret = mpp_element_add(mp_split, MPP_ELEMENT_CONVERT, &infer_conv_params, &infer_conv_h);
    if (ret) {
        PRINTF("Failed to add element CONVERT for inference\n");
        goto err;
    }

    /* configure inference element with SCRFD_KPS model */
    mpp_element_params_t scrfd_kps_params;
    memset(&scrfd_kps_params, 0, sizeof(mpp_element_params_t));
    scrfd_kps_params.ml_inference.model_data = scrfd_kps_data;
    scrfd_kps_params.ml_inference.model_size = scrfd_kps_data_len;
    scrfd_kps_params.ml_inference.tensor_order = MPP_TENSOR_ORDER_NHWC;
    scrfd_kps_params.ml_inference.model_input_mean = SCRFD_KPS_INPUT_MEAN;
    scrfd_kps_params.ml_inference.model_input_std = SCRFD_KPS_INPUT_STD;
    scrfd_kps_params.ml_inference.type = MPP_INFERENCE_TYPE_TFLITE;
    scrfd_kps_params.ml_inference.inference_params.num_inputs = 1;
    scrfd_kps_params.ml_inference.inference_params.num_outputs = 9;
    scrfd_kps_params.stats = &scrfd_kps_stats;

    ret = mpp_element_add(mp_split, MPP_ELEMENT_INFERENCE, &scrfd_kps_params, NULL);
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

    /* Add labeled rectangle element for face detection visualization */
    memset(&elem_params, 0, sizeof(elem_params));
    memset(&user_data.labels, 0, sizeof(user_data.labels));

    /* params init */
    elem_params.labels.max_rect = MAX_LABEL_RECTS;
    elem_params.labels.detected_rect = 1;
    elem_params.labels.rectangles = user_data.labels;
    elem_params.labels.max_landmk = MAX_LABEL_RECTS * SCRFD_NUM_LANDMARKS;

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

        PRINTF("parameters of second image conversion for display branch: \r\n");
        PRINTF("elem_params.convert.scale.width = %d\r\n", elem_params.convert.scale.width);
        PRINTF("elem_params.convert.scale.height = %d\r\n", elem_params.convert.scale.height);
        PRINTF("elem_params.convert.out_buf.width = %d\r\n", elem_params.convert.out_buf.width);
        PRINTF("elem_params.convert.out_buf.height = %d\r\n", elem_params.convert.out_buf.height);

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

    TickType_t xLastPrintTime = 0, tick = 0;
    const TickType_t xFrequency = OUTPUT_PRINT_PERIOD_MS / portTICK_PERIOD_MS;
    uint32_t last_inf_frame_num = user_data.inference_frame_num;

    for (;;) {
        /* manage periodic print */
        tick = xTaskGetTickCount();
        if ((tick > (xLastPrintTime + xFrequency)) ||
            (last_inf_frame_num != user_data.inference_frame_num)) {
            if (last_inf_frame_num != user_data.inference_frame_num) {
                xLastPrintTime = tick;
                print_results(&user_data);
                last_inf_frame_num = user_data.inference_frame_num;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

err:
    for (;;) {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}
