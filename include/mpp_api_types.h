/*
 * Copyright 2021-2026 NXP
 *
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * API types public header
 */
#ifndef _MPP_API_TYPES_H
#define _MPP_API_TYPES_H


#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup MPP_TYPES MPP Types
 *
 * This section provides the detailed documentation for the MCU Media Processing Pipeline types
 *
 * @{
 */
/** Maximum number of inference inputs and outputs **/
#define MPP_INFERENCE_MAX_OUTPUTS 9 /*!< Maximum number of outputs supported by the pipeline */
#define MPP_INFERENCE_MAX_INPUTS 1 /*!< Maximum number of inputs supported by the pipeline */

/** Maximum IP address length (IPv4: 4 octets) */
#define MPP_IP_ADDR_LEN 4

/** Pipeline handle type */
typedef void* mpp_t ;
/** Element handle type */
typedef uintptr_t mpp_elem_handle_t ;
/**Invalid pipeline handle */
#define MPP_INVALID (NULL)

/** Pipeline generated events */
typedef enum {
    MPP_EVENT_INVALID,  /*!< invalid event */
    MPP_EVENT_INFERENCE_INPUT_READY,    /*!< RGB image for inference is ready */
    MPP_EVENT_INFERENCE_OUTPUT_READY,   /*!< inference out is ready */
    MPP_EVENT_QUALITY_CHECK_READY,      /*!< Quality check measurements are ready */
    MPP_EVENT_CONVERT_PARAMS_READY,     /*!< VGLite roate matrix and input crop parameters ready */
    MPP_EVENT_INTERNAL_TEST_RESERVED,   /*!< INTERNAL: DO NOT USE */
    MPP_EVENT_MC_REMOTE_DEINIT,         /*!< remote MC paired pipeline was deinitialized */
    MPP_EVENT_NUM   /*!< DO NOT USE */
} mpp_evt_t;

/** Event mask for pipeline creation */
typedef unsigned int mpp_evt_mask_t;
/** Bit mask to receive all Events */
#define MPP_EVENT_ALL 0xffffffff

/** @brief Maximum number of cores supported in a multicore MPP system */
#ifndef CONFIG_MPP_MAX_CORES
    #define MPP_MAX_CORES               2
#else
    #define MPP_MAX_CORES               CONFIG_MPP_MAX_CORES
#endif

/** @brief Maximum number of RPMsg endpoints per core */
#define MPP_MAX_RPMSG_EPT_PER_CORE      5
/** @brief Base RPMsg endpoint address; override with CONFIG_MPP_RPMSG_ADDR_START */
#ifndef CONFIG_MPP_RPMSG_ADDR_START
    #define CONFIG_MPP_RPMSG_ADDR_START 10
#endif
/** @brief Base MCMGR event data value for MPP; override with CONFIG_MPP_MCMGR_DATA_START */
#ifndef CONFIG_MPP_MCMGR_DATA_START
    #define CONFIG_MPP_MCMGR_DATA_START 1
#endif

/**
 * Execution parameters
 *
 * These parameters control the execution of the elements of an mpp. <br>
 * The "mpps" created using the flag MPP_EXEC_RC are  guaranteed to run up to
 * the completion of all processing elements, while not being preempted by other "mpps". <br>
 * The "mpps" created using the flag MPP_EXEC_PREEMPT are preempted after a given
 * time interval by "mpps" that will run-to-completion again. <br>
 * The "mpps" created with the MPP_EXEC_INHERIT flag inherit the same execution flag
 * as the parent(s) in case of split operation. <br>
 * Note: It is not possible to request run-to-completion execution when spliting preemptable-execution "mpps".
 */
typedef enum {
    MPP_EXEC_INHERIT = 0,  /*!< inherit from parent(s) */
    MPP_EXEC_RC,           /*!< run-to-completion */
    MPP_EXEC_PREEMPT       /*!< preemptable */
} mpp_exec_flag_t;

/** @brief Statistics group identifier */
typedef enum {
    MPP_STATS_GRP_API = 0,  /*!< API (global) stats*/
    MPP_STATS_GRP_MPP,      /*!< mpp_t stats*/
    MPP_STATS_GRP_ELEMENT,  /*!< element stats */
    MPP_STATS_GRP_NUM       /*!< number of groups */
} mpp_stats_grp_t;

/** @brief Statistics data container, content depends on the group */
typedef union {
    struct {
        unsigned int rc_cycle;      /*!< run-to-completion (RC) cycle duration (ms) */
        unsigned int rc_cycle_max;  /*!< run-to-completion work deadline (ms) */
        unsigned int pr_slot;       /*!< available slot for preemptable (PR) work (ms) */
        unsigned int pr_rounds;     /*!< number of RC cycles required to complete one PR cycle (ms) */
        unsigned int app_slot;      /*!< remaining time for application (ms) */
        unsigned int cpu_load;      /*!< CPU load percentage (%) */
    } api; /*!< Global execution performance counters */
    struct {
        mpp_t mpp;
        unsigned int mpp_exec_time; /*!< pipeline execution time (ms) */
        unsigned int fps;           /*!< frames processed per second */
    } mpp; /*!< Pipeline execution performance counters */
    struct {
        mpp_elem_handle_t hnd;
        unsigned int elem_exec_time; /*!< element execution time (ms) */
    } elem; /*!< Element execution performance counters */
} mpp_stats_t;


/** @brief API initialization parameters */
typedef struct {
    mpp_stats_t *stats;         /*!< API stats */
    unsigned int rc_cycle_min;  /*!< minimum cycle duration for RC tasks (ms), 0: sets default value */
    unsigned int rc_cycle_inc;  /*!< time increment for RC tasks (ms),  0: sets default value */
    int pipeline_task_max_prio; /*!< pipeline tasks maximum priority. */
    int pipeline_rc_task_prio;  /*!< pipeline run-to-completion tasks priority. 0: sets default value */
    int pipeline_pr_task_prio;  /*!< pipeline preemptable tasks priority. 0: sets default value */
    /* multicore related parameters */
    struct {
        uint32_t n_remote_cores;           /*!< number of remote cores */
        struct {
            uint32_t remote_core_id;    /*!< remote core id */
            void *rpmsg_inst;           /*!< pointer to rpmsg instance */
        } core[MPP_MAX_CORES - 1];
    } multicore;
} mpp_api_params_t;

/** @brief Pipeline creation parameters */
typedef struct {
        int (*evt_callback_f)(mpp_t mpp, mpp_evt_t evt, void *evt_data, void *user_data); /*!< event callback function */
        mpp_evt_mask_t mask;        /*!< event mask */
        mpp_exec_flag_t exec_flag;  /*!< execution flag */
        void *cb_userdata;          /*!< user data pointer passed to the callback */
        mpp_stats_t *stats;         /*!< pointer to statistics structure */
        uint32_t core_id;           /*!< core ID where the pipeline runs */
} mpp_params_t;

/** Rotation value */
typedef enum {
    ROTATE_0 = 0,       /*!< 0 degree */
    ROTATE_90,     /*!< 90 degrees */
    ROTATE_180,   /*!< 180 degrees */
    ROTATE_270,   /*!< 270 degrees */
    ROTATE_CUSTOM /*!< Indicates custom angle will be used */
} mpp_rotate_degree_t;

/** Flip type */
typedef enum {
    FLIP_NONE = 0,      /*!< no flip */
    FLIP_HORIZONTAL,    /*!< horizontal flip */
    FLIP_VERTICAL,      /*!< vertical flip */
    FLIP_BOTH           /*!< vertical and horizontal flip */
} mpp_flip_mode_t;

/** The convert operations selector flags */
typedef enum {
    MPP_CONVERT_NONE = 0,              /*!< no frame conversion */
    MPP_CONVERT_ROTATE = 1,            /*!< frame rotation and flip */
    MPP_CONVERT_SCALE = (1 << 1),      /*!< scaling from input_frame toward output window */
    MPP_CONVERT_COLOR = (1 << 2),      /*!< frame color conversion */
    MPP_CONVERT_CROP = (1 << 3),       /*!< input frame crop */
    MPP_CONVERT_OUT_WINDOW = (1 << 4), /*!< output window */
} mpp_convert_ops_t;

/** Pixel format */
typedef enum {
    /* 2d frame format */
    MPP_PIXEL_ARGB,         /*!< ARGB 32 bits */
	MPP_PIXEL_BGRA,         /*!< BGRA 32 bits */
	MPP_PIXEL_RGBA,         /*!< RGBA 32 bits */
	MPP_PIXEL_BGRX,         /*!< BGRX 32 bits */
	MPP_PIXEL_RGBX,         /*!< RGBX 32 bits */
    MPP_PIXEL_RGB,          /*!< RGB 24 bits */
    MPP_PIXEL_RGB565,       /*!< RGB 16 bits */
    MPP_PIXEL_BGR,          /*!< BGR 24 bits */
    MPP_PIXEL_GRAY888,      /*!< gray 3x8 bits */
    MPP_PIXEL_GRAY888X,     /*!< gray 3x8 bits +8 unused bits */
    MPP_PIXEL_GRAY,         /*!< gray 8 bits */
    MPP_PIXEL_GRAY16,       /*!< gray 16 bits */
    MPP_PIXEL_YUV1P444,     /*!< YUVX interleaved 4:4:4 */
    MPP_PIXEL_YUYV1P422,    /*!< YUYV interleaved 4:2:2 */
    MPP_PIXEL_VYUY1P422,    /*!< VYUY interleaved 4:2:2 */
    MPP_PIXEL_UYVY1P422,    /*!< UYVY interleaved 4:2:2 */
    MPP_PIXEL_YUYV,         /*!< YUYV interleaved 4:2:2 */

    /* 3d frame format */
    MPP_PIXEL_DEPTH16,      /*!< depth 16 bits */
    MPP_PIXEL_DEPTH8,       /*!< depth 8 bits */

    MPP_PIXEL_YUV420P,      /*!< YUV planar 4:2:0 */

    /* compressed format */
    MPP_PIXEL_JPEG,         /*!< JPEG */

    MPP_PIXEL_INVALID       /*!< invalid pixel format */
} mpp_pixel_format_t;

/** Camera stream type - for multiple stream camera */
typedef enum {
    RGB_STREAM,            /*!< Frames received by the virtual camera element ar in rgb format */
    IR_STREAM,             /*!< Frames received by the virtual camera element ar in ir format */
    NUM_STREAMS            /*!< Total number of frame types suported by virtual camera element */
} mpp_camera_stream_type;

/** Multicore pipeline RPMSG endpoint addresses */
typedef enum {
    MPP_RPMSG_ADDR_INVALID = 0, /*!< invalid endpoint address */
    /* Core endpoints - automatically adapts to MPP_MAX_CORES */
    MPP_RPMSG_ADDR_START = CONFIG_MPP_RPMSG_ADDR_START,  /*!< first core endpoint start address */
    MPP_RPMSG_ADDR_END = CONFIG_MPP_RPMSG_ADDR_START + (MPP_MAX_CORES * MPP_MAX_RPMSG_EPT_PER_CORE) - 1,  /*!< last core endpoint end address */
    /* Remote API endpoints - one pair per core */
    MPP_RPMSG_ADDR_API_TX_BASE = MPP_RPMSG_ADDR_END + 1, /*!< remote api send endpoint base (add core_id) */
    MPP_RPMSG_ADDR_API_RX_BASE = MPP_RPMSG_ADDR_API_TX_BASE + MPP_MAX_CORES, /*!< remote api receive endpoint base (add core_id) */
    MPP_RPMSG_ADDR_MAX = MPP_RPMSG_ADDR_API_RX_BASE + MPP_MAX_CORES - 1  /*!< maximum endpoint address */
} mpp_rpmsg_endpoint_addr_e;

/** @brief First RPMsg endpoint address for a given core */
#define MPP_RPMSG_ADDR_CORE_START(core_id) (MPP_RPMSG_ADDR_START + ((core_id) * MPP_MAX_RPMSG_EPT_PER_CORE))
/** @brief Last RPMsg endpoint address for a given core */
#define MPP_RPMSG_ADDR_CORE_END(core_id)   (MPP_RPMSG_ADDR_CORE_START(core_id) + MPP_MAX_RPMSG_EPT_PER_CORE - 1)
/** @brief Remote API transmit endpoint address for a given core */
#define MPP_RPMSG_ADDR_API_TX(core_id)     (MPP_RPMSG_ADDR_API_TX_BASE + (core_id))
/** @brief Remote API receive endpoint address for a given core */
#define MPP_RPMSG_ADDR_API_RX(core_id)     (MPP_RPMSG_ADDR_API_RX_BASE + (core_id))

/* Compile-time check for address range overflow */
#ifdef CONFIG_MPP_RPMSG_ADDR_END
    _Static_assert(MPP_RPMSG_ADDR_MAX <= CONFIG_MPP_RPMSG_ADDR_END,
                    "RPMSG endpoint addresses exceed configured CONFIG_MPP_RPMSG_ADDR_END");
#endif

/** Multicore pipeline MCMGR remote event data */
typedef enum {
    MPP_MCMGR_DATA_INVALID = 0, /*!< invalid mcmgr event data */
    /* Event data range - same range for all cores */
    MPP_MCMGR_DATA_START = CONFIG_MPP_MCMGR_DATA_START,       /*!< mcmgr event data start */
    MPP_MCMGR_DATA_END = CONFIG_MPP_MCMGR_DATA_START + (MPP_MAX_RPMSG_EPT_PER_CORE - 1),  /*!< mcmgr event data end */
    MPP_MCMGR_DATA_API, /*!< mcmgr event data remote api */
    MPP_MCMGR_DATA_MAX  /*!< maximum mcmgr event data */
} mpp_mcmgr_event_data_e;

/* Compile-time check for event data range overflow */
#ifdef CONFIG_MPP_MCMGR_DATA_END
    _Static_assert(MPP_MCMGR_DATA_MAX <= CONFIG_MPP_MCMGR_DATA_END,
                    "MCMGR event data values exceed configured CONFIG_MPP_MCMGR_DATA_END");
#endif

/** RTP payload types */
typedef enum {
    RTP_PAYLOAD_TYPE_JPEG = 26,     /*!< JPEG video */
    RTP_PAYLOAD_TYPE_H264 = 96,     /*!< H.264 video (dynamic) */
} rtp_payload_type_t;

/** RTP JPEG frame types */
typedef enum {
    RTP_JPEG_FRAME_TYPE_YUV422 = 0, /*!< YUV 4:2:2 format */
    RTP_JPEG_FRAME_TYPE_YUV420 = 1  /*!< YUV 4:2:0 format */
} rtp_frame_type_t;

/** Camera stream configuration for multi-stream cameras */
typedef struct {
    mpp_camera_stream_type type; /*!< Stream type (member of enum mpp_camera_stream_type) */
    bool active;                 /*!< Stream is active or not */
    int height;
    int width;
    int usb_camera_index;        /*!< USB camera slot index to bind this stream to (0-based).
                                   *  Set to -1 (or leave zero-initialised) for automatic
                                   *  assignment (first free g_Videos[] slot).
                                   *  Use a non-negative value to select a specific camera
                                   *  when a single USB device exposes multiple UVC interfaces
                                   *  (e.g. 0 = RGB, 1 = IR). */
} mpp_camera_stream_cfg;

/** Camera parameters */
typedef struct {
    int height; /*!< buffer height */
    int width;  /*!< buffer width */
    mpp_pixel_format_t format; /*!< pixel format */
    int fps;    /*!< frames per second */
    bool stripe; /*!< stripe mode */
    void *rpmsg_inst; /*!< pointer to rpmsg instance */
    volatile uint16_t *mcmgr_event_data; /*!< pointer to mcmgr event data */
    uint32_t n_streams; /*!< number of total output video streams */
    mpp_camera_stream_cfg stream[NUM_STREAMS]; /*!< streams configuration */
    bool in_advance_enqueue; /*!< enable in-advance enqueue mode */
} mpp_camera_params_t;

/** Static image parameters */
typedef struct {
    int height; /*!< buffer height */
    int width;  /*!< buffer width */
    mpp_pixel_format_t format;  /*!< pixel format */
    bool stripe; /*!< stripe mode */
    int compressed_size;   /*!< size in bytes for compressed format */
} mpp_img_params_t;

/** @brief Parameters passed to the callback after a convert element processes a frame */
typedef struct {
    uint32_t id;        /*!< convert element id */
    uint32_t left;      /*!< crop left coordinate */
    uint32_t right;     /*!< crop right coordinate */
    uint32_t top;       /*!< crop top coordinate */
    uint32_t bottom;    /*!< crop bottom coordinate */
    float rotation;     /*!< custom rotation angle */
    void *vg_lite_m;    /*!< vglite matrix pointer */
} mpp_convert_cb_param_t;

/** File slice search function type */
typedef int (*slice_search_func_t)(const uint8_t *data, int32_t len);

/** File source parameters */
typedef struct {
    const char *filepath;                  /*!< Path to file on SD card */
    bool loop;                             /*!< Flag to enable looping the file */
    int file_buffer_size;                  /*!< size in bytes for compressed format */
    slice_search_func_t slice_search_func; /*!< Optional: function to search for slices/chunks of data */
} mpp_filesrc_params_t;

/** File sink parameters */
typedef struct {
    const char *filepath;                  /*!< Path to file on SD card */
    bool append_mode;                      /*!< Flag to enable append mode (false = overwrite) */
    uint64_t max_file_size;                /*!< Maximum file size in bytes */
} mpp_filesink_params_t;

/** RTSP source parameters */
typedef struct _mpp_rtspsrc_params {
    const char *rtsp_url;                     /*!< RTSP URL (rtsp://ip:port/path) */
    uint64_t rtsp_buffer_size;                /*!< Maximum buffer size in bytes */
    int width;                                /*!< Source image width in pixels */
    int height;                               /*!< Source image height in pixels */
    mpp_pixel_format_t format;                /*!< Pixel format of output buffer (use MPP_PIXEL_JPEG for compressed JPEG) */
} mpp_rtspsrc_params_t;

/** RTSP sink parameters */
typedef struct _mpp_rtspsink_params {
    uint16_t port;                     /*!< RTSP server port (default: 8554) */
    uint8_t ip_addr[MPP_IP_ADDR_LEN];  /*!< IP address as array of bytes */
    uint32_t frame_width;              /*!< Frame width in pixels */
    uint32_t frame_height;              /*!< Frame height in pixels */
    rtp_payload_type_t payload_type;   /*!< RTP payload type */
    rtp_frame_type_t frame_type;       /*!< RTP JPEG frame type (YUV422 or YUV420) */
} mpp_rtspsink_params_t;

/** MC element parameters */
typedef struct {
    void *rpmsg_inst;           /*!< pointer to rpmsg instance */
    uint32_t remote_core_id;    /*!< remote core id */
    uint32_t local_rpmsg_addr;  /*!< local rpmsg endpoint address */
    uint32_t remote_rpmsg_addr; /*!< remote rpmsg endpoint address */
    uint16_t event_data;        /*!< event data for mcmgr */
} mpp_mc_params_t;

/** Display parameters */
typedef struct {
    int height; /*!< buffer resolution: setting to 0 will default to panel physical resolution */
    int width;  /*!< buffer resolution: setting to 0 will default to panel physical resolution */
    int pitch;  /*!< buffer resolution: setting to 0 will default to panel physical resolution */
    int left;   /*!< active rect: setting to 0 will default to fullscreen */
    int top;    /*!< active rect: setting to 0 will default to fullscreen */
    int right;  /*!< active rect: setting to 0 will default to fullscreen */
    int bottom; /*!< active rect: setting to 0 will default to fullscreen */
    mpp_rotate_degree_t rotate; /*!< rotate degree */
    mpp_pixel_format_t format;  /*!< pixel format */
    bool stripe; /*!< stripe mode */
    void *handle; /*!< pointer to an lvgl image widget */
} mpp_display_params_t;

/** Processing element ids */
typedef enum {
    MPP_ELEMENT_INVALID,    /*!< Invalid element */
    MPP_ELEMENT_LABELED_RECTANGLE, /*!< Labeled rectangle - bounding box */
    MPP_ELEMENT_TEST,       /*!< Test inplace element - NOT FOR USE */
    MPP_ELEMENT_INFERENCE,  /*!< Inference engine */
    MPP_ELEMENT_CONVERT,    /*!< Image conversion: resolution, orientation, color format */
    MPP_ELEMENT_IMG_DECODE,     /*!< Image decompression: JPEG, PNG */
    MPP_ELEMENT_IMG_COMPOSE,    /*!< compose a simple GUI: logo and text area with the input stream */
    MPP_ELEMENT_IMG_QUALITY_CHECK,  /*!< Image quality check */
    MPP_ELEMENT_VIDEO_DECODE,   /*!< Video decode */
    MPP_ELEMENT_VIDEO_ENCODE,   /*!< Video encode */
    MPP_ELEMENT_NUM         /*!< DO NOT USE */
} mpp_element_id_t;

/* Inference parameter types */
/** Maximum number of dimensions for tensors */
#define MAX_TENSOR_DIMS 4

/** Inference tensor dimensions */
typedef struct
{
  uint32_t size;
  uint32_t data[MAX_TENSOR_DIMS];
} mpp_tensor_dims_t;

/** Inference tensor type */
typedef enum {
    MPP_TENSOR_TYPE_FLOAT32 = 0,/*!< floating point 32 bits */
    MPP_TENSOR_TYPE_UINT8 = 1,  /*!< unsigned integer 8 bits */
    MPP_TENSOR_TYPE_INT8 = 2    /*!< signed integer 8 bits */
} mpp_tensor_type_t;

/** Inference input tensor order */
typedef enum {
    MPP_TENSOR_ORDER_UNKNOWN = 0,  /*!< order not set */
    MPP_TENSOR_ORDER_NHWC = 1,  /*!< order: Batch, Height, Width, Channels */
    MPP_TENSOR_ORDER_NCHW = 2  /*!< order: Batch, Channels, Height, Width */
} mpp_tensor_order_t;

/** Inference type */
typedef enum
{
    MPP_INFERENCE_TYPE_TFLITE = 0,      /*!< TensorFlow-Lite */
    MPP_INFERENCE_TYPE_EXECUTORCH = 1, /*!< ExecuTorch */
} mpp_inference_type_t;

/** tensor parameters */
typedef struct{
    const uint8_t* data;  /*!< data address */
    mpp_tensor_dims_t dims; /*!< tensor data dimensions */
    mpp_tensor_type_t type; /*!< tensor data type */
    float scale; /*! < quantization scale factor */
    int32_t zero_point; /*!< quantization zero point */
} mpp_inference_tensor_params_t;

/** Inference input ready callback parameters */
typedef struct {
    mpp_inference_tensor_params_t *in_tensors[MPP_INFERENCE_MAX_INPUTS];  /*!< input tensors parameters */
    uint32_t model_id;  /*!< model identifier */
} mpp_inference_inp_cb_params;

/** Inference callback parameters */
typedef struct {
    void *user_data;        /*!< callback will pass this pointer */
    mpp_inference_tensor_params_t *out_tensors[MPP_INFERENCE_MAX_OUTPUTS]; /*!< output tensors parameters */
    uint32_t inference_time_ms;  /*!< inference run time measurement - output to user */
    mpp_inference_type_t inference_type; /*!< type of the inference */
    uint32_t model_id; /*! < model identifier */
} mpp_inference_cb_param_t;

/** @brief Image quality metrics */
typedef struct {
    int brightness;      /*!< brightness metric */
    int contrast;        /*!< contrast metric */
} img_quality_metrics_t;

/** mpp color encoding */
typedef union {
    uint32_t raw;   /*!< Raw color */
    /** rgb color values */
    struct {
        uint8_t R;      /*!< Red byte */
        uint8_t G;      /*!< Green byte */
        uint8_t B;      /*!< Blue byte */
        uint8_t pad;    /*!< padding byte */
    } rgb;  /*!< RGB color */
} mpp_color_t;

/** Bundle inference function type */
typedef int (*inference_entry_point_t)(uint8_t * , uint8_t * , uint8_t *);

/** mpp labeled rectangle element structure */
typedef struct {
    uint8_t label[64];      /*!< label to print */
    uint16_t clear;         /*!< clear rectangle */
    uint16_t line_width;    /*!< rectangle line thickness */
    mpp_color_t line_color; /*!< rectangle line color */
    int16_t top;            /*!< rectangle top position */
    int16_t left;           /*!< rectangle left position */
    int16_t bottom;         /*!< rectangle bottom position */
    int16_t right;          /*!< rectangle right position */
    uint16_t tag;           /*!< labeled rectangle tag */
    uint16_t reserved;      /*!< pad for 32 bits alignment */
    bool stripe;            /*!< stripe mode */
} mpp_labeled_rect_t;


/** mpp landmark structure */
typedef struct {
    uint16_t clear;     /*!< clear landmark */
    uint16_t width;     /*!< landmark thickness */
    mpp_color_t color;  /*!< landmark color */
    int16_t x;          /*!< landmark x position */
    int16_t y;          /*!< landmark y position */
    uint16_t tag;       /*!< landmark tag */
    bool stripe;        /*!< stripe mode */
} mpp_landmark_t;

/** Image area coordinates */
typedef struct {
    int top;
    int left;
    int bottom;
    int right;
} mpp_area_t;

/** Image dimensions */
typedef struct {
    unsigned int width;
    unsigned int height;
} mpp_dims_t;

/** Image position */
typedef struct {
    int top;
    int left;
} mpp_position_t;

/** Model parameters */
typedef struct {
    uint64_t constant_weight_MemSize;  /*!< model constant weights memory size */
    uint64_t mutable_weight_MemSize;   /*!< Defines the amount of memory required both input & output data buffers */
    uint64_t activations_MemSize;      /*!< Size of scratch memory used for intermediate computations needed by the model */
    int num_inputs ;                   /*!< model's number of inputs */
    int num_outputs ;                  /*!< model's number of outputs */
    uint64_t inputs_offsets[MPP_INFERENCE_MAX_INPUTS] ;  /*!< offset of each input */
    uint64_t outputs_offsets[MPP_INFERENCE_MAX_OUTPUTS]; /*!< offset of each output */
    inference_entry_point_t model_entry_point;                 /*!< function called to perform the inference */
    mpp_tensor_type_t model_input_tensors_type; /*!< type of input buffer */
} mpp_inference_params_t;

/** Image composition parameters */
typedef struct {
    int height;                 /*!< original image height */
    int width;                  /*!< original image width */
    mpp_pixel_format_t format;  /*!< pixel format */
    void *buffer;               /*!< image buffer address */
    mpp_area_t dest_area;       /*!< area for image in destination */
} mpp_img_compose_param_t;

/** Static image and Processing elements parameters */
typedef struct {
union {
    /** Camera element's parameters */
    mpp_camera_params_t camera;
    /** Static Image element's parameters */
    struct {
        mpp_img_params_t img_params;        /*!< static image parameters */
        void *img_buffer;                   /*!< static image buffer address */
    } static_image;
    /** Multicore source element's parameters */
    mpp_mc_params_t mc_source;              /*!< Multicore source element's parameters */
    /** Compose element's parameters */
    struct {
        int nb_images;                          /*!< number of images to compose */
        mpp_img_compose_param_t *image_list;    /*!< pointer to array of images to compose */
        mpp_area_t input_area;                  /*!< area of input stream in destination */
        mpp_rotate_degree_t out_angle;          /*!< output rotation angle */
        mpp_flip_mode_t out_flip;               /*!< output flip mode */
        mpp_pixel_format_t out_format;          /*!< output color format */
        int out_width;                          /*!< output buffer width */
        int out_height;                         /*!< output buffer height */
    } compose;
    /** Labeled Rectangle and Landmarks element's parameters */
    struct {
        uint32_t max_rect;                  /*!< maximum number of rectangles */
        uint32_t detected_rect;             /*!< detected rectangles */
        mpp_labeled_rect_t *rectangles;     /*!< array of rectangle data */
        uint32_t max_landmk;                /*!< maximum number of landmarks */
        uint32_t detected_landmk;           /*!< detected landmarks */
        mpp_landmark_t *landmarks;          /*!< array of landmark data */
    } labels;
    /** Convert element's parameters */
    struct {
        mpp_dims_t out_buf;                 /*!< output buffer dimensions */
        mpp_pixel_format_t pixel_format;    /*!< new pixel format */
        mpp_rotate_degree_t angle;          /*!< rotation angle */
        float custom_angle;                 /*!< custom value of rotation angle */
        mpp_flip_mode_t flip;               /*!< flip mode */
        mpp_area_t crop;                    /*!< input crop area */
        mpp_position_t out_window;          /*!< output window position */
        mpp_dims_t scale;                   /*!< scaling dimensions */
        mpp_convert_ops_t ops;              /*!< operation selector mask */
        const char* dev_name;               /*!< device name used for graphics */
        uint32_t elem_id;                   /*!< element id used for callback parameters */
        bool stripe_in;                     /*!< input stripe mode */
        bool stripe_out;                    /*!< output stripe mode */
    } convert;
    /** Resize element's parameters */
    struct {
    	unsigned int width;
    	unsigned int height;
    } resize;
    /** Color convert element's parameters */
    struct {
    	mpp_pixel_format_t pixel_format;
    } color_conv;
    /** Rotate element's parameters */
    struct {
        mpp_rotate_degree_t angle;
    } rotate;
    /** Test element's parameters */
    struct {
        _Bool inp;
        /* output parameters */
        unsigned int width;
        unsigned int height;
        mpp_pixel_format_t format;
    } test;
    /** Decoder element's parameters */
    struct {
        const char* dev_name;               /*!< device name used for decoder */
        /* output parameters. TODO remove: parser should auto-detect */
        unsigned int width;
        unsigned int height;
        mpp_pixel_format_t out_format;
    } decode;
    /** Encoder element's parameters */
    struct {
        const char* dev_name;               /*!< device name used for encoder */
        unsigned int width;                 /*!< frame width */
        unsigned int height;                /*!< frame height */
        unsigned int fps;                   /*!< frame rate */
        mpp_pixel_format_t format;          /*!< pixel format */
        bool intraframe;                    /*!< only intraframe encoding */
        /* H.264 encoder configuration parameters */
        uint32_t intra_period;              /*!< I-frame period (GOP size) */
        int32_t num_ref_frame;              /*!< Number of reference frames */
        int32_t rc_mode;                    /*!< Rate control mode */
        int32_t target_bitrate;             /*!< Target bitrate in bps */
        int32_t max_bitrate;                /*!< Maximum bitrate in bps */
        bool enable_frame_skip;             /*!< Enable/disable frame skipping */
        int32_t temporal_layer_num;         /*!< Number of temporal layers */
        int32_t profile_idc;                /*!< Profile (e.g., baseline, main, high) */
        int32_t level_idc;                  /*!< Level (e.g., 3.0, 3.1, 4.0) */
        int32_t spatial_bitrate;            /*!< Spatial layer bitrate */
        int32_t max_spatial_bitrate;        /*!< Maximum spatial layer bitrate */
        int32_t entropy_coding_mode;        /*!< 0: CAVLC, 1: CABAC */
    } encode;
    /** ML inference element's parameters */
    struct {
        const void *model_data; /*!< pointer to model binary */
        mpp_inference_type_t type; /*!< inference type */
        int model_size;         /*!< model binary size */
        uint32_t model_id;      /*! model identifier */
        float model_input_mean; /*!< model 'mean' of input values, used for normalization */
        float model_input_std;  /*!< model 'standard deviation' of input values, used for normalization */
        mpp_tensor_order_t tensor_order; /*!< model input tensor component order */
        mpp_inference_params_t inference_params; /*!< model specific parameters used by the inference */
    } ml_inference;
    /** Image quality check element's parameters */
    struct {
        _Bool disable;           /*!< disable quality check */
    } img_quality_check;
};
    mpp_stats_t *stats;
} mpp_element_params_t;

/** @}*/

/** @defgroup return_codes Return Codes
 *  MPP APIs return status definitions
 * @{
 */
#define MPP_SUCCESS         (0)     /*!< Success return code */
#define MPP_ERROR           (10)    /*!< A generic error occured */
#define MPP_INVALID_ELEM    (20)    /*!< Invalid element provided */
#define MPP_INVALID_PARAM   (30)    /*!< Invalid parameter provided */
#define MPP_ERR_ALLOC_MUTEX (40)    /*!< Error occured while allocating mutex */
#define MPP_INVALID_MUTEX   (41)    /*!< Invalid mutex provided */
#define MPP_MUTEX_TIMEOUT   (42)    /*!< Mutex timeout occured */
#define MPP_MUTEX_ERROR     (43)    /*!< Mutex error occured */
#define MPP_MALLOC_ERROR    (50)    /*!< Memory allocation error occured */
#define MPP_UNSUPPORTED     (60)    /*!< Operation not supported for this element */

/** @} */

#endif
