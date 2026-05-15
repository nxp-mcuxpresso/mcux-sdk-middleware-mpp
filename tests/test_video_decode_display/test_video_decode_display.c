/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief H.264 Video Decode and Display Test Application
 *
 * @section test_overview Overview:
 * This test application demonstrates the H.264 video decode pipeline:
 *
 *   File Source (SD Card) → Video Decoder → Format Converter → Display
 *        (H.264)               (YUV420)         (RGB565)         (LCD)
 *
 * The pipeline reads an H.264 encoded video file from SD card, decodes it
 * using the OpenH264 library, converts the YUV420 output to RGB format,
 * and displays it on the LCD panel.
 *
 * @section test_setup Setup Instructions:
 * 1) Download a compatible H.264 test file:
 *    https://github.com/cisco/openh264/tree/v2.1.1/res/test_cif_P_CABAC_slice.264
 * 2) Rename the file to match H264_FILE_PATH configuration
 * 3) Copy the H.264 file to SD card root directory (H264_FILE_PATH)
 *
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"

#ifndef EMULATOR
/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "app.h"
#include "board.h"
#else
#include <stdio.h>
#define PRINTF printf
#define main app_main
#endif

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "test_config.h"
#include "hal_os.h"
#include "hal_utils.h"

/* test image include file */
#ifndef EMULATOR   /* i.MX RT1170 EVK */
#define IMAGE_WIDTH  DECODED_VIDEO_WIDTH
#define IMAGE_HEIGHT DECODED_VIDEO_HEIGHT
#define IMAGE_FORMAT DECODED_VIDEO_FORMAT
#endif /* EMULATOR */

#define H264_FILE_BUF_SIZE (128 * 1024)

/* set IMG_CONVERT_CPU or IMG_CONVERT_GPU flag to 1 to perform image conversion using CPU or GPU
 * (otherwise first gfx device in the list is used by default) */
#if !defined(IMG_CONVERT_CPU) && !defined(IMG_CONVERT_GPU)
#define IMG_CONVERT_CPU 0
#define IMG_CONVERT_GPU 0
#elif defined(IMG_CONVERT_CPU)
#define IMG_CONVERT_GPU 0
#elif defined(IMG_CONVERT_GPU)
#define IMG_CONVERT_CPU 0
#endif

#if (IMG_CONVERT_CPU == 1)
#define IMG_CONVERT_DEV_NAME "gfx_CPU"
#elif (IMG_CONVERT_GPU == 1)
#define IMG_CONVERT_DEV_NAME "gfx_GPU"
#else
/* pick default device from the first listed and supported by Hw */
#define IMG_CONVERT_DEV_NAME NULL
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
typedef struct _args_t {
    char display_name[32];
    mpp_pixel_format_t display_format;
    mpp_pixel_format_t image_format;
} args_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/

#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

/* define this flag to enable MPP stop and start */
#ifndef CONFIG_STOP_MPP
#define CONFIG_STOP_MPP 0
#endif

/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_video_decode_display_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_video_decode_display"
#endif

SDK_ALIGN(static uint8_t s_fileBuf[H264_FILE_BUF_SIZE], FSL_FEATURE_L1DCACHE_LINESIZE_BYTE);

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void main_task(void *params);

/*******************************************************************************
 * Code
 ******************************************************************************/

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

/*
 * @brief   Application entry point.
 */
