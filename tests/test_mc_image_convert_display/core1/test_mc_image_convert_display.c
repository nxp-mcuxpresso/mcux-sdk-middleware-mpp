/*
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> image converter -> image display
 * The static image is displayed on screen.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"

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

#include "hal_debug.h"
#include "hal_utils.h"
#include "hal_os.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "test_config.h"

#include "rpmsg_lite.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define CROP_RATIO_MAX  4
#define CROP_RATIO_INIT 3
#define CROP_RATIO_MIN  2

#define SPEEDX 2
#define SPEEDY 1
#define ZOOMINC 1

#define TEST_CHECK_PERIOD_MS 1000

#ifdef TEST_AUTO_DISABLE
#define TEST_MODE ""
#else
#define TEST_MODE "AUTO"
#endif

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

/* set this flag to perform an image rotation (1: 90°, 2: 180°, 3: 270°) */
#ifndef IMG_ROTATE
#define IMG_ROTATE 0
#endif

/* set this flag to perform an image flip operation (1: horizontal, 2: vertical, 3: both) */
#ifndef IMG_FLIP
#define IMG_FLIP 0
#endif

/* set this flag to perform image crop and output window */
#ifndef IMG_CROP
#define IMG_CROP 0
#endif

/* set this flag to perform dynamic image crop window */
#ifndef IMG_DYN_CROP
#define IMG_DYN_CROP 0
#endif

/* set this flag to 1 to perform image scaling to display dimensions */
#ifndef IMG_FULL_SCREEN
#define IMG_FULL_SCREEN 0
#endif

/* set this flag to 1 to process image stripe by stripe */
#ifndef APP_STRIPE_MODE
#define APP_STRIPE_MODE 0
#endif

#if ( defined(APP_DISPLAY_REMOTE_FB) && (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1))
    #define DISP_BUF_WIDTH SRC_IMAGE_WIDTH
    #define DISP_BUF_HEIGHT SRC_IMAGE_HEIGHT
#else
    #define DISP_BUF_WIDTH APP_DISPLAY_WIDTH
    #define DISP_BUF_HEIGHT APP_DISPLAY_HEIGHT
#endif  /* (APP_DISPLAY_REMOTE_FB == 1) && (IMG_FULL_SCREEN != 1)) */

#if APP_CONFIG
#define ARG2STR(x) #x
#define CONFIG2STR(x) ARG2STR(x)
#define TC_NAME "test_image_convert_display_config" CONFIG2STR(APP_CONFIG)
#else
#define TC_NAME "test_image_convert_display"
#endif

#define APP_RPMSG_READY_EVENT_DATA      1

typedef struct _args_t {
    char display_name[32];
    mpp_pixel_format_t display_format;
    mpp_pixel_format_t source_format;
} args_t;

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
/** Default priority for application tasks
   Tasks created by the application have a lower priority than pipeline tasks by default.
   Pipeline_task_max_prio in mpp_api_params_t structure should be adjusted with other application tasks.*/
#define APP_DEFAULT_PRIO        1
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void app_task(void *params);

static struct rpmsg_lite_instance *rpmsg_inst = NULL;

static mpp_stats_t mp_stats = {0};

/*******************************************************************************
 * Code
 ******************************************************************************/

