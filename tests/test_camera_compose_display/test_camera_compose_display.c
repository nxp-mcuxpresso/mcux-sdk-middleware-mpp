/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * 2D camera -> image converter -> image compose -> display
 * The camera view finder is displayed with a blue logo and green text overlay
 * The position of logo and text is periodically updated.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"
#include "stdio.h"

/* NXP includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"
#include "fsl_cache.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"
#include "common/draw_text.h"
#include "hal_os.h"

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

#include "images/tiger_yuyv_jpg.h"

#include "images/NXP_Logo_RGB_Colour_320.h"

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1
#ifdef HAL_ENABLE_CAMERA
#define FRAME_WIDTH     APP_CAMERA_WIDTH
#define FRAME_HEIGHT    APP_CAMERA_HEIGHT
#else
#define FRAME_WIDTH     SRC_IMAGE_TIGER_JPEG_WIDTH
#define FRAME_HEIGHT    SRC_IMAGE_TIGER_JPEG_HEIGHT
#endif

#define FRAME_ASP_RATIO    ((float)FRAME_WIDTH / FRAME_HEIGHT)

/* compile option for rotating the display composition */
#define ROTATE_COMPOSE 270  /* 90 or 270 degrees */

#define LOGO_WIDTH     (320)
#define LOGO_HEIGHT    (150)
#define LOGO_BPP       (3)
#ifndef ROTATE_COMPOSE  /* top-left corner of non-rotated display */
    #define LOGO_LEFT_POS  (APP_DISPLAY_WIDTH - LOGO_WIDTH)
    #define LOGO_TOP_POS   (0)
    #define LOGO_RIGHT_POS (LOGO_LEFT_POS + LOGO_WIDTH - 1)
    #define LOGO_BOTTOM_POS (LOGO_TOP_POS + LOGO_HEIGHT - 1)
#else   /* bottom-right corner of non-rotated display */
#if (ROTATE_COMPOSE == 90)  /* 90 deg*/
    #define LOGO_LEFT_POS  (APP_DISPLAY_WIDTH - LOGO_HEIGHT)
    #define LOGO_TOP_POS   (APP_DISPLAY_HEIGHT - LOGO_WIDTH)
    #define LOGO_RIGHT_POS (LOGO_LEFT_POS + LOGO_HEIGHT - 1)
    #define LOGO_BOTTOM_POS (LOGO_TOP_POS + LOGO_WIDTH - 1)
#elif (ROTATE_COMPOSE == 270)  /* 270 deg */
    #define LOGO_LEFT_POS  (0)
    #define LOGO_TOP_POS   (0)
    #define LOGO_RIGHT_POS (LOGO_HEIGHT - 1)
    #define LOGO_BOTTOM_POS (LOGO_WIDTH - 1)
#endif
#endif

#define TEXT_WIDTH     (320)
#define TEXT_HEIGHT    (570)
#define TEXT_BPP       (2)
#ifndef ROTATE_COMPOSE
    #define TEXT_LEFT_POS  (APP_DISPLAY_WIDTH - TEXT_WIDTH)
    #define TEXT_TOP_POS   (LOGO_HEIGHT)
    #define TEXT_RIGHT_POS (TEXT_LEFT_POS + TEXT_WIDTH - 1)
    #define TEXT_BOTTOM_POS (TEXT_TOP_POS + TEXT_HEIGHT - 1)
#else
#if (ROTATE_COMPOSE == 90)  /* 90 deg*/
    #define TEXT_LEFT_POS  (APP_DISPLAY_WIDTH - TEXT_HEIGHT - LOGO_HEIGHT)
    #define TEXT_TOP_POS   (APP_DISPLAY_HEIGHT - TEXT_WIDTH)
    #define TEXT_RIGHT_POS (TEXT_LEFT_POS + TEXT_HEIGHT - 1)
    #define TEXT_BOTTOM_POS (TEXT_TOP_POS + TEXT_WIDTH - 1)
#elif (ROTATE_COMPOSE == 270)  /* 270 deg */
    #define TEXT_LEFT_POS  (LOGO_HEIGHT)
    #define TEXT_TOP_POS   (0)
    #define TEXT_RIGHT_POS (TEXT_LEFT_POS + TEXT_HEIGHT - 1)
    #define TEXT_BOTTOM_POS (TEXT_TOP_POS + TEXT_WIDTH - 1)
#endif
#endif

__ALIGNED(64) uint8_t  g_text_img[TEXT_WIDTH*TEXT_HEIGHT*TEXT_BPP];

#define MAX_STRING_SIZE 64
#define MAX_WORD_SIZE 32

/*******************************************************************************
 * Definitions
 ******************************************************************************/

typedef struct _args_t {
    char camera_name[MAX_WORD_SIZE];
    char display_name[MAX_WORD_SIZE];
    mpp_pixel_format_t camera_format;
    mpp_pixel_format_t display_format;
} args_t;