int main(int argc, char *argv[]) {
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

	/* Init board hardware. */
#ifndef EMULATOR
    BOARD_Init();
#endif /* i.MX RT1170 EVK */

    PRINTF("****** TEST test_video_decode_display ******\r\n");
    PRINTF("****** PARAMS: CONFIG_STOP_MPP = [%d] ******\r\n", CONFIG_STOP_MPP);
    PRINTF("\n");

	args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

    /* Set input arguments*/
#ifdef EMULATOR
    strcpy(args->display_name, "opencv0");
    if (argc == 3) {
        if (strcmp(argv[1], "rgb_sim") &&
            strcmp(argv[1], "ir_sim") &&
            strcmp(argv[1], "yuv_sim")) {
                goto err;
        }

        if (!strncmp(argv[2], "rgb",3)) {
            args->image_format = MPP_PIXEL_RGB;
        } else if (!strncmp(argv[2], "gray",3)) {
            args->image_format = MPP_PIXEL_GRAY;
        } else if (!strncmp(argv[2], "yuv",3)) {
            args->image_format = MPP_PIXEL_YUYV;
        } else {
            PRINTF("Invalid pixel format %s\n", argv[2]);
            goto err;
        }
        args->display_format = args->image_format;
    } else {
        PRINTF("Invalid argument number %d\n", argc);
        goto err;
    }
#else /* i.MX RT1170 EVK */
    strcpy(args->display_name, APP_DISPLAY_NAME);
    args->image_format = APP_DISPLAY_FORMAT;
#endif /* EMULATOR */

	ret = xTaskCreate(
			main_task,
			"main_task",
			configMINIMAL_STACK_SIZE + 1000,
			(void *) args,
			APP_DEFAULT_PRIO,
			&handle);

err:
	if (pdPASS != ret)
	{
		PRINTF("Failed to create main_task task");
		while (1);
	}

	vTaskStartScheduler();
	for (;;)
		vTaskSuspend(NULL);

	return 0;
}

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {

    checksum_data_t *chksm = (checksum_data_t *)evt_data;
    static bool test_done = false;
    static int chksm_time = 0;  /* time of checksum */
    static int cnt = 0;
    int time;

    switch(evt) {
    case MPP_EVENT_INTERNAL_TEST_RESERVED:
    case MPP_EVENT_INVALID:
    default:
        /* nothing to do */
        break;
    }
    return 0;
}

/* Set params for image convert element */
static void set_img_convert_params(mpp_element_params_t *elem_params)
{
    memset(elem_params, 0, sizeof(mpp_element_params_t));

    elem_params->convert.dev_name = IMG_CONVERT_DEV_NAME;
    elem_params->convert.out_buf.height = APP_DISPLAY_HEIGHT;
    elem_params->convert.out_buf.width  = APP_DISPLAY_WIDTH;
    /* pixel format */
    elem_params->convert.pixel_format = APP_DISPLAY_FORMAT;
    /* scaling parameters */
    elem_params->convert.scale.width  = APP_DISPLAY_WIDTH;
    elem_params->convert.scale.height = APP_DISPLAY_HEIGHT;
    /* rotate */
    elem_params->convert.angle = APP_DISPLAY_LANDSCAPE_ROTATE;
    elem_params->convert.flip = FLIP_NONE;

    elem_params->convert.ops = MPP_CONVERT_COLOR;
    elem_params->convert.ops |= MPP_CONVERT_ROTATE;
    elem_params->convert.ops |= MPP_CONVERT_SCALE;

#if (IMG_FULL_SCREEN != 0)
    /* scaling parameters */
    elem_params->convert.scale.width  = APP_DISPLAY_WIDTH;
    elem_params->convert.scale.height = APP_DISPLAY_HEIGHT;
#endif
}