int mpp_event_listener(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data) {
    checksum_data_t *chksm;
    static bool chksm_ok = false;
    static bool chksm_done = false;
    static int count = 0;   /* frame counter, to ignore first frames */
    static int chksm_time = 0;  /* time of checksum */

    switch(evt) {
    case MPP_EVENT_INTERNAL_TEST_RESERVED:
        chksm = (checksum_data_t *)evt_data;
        if (chksm == NULL) {
            return 0;
        }
#if defined(CHECKSUM_TYPE_EXPECTED_PISANO) && (CHECKSUM_TYPE_EXPECTED_PISANO == 1)
        if (chksm->type != CHECKSUM_TYPE_PISANO) {
            PRINTF("ERROR: checksum calculated should be using PISANO for MCXN CPUs\n");
            return 0;
        }
#else
        if (chksm->type != CHECKSUM_TYPE_CRC_ELCDIF) {
            PRINTF("ERROR: checksum calculated should be using CRC LCDIF\n");
            return 0;
        }
#endif
        /* if check period elapsed, test again */
        int time = hal_tick_to_ms(hal_get_ostick());
        if (time > chksm_time + TEST_CHECK_PERIOD_MS)
        {
            chksm_done = false;
            chksm_time = time;
        }
        /* verify checksum if needed */
        if (!chksm_done && count > 1)
        {
            PRINTF("\r\nStart %s\r\n", TC_NAME);
            chksm_done = true;
            chksm_ok = ((chksm->value == EXPECTED_CHECKSUM) || (APP_STRIPE_MODE > 0)
                             || (EXPECTED_CHECKSUM == 0));  /* ignore checksum for stripes */
            if (chksm_ok)
            {
                if (APP_STRIPE_MODE > 0)
                    PRINTF("APP_STRIPE_MODE enabled. Skip checksum validation\r\n");
                if (EXPECTED_CHECKSUM == 0)
                    PRINTF("EXPECTED_CHECKSUM is 0; computed checksum is 0x%x\r\nSkip checksum validation\r\n",
                           chksm->value);
                PRINTF("%s - PASSED\r\n", TC_NAME);
            }
            else
            {
                PRINTF("Bad checksum 0x%08x\r\n", chksm->value);
                PRINTF("%s - FAILED\r\n", TC_NAME);
            }
            PRINTF("%s finished\r\n", TC_NAME);
        }
        count++;
        break;

    case MPP_EVENT_INVALID:
    default:
        /* nothing to do */
        break;
    }

    return 0;
}

/*!
 * @brief Application entry point.
 */