typedef struct {
	char name[MAX_WORD_SIZE];
	float draw_txt_time;
	int compose_time;
	char rotation[MAX_WORD_SIZE];
} text_info_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void app_task(void *params);

/*******************************************************************************
 * Code
 ******************************************************************************/
static void draw_text_area(void* buf, uint32_t size, text_info_t* text)
{
    int space4items  = get_font_height() + 6;
    int start_y_middle = get_font_height()/2 + 2;
    int row_indentation = get_font_width()/2;
    char tmp[MAX_STRING_SIZE];
    text_context_t ctx;

    /*Clear the buffer. */
    memset(buf,0,size);
    init_text_buf(&ctx, buf, TEXT_WIDTH, TEXT_HEIGHT, TEXT_WIDTH);

    draw_text_line(&ctx, "User name:",row_indentation,start_y_middle);
    start_y_middle += get_font_height();
    memset(tmp,0,sizeof(tmp));
    snprintf(tmp,sizeof(tmp),"     %s",text->name);
    draw_text_line(&ctx, tmp, row_indentation, start_y_middle );

    start_y_middle += space4items;
    draw_text_line(&ctx, "Draw text time:", row_indentation, start_y_middle);
    start_y_middle += get_font_height();
    memset(tmp,0,sizeof(tmp));
    snprintf(tmp,sizeof(tmp),"     %d.%dms",
        (int)(text->draw_txt_time),
        (int)(text->draw_txt_time * 100)%100);
    draw_text_line(&ctx, tmp, row_indentation, start_y_middle );

    start_y_middle += space4items;
    draw_text_line(&ctx, "Compose time:",row_indentation,start_y_middle);
    start_y_middle += get_font_height();
    memset(tmp,0,sizeof(tmp));
    snprintf(tmp,sizeof(tmp),"     %dms",text->compose_time);
	draw_text_line(&ctx, tmp, row_indentation, start_y_middle );

	start_y_middle += space4items;
	draw_text_line(&ctx, "Rotation:",row_indentation,start_y_middle);
    start_y_middle += get_font_height();
    memset(tmp,0,sizeof(tmp));
    snprintf(tmp,sizeof(tmp),"     %s",text->rotation);
	draw_text_line(&ctx, tmp, row_indentation, start_y_middle );

	//make sure all data are pushed to the memory.
	XCACHE_CleanCacheByRange((uint32_t)buf,size);
}

/*!
 * @brief Application entry point.
 */