static void main_task(void *params) {
	args_t *args = (args_t *) params;
	int ret;
	bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;
    mpp_elem_handle_t convert_h = (mpp_elem_handle_t) NULL;

    PRINTF("[%s]\r\n", mpp_get_version());

    mpp_api_params_t api_param = {0};
#if ((defined APP_RC_CYCLE_INC) && (defined APP_RC_CYCLE_MIN))
    api_param.rc_cycle_inc = APP_RC_CYCLE_INC;
    api_param.rc_cycle_min = APP_RC_CYCLE_MIN;
#endif
    ret = mpp_api_init(&api_param);
    if (ret)
        goto err;

    /* Initialize storage (SD card + filesystem) */
    ret = mpp_storage_init();
    if (ret == MPP_SUCCESS)
    {
        uint64_t free_space, total_space;
        ret = mpp_storage_get_free_space(&free_space, &total_space);
        if (ret == MPP_SUCCESS)
        {
            /* in MB */
            uint32_t free_mb = (uint32_t)(free_space >> 20);
            uint32_t total_mb = (uint32_t)(total_space >> 20);
            PRINTF("SD Card - Free: %u MB, Total: %u MB\r\n", free_mb, total_mb);
        }
        else
        {
            PRINTF("SD Card - Failed to get storage space information\r\n");
        }
    }
    else
    {
        PRINTF("SD Card initialization failed\r\n");
        goto err;
    }

    mpp_t mp;
    mpp_params_t mpp_params;
    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.evt_callback_f = &mpp_event_listener;
    mpp_params.mask = MPP_EVENT_ALL;
    mpp_params.cb_userdata = NULL;
    mpp_params.exec_flag = MPP_EXEC_RC;
	mp = mpp_create(&mpp_params, &ret);
	if (mp == MPP_INVALID)
		goto err;

    /* Set filesrc element parameters */
    mpp_filesrc_params_t filesrc_params;
    memset(&filesrc_params, 0, sizeof(mpp_filesrc_params_t));
    filesrc_params.filepath = H264_FILE_PATH;
    filesrc_params.file_buffer_size = H264_FILE_BUF_SIZE;
    filesrc_params.loop = H264_FILE_LOOP;
    filesrc_params.slice_search_func = search_h264_nalu;

    ret = mpp_filesrc_add(mp, &filesrc_params, s_fileBuf, NULL);
	if (ret) {
		PRINTF("Failed to add filesrc");
		goto err;
	}

    mpp_elem_handle_t mpp_elem_handle = (mpp_elem_handle_t) NULL;
    mpp_element_params_t vdec_elem_params;
    memset(&vdec_elem_params, 0, sizeof(mpp_element_params_t));
    vdec_elem_params.decode.width = IMAGE_WIDTH;
    vdec_elem_params.decode.height = IMAGE_HEIGHT;
    vdec_elem_params.decode.out_format = IMAGE_FORMAT;
    ret = mpp_element_add(mp, MPP_ELEMENT_VIDEO_DECODE, &vdec_elem_params, &mpp_elem_handle);
    if (ret ) {
        PRINTF("Failed to add video decode element\n");
        goto err;
    }

    /* Add element convert */
    mpp_element_params_t elem_params;
    set_img_convert_params(&elem_params);
    ret = mpp_element_add(mp, MPP_ELEMENT_CONVERT, &elem_params, &convert_h);
    if (ret)
    {
        PRINTF("Failed to add element CONVERT\n");
        goto err;
    }

	/* Set display's parameters */
	mpp_display_params_t disp_params;
	memset(&disp_params, 0 , sizeof(disp_params));
    disp_params.height = APP_DISPLAY_HEIGHT;
    disp_params.width = APP_DISPLAY_WIDTH;
	disp_params.format = args->image_format;
	disp_params.stripe = stripe_mode;

    /* if display buffer is remote, define the output position in the screen */
#if(defined(APP_DISPLAY_REMOTE_FB) && (APP_DISPLAY_REMOTE_FB == 1))
    disp_params.width = SRC_IMAGE_WIDTH;
    disp_params.height = SRC_IMAGE_HEIGHT;
    disp_params.top = OUTPUT_WINDOW_TOP;
    disp_params.left = OUTPUT_WINDOW_LEFT;
#endif

	ret = mpp_display_add(mp, args->display_name, &disp_params);
	if (ret) {
		PRINTF("Failed to add display\n");
		goto err;
	}
	ret = mpp_start(mp, 1, false);
	if (ret) {
		PRINTF("Failed to start pipeline\n");
		goto err;
	}

#if (CONFIG_STOP_MPP == 1)
	/* run for 3 seconds  */
	vTaskDelay(3000/portTICK_PERIOD_MS);

	/* stop the pipeline */
	ret = mpp_stop(mp);
	if (ret) {
	    PRINTF("Failed to stop pipeline\n");
	    goto err;
	}
	/* wait 6 seconds */
	vTaskDelay(6000/portTICK_PERIOD_MS);

	/* restart the pipeline */
	ret = mpp_start(mp, 0, false);
	if (ret) {
	    PRINTF("Failed to restart pipeline\n");
	    goto err;
	}
#endif

    /* pause application task */
    vTaskSuspend(NULL);
err:
	for (;;)
	{
		PRINTF("Error building application pipeline : ret %d\r\n", ret);
		vTaskSuspend(NULL);
	}
}