int main(int argc, char *argv[])
{
    BaseType_t ret = pdFAIL;
    TaskHandle_t handle = NULL;

    /* Initialize standard SDK demo application pins */
    BOARD_InitHardware();

#ifdef MCMGR_USED
    /* Initialize MCMGR before calling its API */
    (void)MCMGR_Init();
#endif /* MCMGR_USED */

    PRINTF("****** %s TEST test_image_convert_display ******\r\n", TEST_MODE);
    PRINTF("****** PARAMS: IMG_CONVERT_CPU = [%d] ******\r\n", IMG_CONVERT_CPU);
    PRINTF("****** PARAMS: IMG_CONVERT_GPU = [%d] ******\r\n", IMG_CONVERT_GPU);
    PRINTF("****** PARAMS: IMAGE_NAME = [%s] ******\r\n", IMAGE_NAME);
    PRINTF("****** PARAMS: IMAGE_FORMAT = [%d] ******\r\n", SRC_IMAGE_FORMAT);
    PRINTF("****** PARAMS: IMG_ROTATE = [%d] ******\r\n", IMG_ROTATE);
    PRINTF("****** PARAMS: IMG_FLIP = [%d] ******\r\n", IMG_FLIP);
    PRINTF("****** PARAMS: IMG_CROP = [%d] ******\r\n", IMG_CROP);
    PRINTF("****** PARAMS: IMG_FULL_SCREEN = [%d] ******\r\n", IMG_FULL_SCREEN);
    PRINTF("\r\n");

    args_t *args = pvPortMalloc(sizeof(args_t));
    if (!args) {
        PRINTF("Allocation failed\n");
        goto err;
    }

    strcpy(args->display_name, APP_DISPLAY_NAME);
    args->display_format = APP_DISPLAY_FORMAT;
    args->source_format = SRC_IMAGE_FORMAT;

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

#if (IMG_DYN_CROP == 1)
static int dyn_crop_loop(mpp_t mp, mpp_elem_handle_t convert_h, mpp_element_params_t * pelem_params)
{
    int ret = 0;
    int crop_height = SRC_IMAGE_HEIGHT / CROP_RATIO_INIT;
    int crop_width = SRC_IMAGE_WIDTH / CROP_RATIO_INIT;
    int crop_x = SRC_IMAGE_WIDTH / CROP_RATIO_INIT, crop_y = 0;
    int speedx = SPEEDX, speedy = SPEEDY, zoominc = ZOOMINC;
    for (;;)
    {
        vTaskDelay(20);
        /* compute new position */
        crop_x += speedx;
        crop_y += speedy;
        crop_height += zoominc;
        crop_width += zoominc;
        /* inverse speed when reaching border */
        if (crop_height + crop_y >= SRC_IMAGE_HEIGHT)
        {
            speedy = 0 - SPEEDY;
            crop_y += speedy;
        }
        else if (crop_y <= 0)
        {
            speedy = SPEEDY;
            crop_y += speedy;
        }
        if (crop_width + crop_x >= SRC_IMAGE_WIDTH)
        {
            speedx = 0 - SPEEDX;
            crop_x += speedx;
        }
        else if (crop_x <= 0)
        {
            speedx = SPEEDX;
            crop_x += speedx;
        }
        /* inverse zoom when reaching limits */
        if ((crop_width > SRC_IMAGE_WIDTH / CROP_RATIO_MIN) || (crop_width < SRC_IMAGE_WIDTH / CROP_RATIO_MAX))
        {
            zoominc = 0 - zoominc;
        }
        pelem_params->convert.crop.top = crop_y;
        pelem_params->convert.crop.left = crop_x;
        pelem_params->convert.crop.bottom = crop_height + crop_y - 1;
        pelem_params->convert.crop.right = crop_width + crop_x - 1;
        pelem_params->convert.ops |= MPP_CONVERT_CROP;
        ret = mpp_element_update(mp, convert_h, pelem_params, true);
        if (ret) {
            PRINTF("Failed mpp_element_update\r\n");
            break;
        }
    }
    return ret;
}
#endif

static void app_task(void *params)
{
    int src_width = SRC_IMAGE_WIDTH;
    int src_height = SRC_IMAGE_HEIGHT;
    bool stripe_mode = (APP_STRIPE_MODE > 0)? true : false;
    int ret = 0;
    args_t *args = (args_t *) params;

    PRINTF("[%s]\r\n", mpp_get_version());

    rpmsg_inst = mpp_secondary_core_rpmsg_init();
    if (rpmsg_inst == NULL) {
        PRINTF("RPMSG init failed\r\n");
        goto err;
    }
    PRINTF("RPMsg intialized\r\n");
    PRINTF("RPMsg shared mem base: 0x%x\r\n", (uint32_t) rpmsg_inst->sh_mem_base);

    /* add support for params */
    ret = mpp_api_init(NULL);
    if (ret)
        goto err;

    mpp_t mp;
    mpp_params_t mpp_params;
    memset(&mpp_params, 0, sizeof(mpp_params));
    mpp_params.exec_flag = MPP_EXEC_RC;
    mpp_params.evt_callback_f = &mpp_event_listener;
    mpp_params.mask = MPP_EVENT_ALL;
    mpp_params.stats = &mp_stats;
    mp = mpp_create(&mpp_params, &ret);
    if (mp == MPP_INVALID)
	goto err;

    /* add image static */
    /* Set static image element parameters */
    mpp_img_params_t img_params ;
    memset(&img_params, 0 , sizeof(img_params));
    img_params.height = src_height;
    img_params.width =  src_width;
    img_params.format = args->source_format;
    img_params.stripe = stripe_mode;
    ret = mpp_static_img_add(mp, &img_params, (void *)image_data, NULL);
    if (ret) {
        PRINTF("Failed to add static image");
        goto err;
    }

    /* add mc sink element */
    static mpp_mc_params_t mc_sink_params;
    memset(&mc_sink_params, 0, sizeof(mc_sink_params));
    mc_sink_params.rpmsg_inst = rpmsg_inst;
    mc_sink_params.remote_event_data = MPP_MCMGR_EVENT_DATA_START;
    mc_sink_params.local_rpmsg_addr = MPP_RPMSG_EPT_ADDR_CORE1_START;
    mc_sink_params.remote_rpmsg_addr = MPP_RPMSG_EPT_ADDR_CORE0_START;
    ret = mpp_mc_sink_add(mp, &mc_sink_params);
    if (ret) {
        PRINTF("Failed to add element SINK_MC\n");
        goto err;
    }

    mpp_stats_enable(MPP_STATS_GRP_MPP);

    ret = mpp_start(mp, 1, false);
    if (ret) {
        PRINTF("Failed to start pipeline\r\n");
        goto err;
    }

#if (IMG_DYN_CROP == 1)
    ret = dyn_crop_loop(mp, convert_h, &elem_params);
    if (ret) {
        mpp_stop(mp);
        goto err;
    }
#endif

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(TEST_CHECK_PERIOD_MS));
#ifdef DISPLAY_FPS
        mpp_stats_disable(MPP_STATS_GRP_MPP);
        PRINTF("==========\n\rFPS: %d\n\r==========\n\r", mp_stats.mpp.fps);
        mpp_stats_enable(MPP_STATS_GRP_MPP);
#endif
    }


err:
    for (;;)
    {
        PRINTF("Error building application pipeline : ret %d\r\n", ret);
        vTaskSuspend(NULL);
    }
}
