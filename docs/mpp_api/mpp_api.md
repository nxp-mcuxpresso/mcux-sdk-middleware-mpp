# eIQ MCU Media Processing Pipeline API

MPP VERSION 4.3

## 1. MPP API

**Functions**

- int[ mpp_api_init (](#mpp_api_init)[mpp_api_params_t ](#mpp_api_params_t)∗params)
- [mpp_t ](#mpp_t)[mpp_create (](#mpp_create)[mpp_params_t ](#mpp_params_t)∗params, int ∗ret)
- int[ mpp_camera_add (](#mpp_camera_add)[mpp_t ](#mpp_t) mpp, const char ∗name, [mpp_camera_params_t ](#mpp_camera_params_t)∗params, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗elem\_h)
- int[ mpp_static_img_add (](#mpp_static_img_add)[mpp_t ](#mpp_t) mpp, [mpp_img_params_t ](#mpp_img_params_t)∗params, void ∗addr, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗elem\_h)
- int[ mpp_filesrc_add (](#mpp_filesrc_add)[mpp_filesrc_params_t ](#mpp_filesrc_params_t)∗params, void ∗addr, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗elem\_h)
- int[ mpp_mc_source_add (](#mpp_mc_source_add)[mpp_t ](#mpp_t) mpp, [mpp_mc_params_t ](#mpp_mc_params_t)∗params, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗elem\_h)
- int[ mpp_display_add (](#mpp_display_add)[mpp_t ](#mpp_t) mpp, const char ∗name, [mpp_display_params_t ](#mpp_display_params_t)∗params)
- int[ mpp_mc_sink_add (](#mpp_mc_sink_add)[mpp_t ](#mpp_t) mpp, [mpp_mc_params_t ](#mpp_mc_params_t)∗params)
- int[ mpp_nullsink_add (](#mpp_nullsink_add)[mpp_t ](#mpp_t) mpp)
- int[ mpp_element_add (](#mpp_element_add)[mpp_t ](#mpp_t) mpp, [mpp_element_id_t id, ](#mpp_element_id_t)[mpp_element_params_t ](#mpp_element_params_t)∗params, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗elem\_h)
- int[ mpp_split (](#mpp_split)[mpp_t ](#mpp_t)mpp, unsigned int num, [mpp_params_t ](#mpp_params_t)∗params, [mpp_t ](#mpp_t)∗out\_list)
- int[ mpp_background (](#mpp_background)[mpp_t ](#mpp_t)mpp, [mpp_params_t ](#mpp_params_t)∗params, [mpp_t ](#mpp_t)∗out\_mpp)
- int[ mpp_element_update (](#mpp_element_update)[mpp_t ](#mpp_t)mpp, [mpp_elem_handle_t elem_h,](#mpp_elem_handle_t) [mpp_element_params_t ](#mpp_element_params_t)∗params, bool force\_update)
- bool[ mpp_is_running (](#mpp_is_running)[mpp_t ](#mpp_t)mpp)
- int[ mpp_start (](#mpp_start)[mpp_t ](#mpp_t)mpp, int last, bool force\_update)
- int[ mpp_stop (](#mpp_stop)[mpp_t ](#mpp_t)mpp)
- int [mpp_force_update(](#mpp_force_update)[mpp_t ](#mpp_t)mpp)
- void [mpp_stats_enable (](#mpp_stats_enable)[mpp_stats_grp_t ](#mpp_stats_grp_t)grp)
- void [mpp_stats_disable (](#mpp_stats_disable)[mpp_stats_grp_t ](#mpp_stats_grp_t)grp)
- char ∗[mpp_get_version (](#mpp_get_version)void)
- int **mpp\_storage\_init** (void)


### 1.1 Detailed Description

This section provides the detailed documentation for the MCU Media Processing Pipeline API.

#### 1.1.1 Function Documentation


##### mpp_api_init

int mpp\_api\_init ( [mpp_api_params_t ](#mpp_api_params_t)∗ params )

Pipeline initialization.
This function initializes the library and its data structures.
It must be called before any other function of the API is called.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|params|API global parameters|
|out|ret|return code (0 - success, non-zero - error)|

**Returns**

[Return_codes](#return_codes)

##### mpp_create

[mpp_t ](#mpp_t) mpp\_create ( [mpp_params_t ](#mpp_params_t)∗ params, int ∗ ret )

Basic pipeline creation.
This function returns a handle to the pipeline.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|params|pipeline parameters|
|out|ret|return code (0 - success, non-zero - error)|

**Returns**

handle to the pipeline if success, NULL if there is an error.

##### mpp_camera_add

int mpp\_camera\_add ( [mpp_t ](#mpp_t)mpp, const char ∗ name, [mpp_camera_params_t ](#mpp_camera_params_t)∗ params, [mpp_elem_handle_t elem_h,](#mpp_elem_handle_t) )

Camera addition.

This function adds a camera to the pipeline.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|name|camera driver name|
|in|params|parameters to be configured on the camera|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return_codes](#return_codes)

##### mpp_static_img_add

int mpp\_static\_img\_add ( [mpp_t ](#mpp_t)mpp, [mpp_img_params_t ](#mpp_img_params_t)∗ params, void ∗ addr, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗ elem\_h )

Static image addition.

**Parameters**


|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|static image parameters|
|in|addr|image buffer|
|out|elem_h|element handle in pipeline|

**Returns**

[Return_codes ](#return_codes)

**Precondition**

- Image buffer allocation/free is the responsibility of the user.

##### mpp_filesrc_add

int mpp\_filesrc\_add ( [mpp_t ](#mpp_t)mpp, [mpp_filesrc_params_t ](#mpp_filesrc_params_t)∗ params, void ∗ addr, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗ elem\_h )

Source file addition.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|file parameters|
|in|addr|file buffer|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return_codes](#return_codes)

##### mpp_mc_source_add

int mpp\_mc\_source\_add ( [mpp_t ](#mpp_t)mpp, [mpp_mc_params_t ](#mpp_mc_params_t)∗ params, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗ elem\_h )

Multi core source addition.

This function adds a multi core source to the pipeline.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|parameters that are configured on the multi core source|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return_codes](#return_codes)

##### mpp_display_add

int mpp\_display\_add ( [mpp_t ](#mpp_t)mpp, const char ∗ name, [mpp_display_params_t ](#mpp_display_params_t)∗ params )

Display addition.

This function adds a display to the pipeline. 

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|name|display driver name|
|in|params|parameters that are configured on the display|

**Returns**

[Return_codes](#return_codes)

##### mpp_mc_sink_add

int mpp\_mc\_sink\_add ( [mpp_t ](#mpp_t)mpp, [mpp_mc_params_t ](#mpp_mc_params_t)∗ params )

Multi core sink addition.

This function adds a multi core sink to the pipeline.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|parameters that are configured on the multi core sink|

**Returns**

[Return_codes](#return_codes)

##### mpp_nullsink_add

int mpp\_nullsink\_add ( [mpp_t ](#mpp_t)mpp )

Null sink addition.

This function adds a null-type sink to the pipeline.

After this call pipeline is closed and no further elements can be added. Input frames are discarded.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|

**Returns**

[Return_codes](#return_codes)


##### mpp_element_add

int mpp\_element\_add ( [mpp_t ](#mpp_t)mpp, [mpp_element_id_t ](#mpp_element_id_t)id, [mpp_element_params_t ](#mpp_element_params_t)∗ params, [mpp_elem_handle_t ](#mpp_elem_handle_t)∗ elem\_h )

Add processing element (single input, single output) This function adds an element to the pipeline. Available elements are:
- 2D image processing
- ML inference engine
- Labeled rectangle
- Compositor

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|id|element id|
|in|params|element parameters|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return_codes](#return_codes)


##### mpp_split

int mpp\_split ( [mpp_t ](#mpp_t)mpp, unsigned int num, [mpp_params_t ](#mpp_params_t)∗ params, [mpp_t ](#mpp_t)∗ out\_list )

Pipeline multiplication. 

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|num|number of output pipeline|
|in|params|split mpp parameters|
|out|out\_list|list of output pipelines|

**Returns**

[Return_codes ](#return_codes)

**Precondition**

- out\_list array must contain at least num elements.


##### mpp_background

int mpp\_background ( [mpp_t ](#mpp_t)mpp, [mpp_params_t ](#mpp_params_t)∗ params, [mpp_t ](#mpp_t)∗ out\_mpp )

Put next elements processing in background. 

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|new mpp parameters (exec\_flag must be MPP\_EXEC\_PREEMPT)|
|out|out\_mpp|output pipeline|

**Returns**

[Return_codes](#return_codes)


##### mpp_element_update

int mpp\_element\_update ( [mpp_t ](#mpp_t)mpp, [mpp_elem_handle_t ](#mpp_elem_handle_t)elem\_h, [mpp_element_params_t ](#mpp_element_params_t)∗ params, bool force\_update )

Update element parameters.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|elem\_h|element handle in the pipeline.|
|in|params|new element parameters|
|in|force\_update|force the pipeline to run even though there is no input frame update for processing elements after update. If the force\_update flag was already requested before, the current value is ignored|

**Returns**

[Return_codes](#return_codes)

##### mpp_is_running

bool mpp\_is\_running ( [mpp_t](#mpp_t)mpp )

Check if the pipeline is currently running

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|

**Returns**

true if pipeline is in running state, else false

##### mpp_start

int mpp\_start ( [mpp_t ](#mpp_t)mpp, int last, bool force\_update )

Start pipeline.

When called with last=0, this function prepares the branch of the pipeline specified with mpp. When called with last!=0, this function starts the data flow of the pipeline.

Data flow should start after all the branches of the pipeline have been prepared.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|pipeline branch handle to start/prepare|
|in|last|if non-zero start pipeline processing. No further start call is possible thereafter.|
|in|force\_update|force the pipeline to run even though there is no input frame update for processing elements. If the force\_update flag was already requested before, the current value is ignored|

**Returns**

[Return_codes](#return_codes)

##### mpp_stop

int mpp\_stop ( [mpp_t ](#mpp_t)mpp )

Stop a branch of the pipeline.

This function stops the data processing and peripherals of a pipeline branch. 

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|pipeline branch to stop|

**Returns**

[Return_codes](#return_codes)

##### mpp_force_update

int mpp\_force\_update ( [mpp_t](#mpp_t) mpp )

Force the update of a branch of the pipeline.

This function forces and update of the branch of the pipeline even if there is no new input frame.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|pipeline branch to set the flag force_update to true|

**Returns**

[Return_codes](#return_codes)

##### mpp_stats_enable

void mpp\_stats\_enable ( [mpp_stats_grp_t ](#mpp_stats_grp_t)grp ) 

Enable statistics collection.

This function enables statistics collection for a given group. Statistics collection is disabled by default after API initialization. Calling this function when stats are enabled has no effect.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|grp|statistics group|

**Returns**

##### mpp_stats_disable

void mpp\_stats\_disable ( [mpp_stats_grp_t ](#mpp_stats_grp_t)grp )

Disable statistics collection.

This function disables statistics collection for a given group. Calling this function when stats are disabled has no effect. This function is used to ensure stats are not updated while application tasks use the stats structures.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|grp|statistics group|

**Returns**


##### mpp_get_version

char ∗ mpp\_get\_version ( void )

Get MPP version. 

**Parameters**


**Returns**

pointer to the MPP version string

## 2. MPP Types

**Data Structures**

- union [mpp_stats_t](#mpp_stats_t)
- struct [mpp_api_params_t](#mpp_api_params_t)
- struct [mpp_params_t](#mpp_params_t)
- struct [mpp_camera_stream_cfg](#mpp_camera_stream_cfg)
- struct [mpp_camera_params_t](#mpp_camera_params_t)
- struct [mpp_img_params_t](#mpp_img_params_t)
- struct [mpp_filesrc_params_t](#mpp_filesrc_params_t)
- struct [mpp_mc_params_t](#mpp_mc_params_t)
- struct [mpp_display_params_t](#mpp_display_params_t)
- struct [mpp_tensor_dims_t](#mpp_tensor_dims_t)
- struct [mpp_inference_tensor_params_t](#mpp_inference_tensor_params_t)
- struct [mpp_inference_cb_param_t](#mpp_inference_cb_param_t)
- struct[img_quality_metrics_t](#img_quality_metrics_t)
- union [mpp_color_t](#mpp_color_t)
- struct [mpp_labeled_rect_t](#mpp_labeled_rect_t)
- struct [mpp_landmark_t](#mpp_landmark_t)
- struct [mpp_area_t](#mpp_area_t)
- struct [mpp_dims_t](#mpp_dims_t)
- struct [mpp_position_t](#mpp_position_t)
- struct [mpp_inference_params_t](#mpp_inference_params_t)
- struct [mpp_img_compose_param_t](#mpp_img_compose_param_t)
- struct [mpp_element_params_t](#mpp_element_params_t)
- struct [mpp_stats_t.api](#mpp_stats_tapi)
- struct [mpp_stats_t.mpp](#mpp_stats_tmpp)
- struct [mpp_stats_t.elem](#mpp_stats_telem)
- struct [mpp_color_t.rgb](#mpp_color_trgb)
- union [mpp_element_params_t.__unnamed5__](#mpp_element_params_t__unnamed5__)
- struct [mpp_element_params_t.__unnamed5__.static_image](#mpp_element_params_t__unnamed5__static_image)
- struct [mpp_element_params_t.__unnamed5__.compose](#mpp_element_params_t__unnamed5__compose)
- struct [mpp_element_params_t.__unnamed5__.labels](#mpp_element_params_t__unnamed5__labels)
- struct [mpp_element_params_t.__unnamed5__.convert](#mpp_element_params_t__unnamed5__convert)
- struct [mpp_element_params_t.__unnamed5__.resize](#mpp_element_params_t__unnamed5__resize)
- struct [mpp_element_params_t.__unnamed5__.color_conv](#mpp_element_params_t__unnamed5__color_conv)
- struct [mpp_element_params_t.__unnamed5__.rotate](#mpp_element_params_t__unnamed5__rotate)
- struct [mpp_element_params_t.__unnamed5__.test](#mpp_element_params_t__unnamed5__test)
- struct [mpp_element_params_t.__unnamed5__.decode](#mpp_element_params_t__unnamed5__decode)
- struct [mpp_element_params_t.__unnamed5__.ml_inference](#mpp_element_params_t__unnamed5__ml_inference)
- struct [mpp_element_params_t.__unnamed5__.img_quality_check](#[mpp_element_params_t__unnamed5__img_quality_check)

**Macros**

- #define [MPP_INFERENCE_MAX_OUTPUTS](#mpp_inference_max_outputs)
- #define [MPP_INFERENCE_MAX_INPUTS](#mpp_inference_max_inputs)
- #define [MPP_INVALID](#mpp_invalid)
- #define [MPP_EVENT_ALL](#mpp_event_all)
- #define **MPP\_MAX\_RPMSG\_EPT\_PER\_CORE**
- #define [MAX_TENSOR_DIMS](#max_tensor_dims)

**Typedefs**

- typedef void ∗[mpp_t](#mpp_t)
- typedef uintptr\_t [mpp_elem_handle_t](#mpp_elem_handle_t)
- typedef unsigned int [mpp_evt_mask_t](#mpp_evt_mask_t)
- typedef int(∗[slice_search_func_t) ](#slice_search_func_t)(const uint8\_t ∗data, int32\_t len)
- typedef int(∗[inference_entry_point_t) ](#inference_entry_point_t)(uint8\_t ∗, uint8\_t ∗, uint8\_t ∗)

**Enumerations**

- enum [mpp_evt_t](#mpp_evt_t) {

  [MPP_EVENT_INVALID](#mpp_event_invalid),
  
  [MPP_EVENT_INFERENCE_INPUT_READY](#mpp_event_inference_input_ready),
  
  [MPP_EVENT_INFERENCE_OUTPUT_READY](#mpp_event_inference_output_ready),
  
  [MPP_EVENT_QUALITY_CHECK_READY](#mpp_event_quality_check_ready),
  
  [MPP_EVENT_INTERNAL_TEST_RESERVED](#mpp_event_internal_test_reserved),
  
  [MPP_EVENT_NUM](#mpp_event_num) }

- enum [mpp_exec_flag_t](#mpp_exec_flag_t) {

  [MPP_EXEC_INHERIT](#mpp_exec_inherit),
  
  [MPP_EXEC_RC](#mpp_exec_rc),
  
  [MPP_EXEC_PREEMPT](#mpp_exec_preempt) }

- enum [mpp_stats_grp_t](#mpp_stats_grp_t) {

  [MPP_STATS_GRP_API](#mpp_stats_grp_api),

  [MPP_STATS_GRP_MPP](#mpp_stats_grp_mpp),

  [MPP_STATS_GRP_ELEMENT](#mpp_stats_grp_element),

  [MPP_STATS_GRP_NUM](#mpp_stats_grp_num) }

- enum [mpp_rotate_degree_t](#mpp_rotate_degree_t) {

  [ROTATE_0](#rotate_0),

  [ROTATE_90](#rotate_90),

  [ROTATE_180](#rotate_180),

  [ROTATE_270](#rotate_270) }

- enum [mpp_flip_mode_t](#mpp_flip_mode_t) {

  [FLIP_NONE](#flip_none),

  [FLIP_HORIZONTAL](#flip_horizontal),

  [FLIP_VERTICAL](#flip_vertical),

  [FLIP_BOTH](#flip_both) }

- enum [mpp_convert_ops_t](#mpp_convert_ops_t) {

  [MPP_CONVERT_NONE](#mpp_convert_none),

  [MPP_CONVERT_ROTATE](#mpp_convert_rotate),

  [MPP_CONVERT_SCALE](#mpp_convert_scale),

  [MPP_CONVERT_COLOR](#mpp_convert_color),

  [MPP_CONVERT_CROP](#mpp_convert_crop),

  [MPP_CONVERT_OUT_WINDOW](#mpp_convert_out_window) }

- enum [mpp_pixel_format_t](#mpp_pixel_format_t) {

  [MPP_PIXEL_ARGB](#mpp_pixel_argb),

  [MPP_PIXEL_BGRA](#mpp_pixel_bgra),

  [MPP_PIXEL_RGBA](#mpp_pixel_rgba),

  [MPP_PIXEL_BGRX](#mpp_pixel_bgrx),

  [MPP_PIXEL_RGBX](#mpp_pixel_rgbx),

  [MPP_PIXEL_RGB](#mpp_pixel_rgb),

  [MPP_PIXEL_RGB565](#mpp_pixel_rgb565),

  [MPP_PIXEL_BGR](#mpp_pixel_bgr),

  [MPP_PIXEL_GRAY888](#mpp_pixel_gray888),

  [MPP_PIXEL_GRAY888X](#mpp_pixel_gray888x),

  [MPP_PIXEL_GRAY](#mpp_pixel_gray),

  [MPP_PIXEL_GRAY16](#mpp_pixel_gray16),

  [MPP_PIXEL_YUV1P444](#mpp_pixel_yuv1p444),

  [MPP_PIXEL_VYUY1P422](#mpp_pixel_vyuy1p422),

  [MPP_PIXEL_UYVY1P422](#mpp_pixel_uyvy1p422),

  [MPP_PIXEL_YUYV](#mpp_pixel_yuyv), 

  [MPP_PIXEL_DEPTH16](#mpp_pixel_depth16),

  [MPP_PIXEL_DEPTH8](#mpp_pixel_depth8),

  [MPP_PIXEL_YUV420P](#mpp_pixel_yuv420p),

  [MPP_PIXEL_JPEG](#mpp_pixel_jpeg),

  [MPP_PIXEL_INVALID](#mpp_pixel_invalid) }

- enum [mpp_camera_stream_type](#mpp_camera_stream_type) { 
  
  [RGB_STREAM](#rgb_stream),
  
  [IR_STREAM](#ir_stream),

  [NUM_STREAMS](#num_streams) }

  - enum [mpp_rpmsg_endpoint_addr_e](#mpp_rpmsg_endpoint_addr_e) {
  
  [MPP_RPMSG_EPT_ADDR_INVALID](#mpp_rpmsg_ept_addr_invalid),
  
  [MPP_RPMSG_EPT_ADDR_CORE0_START](#mpp_rpmsg_ept_addr_core0_start),
  
  [MPP_RPMSG_EPT_ADDR_CORE0_STOP](#mpp_rpmsg_ept_addr_core0_stop),
  
  [MPP_RPMSG_EPT_ADDR_CORE1_START](#mpp_rpmsg_ept_addr_core1_start),
  
  [MPP_RPMSG_EPT_ADDR_CORE1_STOP](#mpp_rpmsg_ept_addr_core1_stop) }

- enum [mpp_mcmgr_event_data_e](#mpp_mcmgr_event_data_e) {
  
  [MPP_MCMGR_EVENT_DATA_INVALID](#mpp_mcmgr_event_data_invalid),
  
  [MPP_MCMGR_EVENT_DATA_START](#mpp_mcmgr_event_data_start),
  
  [MPP_MCMGR_EVENT_DATA_STOP](#mpp_mcmgr_event_data_stop) }

- enum [mpp_element_id_t](#mpp_element_id_t) {

  [MPP_ELEMENT_INVALID](#mpp_element_invalid),

  [MPP_ELEMENT_LABELED_RECTANGLE](#mpp_element_labeled_rectangle),

  [MPP_ELEMENT_TEST](#mpp_element_test),

  [MPP_ELEMENT_INFERENCE](#mpp_element_inference),

  [MPP_ELEMENT_CONVERT](#mpp_element_convert),

  [MPP_ELEMENT_IMG_DECODE](#mpp_element_img_decode),

  [MPP_ELEMENT_IMG_COMPOSE](#mpp_element_img_compose),

  [MPP_ELEMENT_IMG_QUALITY_CHECK](#mpp_element_img_quality_check),

  [MPP_ELEMENT_VIDEO_DECODE](#mpp_element_video_decode),

  [MPP_ELEMENT_NUM](#mpp_element_num) }

- enum [mpp_tensor_type_t](#mpp_tensor_type_t) {

  [MPP_TENSOR_TYPE_FLOAT32](#mpp_tensor_type_float32),

  [MPP_TENSOR_TYPE_UINT8](#mpp_tensor_type_uint8),

  [MPP_TENSOR_TYPE_INT8](#mpp_tensor_type_int8) }

- enum [mpp_tensor_order_t](#mpp_tensor_order_t) {

  [MPP_TENSOR_ORDER_UNKNOWN](#mpp_tensor_order_unknown),

  [MPP_TENSOR_ORDER_NHWC](#mpp_tensor_order_nhwc),

  [MPP_TENSOR_ORDER_NCHW](#mpp_tensor_order_nchw) }

- enum [mpp_inference_type_t](#mpp_inference_type_t) { 
  
  [MPP_INFERENCE_TYPE_TFLITE](#mpp_inference_type_tflite),
  
  [MPP_INFERENCE_TYPE_EXECUTORCH](#mpp_inference_type_executorch) }

### 2.1  Detailed Description

This section provides the detailed documentation for the MCU Media Processing Pipeline types.

#### 2.1.1 Data Structure Documentation

##### mpp_stats_t

**union mpp\_stats\_t**

**Data Fields**

|type|name|description|
| - | - | - |
|struct [mpp_stats_t.api](#mpp_stats_tapi)|api|Global execution performance counters.|
|struct [mpp_stats_t.mpp](#mpp_stats_tmpp)|mpp|Pipeline execution performance counters.|
|struct [mpp_stats_t.elem](#mpp_stats_telem)|elem|Element execution performance counters.|

##### mpp_api_params_t

**struct mpp\_api\_params\_t**

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_stats_t ](#mpp_stats_t)∗|stats|API stats.|
|unsigned int|rc\_cycle\_min|minimum cycle duration for RC tasks (ms), 0: sets default value|
|unsigned int|rc\_cycle\_inc|time increment for RC tasks (ms), 0: sets default value|
|int|pipeline\_task\_max\_prio|pipeline tasks maximum priority.|
|int|pipeline_rc_task_prio|pipeline run-to-completion tasks priority. 0: sets default value|
|int|pipeline_pr_task_prio|pipeline preemptable tasks priority. 0: sets default value|

##### mpp_params_t

**struct mpp\_params\_t**

Pipeline creation parameters.

**Data Fields**

- int(∗evt\_callback\_f )([mpp_t ](#mpp_t)mpp, [mpp_evt_t evt,](#mpp_evt_t) void ∗evt\_data, void ∗user\_data)
- [mpp_evt_mask_t ](#mpp_evt_mask_t)mask
- [mpp_exec_flag_t ](#mpp_exec_flag_t)exec\_flag
- void ∗cb\_userdata

##### mpp_stats_t

**struct mpp\_stats\_t**

**Data Fields**

- [mpp_stats_t ](#mpp_stats_t)∗stats

##### mpp_camera_stream_cfg

**struct  mpp\_camera\_stream\_cfg**

Camera stream configuration for multi-stream cameras. 

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_camera_stream_type](#mpp_camera_stream_type)|type|Stream type (member of enum mpp_camera_stream_type)|
|bool|active|Stream is active or not.|
|int|height||
|int|width||

##### mpp_camera_params_t

**struct mpp\_camera\_params\_t**

Camera parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|int|height|buffer height|
|int|width|buffer width|
|[mpp_pixel_format_t](#mpp_pixel_format_t)|format|pixel format|
|int|fps|frames per second|
|bool|stripe|stripe mode|
|void *|rpmsg\_inst|pointer to rpmsg instance|
|volatile uint16_t *|mcmgr\_event\_data|pointer to mcmgr event data|
|uint32_t|n\_streams|number of total output video streams|
|mpp\_camera\_stream\_cfg|stream[NUM\_STREAMS]|streams configuration|
|bool|in\_advance\_enqueue|enable in-advance enqueue mode|

##### mpp_img_params_t

**struct mpp\_img\_params\_t**

Static image parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|int|height|buffer height|
|int|width|buffer width|
|[mpp_pixel_format_t](#mpp_pixel_format_t)|format|pixel format|
|bool|stripe|stripe mode|
|int|compressed\_size|size in bytes for compressed format|

##### mpp_filesrc_params_t

**struct mpp\_filesrc\_params\_t**

File source parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|const char ∗|filepath|Path to file on SD card.|
|bool|loop|Flag to enable looping the file.|
|int|file\_buffer\_size|size in bytes for compressed format|
|[slice_search_func_t](#slice_search_func_t)|slice\_search\_func|Optional: function to search for slices/chunks of data.|

##### mpp_mc_params_t

**struct mpp\_mc\_params\_t**

MC element parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|void ∗|rpmsg\_inst|pointer to rpmsg instance|
|uint16\_t|remote\_event\_data|remote event data for mcmgr|
|uint32\_t|local\_rpmsg\_addr|local rpmsg endpoint address|
|uint32\_t|remote\_rpmsg\_addr|remote rpmsg endpoint address|

##### mpp_display_params_t

**struct mpp\_display\_params\_t**

Display parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|int|height|buffer resolution: setting to 0 will default to panel physical resolution|
|int|width|buffer resolution: setting to 0 will default to panel physical resolution|
|int|pitch|buffer resolution: setting to 0 will default to panel physical resolution|
|int|left|active rect: setting to 0 will default to fullscreen|
|int|top|active rect: setting to 0 will default to fullscreen|
|int|right|active rect: setting to 0 will default to fullscreen|
|int|bottom|active rect: setting to 0 will default to fullscreen|
|[mpp_rotate_degree_t](#mpp_rotate_degree_t)|rotate|rotate degree|
|[mpp_pixel_format_t](#mpp_pixel_format_t)|format|pixel format|
|bool|stripe|stripe mode|
|void ∗|handle|pointer to an lvgl image widget|

##### mpp_tensor_dims_t

**struct mpp\_tensor\_dims\_t**

Inference tensor dimensions.

**Data Fields**

|type|name|description|
| - | - | - |
|uint32\_t|size||
|uint32\_t|data[[MAX_TENSOR_DIMS\]]||

##### mpp_inference_tensor_params_t

**struct mpp\_inference\_tensor\_params\_t**

tensor parameters 

**Data Fields**

|type|name|description|
| - | - | - |
|const uint8\_t ∗|data|data address|
|[mpp_tensor_dims_t](#mpp_tensor_dims_t)|dims|tensor data dimensions|
|[mpp_tensor_type_t](#mpp_tensor_type_t)|type|tensor data type|

##### mpp_inference_cb_param_t

**struct mpp\_inference\_cb\_param\_t**

Inference callback parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|void ∗|user\_data|callback will pass this pointer|
|[mpp_inference_tensor_params_t](#mpp_inference_tensor_params_t) ∗|out\_tensors[[MPP_INFERENCE_MAX_OUTPUTS]]|output tensors parameters|
|int|inference\_time\_ms|inference run time measurement - output to user|
|[mpp_inference_type_t](#mpp_inference_type_t)|inference\_type|type of the inference|

##### img_quality_metrics

**struct img\_quality\_metrics\_t**

**Data Fields**

|type|name|description|
| - | - | - |
|int|brightness|brightness metric|
|int|contrast|contrast metric|

##### mpp_color_t

**union mpp\_color\_t**

mpp color encoding 

**Data Fields**

|type|name|description|
| - | - | - |
|uint32\_t|raw|Raw color.|
|struct [mpp_color_t.rgb](#mpp_color_trgb)|rgb|rgb color values RGB color|

##### mpp_labeled_rect_t

**struct mpp\_labeled\_rect\_t**

mpp labeled rectangle element structure 

**Data Fields**

|type|name|description|
| - | - | - |
|uint8\_t|label[64]|label to print|
|uint16\_t|clear|clear rectangle|
|uint16\_t|line\_width|rectangle line thickness|
|[mpp_color_t](#mpp_color_t)|line\_color|rectangle line color|
|int16\_t|top|rectangle top position|
|int16\_t|left|rectangle left position|
|int16\_t|bottom|rectangle bottom position|
|int16\_t|right|rectangle right position|
|uint16\_t|tag|labeled rectangle tag|
|uint16\_t|reserved|pad for 32 bits alignment|
|bool|stripe|stripe mode|

##### mpp_landmark_t

**struct mpp\_landmark\_t**

mpp landmark structure

**Data Fields**

|type|name|description|
| - | - | - |
|uint16\_t|clear|clear landmark|
|uint16\_t|width|landmark thickness|
|[mpp_color_t](#mpp_color_t)|color|landmark color|
|int16\_t|x|landmark x position|
|int16\_t|y|landmark y position|
|uint16\_t|tag|landmark tag|
|bool|stripe|stripe mode|

##### mpp_area_t

**struct mpp\_area\_t**

Image area coordinates. 

**Data Fields**

|type|name|description|
| - | - | - |
|int|top||
|int|left||
|int|bottom||
|int|right||

##### mpp_dims_t

**struct mpp\_dims\_t**

Image dimensions. 

**Data Fields**

|type|name|description|
| - | - | - |
|unsigned int|width||
|unsigned int|height||

##### mpp_position_t

**struct mpp\_position\_t**

Image position. 

**Data Fields**

|type|name|description|
| - | - | - |
|int|<a name="mpp_inference_params_t"></a>top||
|int|left||

**struct mpp\_inference\_params\_t**

Model parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|uint64\_t|constant\_weight\_MemSize|model constant weights memory size|
|uint64\_t|mutable\_weight\_MemSize|Defines the amount of memory required both input & output data buffers.|
|uint64\_t|activations\_MemSize|Size of scratch memory used for intermediate computations needed by the model.|
|int|num\_inputs|model's number of inputs|
|int|num\_outputs|model's number of outputs|
|uint64\_t|inputs\_offsets[[MPP_INFERENCE_MAX_INPUTS](#mpp_inference_max_inputs)]|offset of each input|
|uint64\_t|outputs\_offsets[[MPP_INFERENCE_MAX_OUTPUTS](#mpp_inference_max_outputs)]|offset ofeach output|
|[inference_entry_point_t](#inference_entry_point_t)|model\_entry\_point|function called to perform the inference|
|[mpp_tensor_type_t](#mpp_tensor_type_t)|model\_input\_tensors\_type|type of input buffer|

##### mpp_img_compose_param_t

**struct mpp\_img\_compose\_param\_t**

Image composition parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|int|height|original image height|
|int|width|original image width|
|[mpp_pixel_format_t](#_page31_x70.87_y743.32)|format|pixel format|
|void ∗|buffer|image buffer address|
|[mpp_area_t](#mpp_area_t)|dest\_area|area for image in destination|

##### mpp_element_params_t

**struct mpp\_element\_params\_t**

Static image and Processing elements parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|union [mpp_element_params_t.__unnamed5__](#mpp_element_params_t__unnamed5__)|\_\_unnamed\_\_||
|[mpp_stats_t ](#mpp_stats_t)∗|stats||

##### mpp_stats_tapi

**struct mpp\_stats\_t.api**

**Data Fields**

|type|name|description|
| - | - | - |
|unsigned int|rc\_cycle|run-to-completion (RC) cycle duration (ms)|
|unsigned int|rc\_cycle\_max|run-to-completion work deadline (ms)|
|unsigned int|pr\_slot|available slot for preemptable (PR) work (ms)|
|unsigned int|pr\_rounds|number of RC cycles required to complete one PR cycle (ms)|
|unsigned int|app\_slot|remaining time for application (ms)|
|unsigned int|cpu\_load|CPU load percentage (%)|

##### mpp_stats_tmpp

**struct mpp\_stats\_t.mpp**

**Data Fields**
 
|type|name|description|
| - | - | - |
|[mpp_t](#mpp_t)|mpp||
|unsigned int|mpp\_exec\_time|pipeline execution time (ms)|
|unsigned int|fps|frames processed per second|

##### mpp_stats_telem

**struct mpp\_stats\_t.elem**

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_elem_handle_t](#mpp_elem_handle_t)|hnd||
|unsigned int|elem\_exec\_time|element execution time (ms)|

##### mpp_color_trgb

**struct mpp\_color\_t.rgb**

rgb color values 

**Data Fields**

|type|name|description|
| - | - | - |
|uint8\_t|R|Red byte.|
|uint8\_t|G|Green byte.|
|uint8\_t|B|Blue byte.|
|uint8\_t|pad|padding byte|

##### mpp_element_params_t__unnamed5__

**union mpp\_element\_params\_t.\_\_unnamed5\_\_**

**Data Fields**
 
|type|name|description|
| - | - | - |
|[mpp_camera_params_t](#mpp_camera_params_t)|camera|Camera element's parameters|
|struct [mpp_element_params_t.__unnamed5__.static\_image](#mpp_element_params_t__unnamed5__static_image)|static\_image|Static Image element's parameters.|
|[mpp_mc_params_t](#mpp_mc_params_t)|mc\_source|Multicore source element's parameters. Multicore source element's parameters|
|struct [mpp_element_params_t.__unnamed5__.compose](#mpp_element_params_t__unnamed5__compose)|compose|Compose element's parameters.|
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.labels](#mpp_element_params_t__unnamed5__labels)</p>|labels|Labeled Rectangle and Landmarks element parameters.|
|struct [mpp_element_params_t.__unnamed5__.convert](#mpp_element_params_t__unnamed5__convert)|convert|Convert element's parameters.|
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.resize](#mpp_element_params_t__unnamed5__resize)</p>|resize|Resize element's parameters.|
|struct [mpp_element_params_t.__unnamed5__.color_conv](#mpp_element_params_t__unnamed5__color_conv)|color\_conv|Color convert element's parameters.|
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.rotate](#mpp_element_params_t__unnamed5__rotate)</p>|rotate|Rotate element's parameters.|
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.test](#mpp_element_params_t__unnamed5__test)</p>|test|Test element's parameters.|
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.decode](#mpp_element_params_t__unnamed5__decode)|decode|Decoder element's parameters.|
|struct [mpp_element_params_t.__unnamed5__.ml_inference](#mpp_element_params_t__unnamed5__ml_inference)|ml\_inference|ML inference element's parameters.|
|struct [mpp_element_params_t.__unnamed5__.img_quality_check](#mpp_element_params_t__unnamed5__img_quality_check)|img\_quality\_check|Image quality check element's parameters.|

##### mpp_element_params_t__unnamed5__static_image

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.static\_image**

Static Image element's parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_img_params_t ](#mpp_img_params_t)|img\_params|static image parameters|
|void ∗|img\_buffer|static image buffer address|

##### mpp_element_params_t__unnamed5__compose

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.compose**

Compose element's parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|int|nb\_images|number of images to compose|
|[mpp_img_compose_param_t ](#mpp_img_compose_param_t)∗|image\_list|pointer to array of images to compose|
|[mpp_area_t](#mpp_area_t)|input\_area|area of input stream in destination|
|[mpp_rotate_degree_t](#mpp_rotate_degree_t)|out\_angle|output rotation angle|
|[mpp_flip_mode_t](#mpp_flip_mode_t)|out\_flip|output flip mode|
|[mpp_pixel_format_t](#mpp_flip_mode_t)|out\_format|output color format|
|int|out\_width|output buffer width|
|int|out\_height|output buffer height|

##### mpp_element_params_t__unnamed5__labels

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.labels**

Labeled Rectangle element's parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|uint32\_t|max\_rect|maximum number of rectangles|
|uint32\_t|detected\_rect|detected rectangles|
|[mpp_labeled_rect_t ](#mpp_labeled_rect_t)∗|rectangles|array of rectangle data|
|uint32\_t|max\_landmk|maximum number of landmarks|
|uint32\_t|detected\_landmk|detected landmarks|
|mpp\_landmark\_t *|landmarks|array of landmark data|

##### mpp_element_params_t__unnamed5__convert

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.convert**

Convert element's parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_dims_t](#mpp_dims_t)|out\_buf|output buffer dimensions|
|[mpp_pixel_format_t](#mpp_pixel_format_t)|pixel\_format|new pixel format|
|[mpp_rotate_degree_t](#mpp_rotate_degree_t)|angle|rotation angle|
|[mpp_flip_mode_t](#mpp_flip_mode_t)|flip|flip mode|
|[mpp_area_t](#mpp_area_t)|crop|input crop area|
|[mpp_position_t](#mpp_position_t)|out\_window|output window position|
|[mpp_dims_t](#mpp_dims_t)|scale|scaling dimensions|
|[mpp_convert_ops_t](#mpp_convert_ops_t)|ops|operation selector mask|
|const char ∗|dev\_name|device name used for graphics|
|bool|stripe\_in|input stripe mode|
|bool|stripe\_out|output stripe mode|

##### mpp_element_params_t__unnamed5__resize

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.resize**

Resize element's parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|unsigned int|width||
|unsigned int|height||

##### mpp_element_params_t__unnamed5__color_conv

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.color\_conv**

Color convert element's parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_pixel_format_t](#mpp_pixel_format_t)|<a name="mpp_element_params_t__unnamed5__rotate"></a>pixel\_format||

##### mpp_element_params_t__unnamed5__rotate

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.rotate**

Rotate element's parameters. 

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_rotate_degree_t](#mpp_rotate_degree_t)|angle||

##### mpp_element_params_t__unnamed5__test

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.test**

Test element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|\_Bool|inp||
|unsigned int|width||
|unsigned int|height||
|[mpp_pixel_format_t](#mpp_pixel_format_t)|format||

##### mpp_element_params_t__unnamed5__decode

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.decode**

Decoder element's parameters.

 **Data Fields**

|type|name|description|
| - | - | - |
|const char ∗|dev\_name|device name used for decoder|
|unsigned int|width||
|unsigned int|height||
|[mpp_pixel_format_t](#mpp_pixel_format_t)|out\_format||

##### mpp_element_params_t__unnamed5__ml_inference

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.ml\_inference**

ML inference element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|const void ∗|model\_data|pointer to model binary|
|[mpp_inference_type_t](#mpp_inference_type_t)|type|inference type|
|int|model\_size|model binary size|
|float|model\_input\_mean|model 'mean' of input values, used for normalization|
|float|model\_input\_std|model 'standard deviation' of input values, used for normalization|
|[mpp_tensor_order_t](#mpp_tensor_order_t)|tensor\_order|model input tensor component order|
|[mpp_inference_params_t](#mpp_inference_params_t)|inference\_params|model specific parameters used by the inference|

##### mpp_element_params_t__unnamed5__img_quality_check

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.img\_quality\_check**

Image quality check element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|\_Bool|disable|disable quality check|

#### 2.1.2 Macro Definition Documentation

**MPP\_INFERENCE\_MAX\_OUTPUTS**

##### mpp_inference_max_outputs

#define MPP\_INFERENCE\_MAX\_OUTPUTS

Maximum number of inference inputs and outputs. Maximum number of outputs supported by the pipeline

##### mpp_inference_max_inputs

#define MPP\_INFERENCE\_MAX\_INPUTS

Maximum number of inputs supported by the pipeline.


##### mpp_invalid

#define MPP\_INVALID Invalid pipeline handle.


##### mpp_event_all

#define MPP\_EVENT\_ALL

Bit mask to receive all Events.

##### max_tensor_dims

#define MAX\_TENSOR\_DIMS

Maximum number of dimensions for tensors.

#### 2.1.3 Typedef Documentation


##### mpp_t

typedef void∗ [mpp_t ](#mpp_t)Pipeline handle type.


##### mpp_elem_handle_t

typedef uintptr\_t [mpp_elem_handle_t ](#mpp_elem_handle_t)Element handle type.


##### mpp_evt_mask_t

typedef unsigned int [mpp_evt_mask_t ](#mpp_evt_mask_t)Event mask for pipeline creation.

##### slice_search_func_t

typedef int(∗ slice\_search\_func\_t) (const uint8\_t ∗data, int32\_t len)

File slice search function type.

##### inference_entry_point_t

typedef int(∗ inference\_entry\_point\_t) (uint8\_t ∗, uint8\_t ∗, uint8\_t ∗) Bundle inference function type.

#### 2.1.4 Enumeration Type Documentation


##### mpp_evt_t

enum [mpp_evt_t](#mpp_evt_t)

Pipeline generated events.

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_event_invalid"></a>MPP\_EVENT\_INVALID|invalid event|
|<a name="mpp_event_inference_input_ready"></a>MPP\_EVENT\_INFERENCE\_INPUT\_READY|RGB image for inference is ready.|
|<a name="mpp_event_inference_output_ready"></a>MPP\_EVENT\_INFERENCE\_OUTPUT\_READY|inference out is ready|
|<a name="mpp_event_quality_check_ready"></a>MPP\_EVENT\_QUALITY\_CHECK\_READY|quality check measurements are ready|
|<a name="mpp_event_internal_test_reserved"></a>MPP\_EVENT\_INTERNAL\_TEST\_RESERVED|INTERNAL: DO NOT USE.|
|<a name="mpp_event_num"></a>MPP\_EVENT\_NUM|DO NOT USE.|


##### mpp_exec_flag_t

enum [mpp_exec_flag_t ](#mpp_exec_flag_t)

Execution parameters.

These parameters control the execution of the elements of an mpp.

The "mpps" created using the flag MPP\_EXEC\_RC are guaranteed to run up to the completion of all processing elements, while not being preempted by other "mpps".

The "mpps" created using the flag MPP\_EXEC\_PREEMPT are preempted after a given time interval by "mpps" that will run-to-completion again.

The "mpps" created with the MPP\_EXEC\_INHERIT flag inherit the same execution flag as the parent(s) in case of split operation.

Note: It is not possible to request run-to-completion execution when spliting preemptable-execution "mpps".

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_exec_inherit"></a>MPP\_EXEC\_INHERIT|inherit from parent(s)|
|<a name="mpp_exec_rc"></a>MPP\_EXEC\_RC|run-to-completion|
|<a name="mpp_exec_preempt"></a>MPP\_EXEC\_PREEMPT|preemptable|


##### mpp_stats_grp_t

enum [mpp_stats_grp_t ](#mpp_stats_grp_t)

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_stats_grp_api"></a>MPP\_STATS\_GRP\_API|API (global) stats.|
|<a name="mpp_stats_grp_mpp"></a>MPP\_STATS\_GRP\_MPP|mpp\_t stats|
|<a name="mpp_stats_grp_element"></a>MPP\_STATS\_GRP\_ELEMENT|element stats|
|<a name="mpp_stats_grp_num"></a>MPP\_STATS\_GRP\_NUM|number of groups|


##### mpp_rotate_degree_t

enum [mpp_rotate_degree_t ](#mpp_rotate_degree_t)

Rotation value.

**Enumerator**

|label|description|
| - | - |
|<a name="rotate_0"></a>ROTATE\_0|0 degree|
|<a name="rotate_90"></a>ROTATE\_90|90 degrees|
|<a name="rotate_180"></a>ROTATE\_180|180 degrees|
|<a name="rotate_270"></a>ROTATE\_270|270 degrees|


##### mpp_flip_mode_t

enum [mpp_flip_mode_t ](#mpp_flip_mode_t)

Flip type. 

**Enumerator**

|label|description|
| - | - |
|<a name="flip_none"></a>FLIP\_NONE|no flip|
|<a name="flip_horizontal"></a>FLIP\_HORIZONTAL|horizontal flip|
|<a name="flip_vertical"></a>FLIP\_VERTICAL|vertical flip|
|<a name="flip_both"></a>FLIP\_BOTH|vertical and horizontal flip|


##### mpp_convert_ops_t

enum [mpp_convert_ops_t](#mpp_convert_ops_t)

The convert operations selector flags. 

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_convert_none"></a>MPP\_CONVERT\_NONE|no frame conversion|
|<a name="mpp_convert_rotate"></a>MPP\_CONVERT\_ROTATE|frame rotation and flip|
|<a name="mpp_convert_scale"></a>MPP\_CONVERT\_SCALE|scaling from input\_frame toward output window|
|<a name="mpp_convert_color"></a>MPP\_CONVERT\_COLOR|frame color conversion|
|<a name="mpp_convert_crop"></a>MPP\_CONVERT\_CROP|input frame crop|
|<a name="mpp_convert_out_window"></a>MPP\_CONVERT\_OUT\_WINDOW|output window|


##### mpp_pixel_format_t

enum [mpp_pixel_format_t ](#mpp_pixel_format_t)

Pixel format. 

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_pixel_argb"></a>MPP\_PIXEL\_ARGB|ARGB 32 bits.|
|<a name="mpp_pixel_bgra"></a>MPP\_PIXEL\_BGRA|BGRA 32 bits.|
|<a name="mpp_pixel_rgba"></a>MPP\_PIXEL\_RGBA|RGBA 32 bits.|
|<a name="mpp_pixel_bgrx"></a>MPP\_PIXEL\_BGRX|BGRX 32 bits.|
|<a name="mpp_pixel_rgbx"></a>MPP\_PIXEL\_RGBX|RGBX 32 bits.|
|<a name="mpp_pixel_rgb"></a>MPP\_PIXEL\_RGB|RGB 24 bits.|
|<a name="mpp_pixel_rgb565"></a>MPP\_PIXEL\_RGB565|RGB 16 bits.|
|<a name="mpp_pixel_bgr"></a>MPP\_PIXEL\_BGR|BGR 24 bits.|
|<a name="mpp_pixel_gray888"></a>MPP\_PIXEL\_GRAY888|gray 3x8 bits|
|<a name="mpp_pixel_gray888x"></a>MPP\_PIXEL\_GRAY888X|gray 3x8 bits +8 unused bits|
|<a name="mpp_pixel_gray"></a>MPP\_PIXEL\_GRAY|gray 8 bits|
|<a name="mpp_pixel_gray16"></a>MPP\_PIXEL\_GRAY16|gray 16 bits|
|<a name="mpp_pixel_yuv1p444"></a>MPP\_PIXEL\_YUV1P444|YUVX interleaved 4:4:4.|
|<a name="mpp_pixel_vyuy1p422"></a>MPP\_PIXEL\_VYUY1P422|VYUY interleaved 4:2:2.|
|<a name="mpp_pixel_uyvy1p422"></a>MPP\_PIXEL\_UYVY1P422|UYVY interleaved 4:2:2.|
|<a name="mpp_pixel_yuyv"></a>MPP\_PIXEL\_YUYV|YUYV interleaved 4:2:2.|
|<a name="mpp_pixel_depth16"></a>MPP\_PIXEL\_DEPTH16|depth 16 bits|
|<a name="mpp_pixel_depth8"></a>MPP\_PIXEL\_DEPTH8|depth 8 bits|
|<a name="mpp_pixel_yuv420p"></a>MPP\_PIXEL\_YUV420P|YUV planar 4:2:0.|
|<a name="mpp_pixel_jpeg"></a>MPP\_PIXEL\_JPEG|JPEG.|
|<a name="mpp_pixel_invalid"></a>MPP\_PIXEL\_INVALID|invalid pixel format|

##### mpp_camera_stream_type

enum [mpp_camera_stream_type](#mpp_camera_stream_type)

Camera stream type for multiple stream camera.

**Enumerator**

|label|description|
| - | - |
|<a name="rgb_stream"></a>RGB\_STREAM|Frames received by the virtual camera element ar in rgb format.|
|<a name="ir_stream"></a>IR\_STREAM|Frames received by the virtual camera element ar in ir format.|
|<a name="num_stream"></a>NUM\_STREAMS|Total number of frame types suported by virtual camera element.|

##### mpp_rpmsg_endpoint_addr_e

enum [mpp_rpmsg_endpoint_addr_e](#mpp_rpmsg_endpoint_addr_e)

Multicore pipeline RPMSG endpoint addresses.

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_rpmsg_ept_addr_invalid"></a>MPP\_RPMSG\_EPT\_ADDR\_INVALID|invalid endpoint address|
|<a name="mpp_rpmsg_ept_addr_core0_start"></a>MPP\_RPMSG\_EPT\_ADDR\_CORE0\_START|core 0 endpoint start address|
|<a name="mpp_rpmsg_ept_addr_core0_stop"></a>MPP\_RPMSG\_EPT\_ADDR\_CORE0\_STOP|core 0 endpoint stop address|
|<a name="mpp_rpmsg_ept_addr_core1_start"></a>MPP\_RPMSG\_EPT\_ADDR\_CORE1\_START|core 1 endpoint start address|
|<a name="mpp_rpmsg_ept_addr_core1_stop"></a>MPP\_RPMSG\_EPT\_ADDR\_CORE1\_STOP|core 1 endpoint stop address|

##### mpp_mcmgr_event_data_e

enum [mpp_mcmgr_event_data_e](#mpp_mcmgr_event_data_e)

Multicore pipeline MCMGR remote event data.

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_mcmgr_event_data_invalid"></a>MPP\_MCMGR\_EVENT\_DATA\_INVALID|invalid mcmgr event data|
|<a name="mpp_mcmgr_event_data_start"></a>MPP\_MCMGR\_EVENT\_DATA\_START|mcmgr event data start|
|<a name="mpp_mcmgr_event_data_stop"></a>MPP\_MCMGR\_EVENT\_DATA\_STOP|mcmgr event data stop|

##### mpp_element_id_t

enum [mpp_element_id_t ](#mpp_element_id_t)

Processing element ids.

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_element_invalid"></a>MPP\_ELEMENT\_INVALID|Invalid element.|
|<a name="mpp_element_labeled_rectangle"></a>MPP\_ELEMENT\_LABELED\_RECTANGLE|Labeled rectangle - bounding box.|
|<a name="mpp_element_test"></a>MPP\_ELEMENT\_TEST|Test inplace element - NOT FOR USE.|
|<a name="mpp_element_inference"></a>MPP\_ELEMENT\_INFERENCE|Inference engine.|
|<a name="mpp_element_convert"></a>MPP\_ELEMENT\_CONVERT|Image conversion: resolution, orientation, color format.|
|<a name="mpp_element_img_decode"></a>MPP\_ELEMENT\_IMG\_DECODE|Image decompression: JPEG, PNG.|
|<a name="mpp_element_img_compose"></a>MPP\_ELEMENT\_IMG\_COMPOSE|compose a simple GUI: logo and text area with the input stream|
|<a name="mpp_element_img_quality_check"></a>MPP\_ELEMENT\_IMG\_QUALITY\_CHECK|Image quality check: brightness, contrast.|
|<a name="mpp_element_video_decode"></a>MPP\_ELEMENT\_VIDEO\_DECODE|Video decode.|
|<a name="mpp_element_num"></a>MPP\_ELEMENT\_NUM|DO NOT USE.|


##### mpp_tensor_type_t

enum [mpp_tensor_type_t ](#mpp_tensor_type_t)

Inference tensor type. 

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_tensor_type_float32"></a>MPP\_TENSOR\_TYPE\_FLOAT32|<a name="mpp_tensor_order_t"></a>floating point 32 bits|
|<a name="mpp_tensor_type_uint8"></a>MPP\_TENSOR\_TYPE\_UINT8|unsigned integer 8 bits|
|<a name="mpp_tensor_type_int8"></a>MPP\_TENSOR\_TYPE\_INT8|signed integer 8 bits|

##### mpp_tensor_order_t

enum [mpp_tensor_order_t ](#mpp_tensor_order_t)

Inference input tensor order.

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_tensor_order_unknown"></a>MPP\_TENSOR\_ORDER\_UNKNOWN|order not set|
|<a name="mpp_tensor_order_nhwc"></a>MPP\_TENSOR\_ORDER\_NHWC|order: Batch, Height, Width, Channels|
|<a name="mpp_tensor_order_nchw"></a>MPP\_TENSOR\_ORDER\_NCHW|order: Batch, Channels, Height, Width|


##### mpp_inference_type_t

enum [mpp_inference_type_t ](#mpp_inference_type_t)

Inference type. 

**Enumerator**

|label|description|
| - | - |
|<a name="mpp_inference_type_tflite"></a>MPP\_INFERENCE\_TYPE\_TFLITE|TensorFlow-Lite.|
|<a name="mpp_inference_type_executorch"></a>MPP\_INFERENCE\_TYPE\_EXECUTORCH|ExecuTorch.|


## 3. Return\_codes

##### return_codes

**Macros**

- #define [MPP_SUCCESS](#mpp_success)
- #define [MPP_ERROR](#mpp_error)
- #define [MPP_INVALID_ELEM](#mpp_invalid_elem)
- #define [MPP_INVALID_PARAM](#mpp_invalid_param)
- #define [MPP_ERR_ALLOC_MUTEX](#mpp_err_alloc_mutex)
- #define [MPP_INVALID_MUTEX](#mpp_invalid_mutex)
- #define [MPP_MUTEX_TIMEOUT](#mpp_mutex_timeout)
- #define [MPP_MUTEX_ERROR](#mpp_mutex_error)
- #define [MPP_MALLOC_ERROR](#mpp_malloc_error)

### 3.1 Detailed Description

MPP APIs return status definitions.

#### 3.1.1 Macro Definition Documentation


##### mpp_success

#define MPP\_SUCCESS

Success return code.


##### mpp_error

#define MPP\_ERROR

A generic error occured.


##### mpp_invalid_elem

#define MPP\_INVALID\_ELEM

Invalid element provided.


##### mpp_invalid_param

#define MPP\_INVALID\_PARAM

Invalid parameter provided.


##### mpp_err_alloc_mutex

#define MPP\_ERR\_ALLOC\_MUTEX

Error occured while allocating mutex.


##### mpp_invalid_mutex

#define MPP\_INVALID\_MUTEX

Invalid mutex provided.


##### mpp_mutex_timeout

#define MPP\_MUTEX\_TIMEOUT

Mutex timeout occured.


##### mpp_mutex_error

#define MPP\_MUTEX\_ERROR

Mutex error occured.


##### mpp_malloc_error

#define MPP\_MALLOC\_ERROR 

Memory allocation error occured.

