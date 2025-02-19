# eIQ MCU Media Processing Pipeline API

<a name="_page0_x0.00_y841.89"></a>MPP VERSION 3.3

1. **MPP API**

**Functions**

- int[ mpp_api_init (](#_page8_x70.87_y643.15)[mpp_api_params_t ](#_page17_x70.87_y451.66)∗params)
- [mpp_t ](#_page25_x70.87_y384.91)[mpp_create (](#_page9_x70.87_y180.36)[mpp_params_t ](#_page17_x70.87_y590.69)∗params, int ∗ret)
- int[ mpp_camera_add (](#_page9_x70.87_y463.35)[mpp_t mpp](#_page25_x70.87_y384.91), const char ∗name, [mpp_camera_params_t ](#_page17_x70.87_y771.02)∗params)
- int[ mpp_static_img_add (](#_page9_x70.87_y769.98)[mpp_t mpp](#_page25_x70.87_y384.91), [mpp_img_params_t ](#_page18_x70.87_y234.02)∗params, void ∗addr)
- int[ mpp_display_add (](#_page10_x70.87_y386.19)[mpp_t mpp](#_page25_x70.87_y384.91), const char ∗name, [mpp_display_params_t ](#_page18_x70.87_y412.84)∗params)
- int[ mpp_nullsink_add (](#_page10_x70.87_y690.73)[mpp_t mpp)](#_page25_x70.87_y384.91)
- int[ mpp_element_add (](#_page11_x70.87_y240.84)[mpp_t mpp](#_page25_x70.87_y384.91), [mpp_element_id_t id, ](#_page28_x70.87_y318.21)[mpp_element_params_t ](#_page21_x70.87_y327.65)∗params, [mpp_elem_handle_t ](#_page25_x70.87_y443.37)∗elem\_h)
- int[ mpp_split (](#_page11_x70.87_y698.27)[mpp_t ](#_page25_x70.87_y384.91)mpp, unsigned int num, [mpp_params_t ](#_page17_x70.87_y590.69)∗params, [mpp_t ](#_page25_x70.87_y384.91)∗out\_list)
- int[ mpp_background (](#_page12_x70.87_y356.49)[mpp_t mpp](#_page25_x70.87_y384.91), [mpp_params_t ](#_page17_x70.87_y590.69)∗params, [mpp_t ](#_page25_x70.87_y384.91)∗out\_mpp)
- int[ mpp_element_update (](#_page12_x70.87_y633.66)[mpp_t mpp](#_page25_x70.87_y384.91), [mpp_elem_handle_t elem_h,](#_page25_x70.87_y443.37) [mpp_element_params_t ](#_page21_x70.87_y327.65)∗params)
- int[ mpp_start (](#_page13_x70.87_y220.09)[mpp_t mpp](#_page25_x70.87_y384.91), int last)
- int[ mpp_stop (](#_page13_x70.87_y526.48)[mpp_t mpp)](#_page25_x70.87_y384.91)
- void [mpp_stats_enable (](#_page13_x70.87_y771.02)[mpp_stats_grp_t grp)](#_page26_x70.87_y460.85)
- void [mpp_stats_disable (](#_page14_x70.87_y308.61)[mpp_stats_grp_t grp)](#_page26_x70.87_y460.85)
- char ∗[mpp_get_version (v](#_page14_x70.87_y517.49)oid)
1. **Detailed<a name="_page8_x70.87_y548.45"></a> Description**

This section provides the detailed documentation for the MCU Media Processing Pipeline API.

2. **Function<a name="_page8_x70.87_y614.76"></a> Documentation**
1. **mpp\_api\_init()**

<a name="_page8_x70.87_y643.15"></a>int mpp\_api\_init (

[mpp_api_params_t ](#_page17_x70.87_y451.66)∗ params ) Pipeline initialization.

This function initializes the library and its data structures.

It must be called before any other function of the API is called.

**Parameters**

|in|params|API global parameters|
| - | - | - |
|out|ret|return code (0 - success, non-zero - error)|

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

**2. mpp\_create()**

<a name="_page9_x70.87_y195.60"></a><a name="_page9_x70.87_y180.36"></a>[mpp_t ](#_page25_x70.87_y384.91) mpp\_create (

[mpp_params_t ](#_page17_x70.87_y590.69)∗ params, int ∗ ret )

Basic pipeline creation.

This function returns a handle to the pipeline. **Parameters**


|in|params|pipeline parameters|
| - | - | - |
|out|ret|return code (0 - success, non-zero - error)|

**Returns**

handle to the pipeline if success, NULL if there is an error.

3. **mpp\_camera\_add()**

<a name="_page9_x70.87_y478.59"></a><a name="_page9_x70.87_y463.35"></a>int mpp\_camera\_add (

[mpp_t ](#_page25_x70.87_y384.91)mpp,

const char ∗ name, [mpp_camera_params_t ](#_page17_x70.87_y771.02)∗ params )

Camera addition.

This function adds a camera to the pipeline.

**Parameters**



|in|mpp|input pipeline|
| - | - | - |
|in|name|camera driver name|
|in|params|parameters to be configured on the camera|

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

4. <a name="_page9_x70.87_y769.98"></a>**mpp\_static\_img\_add()**

<a name="_page10_x70.87_y70.87"></a>int mpp\_static\_img\_add (

[mpp_t ](#_page25_x70.87_y384.91)mpp,

[mpp_img_params_t ](#_page18_x70.87_y234.02)∗ params, void ∗ addr )

Static image addition. **Parameters**



|in|mpp|input pipeline|
| - | - | - |
|in|params|static image parameters|
|in|addr|image buffer|

**Returns**

[Return_codes ](#_page29_x70.87_y444.70)**Precondition**

- Image buffer allocation/free is the responsibility of the user.
5. **mpp\_display\_add()**

<a name="_page10_x70.87_y401.43"></a><a name="_page10_x70.87_y386.19"></a>int mpp\_display\_add (

[mpp_t ](#_page25_x70.87_y384.91)mpp,

const char ∗ name, [mpp_display_params_t ](#_page18_x70.87_y412.84)∗ params )

Display addition.

This function adds a display to the pipeline. **Parameters**



|in|mpp|input pipeline|
| - | - | - |
|in|name|display driver name|
|in|params|parameters that are configured on the display|

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

6. **mpp\_nullsink\_add()<a name="_page10_x70.87_y705.84"></a><a name="_page10_x70.87_y690.73"></a>**int mpp\_nullsink\_add (

[mpp_t ](#_page25_x70.87_y384.91)mpp )

Null sink addition.

This function adds a null-type sink to the pipeline.

After this call pipeline is closed and no further elements can be added. Input frames are discarded.

**Parameters**



|in|mpp|input pipeline|
| - | - | - |

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

7. **mpp\_element\_add()**

<a name="_page11_x70.87_y256.08"></a><a name="_page11_x70.87_y240.84"></a>int mpp\_element\_add (

[mpp_t ](#_page25_x70.87_y384.91)mpp,

[mpp_element_id_t ](#_page28_x70.87_y318.21)id, [mpp_element_params_t ](#_page21_x70.87_y327.65)∗ params, [mpp_elem_handle_t ](#_page25_x70.87_y443.37)∗ elem\_h )

Add processing element (single input, single output) This function adds an element to the pipeline. Available elements are:

- 2D image processing
- ML inference engine
- Labeled rectangle
- Compositor

**Parameters**



|in|mpp|input pipeline|
| - | - | - |
|in|id|element id|
|in|params|element parameters|
|out|elem← \_h|element handle in pipeline|

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

8. **mpp\_split()**

<a name="_page11_x70.87_y712.99"></a><a name="_page11_x70.87_y698.27"></a>int mpp\_split (

[mpp_t ](#_page25_x70.87_y384.91)mpp,

unsigned int num, [mpp_params_t ](#_page17_x70.87_y590.69)∗ params, [mpp_t ](#_page25_x70.87_y384.91)∗ out\_list )

Pipeline multiplication. **Parameters**



|in|mpp|input pipeline|
| - | - | - |
|in|num|number of output pipeline|
|in|params|split mpp parameters|
|out|out\_list|list of output pipelines|

**Returns**

[Return_codes ](#_page29_x70.87_y444.70)**Precondition**

- out\_list array must contain at least num elements.
9. **mpp\_background()**

<a name="_page12_x70.87_y371.74"></a><a name="_page12_x70.87_y356.49"></a>int mpp\_background (

[mpp_t ](#_page25_x70.87_y384.91)mpp,

[mpp_params_t ](#_page17_x70.87_y590.69)∗ params, [mpp_t ](#_page25_x70.87_y384.91)∗ out\_mpp )

Put next elements processing in background. **Parameters**



|in|mpp|input pipeline|
| - | - | - |
|in|params|new mpp parameters (exec\_flag must be MPP\_EXEC\_PREEMPT)|
|out|out\_mpp|output pipeline|

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

10. **mpp\_element\_update()**

<a name="_page12_x70.87_y648.66"></a><a name="_page12_x70.87_y633.66"></a>int mpp\_element\_update (

[mpp_t ](#_page25_x70.87_y384.91)mpp,

[mpp_elem_handle_t ](#_page25_x70.87_y443.37)elem\_h, [mpp_element_params_t ](#_page21_x70.87_y327.65)∗ params )

Update element parameters.

**MCU Media Processing Pipeline**
2. **MPP Types 29****

**Parameters**



|in|mpp|input pipeline|
| - | - | - |
|in|elem\_h|element handle in the pipeline.|
|in|params|new element parameters|

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

11. **mpp\_start()<a name="_page13_x70.87_y235.33"></a><a name="_page13_x70.87_y220.09"></a>**int mpp\_start (

[mpp_t ](#_page25_x70.87_y384.91)mpp, int last )

Start pipeline.

When called with last=0, this function prepares the branch of the pipeline specified with mpp. When called with last!=0, this function starts the data flow of the pipeline.

Data flow should start after all the branches of the pipeline have been prepared.

**Parameters**



|in|mpp|pipeline branch handle to start/prepare|
| - | - | - |
|in|last|if non-zero start pipeline processing. No further start call is possible thereafter.|

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

12. **mpp\_stop()<a name="_page13_x70.87_y541.72"></a><a name="_page13_x70.87_y526.48"></a>**int mpp\_stop (

[mpp_t ](#_page25_x70.87_y384.91)mpp )

Stop a branch of the pipeline.

This function stops the data processing and peripherals of a pipeline branch. **Parameters**



|in|mpp|pipeline branch to stop|
| - | - | - |

**Returns**

[Return_codes](#_page29_x70.87_y444.70)

13. <a name="_page13_x70.87_y771.02"></a>**mpp\_stats\_enable()<a name="_page14_x70.87_y70.87"></a>**void mpp\_stats\_enable (

    [mpp_stats_grp_t ](#_page26_x70.87_y460.85)grp ) Enable statistics collection.

    This function enables statistics collection for a given group. Statistics collection is disabled by default after API initialization. Calling this function when stats are enabled has no effect.

    **Parameters**

 

|in|grp|statistics group|
| - | - | - |

**Returns**

14. **mpp\_stats\_disable()<a name="_page14_x70.87_y323.85"></a><a name="_page14_x70.87_y308.61"></a>**void mpp\_stats\_disable (

    [mpp_stats_grp_t ](#_page26_x70.87_y460.85)grp ) Disable statistics collection.

    This function disables statistics collection for a given group. Calling this function when stats are disabled has no effect. This function is used to ensure stats are not updated while application tasks use the stats structures.

    **Parameters**

 

|[in}|grp statistics group|
| - | - |

15. **mpp\_get\_version()<a name="_page14_x70.87_y532.57"></a><a name="_page14_x70.87_y517.49"></a>**char ∗ mpp\_get\_version (

void )

Get MPP version. **Returns**

pointer to the MPP version string

2. **MPP<a name="_page14_x70.87_y686.10"></a> Types**

**Data Structures**

- union [mpp_stats_t](#_page17_x70.87_y326.50)
- struct [mpp_api_params_t](#_page17_x70.87_y451.66)
- struct [mpp_params_t](#_page17_x70.87_y590.69)
- struct [mpp_camera_params_t](#_page17_x70.87_y771.02)
- struct [mpp_img_params_t](#_page18_x70.87_y234.02)
- struct [mpp_display_params_t](#_page18_x70.87_y412.84)
- struct [mpp_tensor_dims_t](#_page18_x70.87_y680.52)
- struct [mpp_inference_tensor_params_t](#_page19_x70.87_y134.80)
- struct [mpp_inference_cb_param_t](#_page19_x70.87_y298.76)
- union [mpp_color_t](#_page19_x70.87_y500.52)
- struct [mpp_labeled_rect_t](#_page19_x70.87_y660.49)
- struct [mpp_area_t](#_page20_x70.87_y251.67)
- struct [mpp_dims_t](#_page20_x70.87_y428.93)
- struct [mpp_position_t](#_page20_x70.87_y579.68)
- struct [mpp_inference_params_t](#_page20_x70.87_y729.29)
- struct [mpp_element_params_t](#_page21_x70.87_y327.65)
- struct [mpp_stats_t.api](#_page21_x70.87_y477.88)
- struct [mpp_stats_t.mpp](#_page21_x70.87_y648.05)
- struct [mpp_stats_t.elem](#_page22_x70.87_y70.87)
- struct [mpp_color_t.rgb](#_page22_x70.87_y166.66)
- union [mpp_element_params_t.__unnamed5__](#_page22_x70.87_y330.95)
- struct [mpp_element_params_t.__unnamed5__.compose](#_page22_x70.87_y639.33)
- struct [mpp_element_params_t.__unnamed5__.labels](#_page22_x70.87_y771.02)
- struct [mpp_element_params_t.__unnamed5__.convert](#_page23_x70.87_y206.38)
- struct [mpp_element_params_t.__unnamed5__.resize](#_page23_x70.87_y487.05)
- struct [mpp_element_params_t.__unnamed5__.color_conv](#_page23_x70.87_y637.69)
- struct [mpp_element_params_t.__unnamed5__.rotate](#_page23_x70.87_y771.02)
- struct [mpp_element_params_t.__unnamed5__.test](#_page24_x70.87_y176.96)
- struct [mpp_element_params_t.__unnamed5__.ml_inference](#_page24_x70.87_y356.50)

**Macros**

- #define [MPP_INFERENCE_MAX_OUTPUTS](#_page24_x70.87_y638.86)
- #define [MPP_INFERENCE_MAX_INPUTS](#_page24_x289.64_y733.09)
- #define [MPP_INVALID](#_page25_x70.87_y124.26)
- #define [MPP_EVENT_ALL](#_page25_x70.87_y197.47)
- #define [MAX_TENSOR_DIMS](#_page25_x70.87_y268.89)

**Typedefs**

- typedef void ∗[mpp_t](#_page25_x70.87_y384.91)
- typedef uintptr\_t [mpp_elem_handle_t](#_page25_x70.87_y443.37)
- typedef unsigned int [mpp_evt_mask_t](#_page25_x70.87_y516.58)
- typedef int(∗[inference_entry_point_t) ](#_page25_x70.87_y589.78)(uint8\_t ∗, uint8\_t ∗, uint8\_t ∗)

**Enumerations**

- enum [mpp_evt_t {](#_page25_x70.87_y704.02)

  [MPP_EVENT_INVALID ](#_page26_x171.80_y85.99), [MPP_EVENT_INFERENCE_OUTPUT_READY , ](#_page26_x77.24_y99.73)[MPP_EVENT_INTERNAL_TEST_RESERVED , ](#_page26_x79.86_y114.28)[MPP_EVENT_NUM }](#_page26_x185.55_y128.02)

- enum [mpp_exec_flag_t {](#_page26_x70.87_y162.22)

  [MPP_EXEC_INHERIT ,](#_page26_x84.72_y396.74)

  [MPP_EXEC_RC ,](#_page26_x107.64_y411.29)

  [MPP_EXEC_PREEMPT }](#_page26_x77.24_y425.84)

- enum [mpp_stats_grp_t {](#_page26_x70.87_y460.85)

  [MPP_STATS_GRP_API , ](#_page26_x105.14_y561.28)[MPP_STATS_GRP_MPP , ](#_page26_x100.16_y575.93)[MPP_STATS_GRP_ELEMENT , ](#_page26_x77.24_y590.48)[MPP_STATS_GRP_NUM }](#_page26_x99.17_y604.22)

- enum [mpp_rotate_degree_t {](#_page26_x70.87_y639.34)

  [ROTATE_0 ,](#_page27_x87.21_y86.20)

  [ROTATE_90 ,](#_page27_x82.23_y100.86)

  [ROTATE_180 ,](#_page27_x77.24_y115.51)

  [ROTATE_270 }](#_page27_x77.24_y130.17)

- enum [mpp_flip_mode_t {](#_page27_x70.87_y165.29)

  [FLIP_NONE ,](#_page27_x108.05_y291.20)

  [FLIP_HORIZONTAL ](#_page27_x77.24_y305.75),

  [FLIP_VERTICAL ](#_page27_x90.38_y320.30),

  [FLIP_BOTH ](#_page27_x109.40_y334.85)}

- enum [mpp_convert_ops_t { ](#_page27_x70.87_y369.85)[MPP_CONVERT_NONE , ](#_page27_x114.33_y495.87)[MPP_CONVERT_ROTATE , ](#_page27_x106.55_y509.61)[MPP_CONVERT_SCALE , ](#_page27_x110.83_y524.16)[MPP_CONVERT_COLOR , ](#_page27_x108.35_y538.82)[MPP_CONVERT_CROP , ](#_page27_x114.51_y552.56)[MPP_CONVERT_OUT_WINDOW }](#_page27_x77.24_y567.10)
- enum [mpp_pixel_format_t { ](#_page27_x70.87_y602.11)[MPP_PIXEL_ARGB ,](#_page27_x102.17_y726.24)

  [MPP_PIXEL_BGRA ,](#_page27_x102.17_y739.98)

  [MPP_PIXEL_RGBA ,](#_page27_x102.17_y753.72)

  [MPP_PIXEL_RGB ,](#_page28_x108.15_y86.20)

  [MPP_PIXEL_RGB565 ,](#_page28_x93.19_y99.94)

  [MPP_PIXEL_BGR ,](#_page28_x108.15_y113.67)

  [MPP_PIXEL_GRAY888 , ](#_page28_x88.11_y127.41)[MPP_PIXEL_GRAY888X , ](#_page28_x82.13_y142.07)[MPP_PIXEL_GRAY ,](#_page28_x103.07_y156.73)

  [MPP_PIXEL_GRAY16 , ](#_page28_x93.09_y171.39)[MPP_PIXEL_YUV1P444 , ](#_page28_x83.22_y185.84)[MPP_PIXEL_VYUY1P422 , ](#_page28_x77.24_y199.37)[MPP_PIXEL_UYVY1P422 , ](#_page28_x77.24_y212.90)[MPP_PIXEL_YUYV ](#_page28_x103.16_y226.43), [MPP_PIXEL_DEPTH16 , ](#_page28_x87.22_y239.97)[MPP_PIXEL_DEPTH8 , ](#_page28_x92.21_y254.31)[MPP_PIXEL_YUV420P , ](#_page28_x88.21_y268.65)[MPP_PIXEL_INVALID ](#_page28_x93.42_y283.20)}

- enum [mpp_element_id_t { ](#_page28_x70.87_y318.21)[MPP_ELEMENT_INVALID ](#_page28_x142.16_y444.02), [MPP_ELEMENT_COMPOSE , ](#_page28_x130.48_y457.76)[MPP_ELEMENT_LABELED_RECTANGLE , ](#_page28_x77.24_y472.42)[MPP_ELEMENT_TEST , ](#_page28_x153.40_y487.07)[MPP_ELEMENT_INFERENCE ,](#_page28_x124.51_y501.62)

  [MPP_ELEMENT_CONVERT , ](#_page28_x132.75_y516.28)[MPP_ELEMENT_NUM }](#_page28_x155.90_y530.94)

- enum [mpp_tensor_type_t { ](#_page28_x70.87_y565.14)[MPP_TENSOR_TYPE_FLOAT32 , ](#_page28_x77.24_y691.05)[MPP_TENSOR_TYPE_UINT8 , ](#_page28_x88.95_y705.70)[MPP_TENSOR_TYPE_INT8 }](#_page28_x95.43_y720.36)
- enum [mpp_tensor_order_t { ](#_page28_x70.87_y755.48)[MPP_TENSOR_ORDER_UNKNOWN , ](#_page29_x77.24_y166.68)[MPP_TENSOR_ORDER_NHWC , ](#_page29_x96.40_y180.42)[MPP_TENSOR_ORDER_NCHW }](#_page29_x96.40_y195.07)
- enum [mpp_inference_type_t { ](#_page29_x70.87_y230.19)[MPP_INFERENCE_TYPE_TFLITE }](#_page29_x77.24_y356.10)
1. **Detailed<a name="_page17_x70.87_y220.50"></a> Description**

This section provides the detailed documentation for the MCU Media Processing Pipeline types.

2. **Data<a name="_page17_x70.87_y294.51"></a> Structure Documentation**
1. **union<a name="_page17_x70.87_y326.50"></a> mpp\_stats\_t Data Fields**

 

|struct [mpp_stats_t.api](#_page21_x70.87_y477.88)|api|Global execution performance counters.|
| - | - | - |
|struct [mpp_stats_t.mpp](#_page21_x70.87_y648.05)|mpp|Pipeline execution performance counters.|
|struct [mpp_stats_t.elem](#_page22_x70.87_y70.87)|elem|Element execution performance counters.|

2. **struct<a name="_page17_x70.87_y451.66"></a> mpp\_api\_params\_t Data Fields**

 

|[mpp_stats_t ](#_page17_x70.87_y326.50)∗|stats|API stats.|
| - | - | - |
|unsigned int|rc\_cycle\_min|minimum cycle duration for RC tasks (ms), 0: sets default value|
|unsigned int|rc\_cycle\_inc|time increment for RC tasks (ms), 0: sets default value|
|int|pipeline\_task\_max\_prio|pipeline tasks maximum priority.|

3. **struct<a name="_page17_x70.87_y590.69"></a> mpp\_params\_t** Pipeline creation parameters.

   **Data Fields**

- int(∗**evt\_callback\_f** )([mpp_t ](#_page25_x70.87_y384.91)mpp, [mpp_evt_t evt,](#_page25_x70.87_y704.02) void ∗evt\_data, void ∗user\_data)
- [mpp_evt_mask_t ](#_page25_x70.87_y516.58)**mask**
- [mpp_exec_flag_t ](#_page26_x70.87_y162.22)**exec\_flag**
- void ∗**cb\_userdata**
- [mpp_stats_t ](#_page17_x70.87_y326.50)<a name="_page17_x70.87_y771.02"></a>∗**stats**
4. **struct<a name="_page18_x70.87_y70.87"></a> mpp\_camera\_params\_t**

Camera parameters. **Data Fields**



|int|height|buffer height|
| - | - | - |
|int|width|buffer width|
|[mpp_pixel_format_t](#_page27_x70.87_y602.11)|format|pixel format|
|int|fps|frames per second|
|bool|stripe|stripe mode|

5. **struct<a name="_page18_x70.87_y249.26"></a><a name="_page18_x70.87_y234.02"></a> mpp\_img\_params\_t**

Static image parameters. **Data Fields**



|int|height|buffer height|
| - | - | - |
|int|width|buffer width|
|[mpp_pixel_format_t](#_page27_x70.87_y602.11)|format|pixel format|
|bool|stripe|stripe mode|

6. **struct<a name="_page18_x70.87_y428.08"></a><a name="_page18_x70.87_y412.84"></a> mpp\_display\_params\_t**

Display parameters. **Data Fields**



|int|height|buffer resolution: setting to 0 will default to panel physical resolution|
| - | - | - |
|int|width|buffer resolution: setting to 0 will default to panel physical resolution|
|int|pitch|buffer resolution: setting to 0 will default to panel physical resolution|
|int|left|active rect: setting to 0 will default to fullscreen|
|int|top|active rect: setting to 0 will default to fullscreen|
|int|right|active rect: setting to 0 will default to fullscreen|
|int|bottom|active rect: setting to 0 will default to fullscreen|
|[mpp_rotate_degree_t](#_page26_x70.87_y639.34)|rotate|rotate degree|
|[mpp_pixel_format_t](#_page27_x70.87_y602.11)|format|pixel format|
|bool|stripe|stripe mode|

7. **struct<a name="_page18_x70.87_y695.77"></a><a name="_page18_x70.87_y680.52"></a> mpp\_tensor\_dims\_t** Inference tensor dimensions.

   **Data Fields**

 

|uint32\_t|size||
| - | - | :- |
|uint32\_t|data[[MAX_TENSOR_DIMS\]](#_page25_x70.87_y268.89)||
8. **struct<a name="_page19_x70.87_y150.04"></a><a name="_page19_x70.87_y134.80"></a> mpp\_inference\_tensor\_params\_t**

tensor parameters **Data Fields**



|const uint8\_t ∗|data|data address|
| - | - | - |
|[mpp_tensor_dims_t](#_page18_x70.87_y680.52)|dims|tensor data dimensions|
|[mpp_tensor_type_t](#_page28_x70.87_y565.14)|type|tensor data type|

9. **struct<a name="_page19_x70.87_y314.00"></a><a name="_page19_x70.87_y298.76"></a> mpp\_inference\_cb\_param\_t**

Inference callback parameters. **Data Fields**



|void ∗|user\_data|callback will pass this pointer|
| - | - | - |
|<p>[mpp_inference_tensor_params_t](#_page19_x70.87_y134.80)</p><p>∗</p>|out\_tensors[[MPP_INFERENCE_MAX](#_page24_x70.87_y638.86)|[_OUTPUTSoutput tensors\] ](#_page24_x70.87_y638.86)parameters|
|int|inference\_time\_ms|inference run time measurement - output to user|
|[mpp_inference_type_t](#_page29_x70.87_y230.19)|inference\_type|type of the inference|

10. **union<a name="_page19_x70.87_y515.76"></a><a name="_page19_x70.87_y500.52"></a> mpp\_color\_t**

mpp color encoding **Data Fields**



|uint32\_t|raw|Raw color.|
| - | - | - |
|struct [mpp_color_t.rgb](#_page22_x70.87_y166.66)|rgb|rgb color values RGB color|

11. **struct<a name="_page19_x70.87_y675.73"></a><a name="_page19_x70.87_y660.49"></a> mpp\_labeled\_rect\_t**

mpp labeled rectangle element structure **Data Fields**



|uint8\_t|label[64]|label to print|
| - | - | - |

**Data Fields**



|uint16\_t|clear|clear rectangle|
| - | - | - |
|uint16\_t|line\_width|rectangle line thickness|
|[mpp_color_t](#_page19_x70.87_y500.52)|line\_color|rectangle line color|
|uint16\_t|top|rectangle top position|
|uint16\_t|left|rectangle left position|
|uint16\_t|bottom|rectangle bottom position|
|uint16\_t|right|rectangle right position|
|uint16\_t|tag|labeled rectangle tag|
|uint16\_t|reserved|pad for 32 bits alignment|
|bool|stripe|stripe mode|

12. **struct<a name="_page20_x70.87_y266.91"></a><a name="_page20_x70.87_y251.67"></a> mpp\_area\_t**

Image area coordinates. **Data Fields**



|int|top||
| - | - | :- |
|int|left||
|int|bottom||
|int|right||
13. **struct<a name="_page20_x70.87_y444.17"></a><a name="_page20_x70.87_y428.93"></a> mpp\_dims\_t**

Image dimensions. **Data Fields**



|unsigned int|width||
| - | - | :- |
|unsigned int|height||
14. **struct<a name="_page20_x70.87_y594.92"></a><a name="_page20_x70.87_y579.68"></a> mpp\_position\_t**

Image position. **Data Fields**



|int|<a name="_page20_x70.87_y729.29"></a>top||
| - | - | :- |
|int|left||
15. **struct<a name="_page21_x70.87_y70.87"></a> mpp\_inference\_params\_t**

Model parameters. **Data Fields**



|uint64\_t|constant\_weight\_MemSize|model constant weights memory size|
| - | - | - |
|uint64\_t|mutable\_weight\_MemSize|Defines the amount of memory required both input & output data buffers.|
|uint64\_t|activations\_MemSize|Size of scratch memory used for intermediate computations needed by the model.|
|int|num\_inputs|model's number of inputs|
|int|num\_outputs|model's number of outputs|
|uint64\_t|inputs\_offsets[[MPP_INFERENCE_MAX_IN](#_page24_x289.64_y733.09)|[PUTSoffset\]of ](#_page24_x289.64_y733.09)each input|
|uint64\_t|outputs\_offsets[[MPP_INFERENCE_MAX_O](#_page24_x70.87_y638.86)|[UTPUTSoffset ofeach\] ](#_page24_x70.87_y638.86)output|
|[inference_entry_point_t](#_page25_x70.87_y589.78)|model\_entry\_point|function called to perform the inference|
|[mpp_tensor_type_t](#_page28_x70.87_y565.14)|model\_input\_tensors\_type|type of input buffer|

16. **struct<a name="_page21_x70.87_y342.89"></a><a name="_page21_x70.87_y327.65"></a> mpp\_element\_params\_t**

Processing element parameters. **Data Fields**



|union [mpp_element_params_t.__unnamed5__](#_page22_x70.87_y330.95)|\_\_unnamed\_\_||
| - | - | :- |
|[mpp_stats_t ](#_page17_x70.87_y326.50)∗|stats||
17. **struct<a name="_page21_x70.87_y493.12"></a><a name="_page21_x70.87_y477.88"></a> mpp\_stats\_t.api Data Fields**

 

|unsigned int|rc\_cycle|run-to-completion (RC) cycle duration (ms)|
| - | - | - |
|unsigned int|rc\_cycle\_max|run-to-completion work deadline (ms)|
|unsigned int|pr\_slot|available slot for preemptable (PR) work (ms)|
|unsigned int|pr\_rounds|number of RC cycles required to complete one PR cycle (ms)|
|unsigned int|app\_slot|remaining time for application (ms)|

18. **struct<a name="_page21_x70.87_y648.05"></a> mpp\_stats\_t.mpp Data Fields**

 

|[mpp_t](#_page25_x70.87_y384.91)|mpp||
| - | - | :- |
|unsigned int|mpp\_exec\_time|pipeline execution time (ms)|

19. **struct<a name="_page22_x70.87_y70.87"></a> mpp\_stats\_t.elem Data Fields**

 

|[mpp_elem_handle_t](#_page25_x70.87_y443.37)|hnd||
| - | - | :- |
|unsigned int|elem\_exec\_time|element execution time (ms)|

20. **struct<a name="_page22_x70.87_y166.66"></a> mpp\_color\_t.rgb**

rgb color values **Data Fields**



|uint8\_t|R|Red byte.|
| - | - | - |
|uint8\_t|G|Green byte.|
|uint8\_t|B|Blue byte.|
|uint8\_t|pad|padding byte|

21. **union<a name="_page22_x70.87_y346.19"></a><a name="_page22_x70.87_y330.95"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_ Data Fields**

 

|struct [mpp_element_params_t.__unnamed5__.compose](#_page22_x70.87_y639.33)|compose|Compose element's parameters - NOT IMPLEMENTED YET.|
| :- | - | :- |
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.labels](#_page22_x70.87_y771.02)</p>|labels|Labeled Rectangle element's parameters.|
|struct [mpp_element_params_t.__unnamed5__.convert](#_page23_x70.87_y206.38)|convert|Convert element's parameters.|
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.resize](#_page23_x70.87_y487.05)</p>|resize|Resize element's parameters.|
|struct [mpp_element_params_t.__unnamed5__.color_conv](#_page23_x70.87_y637.69)|color\_conv|Color convert element's parameters.|
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.rotate](#_page23_x70.87_y771.02)</p>|rotate|Rotate element's parameters.|
|<p>struct</p><p>[mpp_element_params_t.__unnamed5__.test](#_page24_x70.87_y176.96)</p>|test|Test element's parameters.|
|struct [mpp_element_params_t.__unnamed5__.ml_inferen](#_page24_x70.87_y356.50)|<p>ml\_inference</p><p>[ce](#_page24_x70.87_y356.50)</p>|ML inference element's parameters.|

22. **struct<a name="_page22_x70.87_y639.33"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_.compose**

Compose element's parameters - NOT IMPLEMENTED YET. **Data Fields**



|float|<a name="_page22_x70.87_y771.02"></a>a||
| - | - | :- |
|float|b||
23. **struct<a name="_page23_x70.87_y70.87"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_.labels**

Labeled Rectangle element's parameters. **Data Fields**



|uint32\_t|max\_count|maximum number of rectangles|
| - | - | - |
|uint32\_t|detected\_count|detected rectangles|
|[mpp_labeled_rect_t ](#_page19_x70.87_y660.49)∗|rectangles|array of rectangle data|

24. **struct<a name="_page23_x70.87_y221.62"></a><a name="_page23_x70.87_y206.38"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_.convert**

Convert element's parameters. **Data Fields**



|[mpp_dims_t](#_page20_x70.87_y428.93)|out\_buf|output buffer dimensions|
| - | - | - |
|[mpp_pixel_format_t](#_page27_x70.87_y602.11)|pixel\_format|new pixel format|
|[mpp_rotate_degree_t](#_page26_x70.87_y639.34)|angle|rotation angle|
|[mpp_flip_mode_t](#_page27_x70.87_y165.29)|flip|flip mode|
|[mpp_area_t](#_page20_x70.87_y251.67)|crop|input crop area|
|[mpp_position_t](#_page20_x70.87_y579.68)|out\_window|output window position|
|[mpp_dims_t](#_page20_x70.87_y428.93)|scale|scaling dimensions|
|[mpp_convert_ops_t](#_page27_x70.87_y369.85)|ops|operation selector mask|
|const char ∗|dev\_name|device name used for graphics|
|bool|stripe\_in|input stripe mode|
|bool|stripe\_out|output stripe mode|

25. **struct<a name="_page23_x70.87_y502.29"></a><a name="_page23_x70.87_y487.05"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_.resize**

Resize element's parameters. **Data Fields**



|unsigned int|width||
| - | - | :- |
|unsigned int|height||
26. **struct<a name="_page23_x70.87_y652.93"></a><a name="_page23_x70.87_y637.69"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_.color\_conv**

Color convert element's parameters. **Data Fields**



|[mpp_pixel_format_t](#_page27_x70.87_y602.11)|<a name="_page23_x70.87_y771.02"></a>pixel\_format||
| - | - | :- |
27. **struct<a name="_page24_x70.87_y70.87"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_.rotate**

Rotate element's parameters. **Data Fields**



|[mpp_rotate_degree_t](#_page26_x70.87_y639.34)|angle||
| - | - | :- |
28. **struct<a name="_page24_x70.87_y192.20"></a><a name="_page24_x70.87_y176.96"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_.test**

Test element's parameters. **Data Fields**



|\_Bool|inp||
| - | - | :- |
|unsigned int|width||
|unsigned int|height||
|[mpp_pixel_format_t](#_page27_x70.87_y602.11)|format||
29. **struct<a name="_page24_x70.87_y371.74"></a><a name="_page24_x70.87_y356.50"></a> mpp\_element\_params\_t.\_\_unnamed5\_\_.ml\_inference**

ML inference element's parameters. **Data Fields**



|const void ∗|model\_data|pointer to model binary|
| - | - | - |
|[mpp_inference_type_t](#_page29_x70.87_y230.19)|type|inference type|
|int|model\_size|model binary size|
|float|model\_input\_mean|model 'mean' of input values, used for normalization|
|float|model\_input\_std|model 'standard deviation' of input values, used for normalization|
|[mpp_tensor_order_t](#_page28_x70.87_y755.48)|tensor\_order|model input tensor component order|
|[mpp_inference_params_t](#_page20_x70.87_y729.29)|inference\_params|model specific parameters used by the inference|

3. **Macro<a name="_page24_x70.87_y604.88"></a> Definition Documentation**
1. **MPP\_INFERENCE\_MAX\_OUTPUTS**

<a name="_page24_x70.87_y638.86"></a>#define MPP\_INFERENCE\_MAX\_OUTPUTS

Maximum number of inference inputs and outputs. Maximum number of outputs supported by the pipeline

2. <a name="_page24_x289.64_y733.09"></a>**MPP\_INFERENCE\_MAX\_INPUTS**

<a name="_page25_x70.87_y70.87"></a>#define MPP\_INFERENCE\_MAX\_INPUTS

Maximum number of inputs supported by the pipeline.

3. **MPP\_INVALID**

<a name="_page25_x70.87_y138.67"></a><a name="_page25_x70.87_y124.26"></a>#define MPP\_INVALID Invalid pipeline handle.

4. **MPP\_EVENT\_ALL**

<a name="_page25_x70.87_y211.88"></a><a name="_page25_x70.87_y197.47"></a>#define MPP\_EVENT\_ALL

<a name="_page25_x70.87_y268.89"></a>Bit mask to receive all Events.

5. **MAX\_TENSOR\_DIMS**

<a name="_page25_x70.87_y283.30"></a>#define MAX\_TENSOR\_DIMS

Maximum number of dimensions for tensors.

4. **Typedef<a name="_page25_x70.87_y356.51"></a> Documentation**
1. **mpp\_t**

<a name="_page25_x70.87_y384.91"></a>typedef void∗ [mpp_t ](#_page25_x70.87_y384.91)Pipeline handle type.

2. **mpp\_elem\_handle\_t**

<a name="_page25_x70.87_y457.78"></a><a name="_page25_x70.87_y443.37"></a>typedef uintptr\_t [mpp_elem_handle_t ](#_page25_x70.87_y443.37)Element handle type.

3. **mpp\_evt\_mask\_t**

<a name="_page25_x70.87_y530.99"></a><a name="_page25_x70.87_y516.58"></a>typedef unsigned int [mpp_evt_mask_t ](#_page25_x70.87_y516.58)Event mask for pipeline creation.

4. **inference\_entry\_point\_t**

<a name="_page25_x70.87_y604.20"></a><a name="_page25_x70.87_y589.78"></a>typedef int(∗ inference\_entry\_point\_t) (uint8\_t ∗, uint8\_t ∗, uint8\_t ∗) Bundle inference function type.

5. **Enumeration<a name="_page25_x70.87_y677.40"></a> Type Documentation**
1. **mpp\_evt\_t**

<a name="_page25_x70.87_y704.02"></a>enum [mpp_evt_t](#_page25_x70.87_y704.02)

Pipeline generated events.

**Enumerator**



|<a name="_page26_x171.80_y85.99"></a>MPP\_EVENT\_INVALID|invalid event|
| - | - |
|<a name="_page26_x77.24_y99.73"></a>MPP\_EVENT\_INFERENCE\_OUTPUT\_READY|inference out is ready|
|<a name="_page26_x79.86_y114.28"></a>MPP\_EVENT\_INTERNAL\_TEST\_RESERVED|INTERNAL: DO NOT USE.|
|<a name="_page26_x185.55_y128.02"></a>MPP\_EVENT\_NUM|DO NOT USE.|

2. **mpp\_exec\_flag\_t**

<a name="_page26_x70.87_y177.46"></a><a name="_page26_x70.87_y162.22"></a>enum [mpp_exec_flag_t ](#_page26_x70.87_y162.22)Execution parameters.

These parameters control the execution of the elements of an mpp.

The "mpps" created using the flag MPP\_EXEC\_RC are guaranteed to run up to the completion of all processing elements, while not being preempted by other "mpps".

The "mpps" created using the flag MPP\_EXEC\_PREEMPT are preempted after a given time interval by "mpps" that will run-to-completion again.

The "mpps" created with the MPP\_EXEC\_INHERIT flag inherit the same execution flag as the parent(s) in case of split operation.

Note: It is not possible to request run-to-completion execution when spliting preemptable-execution "mpps".

**Enumerator**



|<a name="_page26_x84.72_y396.74"></a>MPP\_EXEC\_INHERIT|inherit from parent(s)|
| - | - |
|<a name="_page26_x107.64_y411.29"></a>MPP\_EXEC\_RC|run-to-completion|
|<a name="_page26_x77.24_y425.84"></a>MPP\_EXEC\_PREEMPT|preemptable|

3. **mpp\_stats\_grp\_t**

<a name="_page26_x70.87_y476.09"></a><a name="_page26_x70.87_y460.85"></a>enum [mpp_stats_grp_t ](#_page26_x70.87_y460.85)**Enumerator**



|<a name="_page26_x105.14_y561.28"></a>MPP\_STATS\_GRP\_API|API (global) stats.|
| - | - |
|<a name="_page26_x100.16_y575.93"></a>MPP\_STATS\_GRP\_MPP|mpp\_t stats|
|<a name="_page26_x77.24_y590.48"></a>MPP\_STATS\_GRP\_ELEMENT|element stats|
|<a name="_page26_x99.17_y604.22"></a>MPP\_STATS\_GRP\_NUM|number of groups|

4. **mpp\_rotate\_degree\_t**

<a name="_page26_x70.87_y654.58"></a><a name="_page26_x70.87_y639.34"></a>enum [mpp_rotate_degree_t ](#_page26_x70.87_y639.34)Rotation value.

**Enumerator**



|<a name="_page27_x87.21_y86.20"></a>ROTATE\_0|0 degree|
| - | - |
|<a name="_page27_x82.23_y100.86"></a>ROTATE\_90|90 degrees|
|<a name="_page27_x77.24_y115.51"></a>ROTATE\_180|180 degrees|
|<a name="_page27_x77.24_y130.17"></a>ROTATE\_270|270 degrees|

5. **mpp\_flip\_mode\_t**

<a name="_page27_x70.87_y180.53"></a><a name="_page27_x70.87_y165.29"></a>enum [mpp_flip_mode_t ](#_page27_x70.87_y165.29)Flip type. **Enumerator**



|<a name="_page27_x108.05_y291.20"></a>FLIP\_NONE|no flip|
| - | - |
|<a name="_page27_x77.24_y305.75"></a>FLIP\_HORIZONTAL|horizontal flip|
|<a name="_page27_x90.38_y320.30"></a>FLIP\_VERTICAL|vertical flip|
|<a name="_page27_x109.40_y334.85"></a>FLIP\_BOTH|vertical and horizontal flip|

6. **mpp\_convert\_ops\_t**

<a name="_page27_x70.87_y385.09"></a><a name="_page27_x70.87_y369.85"></a>enum [mpp_convert_ops_t](#_page27_x70.87_y369.85)

The convert operations selector flags. **Enumerator**



|<a name="_page27_x114.33_y495.87"></a>MPP\_CONVERT\_NONE|no frame conversion|
| - | - |
|<a name="_page27_x106.55_y509.61"></a>MPP\_CONVERT\_ROTATE|frame rotation and flip|
|<a name="_page27_x110.83_y524.16"></a>MPP\_CONVERT\_SCALE|scaling from input\_frame toward output window|
|<a name="_page27_x108.35_y538.82"></a>MPP\_CONVERT\_COLOR|frame color conversion|
|<a name="_page27_x114.51_y552.56"></a>MPP\_CONVERT\_CROP|input frame crop|
|<a name="_page27_x77.24_y567.10"></a>MPP\_CONVERT\_OUT\_WINDOW|output window|

7. **mpp\_pixel\_format\_t**

<a name="_page27_x70.87_y617.35"></a><a name="_page27_x70.87_y602.11"></a>enum [mpp_pixel_format_t ](#_page27_x70.87_y602.11)Pixel format. **Enumerator**



|<a name="_page27_x102.17_y726.24"></a>MPP\_PIXEL\_ARGB|ARGB 32 bits.|
| - | - |
|<a name="_page27_x102.17_y739.98"></a>MPP\_PIXEL\_BGRA|BGRA 32 bits.|
|<a name="_page27_x102.17_y753.72"></a>MPP\_PIXEL\_RGBA|RGBA 32 bits.|

**Enumerator**



|<a name="_page28_x108.15_y86.20"></a>MPP\_PIXEL\_RGB|RGB 24 bits.|
| - | - |
|<a name="_page28_x93.19_y99.94"></a>MPP\_PIXEL\_RGB565|RGB 16 bits.|
|<a name="_page28_x108.15_y113.67"></a>MPP\_PIXEL\_BGR|BGR 24 bits.|
|<a name="_page28_x88.11_y127.41"></a>MPP\_PIXEL\_GRAY888|gray 3x8 bits|
|<a name="_page28_x82.13_y142.07"></a>MPP\_PIXEL\_GRAY888X|gray 3x8 bits +8 unused bits|
|<a name="_page28_x103.07_y156.73"></a>MPP\_PIXEL\_GRAY|gray 8 bits|
|<a name="_page28_x93.09_y171.39"></a>MPP\_PIXEL\_GRAY16|gray 16 bits|
|<a name="_page28_x83.22_y185.84"></a>MPP\_PIXEL\_YUV1P444|YUVX interleaved 4:4:4.|
|<a name="_page28_x77.24_y199.37"></a>MPP\_PIXEL\_VYUY1P422|VYUY interleaved 4:2:2.|
|<a name="_page28_x77.24_y212.90"></a>MPP\_PIXEL\_UYVY1P422|UYVY interleaved 4:2:2.|
|<a name="_page28_x103.16_y226.43"></a>MPP\_PIXEL\_YUYV|YUYV interleaved 4:2:2.|
|<a name="_page28_x87.22_y239.97"></a>MPP\_PIXEL\_DEPTH16|depth 16 bits|
|<a name="_page28_x92.21_y254.31"></a>MPP\_PIXEL\_DEPTH8|depth 8 bits|
|<a name="_page28_x88.21_y268.65"></a>MPP\_PIXEL\_YUV420P|YUV planar 4:2:0.|
|<a name="_page28_x93.42_y283.20"></a>MPP\_PIXEL\_INVALID|invalid pixel format|

8. **mpp\_element\_id\_t**

<a name="_page28_x70.87_y333.45"></a><a name="_page28_x70.87_y318.21"></a>enum [mpp_element_id_t ](#_page28_x70.87_y318.21)Processing element ids. **Enumerator**



|<a name="_page28_x142.16_y444.02"></a>MPP\_ELEMENT\_INVALID|Invalid element.|
| - | - |
|<a name="_page28_x130.48_y457.76"></a>MPP\_ELEMENT\_COMPOSE|Image composition - NOT IMPLEMENTED YET.|
|<a name="_page28_x77.24_y472.42"></a>MPP\_ELEMENT\_LABELED\_RECTANGLE|Labeled rectangle - bounding box.|
|<a name="_page28_x153.40_y487.07"></a>MPP\_ELEMENT\_TEST|Test inplace element - NOT FOR USE.|
|<a name="_page28_x124.51_y501.62"></a>MPP\_ELEMENT\_INFERENCE|Inference engine.|
|<a name="_page28_x132.75_y516.28"></a>MPP\_ELEMENT\_CONVERT|Image conversion: resolution, orientation, color format.|
|<a name="_page28_x155.90_y530.94"></a>MPP\_ELEMENT\_NUM|DO NOT USE.|

9. **mpp\_tensor\_type\_t**

<a name="_page28_x70.87_y580.38"></a><a name="_page28_x70.87_y565.14"></a>enum [mpp_tensor_type_t ](#_page28_x70.87_y565.14)Inference tensor type. **Enumerator**



|<a name="_page28_x77.24_y691.05"></a>MPP\_TENSOR\_TYPE\_FLOAT32|<a name="_page28_x70.87_y755.48"></a>floating point 32 bits|
| - | - |
|<a name="_page28_x88.95_y705.70"></a>MPP\_TENSOR\_TYPE\_UINT8|unsigned integer 8 bits|
|<a name="_page28_x95.43_y720.36"></a>MPP\_TENSOR\_TYPE\_INT8|signed integer 8 bits|

**MCU Media Processing Pipeline**

10. **mpp\_tensor\_order\_t**

<a name="_page29_x70.87_y70.87"></a>enum [mpp_tensor_order_t ](#_page28_x70.87_y755.48)Inference input tensor order. **Enumerator**



|<a name="_page29_x77.24_y166.68"></a>MPP\_TENSOR\_ORDER\_UNKNOWN|order not set|
| - | - |
|<a name="_page29_x96.40_y180.42"></a>MPP\_TENSOR\_ORDER\_NHWC|order: Batch, Height, Width, Channels|
|<a name="_page29_x96.40_y195.07"></a>MPP\_TENSOR\_ORDER\_NCHW|order: Batch, Channels, Height, Width|

11. **mpp\_inference\_type\_t**

<a name="_page29_x70.87_y245.43"></a><a name="_page29_x70.87_y230.19"></a>enum [mpp_inference_type_t ](#_page29_x70.87_y230.19)Inference type. **Enumerator**



|<a name="_page29_x77.24_y356.10"></a>MPP\_INFERENCE\_TYPE\_TFLITE|TensorFlow-Lite.|
| - | - |

3. **Return\_codes**

<a name="_page29_x70.87_y404.84"></a><a name="_page29_x70.87_y444.70"></a>**Macros**

- #define [MPP_SUCCESS](#_page29_x70.87_y699.46)
- #define [MPP_ERROR](#_page29_x70.87_y771.21)
- #define [MPP_INVALID_ELEM](#_page30_x70.87_y135.83)
- #define [MPP_INVALID_PARAM](#_page30_x70.87_y226.75)
- #define [MPP_ERR_ALLOC_MUTEX](#_page30_x70.87_y317.77)
- #define [MPP_INVALID_MUTEX](#_page30_x70.87_y408.91)
- #define [MPP_MUTEX_TIMEOUT](#_page30_x70.87_y499.83)
- #define [MPP_MUTEX_ERROR](#_page30_x70.87_y589.07)
- #define [MPP_MALLOC_ERROR](#_page30_x70.87_y680.10)
1. **Detailed<a name="_page29_x70.87_y602.80"></a> Description**

MPP APIs return status definitions.

2. **Macro<a name="_page29_x70.87_y668.85"></a> Definition Documentation**
1. **MPP\_SUCCESS**

<a name="_page29_x70.87_y699.46"></a>#define MPP\_SUCCESS <a name="_page29_x70.87_y771.21"></a>Success return code.

**3.3 Return\_codes 27**

2. **MPP\_ERROR**

<a name="_page30_x70.87_y70.87"></a>#define MPP\_ERROR

A generic error occured.

3. **MPP\_INVALID\_ELEM**

<a name="_page30_x70.87_y150.87"></a><a name="_page30_x70.87_y135.83"></a>#define MPP\_INVALID\_ELEM Invalid element provided.

4. **MPP\_INVALID\_PARAM**

<a name="_page30_x70.87_y241.79"></a><a name="_page30_x70.87_y226.75"></a>#define MPP\_INVALID\_PARAM Invalid parameter provided.

5. **MPP\_ERR\_ALLOC\_MUTEX**

<a name="_page30_x70.87_y332.81"></a><a name="_page30_x70.87_y317.77"></a>#define MPP\_ERR\_ALLOC\_MUTEX Error occured while allocating mutex.

6. **MPP\_INVALID\_MUTEX**

<a name="_page30_x70.87_y423.95"></a><a name="_page30_x70.87_y408.91"></a>#define MPP\_INVALID\_MUTEX Invalid mutex provided.

7. **MPP\_MUTEX\_TIMEOUT**

<a name="_page30_x70.87_y514.87"></a><a name="_page30_x70.87_y499.83"></a>#define MPP\_MUTEX\_TIMEOUT <a name="_page30_x70.87_y589.07"></a>Mutex timeout occured.

8. **MPP\_MUTEX\_ERROR**

<a name="_page30_x70.87_y604.11"></a>#define MPP\_MUTEX\_ERROR <a name="_page30_x70.87_y680.10"></a>Mutex error occured.

9. **MPP\_MALLOC\_ERROR**

<a name="_page30_x70.87_y695.14"></a>#define MPP\_MALLOC\_ERROR Memory allocation error occured.