int main(int argc, char *argv[])
{
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

    /* Init board hardware. */
    BOARD_Init();

    PRINTF("****** TEST test_camera_compose_display ******\n");
    PRINTF("\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

    strcpy(args->display_name, APP_DISPLAY_NAME);
    strcpy(args->camera_name, APP_CAMERA_NAME);
    args->camera_format = APP_CAMERA_FORMAT;
    args->display_format = APP_DISPLAY_FORMAT;

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

    vTaskStartScheduler();
    for (;;)
        vTaskSuspend(NULL);
    return 0;
}

static void app_task(void *params) {

    int ret;
    mpp_elem_handle_t elem = 0;
    args_t *args = (args_t *) params;
    text_info_t txt_info;
    strcpy(txt_info.name, "Tiger");
    txt_info.draw_txt_time = 0.0f;
    txt_info.compose_time = 0;
#ifndef ROTATE_COMPOSE
    strcpy(txt_info.rotation, "0 degree");
#elif (ROTATE_COMPOSE == 90)
    strcpy(txt_info.rotation, "90 degree");
#elif (ROTATE_COMPOSE == 270)
    strcpy(txt_info.rotation, "270 degree");
#else
#error "Invalid ROTATE_COMPOSE configuration"
#endif    
    PRINTF("[%s]\r\n", mpp_get_version());

    /* init API */
    static mpp_api_params_t api_param = {0};
#if ((defined APP_RC_CYCLE_INC) && (defined APP_RC_CYCLE_MIN))
    /* fine-tune RC cycle for stripe mode */
    api_param.rc_cycle_inc = APP_RC_CYCLE_INC;
    api_param.rc_cycle_min = APP_RC_CYCLE_MIN;
#endif

    ret = mpp_api_init(&api_param);
    if (ret)
        goto err;

    mpp_t mp;
    mpp_params_t mpp_params;
    mpp_element_params_t elem_params;

    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.exec_flag = MPP_EXEC_RC;
    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
        goto err;

#ifdef HAL_ENABLE_CAMERA
    mpp_camera_params_t cam_params;
    memset(&cam_params, 0 , sizeof(cam_params));
    cam_params.height = APP_CAMERA_HEIGHT;
    cam_params.width =  APP_CAMERA_WIDTH;
    cam_params.format = args->camera_format;
    cam_params.fps    = 30;
    ret = mpp_camera_add(mp, args->camera_name, &cam_params, NULL);
    if (ret) {
        PRINTF("Failed to add camera %s\n", args->camera_name);
        goto err;
    }
#else
    /* add image static */
    /* Set static image element parameters */
    mpp_img_params_t img_params ;
    memset(&img_params, 0 , sizeof(img_params));
    img_params.height = FRAME_HEIGHT;
    img_params.width =  FRAME_WIDTH;
    img_params.format = MPP_PIXEL_JPEG;
    img_params.compressed_size = tiger_jpeg_data_len;
    ret = mpp_static_img_add(mp, &img_params, (void *)tiger_yuv_jpg, NULL);
    if (ret)
    {
        PRINTF("Failed to add static image");
        goto err;
    }

    /* Add element jpeg decode */
    memset(&elem_params, 0, sizeof(mpp_element_params_t));
    elem_params.decode.dev_name = IMG_DECODE_DEV_NAME;
    elem_params.decode.width = img_params.width;
    elem_params.decode.height = img_params.height;

    if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_CPU") == 0)
        elem_params.decode.out_format = MPP_PIXEL_BGR; /* TODO auto detect */
    else if (strcmp(IMG_DECODE_DEV_NAME, "jpeg_HW") == 0)
        elem_params.decode.out_format = MPP_PIXEL_YUYV; /* TODO auto detect */

    ret = mpp_element_add(mp, MPP_ELEMENT_IMG_DECODE, &elem_params, NULL);
    if (ret)
    {
        PRINTF("Failed to add element DECODE\n");
        goto err;
    }
#endif
    /* convert element params */
    memset(&elem_params, 0, sizeof(elem_params));
    /* pick default device from the first listed and supported by Hw */
    elem_params.convert.dev_name = NULL;

    /* compose element params */
    mpp_element_params_t elem_params_compose;
    memset(&elem_params_compose, 0, sizeof(mpp_element_params_t));
    mpp_stats_t compose_stats = {0};
    elem_params_compose.stats = &compose_stats;

    draw_text_area(g_text_img, sizeof(g_text_img), &txt_info);

        /* Output params */
#if (defined ROTATE_COMPOSE) && (ROTATE_COMPOSE == 90)
    elem_params_compose.compose.out_angle = ROTATE_90;
#elif (defined ROTATE_COMPOSE) && (ROTATE_COMPOSE == 270)
    elem_params_compose.compose.out_angle = ROTATE_270;
#else
    elem_params_compose.compose.out_angle = ROTATE_0;
#endif
    elem_params_compose.compose.out_flip = FLIP_NONE;
    elem_params_compose.compose.out_format = args->display_format;
    /* TODO set output buffer width and height */

    /* Logo image parameters */
    elem_params_compose.compose.logo_img_params.width = LOGO_WIDTH;
    elem_params_compose.compose.logo_img_params.height = LOGO_HEIGHT;
    elem_params_compose.compose.logo_img_params.format = MPP_PIXEL_RGB;
    elem_params_compose.compose.logo_img_params.stripe = false;
    elem_params_compose.compose.logo_buffer = NXP_Logo_RGB888_Colour_320_map;

    /* Logo area - top left corner */
    elem_params_compose.compose.logo_area.left = LOGO_LEFT_POS;
    elem_params_compose.compose.logo_area.top = LOGO_TOP_POS;
    elem_params_compose.compose.logo_area.right = LOGO_RIGHT_POS;
    elem_params_compose.compose.logo_area.bottom = LOGO_BOTTOM_POS;

    /* Text image parameters */
    elem_params_compose.compose.txt_img_params.width = TEXT_WIDTH;
    elem_params_compose.compose.txt_img_params.height = TEXT_HEIGHT;
    elem_params_compose.compose.txt_img_params.format = MPP_PIXEL_RGB565;
    elem_params_compose.compose.txt_img_params.stripe = false;
    elem_params_compose.compose.txt_buffer = g_text_img;
    
    /* Text area - bottom center */
    elem_params_compose.compose.txt_area.left = TEXT_LEFT_POS;
    elem_params_compose.compose.txt_area.top = TEXT_TOP_POS;
    elem_params_compose.compose.txt_area.right = TEXT_RIGHT_POS;
    elem_params_compose.compose.txt_area.bottom = TEXT_BOTTOM_POS;

    /* Input area - full frame */
#ifndef ROTATE_COMPOSE    
    elem_params_compose.compose.input_area.left = 0;
    elem_params_compose.compose.input_area.top = 0;
    elem_params_compose.compose.input_area.right = FRAME_WIDTH - 1;
    elem_params_compose.compose.input_area.bottom = FRAME_HEIGHT - 1;
#else
    if (ROTATE_COMPOSE == 90)
    {
        elem_params_compose.compose.input_area.left = 0;
        elem_params_compose.compose.input_area.top = 0;
        elem_params_compose.compose.input_area.right = APP_DISPLAY_WIDTH - 1;
        elem_params_compose.compose.input_area.bottom = (int)(APP_DISPLAY_WIDTH * FRAME_ASP_RATIO);
    }
    else if (ROTATE_COMPOSE == 270)
    {
        elem_params_compose.compose.input_area.left = 0;
        elem_params_compose.compose.input_area.top = APP_DISPLAY_HEIGHT - (int)(APP_DISPLAY_WIDTH * FRAME_ASP_RATIO);
        elem_params_compose.compose.input_area.bottom = APP_DISPLAY_HEIGHT - 1;
        elem_params_compose.compose.input_area.right = APP_DISPLAY_WIDTH - 1;
    }
    else
        PRINTF("Unsupported rotation angle\r\n");
#endif
    /* retrieve the element handle while add api */
    ret = mpp_element_add(mp, MPP_ELEMENT_IMG_COMPOSE, &elem_params_compose, &elem);
    if (ret) {
        PRINTF("Failed to add element IMG_COMPOSE (0x%x)\r\n", ret);
        goto err;
    }

    mpp_display_params_t disp_params;
    memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.width = APP_DISPLAY_WIDTH;
    disp_params.format = args->display_format;
    ret = mpp_display_add(mp, args->display_name, &disp_params);
    if (ret) {
        PRINTF("Failed to add display %s\n", args->display_name);
        goto err;
    }

    mpp_stats_enable(MPP_STATS_GRP_ELEMENT);

    ret = mpp_start(mp, 1, false);
    if (ret) {
        PRINTF("Failed to start pipeline\n");
        goto err;
    }

    /* update example - animate logo and text positions */
    const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
    uint32_t var = 0;
#ifdef COMPOSE_ANIMATION
    int logo_step_x = 2, logo_step_y = 1;
    int text_step_x = 1;
#endif
#ifdef TOGGLE_TEXT_VISIBILITY
    bool text_visible = true;
#endif

    do {
#ifdef COMPOSE_ANIMATION
        /* Move logo around the screen */
        if ((elem_params_compose.compose.logo_area.right + logo_step_x) >= APP_DISPLAY_WIDTH || 
            (elem_params_compose.compose.logo_area.left + logo_step_x) <= 0) {
            logo_step_x = -logo_step_x;
        }
        if ((elem_params_compose.compose.logo_area.bottom + logo_step_y) >= APP_DISPLAY_HEIGHT/2 || 
            (elem_params_compose.compose.logo_area.top + logo_step_y) <= 0) {
            logo_step_y = -logo_step_y;
        }
        
        elem_params_compose.compose.logo_area.left += logo_step_x;
        elem_params_compose.compose.logo_area.right += logo_step_x;
        elem_params_compose.compose.logo_area.top += logo_step_y;
        elem_params_compose.compose.logo_area.bottom += logo_step_y;

        /* Move text horizontally */
        if ((elem_params_compose.compose.txt_area.right + text_step_x) >= APP_DISPLAY_WIDTH || 
            (elem_params_compose.compose.txt_area.left + text_step_x) <= 0) {
            text_step_x = -text_step_x;
        }
        
        elem_params_compose.compose.txt_area.left += text_step_x;
        elem_params_compose.compose.txt_area.right += text_step_x;
#endif  /* COMPOSE_ANIMATION */
        /* redraw the text buffer every 2 seconds */
        if ((var % 20) == 0) {
            /* get composition time update */
            mpp_stats_disable(MPP_STATS_GRP_ELEMENT);
            txt_info.compose_time = compose_stats.elem.elem_exec_time;
            mpp_stats_enable(MPP_STATS_GRP_ELEMENT);
            int start_time = hal_get_exec_time();
            draw_text_area(g_text_img, sizeof(g_text_img), &txt_info);
            txt_info.draw_txt_time = hal_get_exec_time() - start_time;
        }

#ifdef TOGGLE_TEXT_VISIBILITY
        /* Toggle text visibility every 2 seconds (20 * 100ms) */
        if ((var % 20) == 0) {
            text_visible = !text_visible;
            if (text_visible) {
                elem_params_compose.compose.txt_buffer = g_text_img;
                PRINTF("Text visible\r\n");
            } else {
                elem_params_compose.compose.txt_buffer = NULL;
                PRINTF("Text hidden\r\n");
            }
        }
#endif
        /* Update the compose element with new positions */
        mpp_element_update(mp, elem, &elem_params_compose);
        var++;
        vTaskDelay(xDelay);

    } while (true);

    /* pause application task */
    vTaskSuspend(NULL);

err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}

