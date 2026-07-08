# eIQ MCU Media Processing Pipeline API

MPP VERSION 4.4

<a id="mpp-api"></a>

## 1. MPP API

**Functions**

- int[ mpp_api_init (](#mpp_api_init)[mpp_api_params_t ](#mpp_api_params_t) * params)
- [mpp_t ](#mpp_t)[ mpp_create (](#mpp_create)[mpp_params_t ](#mpp_params_t) * params, int * ret)
- int[ mpp_camera_add (](#mpp_camera_add)[mpp_t ](#mpp_t) mpp, const char * name, [mpp_camera_params_t ](#mpp_camera_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h)
- int[ mpp_static_img_add (](#mpp_static_img_add)[mpp_t ](#mpp_t) mpp, [mpp_img_params_t ](#mpp_img_params_t) * params, void * addr, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h)
- int[ mpp_filesrc_add (](#mpp_filesrc_add)[mpp_t ](#mpp_t) mpp, [mpp_filesrc_params_t ](#mpp_filesrc_params_t) * params, void * addr, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h)
- int[ mpp_filesink_add (](#mpp_filesink_add)[mpp_t ](#mpp_t) mpp, [mpp_filesink_params_t ](#mpp_filesink_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h)
- int[ mpp_rtspsink_add (](#mpp_rtspsink_add)[mpp_t ](#mpp_t) mpp, [mpp_rtspsink_params_t ](#mpp_rtspsink_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h)
- int[ mpp_mc_source_add (](#mpp_mc_source_add)[mpp_t ](#mpp_t) mpp, [mpp_mc_params_t ](#mpp_mc_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h)
- int[ mpp_display_add (](#mpp_display_add)[mpp_t ](#mpp_t) mpp, const char * name, [mpp_display_params_t ](#mpp_display_params_t) * params)
- int[ mpp_mc_sink_add (](#mpp_mc_sink_add)[mpp_t ](#mpp_t) mpp, [mpp_mc_params_t ](#mpp_mc_params_t) * params)
- int[ mpp_nullsink_add (](#mpp_nullsink_add)[mpp_t ](#mpp_t) mpp)
- void *[ mpp_get_output_buff_address (](#mpp_get_output_buff_address)[mpp_elem_handle_t ](#mpp_elem_handle_t) elem, uint32_t index)
- void *[ mpp_get_input_buff_address (](#mpp_get_input_buff_address)[mpp_elem_handle_t ](#mpp_elem_handle_t) elem, uint32_t index)
- int[ mpp_element_add (](#mpp_element_add)[mpp_t ](#mpp_t) mpp, [mpp_element_id_t ](#mpp_element_id_t) id, [mpp_element_params_t ](#mpp_element_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h)
- int[ mpp_split (](#mpp_split)[mpp_t ](#mpp_t) mpp, unsigned int num, [mpp_params_t ](#mpp_params_t) * params, [mpp_t ](#mpp_t) * out\_list)
- int[ mpp_background (](#mpp_background)[mpp_t ](#mpp_t) mpp, [mpp_params_t ](#mpp_params_t) * params, [mpp_t ](#mpp_t) * out\_mpp)
- int[ mpp_element_update (](#mpp_element_update)[mpp_t ](#mpp_t) mpp, [mpp_elem_handle_t ](#mpp_elem_handle_t) elem\_h, [mpp_element_params_t ](#mpp_element_params_t) * params, bool force\_update)
- bool[ mpp_is_running (](#mpp_is_running)[mpp_t ](#mpp_t) mpp)
- int[ mpp_start (](#mpp_start)[mpp_t ](#mpp_t) mpp, int last, bool force\_update)
- int[ mpp_stop (](#mpp_stop)[mpp_t ](#mpp_t) mpp)
- int[ mpp_force_update (](#mpp_force_update)[mpp_t ](#mpp_t) mpp)
- void[ mpp_stats_enable (](#mpp_stats_enable)[mpp_stats_grp_t ](#mpp_stats_grp_t) grp)
- void[ mpp_stats_disable (](#mpp_stats_disable)[mpp_stats_grp_t ](#mpp_stats_grp_t) grp)
- char *[ mpp_get_version (](#mpp_get_version)void)
- int[ mpp_storage_init (](#mpp_storage_init)void)
- int[ mpp_storage_get_free_space (](#mpp_storage_get_free_space)uint64_t * free\_bytes, uint64_t * total\_bytes)
- int[ mpp_eth_netif_init (](#mpp_eth_netif_init)uint8_t ip\_addr, uint8_t netmask, uint8_t gateway)

### 1.1 Detailed Description

This section provides the detailed documentation for the MCU Media Processing Pipeline API.

#### 1.1.1 Function Documentation

##### mpp_api_init

int mpp\_api\_init ( [mpp_api_params_t ](#mpp_api_params_t) * params )

Pipeline initialization.

This function initializes the library and its data structures. It must be called before any other function of the API is called.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|params|API global parameters|

**Returns**

[Return Codes](#return-codes)

##### mpp_create

[mpp_t ](#mpp_t) mpp\_create ( [mpp_params_t ](#mpp_params_t) * params, int * ret )

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

int mpp\_camera\_add ( [mpp_t ](#mpp_t) mpp, const char * name, [mpp_camera_params_t ](#mpp_camera_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h )

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

[Return Codes](#return-codes)

##### mpp_static_img_add

int mpp\_static\_img\_add ( [mpp_t ](#mpp_t) mpp, [mpp_img_params_t ](#mpp_img_params_t) * params, void * addr, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h )

Static image addition.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|static image parameters|
|in|addr|image buffer|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return Codes](#return-codes)

**Precondition**

- Image buffer allocation/free is the responsibility of the user.

##### mpp_filesrc_add

int mpp\_filesrc\_add ( [mpp_t ](#mpp_t) mpp, [mpp_filesrc_params_t ](#mpp_filesrc_params_t) * params, void * addr, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h )

Source file addition.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|file parameters|
|in|addr|file buffer|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return Codes](#return-codes)

##### mpp_filesink_add

int mpp\_filesink\_add ( [mpp_t ](#mpp_t) mpp, [mpp_filesink_params_t ](#mpp_filesink_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h )

Sink file addition.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|file parameters|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return Codes](#return-codes)

##### mpp_rtspsink_add

int mpp\_rtspsink\_add ( [mpp_t ](#mpp_t) mpp, [mpp_rtspsink_params_t ](#mpp_rtspsink_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h )

Sink RTSP addition.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|file parameters|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return Codes](#return-codes)

##### mpp_mc_source_add

int mpp\_mc\_source\_add ( [mpp_t ](#mpp_t) mpp, [mpp_mc_params_t ](#mpp_mc_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h )

Multi core source addition.

This function adds a multi core source to the pipeline.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|parameters that are configured on the multi core source|
|out|elem\_h|element handle in pipeline|

**Returns**

[Return Codes](#return-codes)

##### mpp_display_add

int mpp\_display\_add ( [mpp_t ](#mpp_t) mpp, const char * name, [mpp_display_params_t ](#mpp_display_params_t) * params )

Display addition.

This function adds a display to the pipeline.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|name|display driver name|
|in|params|parameters that are configured on the display|

**Returns**

[Return Codes](#return-codes)

##### mpp_mc_sink_add

int mpp\_mc\_sink\_add ( [mpp_t ](#mpp_t) mpp, [mpp_mc_params_t ](#mpp_mc_params_t) * params )

Multi core sink addition.

This function adds a multi core sink to the pipeline.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|parameters that are configured on the multi core sink|

**Returns**

[Return Codes](#return-codes)

##### mpp_nullsink_add

int mpp\_nullsink\_add ( [mpp_t ](#mpp_t) mpp )

Null sink addition.

This function adds a null-type sink to the pipeline. After this call pipeline is closed and no further elements can be added. Input frames are discarded.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|

**Returns**

[Return Codes](#return-codes)

##### mpp_get_output_buff_address

void * mpp\_get\_output\_buff\_address ( [mpp_elem_handle_t ](#mpp_elem_handle_t) elem, uint32_t index )

Get output buffer address.

This function returns the address of an output buffer for a given element.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|elem|element handle in pipeline|
|in|index|output buffer index (must be less than MAX_OUTPUT_PORTS and element's nb_out_buf)|

**Returns**

pointer to the output buffer address if successful, NULL if element is invalid or index is out of range

##### mpp_get_input_buff_address

void * mpp\_get\_input\_buff\_address ( [mpp_elem_handle_t ](#mpp_elem_handle_t) elem, uint32_t index )

Get input buffer address.

This function returns the address of an input buffer for a given element.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|elem|element handle in pipeline|
|in|index|input buffer index (must be less than MAX_INPUT_PORTS and element's nb_in_buf)|

**Returns**

pointer to the input buffer address if successful, NULL if element is invalid or index is out of range

##### mpp_element_add

int mpp\_element\_add ( [mpp_t ](#mpp_t) mpp, [mpp_element_id_t ](#mpp_element_id_t) id, [mpp_element_params_t ](#mpp_element_params_t) * params, [mpp_elem_handle_t ](#mpp_elem_handle_t) * elem\_h )

Add processing element (single input, single output)

This function adds an element to the pipeline.
Available elements are:
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

[Return Codes](#return-codes)

##### mpp_split

int mpp\_split ( [mpp_t ](#mpp_t) mpp, unsigned int num, [mpp_params_t ](#mpp_params_t) * params, [mpp_t ](#mpp_t) * out\_list )

Pipeline multiplication.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|num|number of output pipeline|
|in|params|split mpp parameters|
|out|out\_list|list of output pipelines|

**Returns**

[Return Codes](#return-codes)

**Precondition**

- *out_list* array must contain at least *num* elements.

##### mpp_background

int mpp\_background ( [mpp_t ](#mpp_t) mpp, [mpp_params_t ](#mpp_params_t) * params, [mpp_t ](#mpp_t) * out\_mpp )

Put next elements processing in background.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|params|new mpp parameters (exec_flag must be MPP_EXEC_PREEMPT)|
|out|out\_mpp|output pipeline|

**Returns**

[Return Codes](#return-codes)

##### mpp_element_update

int mpp\_element\_update ( [mpp_t ](#mpp_t) mpp, [mpp_elem_handle_t ](#mpp_elem_handle_t) elem\_h, [mpp_element_params_t ](#mpp_element_params_t) * params, bool force\_update )

Update element parameters.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|
|in|elem\_h|element handle in the pipeline.|
|in|params|new element parameters|
|in|force\_update|force the pipeline to run even though there is no input frame update for processing elements after update. If the force_update flag was already requested before, the current value is ignored|

**Returns**

[Return Codes](#return-codes)

##### mpp_is_running

bool mpp\_is\_running ( [mpp_t ](#mpp_t) mpp )

Check if the pipeline is currently running.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|input pipeline|

**Returns**

true if pipeline is in running state, else false

##### mpp_start

int mpp\_start ( [mpp_t ](#mpp_t) mpp, int last, bool force\_update )

Start pipeline.

When called with last=0, this function prepares the branch of the pipeline specified with mpp. When called with last!=0, this function starts the data flow of the pipeline. Data flow should start after all the branches of the pipeline have been prepared.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|pipeline branch handle to start/prepare|
|in|last|if non-zero start pipeline processing. No further start call is possible thereafter.|
|in|force\_update|force the pipeline to run even though there is no input frame update for processing elements. If the force_update flag was already requested before, the current value is ignored|

**Returns**

[Return Codes](#return-codes)

##### mpp_stop

int mpp\_stop ( [mpp_t ](#mpp_t) mpp )

Stop a branch of the pipeline.

This function stops the data processing and peripherals of a pipeline branch.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|pipeline branch to stop|

**Returns**

[Return Codes](#return-codes)

##### mpp_force_update

int mpp\_force\_update ( [mpp_t ](#mpp_t) mpp )

Force the update of a branch of the pipeline.

This function forces an update of the branch of the pipeline even if there is no new input frame.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|mpp|pipeline branch to set the flag force_update to true|

**Returns**

[Return Codes](#return-codes)

##### mpp_stats_enable

void mpp\_stats\_enable ( [mpp_stats_grp_t ](#mpp_stats_grp_t) grp )

Enable statistics collection.

This function enables statistics collection for a given group. Statistics collection is disabled by default after API initialization. Calling this function when stats are enabled has no effect.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|grp|statistics group|

##### mpp_stats_disable

void mpp\_stats\_disable ( [mpp_stats_grp_t ](#mpp_stats_grp_t) grp )

Disable statistics collection.

This function disables statistics collection for a given group. Calling this function when stats are disabled has no effect. This function is used to ensure stats are not updated while application tasks use the stats structures.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|grp|statistics group|

##### mpp_get_version

char * mpp\_get\_version ( void )

Get MPP version.

**Returns**

pointer to the MPP version string

##### mpp_storage_init

int mpp\_storage\_init ( void )

Initialize the storage subsystem.

**Returns**

MPP_SUCCESS on success, MPP_ERROR on failure

##### mpp_storage_get_free_space

int mpp\_storage\_get\_free\_space ( uint64_t * free\_bytes, uint64_t * total\_bytes )

Get free and total storage space.

**Parameters**

|in/out|name|description|
| - | - | - |
|out|free\_bytes|Pointer to store the free space in bytes|
|out|total\_bytes|Pointer to store the total space in bytes|

**Returns**

MPP_SUCCESS on success, MPP_ERROR on failure

##### mpp_eth_netif_init

int mpp\_eth\_netif\_init ( uint8_t ip\_addr, uint8_t netmask, uint8_t gateway )

Initialize Ethernet network interface.

This function initializes the Ethernet network interface with the specified IP address, netmask, and gateway configuration.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|ip\_addr|IP address as array of 4 octets (e.g., {192, 168, 0, 102})|
|in|netmask|Network mask as array of 4 octets (e.g., {255, 255, 255, 0})|
|in|gateway|Gateway address as array of 4 octets (e.g., {192, 168, 0, 100})|

**Returns**

0 on success, non-zero on error

<a id="mpp-types"></a>

## 2. MPP Types

**Data Structures**

- union [mpp_stats_t](#mpp_stats_t)
- struct [mpp_api_params_t](#mpp_api_params_t)
- struct [mpp_params_t](#mpp_params_t)
- struct [mpp_camera_stream_cfg](#mpp_camera_stream_cfg)
- struct [mpp_camera_params_t](#mpp_camera_params_t)
- struct [mpp_img_params_t](#mpp_img_params_t)
- struct [mpp_convert_cb_param_t](#mpp_convert_cb_param_t)
- struct [mpp_filesrc_params_t](#mpp_filesrc_params_t)
- struct [mpp_filesink_params_t](#mpp_filesink_params_t)
- struct [mpp_rtspsink_params_t](#mpp_rtspsink_params_t)
- struct [mpp_mc_params_t](#mpp_mc_params_t)
- struct [mpp_display_params_t](#mpp_display_params_t)
- struct [mpp_tensor_dims_t](#mpp_tensor_dims_t)
- struct [mpp_inference_tensor_params_t](#mpp_inference_tensor_params_t)
- struct [mpp_inference_inp_cb_params](#mpp_inference_inp_cb_params)
- struct [mpp_inference_cb_param_t](#mpp_inference_cb_param_t)
- struct [img_quality_metrics_t](#img_quality_metrics_t)
- union [mpp_color_t](#mpp_color_t)
- struct [mpp_labeled_rect_t](#mpp_labeled_rect_t)
- struct [mpp_landmark_t](#mpp_landmark_t)
- struct [mpp_area_t](#mpp_area_t)
- struct [mpp_dims_t](#mpp_dims_t)
- struct [mpp_position_t](#mpp_position_t)
- struct [mpp_inference_params_t](#mpp_inference_params_t)
- struct [mpp_img_compose_param_t](#mpp_img_compose_param_t)
- struct [mpp_element_params_t](#mpp_element_params_t)
- struct [mpp_stats_t.api](#mpp_stats_t_8api)
- struct [mpp_stats_t.mpp](#mpp_stats_t_8mpp)
- struct [mpp_stats_t.elem](#mpp_stats_t_8elem)
- struct [mpp_color_t.rgb](#mpp_color_t_8rgb)
- union [mpp_element_params_t.__unnamed5__](#mpp_element_params_t_8__unnamed5__)
- struct [mpp_element_params_t.__unnamed5__.static_image](#mpp_element_params_t_8__unnamed5___8static_image)
- struct [mpp_element_params_t.__unnamed5__.compose](#mpp_element_params_t_8__unnamed5___8compose)
- struct [mpp_element_params_t.__unnamed5__.labels](#mpp_element_params_t_8__unnamed5___8labels)
- struct [mpp_element_params_t.__unnamed5__.convert](#mpp_element_params_t_8__unnamed5___8convert)
- struct [mpp_element_params_t.__unnamed5__.resize](#mpp_element_params_t_8__unnamed5___8resize)
- struct [mpp_element_params_t.__unnamed5__.color_conv](#mpp_element_params_t_8__unnamed5___8color_conv)
- struct [mpp_element_params_t.__unnamed5__.rotate](#mpp_element_params_t_8__unnamed5___8rotate)
- struct [mpp_element_params_t.__unnamed5__.test](#mpp_element_params_t_8__unnamed5___8test)
- struct [mpp_element_params_t.__unnamed5__.decode](#mpp_element_params_t_8__unnamed5___8decode)
- struct [mpp_element_params_t.__unnamed5__.encode](#mpp_element_params_t_8__unnamed5___8encode)
- struct [mpp_element_params_t.__unnamed5__.ml_inference](#mpp_element_params_t_8__unnamed5___8ml_inference)
- struct [mpp_element_params_t.__unnamed5__.img_quality_check](#mpp_element_params_t_8__unnamed5___8img_quality_check)

**Macros**

- #define [MPP_INFERENCE_MAX_OUTPUTS](#mpp_inference_max_outputs)
- #define [MPP_INFERENCE_MAX_INPUTS](#mpp_inference_max_inputs)
- #define [MPP_IP_ADDR_LEN](#mpp_ip_addr_len)
- #define [MPP_INVALID](#mpp_invalid)
- #define [MPP_EVENT_ALL](#mpp_event_all)
- #define [MPP_MAX_RPMSG_EPT_PER_CORE](#mpp_max_rpmsg_ept_per_core)
- #define [MAX_TENSOR_DIMS](#max_tensor_dims)

**Typedefs**

- typedef void * [mpp\_t](#mpp_t)
- typedef uintptr_t [mpp\_elem\_handle\_t](#mpp_elem_handle_t)
- typedef unsigned int [mpp\_evt\_mask\_t](#mpp_evt_mask_t)
- typedef int(* [slice\_search\_func\_t](#slice_search_func_t))(const uint8_t *data, int32_t len)
- typedef int(* [inference\_entry\_point\_t](#inference_entry_point_t))(uint8_t *, uint8_t *, uint8_t *)

**Enumerations**

- enum [mpp_evt_t](#mpp_evt_t) {

  [MPP_EVENT_INVALID](#mpp_event_invalid),

  [MPP_EVENT_INFERENCE_INPUT_READY](#mpp_event_inference_input_ready),

  [MPP_EVENT_INFERENCE_OUTPUT_READY](#mpp_event_inference_output_ready),

  [MPP_EVENT_QUALITY_CHECK_READY](#mpp_event_quality_check_ready),

  [MPP_EVENT_CONVERT_PARAMS_READY](#mpp_event_convert_params_ready),

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

  [ROTATE_270](#rotate_270),

  [ROTATE_CUSTOM](#rotate_custom) }
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

  [MPP_PIXEL_YUYV1P422](#mpp_pixel_yuyv1p422),

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
- enum [rtp_payload_type_t](#rtp_payload_type_t) {

  [RTP_PAYLOAD_TYPE_JPEG](#rtp_payload_type_jpeg),

  [RTP_PAYLOAD_TYPE_H264](#rtp_payload_type_h264) }
- enum [rtp_frame_type_t](#rtp_frame_type_t) {

  [RTP_JPEG_FRAME_TYPE_YUV422](#rtp_jpeg_frame_type_yuv422),

  [RTP_JPEG_FRAME_TYPE_YUV420](#rtp_jpeg_frame_type_yuv420) }
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

  [MPP_ELEMENT_VIDEO_ENCODE](#mpp_element_video_encode),

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

### 2.1 Detailed Description

This section provides the detailed documentation for the MCU Media Processing Pipeline types.

#### 2.1.1 Data Structure Documentation

##### mpp_stats_t

**union mpp\_stats\_t**

Statistics data container, content depends on the group.

**Data Fields**

|type|name|description|
| - | - | - |
|struct mpp_stats_t::@0|api|Global execution performance counters.|
|struct mpp_stats_t::@1|mpp|Pipeline execution performance counters.|
|struct mpp_stats_t::@2|elem|Element execution performance counters.|

##### mpp_api_params_t

**struct mpp\_api\_params\_t**

API initialization parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_stats_t ](#mpp_stats_t) *|stats|API stats.|
|unsigned int|rc\_cycle\_min|minimum cycle duration for RC tasks (ms), 0: sets default value|
|unsigned int|rc\_cycle\_inc|time increment for RC tasks (ms), 0: sets default value|
|int|pipeline\_task\_max\_prio|pipeline tasks maximum priority.|
|int|pipeline\_rc\_task\_prio|0: sets default value|
|int|pipeline\_pr\_task\_prio|0: sets default value|

##### mpp_params_t

**struct mpp\_params\_t**

Pipeline creation parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|int|(*evt\_callback\_f)(mpp\_t mpp, mpp\_evt\_t evt, void *evt\_data, void *user\_data)|event callback function|
|[mpp_evt_mask_t ](#mpp_evt_mask_t)|mask|event mask|
|[mpp_exec_flag_t ](#mpp_exec_flag_t)|exec\_flag|execution flag|
|void *|cb\_userdata|user data pointer passed to the callback|
|[mpp_stats_t ](#mpp_stats_t) *|stats|pointer to statistics structure|

##### mpp_camera_stream_cfg

**struct mpp\_camera\_stream\_cfg**

Camera stream configuration for multi-stream cameras.

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_camera_stream_type ](#mpp_camera_stream_type)|type|Stream type (member of enum mpp_camera_stream_type)|
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
|[mpp_pixel_format_t ](#mpp_pixel_format_t)|format|pixel format|
|int|fps|frames per second|
|bool|stripe|stripe mode|
|void *|rpmsg\_inst|pointer to rpmsg instance|
|uint16_t *|mcmgr\_event\_data|pointer to mcmgr event data|
|uint32_t|n\_streams|number of total output video streams|
|[mpp_camera_stream_cfg ](#mpp_camera_stream_cfg)|stream[NUM_STREAMS]|streams configuration|
|bool|in\_advance\_enqueue|enable in-advance enqueue mode|

##### mpp_img_params_t

**struct mpp\_img\_params\_t**

Static image parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|int|height|buffer height|
|int|width|buffer width|
|[mpp_pixel_format_t ](#mpp_pixel_format_t)|format|pixel format|
|bool|stripe|stripe mode|
|int|compressed\_size|size in bytes for compressed format|

##### mpp_convert_cb_param_t

**struct mpp\_convert\_cb\_param\_t**

Parameters passed to the callback after a convert element processes a frame.

**Data Fields**

|type|name|description|
| - | - | - |
|uint32_t|id|convert element id|
|uint32_t|left|crop left coordinate|
|uint32_t|right|crop right coordinate|
|uint32_t|top|crop top coordinate|
|uint32_t|bottom|crop bottom coordinate|
|float|rotation|custom rotation angle|
|void *|vg\_lite\_m|vglite matrix pointer|

##### mpp_filesrc_params_t

**struct mpp\_filesrc\_params\_t**

File source parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|const char *|filepath|Path to file on SD card.|
|bool|loop|Flag to enable looping the file.|
|int|file\_buffer\_size|size in bytes for compressed format|
|[slice_search_func_t ](#slice_search_func_t)|slice\_search\_func|Optional: function to search for slices/chunks of data.|

##### mpp_filesink_params_t

**struct mpp\_filesink\_params\_t**

File sink parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|const char *|filepath|Path to file on SD card.|
|bool|append\_mode|Flag to enable append mode (false = overwrite)|
|uint64_t|max\_file\_size|Maximum file size in bytes.|

##### mpp_rtspsink_params_t

**struct mpp\_rtspsink\_params\_t**

RTSP sink parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|uint16_t|port|RTSP server port (default: 8554)|
|uint8_t|ip\_addr[MPP_IP_ADDR_LEN]|IP address as array of bytes.|
|uint32_t|frame\_width|Frame width in pixels.|
|uint32_t|frame\_height|Frame height in pixels.|
|[rtp_payload_type_t ](#rtp_payload_type_t)|payload\_type|RTP payload type.|
|[rtp_frame_type_t ](#rtp_frame_type_t)|frame\_type|RTP JPEG frame type (YUV422 or YUV420)|

##### mpp_mc_params_t

**struct mpp\_mc\_params\_t**

MC element parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|void *|rpmsg\_inst|pointer to rpmsg instance|
|uint16_t|remote\_event\_data|remote event data for mcmgr|
|uint32_t|local\_rpmsg\_addr|local rpmsg endpoint address|
|uint32_t|remote\_rpmsg\_addr|remote rpmsg endpoint address|

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
|[mpp_rotate_degree_t ](#mpp_rotate_degree_t)|rotate|rotate degree|
|[mpp_pixel_format_t ](#mpp_pixel_format_t)|format|pixel format|
|bool|stripe|stripe mode|
|void *|handle|pointer to an lvgl image widget|

##### mpp_tensor_dims_t

**struct mpp\_tensor\_dims\_t**

Inference tensor dimensions.

**Data Fields**

- uint32_t size
- uint32_t data[MAX_TENSOR_DIMS]

##### mpp_inference_tensor_params_t

**struct mpp\_inference\_tensor\_params\_t**

tensor parameters

**Data Fields**

|type|name|description|
| - | - | - |
|const uint8_t *|data|data address|
|[mpp_tensor_dims_t ](#mpp_tensor_dims_t)|dims|tensor data dimensions|
|[mpp_tensor_type_t ](#mpp_tensor_type_t)|type|tensor data type|
|float|scale||
|int32_t|zero\_point|quantization zero point|

##### mpp_inference_inp_cb_params

**struct mpp\_inference\_inp\_cb\_params**

Inference input ready callback parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_inference_tensor_params_t ](#mpp_inference_tensor_params_t) *|in\_tensors[MPP_INFERENCE_MAX_INPUTS]|input tensors parameters|
|uint32_t|model\_id|model identifier|

##### mpp_inference_cb_param_t

**struct mpp\_inference\_cb\_param\_t**

Inference callback parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|void *|user\_data|callback will pass this pointer|
|[mpp_inference_tensor_params_t ](#mpp_inference_tensor_params_t) *|out\_tensors[MPP_INFERENCE_MAX_OUTPUTS]|output tensors parameters|
|uint32_t|inference\_time\_ms|inference run time measurement - output to user|
|[mpp_inference_type_t ](#mpp_inference_type_t)|inference\_type|type of the inference|
|uint32_t|model\_id||

##### img_quality_metrics_t

**struct img\_quality\_metrics\_t**

Image quality metrics.

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
|uint32_t|raw|Raw color.|
|struct mpp_color_t::@3|rgb|RGB color|

##### mpp_labeled_rect_t

**struct mpp\_labeled\_rect\_t**

mpp labeled rectangle element structure

**Data Fields**

|type|name|description|
| - | - | - |
|uint8_t|label[64]|label to print|
|uint16_t|clear|clear rectangle|
|uint16_t|line\_width|rectangle line thickness|
|[mpp_color_t ](#mpp_color_t)|line\_color|rectangle line color|
|int16_t|top|rectangle top position|
|int16_t|left|rectangle left position|
|int16_t|bottom|rectangle bottom position|
|int16_t|right|rectangle right position|
|uint16_t|tag|labeled rectangle tag|
|uint16_t|reserved|pad for 32 bits alignment|
|bool|stripe|stripe mode|

##### mpp_landmark_t

**struct mpp\_landmark\_t**

mpp landmark structure

**Data Fields**

|type|name|description|
| - | - | - |
|uint16_t|clear|clear landmark|
|uint16_t|width|landmark thickness|
|[mpp_color_t ](#mpp_color_t)|color|landmark color|
|int16_t|x|landmark x position|
|int16_t|y|landmark y position|
|uint16_t|tag|landmark tag|
|bool|stripe|stripe mode|

##### mpp_area_t

**struct mpp\_area\_t**

Image area coordinates.

**Data Fields**

- int top
- int left
- int bottom
- int right

##### mpp_dims_t

**struct mpp\_dims\_t**

Image dimensions.

**Data Fields**

- unsigned int width
- unsigned int height

##### mpp_position_t

**struct mpp\_position\_t**

Image position.

**Data Fields**

- int top
- int left

##### mpp_inference_params_t

**struct mpp\_inference\_params\_t**

Model parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|uint64_t|constant\_weight\_MemSize|model constant weights memory size|
|uint64_t|mutable\_weight\_MemSize|Defines the amount of memory required both input & output data buffers.|
|uint64_t|activations\_MemSize|Size of scratch memory used for intermediate computations needed by the model.|
|int|num\_inputs|model's number of inputs|
|int|num\_outputs|model's number of outputs|
|uint64_t|inputs\_offsets[MPP_INFERENCE_MAX_INPUTS]|offset of each input|
|uint64_t|outputs\_offsets[MPP_INFERENCE_MAX_OUTPUTS]|offset of each output|
|[inference_entry_point_t ](#inference_entry_point_t)|model\_entry\_point|function called to perform the inference|
|[mpp_tensor_type_t ](#mpp_tensor_type_t)|model\_input\_tensors\_type|type of input buffer|

##### mpp_img_compose_param_t

**struct mpp\_img\_compose\_param\_t**

Image composition parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|int|height|original image height|
|int|width|original image width|
|[mpp_pixel_format_t ](#mpp_pixel_format_t)|format|pixel format|
|void *|buffer|image buffer address|
|[mpp_area_t ](#mpp_area_t)|dest\_area|area for image in destination|

##### mpp_element_params_t

**struct mpp\_element\_params\_t**

Static image and Processing elements parameters.

**Data Fields**

- union mpp_element_params_t::@4 @5
- [mpp_stats_t ](#mpp_stats_t) * stats

##### mpp_stats_t.api

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

##### mpp_stats_t.mpp

**struct mpp\_stats\_t.mpp**

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_t ](#mpp_t)|mpp||
|unsigned int|mpp\_exec\_time|pipeline execution time (ms)|
|unsigned int|fps|frames processed per second|

##### mpp_stats_t.elem

**struct mpp\_stats\_t.elem**

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_elem_handle_t ](#mpp_elem_handle_t)|hnd||
|unsigned int|elem\_exec\_time|element execution time (ms)|

##### mpp_color_t.rgb

**struct mpp\_color\_t.rgb**

rgb color values

**Data Fields**

|type|name|description|
| - | - | - |
|uint8_t|R|Red byte.|
|uint8_t|G|Green byte.|
|uint8_t|B|Blue byte.|
|uint8_t|pad|padding byte|

##### mpp_element_params_t.__unnamed5__

**union mpp\_element\_params\_t.\_\_unnamed5\_\_**

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_camera_params_t ](#mpp_camera_params_t)|camera|Camera element's parameters.|
|struct mpp_element_params_t::@4::@6|static\_image|Static Image element's parameters.|
|[mpp_mc_params_t ](#mpp_mc_params_t)|mc\_source|Multicore source element's parameters|
|struct mpp_element_params_t::@4::@7|compose|Compose element's parameters.|
|struct mpp_element_params_t::@4::@8|labels|Labeled Rectangle and Landmarks element's parameters.|
|struct mpp_element_params_t::@4::@9|convert|Convert element's parameters.|
|struct mpp_element_params_t::@4::@10|resize|Resize element's parameters.|
|struct mpp_element_params_t::@4::@11|color\_conv|Color convert element's parameters.|
|struct mpp_element_params_t::@4::@12|rotate|Rotate element's parameters.|
|struct mpp_element_params_t::@4::@13|test|Test element's parameters.|
|struct mpp_element_params_t::@4::@14|decode|Decoder element's parameters.|
|struct mpp_element_params_t::@4::@15|encode|Encoder element's parameters.|
|struct mpp_element_params_t::@4::@16|ml\_inference|ML inference element's parameters.|
|struct mpp_element_params_t::@4::@17|img\_quality\_check|Image quality check element's parameters.|

##### mpp_element_params_t.__unnamed5__.static_image

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.static\_image**

Static Image element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_img_params_t ](#mpp_img_params_t)|img\_params|static image parameters|
|void *|img\_buffer|static image buffer address|

##### mpp_element_params_t.__unnamed5__.compose

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.compose**

Compose element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|int|nb\_images|number of images to compose|
|[mpp_img_compose_param_t ](#mpp_img_compose_param_t) *|image\_list|pointer to array of images to compose|
|[mpp_area_t ](#mpp_area_t)|input\_area|area of input stream in destination|
|[mpp_rotate_degree_t ](#mpp_rotate_degree_t)|out\_angle|output rotation angle|
|[mpp_flip_mode_t ](#mpp_flip_mode_t)|out\_flip|output flip mode|
|[mpp_pixel_format_t ](#mpp_pixel_format_t)|out\_format|output color format|
|int|out\_width|output buffer width|
|int|out\_height|output buffer height|

##### mpp_element_params_t.__unnamed5__.labels

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.labels**

Labeled Rectangle and Landmarks element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|uint32_t|max\_rect|maximum number of rectangles|
|uint32_t|detected\_rect|detected rectangles|
|[mpp_labeled_rect_t ](#mpp_labeled_rect_t) *|rectangles|array of rectangle data|
|uint32_t|max\_landmk|maximum number of landmarks|
|uint32_t|detected\_landmk|detected landmarks|
|[mpp_landmark_t ](#mpp_landmark_t) *|landmarks|array of landmark data|

##### mpp_element_params_t.__unnamed5__.convert

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.convert**

Convert element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|[mpp_dims_t ](#mpp_dims_t)|out\_buf|output buffer dimensions|
|[mpp_pixel_format_t ](#mpp_pixel_format_t)|pixel\_format|new pixel format|
|[mpp_rotate_degree_t ](#mpp_rotate_degree_t)|angle|rotation angle|
|float|custom\_angle|custom value of rotation angle|
|[mpp_flip_mode_t ](#mpp_flip_mode_t)|flip|flip mode|
|[mpp_area_t ](#mpp_area_t)|crop|input crop area|
|[mpp_position_t ](#mpp_position_t)|out\_window|output window position|
|[mpp_dims_t ](#mpp_dims_t)|scale|scaling dimensions|
|[mpp_convert_ops_t ](#mpp_convert_ops_t)|ops|operation selector mask|
|const char *|dev\_name|device name used for graphics|
|uint32_t|elem\_id|element id used for callback parameters|
|bool|stripe\_in|input stripe mode|
|bool|stripe\_out|output stripe mode|

##### mpp_element_params_t.__unnamed5__.resize

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.resize**

Resize element's parameters.

**Data Fields**

- unsigned int width
- unsigned int height

##### mpp_element_params_t.__unnamed5__.color_conv

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.color\_conv**

Color convert element's parameters.

**Data Fields**

- [mpp_pixel_format_t ](#mpp_pixel_format_t) pixel\_format

##### mpp_element_params_t.__unnamed5__.rotate

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.rotate**

Rotate element's parameters.

**Data Fields**

- [mpp_rotate_degree_t ](#mpp_rotate_degree_t) angle

##### mpp_element_params_t.__unnamed5__.test

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.test**

Test element's parameters.

**Data Fields**

- _Bool inp
- unsigned int width
- unsigned int height
- [mpp_pixel_format_t ](#mpp_pixel_format_t) format

##### mpp_element_params_t.__unnamed5__.decode

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.decode**

Decoder element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|const char *|dev\_name|device name used for decoder|
|unsigned int|width||
|unsigned int|height||
|[mpp_pixel_format_t ](#mpp_pixel_format_t)|out\_format||

##### mpp_element_params_t.__unnamed5__.encode

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.encode**

Encoder element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|const char *|dev\_name|device name used for encoder|
|unsigned int|width|frame width|
|unsigned int|height|frame height|
|unsigned int|fps|frame rate|
|[mpp_pixel_format_t ](#mpp_pixel_format_t)|format|pixel format|
|bool|intraframe|only intraframe encoding|
|uint32_t|intra\_period|I-frame period (GOP size)|
|int32_t|num\_ref\_frame|Number of reference frames.|
|int32_t|rc\_mode|Rate control mode.|
|int32_t|target\_bitrate|Target bitrate in bps.|
|int32_t|max\_bitrate|Maximum bitrate in bps.|
|bool|enable\_frame\_skip|Enable/disable frame skipping.|
|int32_t|temporal\_layer\_num|Number of temporal layers.|
|int32_t|profile\_idc|Profile (e.g., baseline, main, high)|
|int32_t|level\_idc|Level (e.g., 3.0, 3.1, 4.0)|
|int32_t|spatial\_bitrate|Spatial layer bitrate.|
|int32_t|max\_spatial\_bitrate|Maximum spatial layer bitrate.|
|int32_t|entropy\_coding\_mode|0: CAVLC, 1: CABAC|

##### mpp_element_params_t.__unnamed5__.ml_inference

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.ml\_inference**

ML inference element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|const void *|model\_data|pointer to model binary|
|[mpp_inference_type_t ](#mpp_inference_type_t)|type|inference type|
|int|model\_size|model binary size|
|uint32_t|model\_id||
|float|model\_input\_mean|model 'mean' of input values, used for normalization|
|float|model\_input\_std|model 'standard deviation' of input values, used for normalization|
|[mpp_tensor_order_t ](#mpp_tensor_order_t)|tensor\_order|model input tensor component order|
|[mpp_inference_params_t ](#mpp_inference_params_t)|inference\_params|model specific parameters used by the inference|

##### mpp_element_params_t.__unnamed5__.img_quality_check

**struct mpp\_element\_params\_t.\_\_unnamed5\_\_.img\_quality\_check**

Image quality check element's parameters.

**Data Fields**

|type|name|description|
| - | - | - |
|_Bool|disable|disable quality check|

#### 2.1.2 Macro Definition Documentation

##### MPP_INFERENCE_MAX_OUTPUTS

#define **MPP\_INFERENCE\_MAX\_OUTPUTS**

Maximum number of inference inputs and outputs.

Maximum number of outputs supported by the pipeline

##### MPP_INFERENCE_MAX_INPUTS

#define **MPP\_INFERENCE\_MAX\_INPUTS**

Maximum number of inputs supported by the pipeline.

##### MPP_IP_ADDR_LEN

#define **MPP\_IP\_ADDR\_LEN**

Maximum IP address length (IPv4: 4 octets)

##### MPP_INVALID

#define **MPP\_INVALID**

Invalid pipeline handle.

##### MPP_EVENT_ALL

#define **MPP\_EVENT\_ALL**

Bit mask to receive all Events.

##### MPP_MAX_RPMSG_EPT_PER_CORE

#define **MPP\_MAX\_RPMSG\_EPT\_PER\_CORE**

Maximum number of RPMsg endpoints per core.

##### MAX_TENSOR_DIMS

#define **MAX\_TENSOR\_DIMS**

Maximum number of dimensions for tensors.

#### 2.1.3 Typedef Documentation

##### mpp_t

typedef void * **mpp\_t**

Pipeline handle type.

##### mpp_elem_handle_t

typedef uintptr_t **mpp\_elem\_handle\_t**

Element handle type.

##### mpp_evt_mask_t

typedef unsigned int **mpp\_evt\_mask\_t**

Event mask for pipeline creation.

##### slice_search_func_t

typedef int(* **slice\_search\_func\_t**)(const uint8_t *data, int32_t len)

File slice search function type.

##### inference_entry_point_t

typedef int(* **inference\_entry\_point\_t**)(uint8_t *, uint8_t *, uint8_t *)

Bundle inference function type.

#### 2.1.4 Enumeration Type Documentation

##### mpp_evt_t

**enum mpp\_evt\_t**

{ MPP_EVENT_INVALID,

  MPP_EVENT_INFERENCE_INPUT_READY,

  MPP_EVENT_INFERENCE_OUTPUT_READY,

  MPP_EVENT_QUALITY_CHECK_READY,

  MPP_EVENT_CONVERT_PARAMS_READY,

  MPP_EVENT_INTERNAL_TEST_RESERVED,

  MPP_EVENT_NUM }

Pipeline generated events.

##### mpp_exec_flag_t

**enum mpp\_exec\_flag\_t**

{ MPP_EXEC_INHERIT,

  MPP_EXEC_RC,

  MPP_EXEC_PREEMPT }

Execution parameters.

These parameters control the execution of the elements of an mpp. The "mpps" created using the flag MPP_EXEC_RC are guaranteed to run up to the completion of all processing elements, while not being preempted by other "mpps". The "mpps" created using the flag MPP_EXEC_PREEMPT are preempted after a given time interval by "mpps" that will run-to-completion again. The "mpps" created with the MPP_EXEC_INHERIT flag inherit the same execution flag as the parent(s) in case of split operation. Note: It is not possible to request run-to-completion execution when spliting preemptable-execution "mpps".

##### mpp_stats_grp_t

**enum mpp\_stats\_grp\_t**

{ MPP_STATS_GRP_API,

  MPP_STATS_GRP_MPP,

  MPP_STATS_GRP_ELEMENT,

  MPP_STATS_GRP_NUM }

Statistics group identifier.

##### mpp_rotate_degree_t

**enum mpp\_rotate\_degree\_t**

{ ROTATE_0,

  ROTATE_90,

  ROTATE_180,

  ROTATE_270,

  ROTATE_CUSTOM }

Rotation value.

##### mpp_flip_mode_t

**enum mpp\_flip\_mode\_t**

{ FLIP_NONE,

  FLIP_HORIZONTAL,

  FLIP_VERTICAL,

  FLIP_BOTH }

Flip type.

##### mpp_convert_ops_t

**enum mpp\_convert\_ops\_t**

{ MPP_CONVERT_NONE,

  MPP_CONVERT_ROTATE,

  MPP_CONVERT_SCALE,

  MPP_CONVERT_COLOR,

  MPP_CONVERT_CROP,

  MPP_CONVERT_OUT_WINDOW }

The convert operations selector flags.

##### mpp_pixel_format_t

**enum mpp\_pixel\_format\_t**

{ MPP_PIXEL_ARGB,

  MPP_PIXEL_BGRA,

  MPP_PIXEL_RGBA,

  MPP_PIXEL_BGRX,

  MPP_PIXEL_RGBX,

  MPP_PIXEL_RGB,

  MPP_PIXEL_RGB565,

  MPP_PIXEL_BGR,

  MPP_PIXEL_GRAY888,

  MPP_PIXEL_GRAY888X,

  MPP_PIXEL_GRAY,

  MPP_PIXEL_GRAY16,

  MPP_PIXEL_YUV1P444,

  MPP_PIXEL_YUYV1P422,

  MPP_PIXEL_VYUY1P422,

  MPP_PIXEL_UYVY1P422,

  MPP_PIXEL_YUYV,

  MPP_PIXEL_DEPTH16,

  MPP_PIXEL_DEPTH8,

  MPP_PIXEL_YUV420P,

  MPP_PIXEL_JPEG,

  MPP_PIXEL_INVALID }

Pixel format.

##### mpp_camera_stream_type

**enum mpp\_camera\_stream\_type**

{ RGB_STREAM,

  IR_STREAM,

  NUM_STREAMS }

Camera stream type - for multiple stream camera.

##### mpp_rpmsg_endpoint_addr_e

**enum mpp\_rpmsg\_endpoint\_addr\_e**

{ MPP_RPMSG_EPT_ADDR_INVALID,

  MPP_RPMSG_EPT_ADDR_CORE0_START,

  MPP_RPMSG_EPT_ADDR_CORE0_STOP,

  MPP_RPMSG_EPT_ADDR_CORE1_START,

  MPP_RPMSG_EPT_ADDR_CORE1_STOP }

Multicore pipeline RPMSG endpoint addresses.

##### mpp_mcmgr_event_data_e

**enum mpp\_mcmgr\_event\_data\_e**

{ MPP_MCMGR_EVENT_DATA_INVALID,

  MPP_MCMGR_EVENT_DATA_START,

  MPP_MCMGR_EVENT_DATA_STOP }

Multicore pipeline MCMGR remote event data.

##### rtp_payload_type_t

**enum rtp\_payload\_type\_t**

{ RTP_PAYLOAD_TYPE_JPEG,

  RTP_PAYLOAD_TYPE_H264 }

RTP payload types.

##### rtp_frame_type_t

**enum rtp\_frame\_type\_t**

{ RTP_JPEG_FRAME_TYPE_YUV422,

  RTP_JPEG_FRAME_TYPE_YUV420 }

RTP JPEG frame types.

##### mpp_element_id_t

**enum mpp\_element\_id\_t**

{ MPP_ELEMENT_INVALID,

  MPP_ELEMENT_LABELED_RECTANGLE,

  MPP_ELEMENT_TEST,

  MPP_ELEMENT_INFERENCE,

  MPP_ELEMENT_CONVERT,

  MPP_ELEMENT_IMG_DECODE,

  MPP_ELEMENT_IMG_COMPOSE,

  MPP_ELEMENT_IMG_QUALITY_CHECK,

  MPP_ELEMENT_VIDEO_DECODE,

  MPP_ELEMENT_VIDEO_ENCODE,

  MPP_ELEMENT_NUM }

Processing element ids.

##### mpp_tensor_type_t

**enum mpp\_tensor\_type\_t**

{ MPP_TENSOR_TYPE_FLOAT32,

  MPP_TENSOR_TYPE_UINT8,

  MPP_TENSOR_TYPE_INT8 }

Inference tensor type.

##### mpp_tensor_order_t

**enum mpp\_tensor\_order\_t**

{ MPP_TENSOR_ORDER_UNKNOWN,

  MPP_TENSOR_ORDER_NHWC,

  MPP_TENSOR_ORDER_NCHW }

Inference input tensor order.

##### mpp_inference_type_t

**enum mpp\_inference\_type\_t**

{ MPP_INFERENCE_TYPE_TFLITE,

  MPP_INFERENCE_TYPE_EXECUTORCH }

Inference type.

<a id="return-codes"></a>

## 3. Return Codes

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

##### MPP_SUCCESS

#define **MPP\_SUCCESS**

Success return code.

##### MPP_ERROR

#define **MPP\_ERROR**

A generic error occured.

##### MPP_INVALID_ELEM

#define **MPP\_INVALID\_ELEM**

Invalid element provided.

##### MPP_INVALID_PARAM

#define **MPP\_INVALID\_PARAM**

Invalid parameter provided.

##### MPP_ERR_ALLOC_MUTEX

#define **MPP\_ERR\_ALLOC\_MUTEX**

Error occured while allocating mutex.

##### MPP_INVALID_MUTEX

#define **MPP\_INVALID\_MUTEX**

Invalid mutex provided.

##### MPP_MUTEX_TIMEOUT

#define **MPP\_MUTEX\_TIMEOUT**

Mutex timeout occured.

##### MPP_MUTEX_ERROR

#define **MPP\_MUTEX\_ERROR**

Mutex error occured.

##### MPP_MALLOC_ERROR

#define **MPP\_MALLOC\_ERROR**

Memory allocation error occured.
