# eIQ MPP Hardware Abstraction Layer API

MPP-HAL VERSION 3.6

## Chapter 1

This is the documentation for the Hardware Abstraction Layer(HAL) API.

### 1.1 HAL overview

The hardware abstraction layer is used to abstract hardware and software components. With the usage of an HAL abstraction, the vision pipeline will be leveraging hardware accelerated components whenever possible.

#### 1.1.1 MPP hal description

The HAL is presented with respect of the following points:

- A common header file "hal.h" includes all hardware top level functions.
- All hardware top level functions are using the prefix: "hal\_ ".
- For each platform all hal\_ functions defined in hal.h should be implemented at least with an empty function.

Here is an overview:

![](hal.png)

**Figure 1.1 HAL overview**

#### 1.1.2 MPP HAL components
1. **Source<a name="_page5_x70.87_y491.80"></a> elements HAL**
- Camera
- Static image
2. **processing<a name="_page5_x70.87_y583.41"></a> elements HAL**
- Graphics driver
- Vision algorithms
- Labeled rectangle
3. **Sink<a name="_page5_x70.87_y695.17"></a> elements HAL**
- Display

#### 1.1.3 Supported devices

At present, the MPP HAL supports the following devices:

- Cameras:
    * OV5640
    * MT9M114
    * OV7670
    * Logitech C920 PRO HD WEBCAM
- Displays:
    * LVGL
    * RK055AHD091
    * RK055MHD091
    * RK043FN02H-CT
    * Mikroe TFT Proto 5(SSD1963 controller)
    * NXP's LCD-PAR-S035 (ST7796S controller)
- Graphics:
    * PXP
    * CPU
    * GPU
#### 1.1.4 Supported boards

Currently, the MPP HAL supports the following boards:

- (deprecated) evkmimxrt1170 is supported with the following devices:
    * Cameras:  OV5640.
    * Displays: RK055AHD091 and RK055MHD091.

- (deprecated) evkbimxrt1050 is supported with the following devices:
    * Cameras:  MT9M114.
    * Displays: RK043FN02H-CT.

- evkbmimxrt1170 is supported by porting the following devices:
    * Cameras:  OV5640.
    * Displays: LVGL, RK055AHD091 and RK055MHD091.
    
- frdmmcxn947 is supported by porting the following devices:
    * Cameras:  OV7670.
    * Displays: Mikroe TFT Proto 5" and NXP's LCD-PAR-S035.

- mimxrt700evk is supported by porting the following devices:
    * Cameras:  OV7670, Logitech C920 PRO HD WEBCAM
    * Displays: RK055AHD091 and RK055MHD091.

How to port new boards/devices: 
The MPP Hal provides the flexibility to the user to port new boards and devices(cameras and displays). 

Supporting new boards: 
To support a new board a new file hal\_{board\_name} should be added under the 'hal' directory. 

Supporting new devices: 
The hal components that can support new devices are: 
- Cameras
- Display
- Graphics processing 

A new device can simply be supported by: 
- Providing the approriate hal\_{device\_module} implementation. 
- Adding his name and setup entry point to the appropriate device list in the associated board hal\_{board\_name} file. 

Enabling/Disabling Hal components and devices: 
- The HAL components can be enabled/disabled from "mpp\_config.h" using the compilation flags(HAL\_ENABLE\_{component\_name}). 
- The HAL devices can also be enabled/disabled from "mpp\_config.h" using the compilation flags(HAL\_ENABLE\_{device\_name}).


## Chapter 2

### 2.1 HAL Types

**Data Structures**

- struct[ camera_dev_static_config_t](#_page12_x70.87_y161.91)
- struct[ camera_dev_private_capability_t](#_page12_x70.87_y603.13)
- struct[ camera_dev_t](#_page12_x70.87_y752.95)
- struct[ static_image_static_config_t](#_page13_x70.87_y356.78)
- struct[ static_image_t](#_page13_x70.87_y607.63)
- struct[ gfx_surface_t](#_page14_x70.87_y70.87)
- struct[ gfx_rotate_config_t](#_page14_x70.87_y319.68)
- struct[ gfx_dev_t](#_page14_x70.87_y470.63)
- struct[ hal_rect_t](#_page14_x70.87_y652.72)
- struct[ vdec_dev_t](#_page15_x70.87_y134.51)
- struct[ model_param_t](#_page15_x70.87_y286.04)
- struct[ valgo_dev_private_capability_t](#_page16_x70.87_y707.58)
- struct[ vision_frame_t](#_page17_x70.87_y121.40)
- struct[ vision_algo_dev_t](#_page17_x70.87_y314.97)
- struct[ display_dev_private_capability_t](#_page17_x70.87_y543.04)
- struct[ display_dev_t](#_page18_x70.87_y208.50)
- struct[ hw_buf_desc_t](#_page18_x70.87_y473.31)
- struct[ hal_img_decoder_setup_t](#_page18_x70.87_y709.18)
- struct[ hal_graphics_setup_t](#_page19_x70.87_y135.04)
- struct[ hal_display_setup_t](#_page19_x70.87_y246.14)
- struct[ hal_camera_setup_t](#_page19_x70.87_y356.68)
- struct[ checksum_data_t](#_page19_x70.87_y466.41)

**Macros**

- #define[ HAL_GFX_DEV_CPU_NAME](#_page19_x70.87_y643.82)
- #define[ GUI_PRINTF_BUF_SIZE](#_page19_x397.39_y759.07)
- #define[ GUI_PRINTF_BUF_SIZE](#_page19_x397.39_y759.07)
- #define[ HAL_VDEC_DEV_NAME](#_page20_x70.87_y250.36)
- #define[ MAX_INPUT_PORTS](#_page20_x115.13_y383.36)
- #define [MAX_INPUT_PORTS ]
- #define[ HAL_DEVICE_NAME_MAX_LENGTH](#_page20_x247.59_y492.45)

**Typedefs**

- [camera_dev_callback_t)](#_page20_x70.87_y647.27)
- [vision_algo_private_data_t]
- [mpp_callback_t)](#_page21_x70.87_y205.33)
- [img_decoder_setup_func_t)](#_page21_x70.87_y292.36)
- [graphic_setup_func_t)](#_page21_x70.87_y379.38)
- [display_setup_func_t)](#_page21_x70.87_y466.41)
- [camera_setup_func_t)](#_page21_x70.87_y553.43)

## Enumerations

- enum [hal_camera_status_t](#_page21_x70.87_y680.23) {

  [kStatus_HAL_CameraSuccess](#_page22_x93.51_y86.20),

  [kStatus_HAL_CameraBusy](#_page22_x107.46_y99.94),

  [kStatus_HAL_CameraNoData](#_page22_x97.00_y114.49),

  [kStatus_HAL_CameraNonBlocking](#_page22_x77.24_y128.22),

  [kStatus_HAL_CameraError](#_page22_x107.47_y142.88)

}

- enum [camera_event_t](#_page22_x70.87_y189.94) {

  [kCameraEvent_SendFrame](#_page22_x101.78_y327.92),

  [kCameraEvent_CameraDeviceInit](#_page22_x77.24_y341.66)

}

- enum [hal_image_status_t](#_page22_x70.87_y389.53) {

  [MPP_kStatus_HAL_ImageSuccess](#_page22_x77.24_y527.50),

  [MPP_kStatus_HAL_ImageError](#_page22_x91.20_y542.16)

}

- enum [gfx_rotate_target_t](#_page22_x70.87_y580.00) {

  **kGFXRotateTarget_None**,

  **kGFXRotate_SRCSurface**,

  **kGFXRotate_DSTSurface**

}

- enum [hal_valgo_status_t](#_page22_x70.87_y673.12) {

  [kStatus_HAL_ValgoSuccess](#_page23_x89.19_y86.20),

  [kStatus_HAL_ValgoMallocError](#_page23_x77.24_y100.86),

  [kStatus_HAL_ValgoInitError](#_page23_x91.19_y115.51),

  [kStatus_HAL_ValgoError](#_page23_x103.15_y130.17),

  [kStatus_HAL_ValgoStop](#_page23_x104.63_y144.83)

}

- enum [display_event_t](#_page23_x70.87_y192.81) {

  [kDisplayEvent_RequestFrame](#_page23_x77.24_y330.78)

}

- enum [hal_display_status_t](#_page23_x70.87_y378.76) {

  [kStatus_HAL_DisplaySuccess](#_page23_x93.51_y516.63),

  [kStatus_HAL_DisplayTxBusy](#_page23_x97.50_y531.18),

  [kStatus_HAL_DisplayNonBlocking](#_page23_x77.24_y545.73),

  [kStatus_HAL_DisplayError](#_page23_x107.47_y560.38)

}

- enum [mpp_memory_policy_t](#_page23_x70.87_y606.20) {

  [HAL_MEM_ALLOC_NONE](#_page24_x88.20_y86.20),

  [HAL_MEM_ALLOC_INPUT](#_page24_x87.20_y100.75),

  [HAL_MEM_ALLOC_OUTPUT](#_page24_x77.24_y127.25),

  [HAL_MEM_ALLOC_BOTH](#_page24_x89.55_y153.76)

}

- enum [checksum_type_t](#_page24_x70.87_y201.63) {

  [CHECKSUM_TYPE_PISANO](#_page24_x99.65_y337.71),

  [CHECKSUM_TYPE_CRC_ELCDIF](#_page24_x77.24_y352.37)

}

**Functions**

- int[ HAL_GfxDev_CPU_Register ](#_page24_x70.87_y449.33)(gfx\_dev\_t ∗dev)
- int[ HAL_GfxDev_GPU_Register ](#_page24_x70.87_y686.24)(gfx\_dev\_t ∗dev)
- int[ HAL_JPEG_CPU_Register ](#_page26_x70.87_y192.15)(vdec\_dev\_t ∗dev)
- int[ HAL_JPEG_HW_Register ](#_page26_x70.87_y448.04)(vdec\_dev\_t ∗dev)
- int **setup\_static\_image\_elt** (static\_image\_t ∗elt)
- uint32\_t **calc\_checksum** (int size\_b, void ∗pbuf)

#### 2.1.1 Detailed Description  

This section provides the detailed documentation for the MPP HAL types.  

##### 2.1.1.1 Data Structure Documentation  

1. **struct<a name="_page12_x70.87_y161.91"></a> camera\_dev\_static\_config\_t**  

Structure that characterizes the camera device.  

**Data Fields**  

|type|name|description|  
|---|---|---|  
|int|height|buffer height|  
|int|width|buffer width|  
|int|pitch|buffer pitch|  
|int|left|left position|  
|int|top|top position|  
|int|right|right position|  
|int|bottom|bottom position|  
|mpp\_rotate\_degree\_t|rotate|rotate degree|  
|mpp\_flip\_mode\_t|flip|flip|  
|int|swapByte|swap byte per two bytes|  
|mpp\_pixel\_format\_t|format|pixel format|  
|int|framerate|frame rate|  
|int|stripe\_size|stripe size in bytes|  
|bool|stripe|stripe mode|  
|uint32\_t|n\_streams|number of total output video streams|  
|uint32\_t|min\_stream\_req\_cnt|minimum number of enqueue calls to wait for|  
|uint32\_t|crt\_stream\_req\_cnt|number of streams requested for enqueue|  
|mpp\_exec\_flag\_t|req\_cnt\_type|flag to control stream request counting|  
|mpp\_camera\_stream\_cfg|stream[NUM\_STREAMS]|stream configuration|  
|bool|stream\_requested[NUM\_STREAMS]|flag to track if a stream is required for enqueue|  
|bool|in\_advance\_enqueue|flag to indicate advance enqueue mode|  

2. **struct<a name="_page12_x70.87_y618.37"></a> camera\_dev\_private\_capability\_t**  

Camera device private capability.  

**Data Fields**  

|type|name|description|  
|-|-|-|
|[camera_dev_callback_t](#_page20_x70.87_y647.27)|callback|callback|
|void ∗|param|parameter for the callback|  

3. **struct<a name="_page13_x70.87_y70.87"></a> \_camera\_dev**  

Camera devices can enqueue and dequeue frames as well as react to events from input devices via the "input← Notify" function. Camera devices can use any number of interfaces, including MIPI and CSI as long as the HAL driver implements the necessary functions found in[ camera_dev_operator_t.](#_page27_x70.87_y515.04) Examples of camera devices include the Orbbec U1S 3D SLM camera module and the OnSemi MT9M114 camera module.
 
**Data Fields**  

|int|id|unique id which is assigned by camera manager during registration|
| - | - | :- |
|char|name[[HAL_DEVICE_NAME_MAX_L](#_page20_x247.59_y492.45)|[ENGTHname of\]](#_page20_x247.59_y492.45) the device|
|const[ camera_dev_operator_t ](#_page27_x70.87_y515.04)∗|ops|operations|
|[camera_dev_static_config_t](#_page12_x70.87_y161.91)|config|static configurations|
|[camera_dev_private_capability_t](#_page12_x70.87_y603.13)|cap|private capability|
|void ∗|data|device private data|

4. **struct<a name="_page13_x70.87_y372.02"></a> static\_image\_static\_config\_t**  

Structure that characterizes the image element.  

**Data Fields**  
|type|name|description|  
|---|---|---|
|int|height|buffer height|  
|int|width|buffer width|  
|int|left|left position|  
|int|top|top position|  
|int|right|right position|  
|int|bottom|bottom position|  
|mpp\_pixel\_format\_t|format|pixel format|  
|bool|stripe|stripe mode|  
|int|compressed\_size|compressed size in bytes|  

5. **struct<a name="_page13_x70.87_y622.87"></a> \_static\_image** 
 
Attributes of an image element. 
 
**Data Fields**  

|type|name|description|  
|---|---|---|  
|int|id|unique id which is assigned by image manager|  
|const[ static_image_operator_t ](#_page29_x70.87_y185.80)∗|ops|operations|
|[static_image_static_config_t](#_page13_x70.87_y356.78)|config|static configs|
|int|stripe\_idx|the current stripe index|
|uint8\_t ∗|buffer|static image buffer|

6. **struct<a name="_page13_x70.87_y649.13"></a> gfx\_surface\_t**  

Gfx surface parameters. 
 
**Data Fields**  

|type|name|description|  
|---|---|---|  
|int|height|buffer height|  
|int|width|buffer width|  
|int|pitch|buffer pitch|  
|int|left|left position|  
|int|top|top position|  
|int|right|right position|  
|int|bottom|bottom position|  
|int|swapByte|swap byte per two bytes|  
|mpp\_pixel\_format\_t|format|pixel format|  
|void ∗|buf|buffer|  
|void ∗|lock|the structure is determined by hal and set to null if not use in hal|  

7. **struct<a name="_page14_x70.87_y334.92"></a> gfx\_rotate\_config\_t**  

gfx rotate configuration. 
 
**Data Fields** 
 
|type|name|description|  
|---|---|---|
|gfx\_rotate\_target\_t|target|| 
|mpp\_rotate\_degree\_t|degree||  

8. **struct<a name="_page14_x70.87_y485.87"></a> \_gfx\_dev** 
 
**Data Fields**  

|type|name|description|  
|---|---|---|  
|int|id||  
|const[ gfx_dev_operator_t ](#_page29_x70.87_y493.39)|ops||
|[gfx_surface_t](#_page14_x70.87_y70.87)|src||
|[gfx_surface_t](#_page14_x70.87_y70.87)|dst||
|[mpp_callback_t](#_page21_x70.87_y205.33)|callback||
|void ∗|user\_data||  

9. **struct<a name="_page14_x70.87_y652.72"></a> hal\_rect\_t**  

rectangle positions.  

**Data Fields**  

|type|name|description|  
|---|---|---|  
|int|top||  
|int|left||  
|int|bottom||  
|int|right||  

10. **struct<a name="_page15_x70.87_y149.75"></a><a name="_page15_x70.87_y134.51"></a> \_vdec\_dev**

**Data Fields**  

|type|name|description|  
|---|---|---|
|int|id||
|const[ vdec_dev_operator_t ](#_page29_x70.87_y771.02)∗|ops||
|[mpp_callback_t](#_page21_x70.87_y205.33)|callback||
|void ∗|user\_data||

11. **struct<a name="_page15_x70.87_y149.75"></a> model\_param\_t**  

Structure passed to HAL as description of the binary model provided by user.  

**Data Fields** 
 
- const void ∗[model_data](#_page15_x217.36_y578.17)
- int[ model_size](#_page15_x70.87_y667.87)
- float[ model_input_mean](#_page15_x70.87_y757.58)
- float[ model_input_std](#_page16_x70.87_y102.56)
- mpp\_inference\_params\_t[ inference_params](#_page16_x70.87_y167.46)
- int[ height](#_page16_x70.87_y232.36)
- int[ width](#_page16_x70.87_y297.26)
- mpp\_pixel\_format\_t[ format](#_page16_x70.87_y374.12)
- mpp\_tensor\_type\_t[ inputType](#_page16_x70.87_y439.02)
- mpp\_tensor\_order\_t[ tensor_order](#_page16_x70.87_y503.92)
- int(∗[evt_callback_f ](#_page16_x70.87_y568.82))(mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data)
- void ∗[cb_userdata](#_page16_x70.87_y645.68)

**Field Documentation** 

<a name="_page15_x217.36_y578.17"></a>**model\_data**

const void∗ model\_param\_t::model\_data pointer to model binary

<a name="_page15_x70.87_y667.87"></a>**model\_size**

int model\_param\_t::model\_size model binary size

<a name="_page15_x70.87_y757.58"></a>**model\_input\_mean**

float model\_param\_t::model\_input\_mean

model 'mean' of input values, used for normalization

<a name="_page16_x70.87_y102.56"></a>**model\_input\_std**

float model\_param\_t::model\_input\_std

model<a name="_page16_x70.87_y102.56"></a> 'standard deviation' of input values, used for normalization

**inference\_params**

mpp\_inference\_params\_t model\_param\_t::inference\_params inference parameters

<a name="_page16_x70.87_y232.36"></a>**height**

int model\_param\_t::height frame height

<a name="_page16_x70.87_y297.26"></a>**width**

int model\_param\_t::width frame width

<a name="_page16_x70.87_y374.12"></a>**format**

mpp\_pixel\_format\_t model\_param\_t::format pixel format

<a name="_page16_x70.87_y439.02"></a>**inputType**

mpp\_tensor\_type\_t model\_param\_t::inputType input type

<a name="_page16_x70.87_y503.92"></a>**tensor\_order**

mpp\_tensor\_order\_t model\_param\_t::tensor\_order <a name="_page16_x70.87_y503.92"></a>tensor order

**evt\_callback\_f**

int(∗ model\_param\_t::evt\_callback\_f) (mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data)

the callback to be called when model output is ready

<a name="_page16_x70.87_y568.82"></a>**cb\_userdata**

void∗ model\_param\_t::cb\_userdata

pointer to user data, should be passed by callback

12. **struct<a name="_page16_x70.87_y721.95"></a><a name="_page16_x70.87_y702.04"></a> valgo\_dev\_private\_capability\_t** 

Valgo devices private capability.

**Data Fields**

|type|name|description|
|-|-|-|
|void ∗|param|param for the callback|

13. **struct<a name="_page17_x70.87_y136.64"></a><a name="_page17_x70.87_y121.40"></a> vision\_frame\_t**

Characteristics that need to be defined by a vision algo.

**Data Fields**

|type|name|description|
|-|-|-|
|int|height|frame height|
|int|width|frame width|
|int|pitch|frame pitch|
|mpp\_pixel\_format\_t|format|pixel format|
|void ∗|input\_buf|pixel input buffer|

14. **struct<a name="_page17_x70.87_y330.21"></a><a name="_page17_x70.87_y314.97"></a> \_vision\_algo\_dev**

Attributes of a vision algo device.

**Data Fields**

|type|name|description|
|-|-|-|
|int|id|unique id which is assigned by algorithm manager during the registration|
|char|name[[HAL_DEVICE_NAME_MAX_L](#_page20_x247.59_y492.45)|[ENGTHname to\]](#_page20_x247.59_y492.45) identify|
|[valgo_dev_private_capability_t](#_page16_x70.87_y707.58)|cap|private capability|
|<p>const[ vision_algo_dev_operator_t](#_page30_x70.87_y240.33)</p><p>∗</p>|ops|operations|
|vision\_algo\_private\_data\_t|priv\_data|private data|

15. **struct<a name="_page17_x70.87_y558.28"></a><a name="_page17_x70.87_y543.04"></a> \_display\_dev\_private\_capability**

Structure that characterizes the display device.

**Data Fields**

|type|name|description|
|-|-|-|
|int|height|buffer height|
|int|width|buffer width|
|int|pitch|buffer pitch|
|int|left|left position|
|int|top|top position|
|int|right|right position|
|int|bottom|bottom position|
|int|stripe\_height|stripe height (0 if stripe mode is off)|
|bool|stripe|stripe mode|
|mpp\_rotate\_degree\_t|rotate|rotate degree|
|mpp\_pixel\_format\_t|format|pixel format|
|int|nbFrameBuffer|number of input buffers|
|void ∗∗|frameBuffers|array of pointers to frame buffer|
|[mpp_callback_t](#_page20_x70.87_y569.51)|callback|callback|
|void ∗|user\_data|parameter for the callback|
|void ∗|handle|Handle to the LVGL widget 'image'.|

16. **struct<a name="_page18_x70.87_y223.74"></a><a name="_page18_x70.87_y208.50"></a> \_display\_dev**

Attributes of a display device. hal display device declaration.

Display devices can be used to display images, GUI overlays, etc. Examples of display devices include display panels like the RK024hh298 display, and external displays like UVC (video over USB).

**Data Fields**

|type|name|description|
|-|-|-|
|int|id|unique id which is assigned by the display manager during the registration|
|char|name[[HAL_DEVICE_NAME_MAX_L](#_page20_x247.59_y492.45)|[ENGTHname of\]](#_page20_x247.59_y492.45) the device|
|const[ display_dev_operator_t ](#_page31_x70.87_y119.49)∗|ops|operations|
|display\_dev\_private\_capability\_t|cap|private capability|

17. **struct<a name="_page18_x70.87_y488.55"></a><a name="_page18_x70.87_y473.31"></a> hw\_buf\_desc\_t**

the hardware specific buffer requirements

**Data Fields**

|type|name|description|
|-|-|-|
|int|stride|the number of bytes between 2 lines of image|
|int|nb\_lines|the number of lines required (set to 0 if the element doesn't require a specific number of lines)|
|int|alignment|alignment requirement in bytes|
|int|max\_image\_size|the number of bytes allocated|
|bool|cacheable|if true, HW will require cache maintenance|
|unsigned char ∗|addr|the aligned buffer address|
|unsigned char ∗|heap\_p|pointer to the heap that should be freed|

18. **struct<a name="_page18_x70.87_y724.42"></a><a name="_page18_x70.87_y709.18"></a> hal\_img\_decoder\_setup\_t!**

**Data Fields**

|type|name|description|
|-|-|-|
|const char ∗|vdec\_dev\_name||
|[img_decoder_setup_func_t](#_page21_x70.87_y292.36)|decoder\_setup\_func||

19. **struct<a name="_page19_x70.87_y135.04"></a> hal\_graphics\_setup\_t**

**Data Fields**

|type|name|description|
|-|-|-|
|const char ∗|gfx\_dev\_name||
|[graphic_setup_func_t](#_page21_x70.87_y379.38)|gfx\_setup\_func||

20. **struct<a name="_page19_x70.87_y246.14"></a> hal\_display\_setup\_t**

**Data Fields**

|type|name|description|
|-|-|-|
|const char ∗|display\_name||
|[display_setup_func_t](#_page21_x70.87_y466.41)|display\_setup\_func||

21. **struct<a name="_page19_x70.87_y356.68"></a> hal\_camera\_setup\_t**

**Data Fields**

|type|name|description|
|-|-|-|
|const char ∗|camera\_name||
|[camera_setup_func_t](#_page21_x70.87_y553.43)|camera\_setup\_func||

22. **struct<a name="_page19_x70.87_y466.41"></a> checksum\_data\_t**

computed checksum

**Data Fields**

|type|name|description|
|-|-|-|
|[checksum_type_t](#_page24_x70.87_y201.63)|type|checksum calculation method|
|uint32\_t|value|checksum value|

##### 2.1.1.2 Macro Definition Documentation

1. **HAL\_GFX\_DEV\_CPU\_NAME**

<a name="_page19_x70.87_y670.21"></a>#define HAL\_GFX\_DEV\_CPU\_NAME hal graphics (gfx) device declaration.

Graphics processing devices can be used to perform conversion from one image format to another, resize images and compose images on top of one another. Examples of graphics devices include<a name="_page19_x397.39_y759.07"></a> the PXP (pixel pipeline) found on many i.MXRT series MCUs. Name of the graphic device using CPU operations

2. **GUI\_PRINTF\_BUF\_SIZE<a name="_page20_x70.87_y70.87"></a> [1/2]**

#define GUI\_PRINTF\_BUF\_SIZE Local text buffer size.

3. **GUI\_PRINTF\_BUF\_SIZE<a name="_page20_x70.87_y168.13"></a> [2/2]**

#define GUI\_PRINTF\_BUF\_SIZE Local text buffer size.

4. **HAL\_VDEC\_DEV\_NAME**

<a name="_page20_x70.87_y277.23"></a><a name="_page20_x70.87_y250.36"></a>#define HAL\_VDEC\_DEV\_NAME

hal video decoder (vdec) device declaration.

Video decoder devices can be used to perform decompression of image. Examples of decoder devices include the PNG/JPEG<a name="_page20_x115.13_y383.36"></a> HW or SW found on many i.MXRT series MCUs. Name of the jpeg decoder device using CPU operations

5. **MAX\_INPUT\_PORTS**

<a name="_page20_x70.87_y412.11"></a>#define MAX\_INPUT\_PORTS

HAL public types header.

maximum number of element inputs/outputs

6. <a name="_page20_x247.59_y492.45"></a>**HAL\_DEVICE\_NAME\_MAX\_LENGTH**

<a name="_page20_x70.87_y521.21"></a>#define HAL\_DEVICE\_NAME\_MAX\_LENGTH maximum length of device name

##### 2.1.1.3 Typedef Documentation

1. **camera\_dev\_callback\_t**

<a name="_page20_x70.87_y674.13"></a><a name="_page20_x70.87_y647.27"></a>typedef int(∗ camera\_dev\_callback\_t) (const camera\_dev\_t ∗dev, [camera_event_t](#_page22_x70.87_y189.94) event, void ∗param, uint8\_t fromISR)

Callback function to notify camera manager that one frame is dequeued.

**Parameters**

|name|description|
|-|-|
|dev|Device structure of the camera device calling this function|
|event|id of the event that took place|
|param|Parameters|
|fromISR|True if this operation takes place in an irq, 0 otherwise|

**Returns**

0 if the operation was successfully

2. **mpp\_callback\_t**

<a name="_page21_x70.87_y231.61"></a><a name="_page21_x70.87_y205.33"></a>typedef int(∗ mpp\_callback\_t) (mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data) The mpp callback function prototype.

3. **img\_decoder\_setup\_func\_t**

<a name="_page21_x70.87_y318.63"></a><a name="_page21_x70.87_y292.36"></a>typedef int(∗ img\_decoder\_setup\_func\_t) (vdec\_dev\_t ∗) video decoder setup

4. **graphic\_setup\_func\_t**

<a name="_page21_x70.87_y405.66"></a><a name="_page21_x70.87_y379.38"></a>typedef int(∗ graphic\_setup\_func\_t) (gfx\_dev\_t ∗) graphics setup

5. **display\_setup\_func\_t**

<a name="_page21_x70.87_y492.69"></a><a name="_page21_x70.87_y466.41"></a>typedef int(∗ display\_setup\_func\_t) (display\_dev\_t ∗) display setup

6. **camera\_setup\_func\_t**

<a name="_page21_x70.87_y579.71"></a><a name="_page21_x70.87_y553.43"></a>typedef int(∗ camera\_setup\_func\_t) (const char ∗, camera\_dev\_t ∗) camera setup

##### 2.1.1.4 Enumeration Type Documentation

1. **hal\_camera\_status\_t**

<a name="_page21_x70.87_y706.50"></a><a name="_page21_x70.87_y680.23"></a>enum [hal_camera_status_t ](#_page21_x70.87_y680.23)Camera return status.

**Enumerator**

|label|description|
|-|-|
|<a name="_page22_x93.51_y86.20"></a>kStatus\_HAL\_CameraSuccess|HAL camera successful.|
|<a name="_page22_x107.46_y99.94"></a>kStatus\_HAL\_CameraBusy|Camera is busy.|
|<a name="_page22_x97.00_y114.49"></a>kStatus\_HAL\_CameraNoData|No data available from camera.|
|<a name="_page22_x77.24_y128.22"></a>kStatus\_HAL\_CameraNonBlocking|Camera will return immediately.|
|<a name="_page22_x107.47_y142.88"></a>kStatus\_HAL\_CameraError|Error occurs on HAL Camera.|

2. **camera\_event\_t**

<a name="_page22_x70.87_y217.14"></a><a name="_page22_x70.87_y189.94"></a>enum [camera_event_t](#_page22_x70.87_y189.94)

Type of events that are supported by calling the callback function.

**Enumerator**

|label|description|
|-|-|
|<a name="_page22_x101.78_y327.92"></a>kCameraEvent\_SendFrame|Camera new frame is available.|
|<a name="_page22_x77.24_y341.66"></a>kCameraEvent\_CameraDeviceInit|Camera device finished the initialization process.|

3. **hal\_image\_status\_t**

<a name="_page22_x70.87_y416.72"></a><a name="_page22_x70.87_y389.53"></a>enum [hal_image_status_t ](#_page22_x70.87_y389.53)static image return status

**Enumerator**

|label|description|
|-|-|
|<a name="_page22_x77.24_y527.50"></a>MPP\_kStatus\_HAL\_ImageSuccess|Successfully.|
|<a name="_page22_x91.20_y542.16"></a>MPP\_kStatus\_HAL\_ImageError|Error occurs on HAL Image.|

4. **gfx\_rotate\_target\_t**

<a name="_page22_x70.87_y606.44"></a><a name="_page22_x70.87_y580.00"></a>enum [gfx_rotate_target_t ](#_page22_x70.87_y580.00)gfx rotate target

5. **hal\_valgo\_status\_t**

<a name="_page22_x70.87_y699.56"></a><a name="_page22_x70.87_y673.12"></a>enum [hal_valgo_status_t](#_page22_x70.87_y673.12)

Valgo Error codes for hal operations.

**Enumerator**

|label|description|
|-|-|
|<a name="_page23_x89.19_y86.20"></a>kStatus\_HAL\_ValgoSuccess|Successfully.|
|<a name="_page23_x77.24_y100.86"></a>kStatus\_HAL\_ValgoMallocError|memory allocation failed for HAL algorithm|
|<a name="_page23_x91.19_y115.51"></a>kStatus\_HAL\_ValgoInitError|algorithm initialization error|
|<a name="_page23_x103.15_y130.17"></a>kStatus\_HAL\_ValgoError|Error occurs in HAL algorithm.|
|<a name="_page23_x104.63_y144.83"></a>kStatus\_HAL\_ValgoStop|HAL algorithm stop.|

6. **display\_event\_t**

<a name="_page23_x70.87_y220.00"></a><a name="_page23_x70.87_y192.81"></a>enum [display_event_t](#_page23_x70.87_y192.81)

Type of events that are supported by calling the callback function.

**Enumerator**

|label|description|
|-|-|
|<a name="_page23_x77.24_y330.78"></a>kDisplayEvent\_RequestFrame|Display finished sending the frame asynchronously, provide another frame.|

7. **hal\_display\_status\_t**

<a name="_page23_x70.87_y405.96"></a><a name="_page23_x70.87_y378.76"></a>enum [hal_display_status_t ](#_page23_x70.87_y378.76)Error codes for display hal devices.

**Enumerator**

|label|description|
|-|-|
|<a name="_page23_x93.51_y516.63"></a>kStatus\_HAL\_DisplaySuccess|HAL display successful.|
|<a name="_page23_x97.50_y531.18"></a>kStatus\_HAL\_DisplayTxBusy|Display tx is busy.|
|<a name="_page23_x77.24_y545.73"></a>kStatus\_HAL\_DisplayNonBlocking|Display will return immediately.|
|<a name="_page23_x107.47_y560.38"></a>kStatus\_HAL\_DisplayError|Error occurs on HAL Display.|

8. **mpp\_memory\_policy\_t**

<a name="_page23_x70.87_y633.24"></a><a name="_page23_x70.87_y606.20"></a>enum [mpp_memory_policy_t](#_page23_x70.87_y606.20)

The memory allocation policy of an element's hal.

During the pipeline construction, the HAL uses this enum to tell the pipeline if it already owns input/ouput buffers. Before the pipeline starts, the memory manager will map the existing buffers to elements and allocate missing buffers from the heap.

**Enumerator**

|label|description|
|-|-|
|<a name="_page24_x88.20_y86.20"></a>HAL\_MEM\_ALLOC\_NONE|element requires buffers to be provided by other elements, or by the pipeline|
|<a name="_page24_x87.20_y100.75"></a>HAL\_MEM\_ALLOC\_INPUT|element allocates its input buffer, it may require output buffers to be provided by other elements, or by the pipeline|
|<a name="_page24_x77.24_y127.25"></a>HAL\_MEM\_ALLOC\_OUTPUT|element allocates its output buffer, it may require input buffers to be provided by other elements, or by the pipeline|
|<a name="_page24_x89.55_y153.76"></a>HAL\_MEM\_ALLOC\_BOTH|element allocates both its input and output buffers|

9. **checksum\_type\_t**

<a name="_page24_x70.87_y228.83"></a><a name="_page24_x70.87_y201.63"></a>enum [checksum_type_t ](#_page24_x70.87_y201.63)checksum calculation method

**Enumerator**

|label|description|
|-|-|
|<a name="_page24_x99.65_y337.71"></a>CHECKSUM\_TYPE\_PISANO|checksum computed using Pisano|
|<a name="_page24_x77.24_y352.37"></a>CHECKSUM\_TYPE\_CRC\_ELCDIF|checksum computed CRC from ELCDIF|

##### 2.1.1.5 Function Documentation

1. **HAL\_GfxDev\_CPU\_Register()**

<a name="_page24_x70.87_y476.53"></a>int HAL\_GfxDev\_CPU\_Register ( gfx\_dev\_t ∗ dev )

Register the graphic device with the CPU operations.

**Parameters**

|in/out|name|description|
|-|-|-|
|in|dev|graphic device to register|

**Returns**

error code (0: success, otherwise: failure)

2. **HAL\_GfxDev\_GPU\_Register()**

<a name="_page24_x70.87_y712.86"></a>int HAL\_GfxDev\_GPU\_Register ( gfx\_dev\_t ∗ dev )

Register the graphic device with the GPU operations.

**Parameters**

|in/out|name|description|
|-|-|-|
|in|dev|graphic device to register|

**Returns**

error code (0: success, otherwise: failure)

3. **HAL\_JPEG\_CPU\_Register()**

<a name="_page26_x70.87_y219.35"></a><a name="_page26_x70.87_y192.15"></a>int HAL\_JPEG\_CPU\_Register (

vdec\_dev\_t ∗ dev ) Register the jpeg SW decoder device. 

**Parameters**

|in/out|name|description|
|-|-|-|
|in|dev|decoder device to register|
| - | - | - |

**Returns**

error code (0: success, otherwise: failure)

4. **HAL\_JPEG\_HW\_Register()**

<a name="_page26_x70.87_y475.23"></a><a name="_page26_x70.87_y448.04"></a>int HAL\_JPEG\_HW\_Register (

vdec\_dev\_t ∗ dev ) Register the jpeg HW decoder device. 

**Parameters**

|in/out|name|description|
|-|-|-|
|in|dev|decoder device to register|
| - | - | - |

**Returns**

error code (0: success, otherwise: failure)

### 2.2 HAL OPERATIONS

**Data Structures**

- struct[ camera_dev_operator_t](#_page27_x70.87_y515.04)
- struct[ static_image_operator_t](#_page29_x70.87_y185.80)
- struct[ gfx_dev_operator_t](#_page29_x70.87_y493.39)
- struct[ vdec_dev_operator_t](#_page29_x70.87_y771.02)
- struct[ vision_algo_dev_operator_t](#_page30_x70.87_y240.33)
- struct[ display_dev_operator_t](#_page31_x70.87_y119.49)

**Typedefs**

- typedef int(∗[mpp_callback_t)](#_page32_x70.87_y270.88) (mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data)
- typedef int(∗[mpp_callback_t)](#_page32_x70.87_y270.88) (mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data)

**Functions**

- void[ GUI_DrawText ](#_page32_x70.87_y547.58)(uint16\_t ∗lcd\_buf, uint16\_t fcolor, uint16\_t bcolor, uint32\_t width, int x, int y, const char ∗label)
- static void[ hal_draw_pixel565 ](#_page33_x70.87_y278.65)(uint16\_t ∗pDst, uint32\_t x, uint32\_t y, uint16\_t color, uint32\_t lcd\_w)
- static void[ hal_draw_text565 ](#_page33_x70.87_y592.07)(uint16\_t ∗lcd\_buf, uint16\_t fcolor, uint16\_t bcolor, uint32\_t width, int x, int y, const char ∗label, int stripe\_top, int stripe\_bottom)
- static void[ hal_draw_rect565 ](#_page34_x70.87_y364.05)(uint16\_t ∗lcd\_buf,[ hal_rect_t ](#_page14_x70.87_y652.72)rect, mpp\_color\_t rgb, uint32\_t width, int stripe← \_top, int stripe\_bottom)
- static int[ get_bitpp ](#_page35_x70.87_y118.09)(mpp\_pixel\_format\_t type)
- void[ swap_2_bytes ](#_page35_x70.87_y251.60)(uint8\_t ∗data, int size)

#### 2.2.1 Detailed Description

This section provides the detailed documentation for the MPP HAL operations that needs to be implemented for each component.

##### 2.2.1.1 Data Structure Documentation

1. **struct<a name="_page27_x70.87_y515.04"></a> camera\_dev\_operator\_t**

Operation that needs to be implemented by a camera device.

**Data Fields**

- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[init ](#_page28_x212.37_y68.87))(camera\_dev\_t ∗dev, mpp\_camera\_params\_t ∗config,[ camera_dev_callback_t ](#_page20_x70.87_y647.27)callback, void ∗param)
- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[deinit ](#_page28_x70.87_y170.53))(camera\_dev\_t ∗dev)
- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[start ](#_page28_x70.87_y260.23))(const camera\_dev\_t ∗dev)
- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[stop ](#_page28_x70.87_y349.93))(const camera\_dev\_t ∗dev)
- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[enqueue ](#_page28_x70.87_y439.64))(const camera\_dev\_t ∗dev, void ∗data)
- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[dequeue ](#_page28_x70.87_y541.29))(const camera\_dev\_t ∗dev, void ∗∗data, int ∗stripe, int ∗compressed\_size)
- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[get_buf_desc ](#_page28_x70.87_y642.95))(const camera\_dev\_t ∗dev,[ hw_buf_desc_t ](#_page18_x70.87_y473.31)∗out\_buf,[ mpp_memory_policy_t ](#_page23_x70.87_y606.20)∗policy)
- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[lock ](#_page28_x70.87_y744.61))(const camera\_dev\_t ∗dev)
- [hal_camera_status_t(](#_page21_x70.87_y680.23)∗[unlock ](#_page29_x70.87_y107.13))(const camera\_dev\_t ∗dev)

**Field Documentation <a name=""_page28_x212.37_y68.87"></a>**

**init**

[hal_camera_status_t](#_page21_x70.87_y680.23)(∗ camera\_dev\_operator\_t::init) (camera\_dev\_t ∗dev, mpp\_camera\_params\_t ∗config, [camera_dev_callback_t](#_page20_x70.87_y647.27) callback, void ∗param)

<a name="_page28_x212.37_y68.87"></a>initialize the dev 

**deinit**

[hal_camera_status_t](#_page21_x70.87_y680.23)(∗ camera\_dev\_operator\_t::deinit) (camera\_dev\_t ∗dev)

<a name="_page28_x70.87_y170.53"></a>deinitialize the dev

**start**

[hal_camera_status_t](#_page21_x70.87_y680.23)(∗ camera\_dev\_operator\_t::start) (const camera\_dev\_t ∗dev)

<a name="_page28_x70.87_y260.23"></a>start the dev

**stop**

[hal_camera_status_t](#_page21_x70.87_y680.23)(∗ camera\_dev\_operator\_t::stop) (const camera\_dev\_t ∗dev)

stop<a name="_page28_x70.87_y349.93"></a> the dev

**enqueue**

[hal_camera_status_t(∗ ](#_page21_x70.87_y680.23)camera\_dev\_operator\_t::enqueue) (const camera\_dev\_t ∗dev, void ∗data) enqueue a buffer to the dev

**dequeue**

[hal_camera_status_t(∗ ](#_page21_x70.87_y680.23)camera\_dev\_operator\_t::dequeue) (const camera\_dev\_t ∗dev, void ∗∗data, int ∗stripe)

dequeue a buffer from the dev (blocking) <a name="_page28_x70.87_y541.29"></a>

**get\_buf\_desc**

[hal_camera_status_t(∗ ](#_page21_x70.87_y680.23)camera\_dev\_operator\_t::get\_buf\_desc) (const camera\_dev\_t ∗dev, [hw_buf_desc_t ](#_page18_x70.87_y473.31)∗out\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y606.20)∗policy)

get buffer descriptors and policy

**lock** 

[hal_camera_status_t](#_page21_x70.87_y680.23)(∗ camera\_dev\_operator\_t::lock) (const camera\_dev\_t ∗dev) lock the device for exclusive access and operations

**unlock<a name="_page29_x70.87_y107.13"></a>** 

[hal_camera_status_t](#_page21_x70.87_y680.23)(∗ camera\_dev\_operator\_t::unlock) (const camera\_dev\_← t ∗dev)

   unlock the device after exclusive operations

2. **struct<a name="_page29_x70.87_y200.41"></a><a name="_page29_x70.87_y185.80"></a> static\_image\_operator\_t**

Operation that needs to be implemented by an image element.

**Data Fields**

- [hal_image_status_t(](#_page22_x70.87_y389.53)∗[init ](#_page29_x212.37_y330.77))(static\_image\_t ∗elt, mpp\_img\_params\_t ∗config, void ∗param)
- [hal_image_status_t(](#_page22_x70.87_y389.53)∗[dequeue ](#_page29_x70.87_y414.72))(static\_image\_t ∗elt,[ hw_buf_desc_t ](#_page18_x70.87_y473.31)∗out\_buf, int ∗stripe\_num)

**Field Documentation <a name="_page29_x212.37_y330.77"></a>**

**init** 

[hal_image_status_t](#_page22_x70.87_y389.53)(∗ static\_image\_operator\_t::init) (static\_image\_t ∗elt, mpp← \_img\_params\_t ∗config, void ∗param)

   initialize the elt
   
**dequeue<a name="_page29_x70.87_y414.72"></a>** 

[hal_image_status_t](#_page22_x70.87_y389.53)(∗ static\_image\_operator\_t::dequeue) (static\_image\_← t ∗elt, [hw_buf_desc_t](#_page18_x70.87_y473.31) ∗out\_buf, int ∗stripe\_num)

   dequeue a buffer from the elt

3. **struct<a name="_page29_x70.87_y508.01"></a><a name="_page29_x70.87_y493.39"></a> gfx\_dev\_operator\_t**

Operation that needs to be implemented by gfx device.

**Data Fields**

- int(∗**init** )(gfx\_dev\_t ∗dev, void ∗param)
- int(∗**deinit** )(gfx\_dev\_t ∗dev)
- int(∗ **get\_buf\_desc** )(const gfx\_dev\_t ∗dev, [hw_buf_desc_t](#_page18_x70.87_y473.31) ∗in\_buf, [hw_buf_desc_t](#_page18_x70.87_y473.31) ∗out\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y606.20)∗policy)
- int(∗ **blit** )(const gfx\_dev\_t ∗dev, const [gfx_surface_t](#_page14_x70.87_y70.87) ∗pSrc, const [gfx_surface_t](#_page14_x70.87_y70.87) ∗pDst, const [gfx_rotate_config_t ](#_page14_x70.87_y319.68)∗pRotate, mpp\_flip\_mode\_t flip)
- int(∗**drawRect** )(const gfx\_dev\_t ∗dev,[ gfx_surface_t ](#_page14_x70.87_y70.87)∗pOverlay, int x, int y, int w, int h, int color)
- int(∗**drawPicture** )(const gfx\_dev\_t ∗dev,[ gfx_surface_t ](#_page14_x70.87_y70.87)∗pOverlay, int x, int y, int w, int h, int alpha, const char ∗pIcon)
- int(∗**drawText** )(const gfx\_dev\_t ∗dev,[ gfx_surface_t ](#_page14_x70.87_y70.87)∗pOverlay, int x, int y, int textColor, int bgColor, int type, const char ∗pText)
- int(∗**compose** )(const gfx\_dev\_t ∗dev,[ gfx_surface_t ](#_page14_x70.87_y70.87)∗pSrc,[ gfx_surface_t ](#_page14_x70.87_y70.87)∗pOverlay,[ gfx_surface_t ](#_page14_x70.87_y70.87)∗pDst, [gfx_rotate_config_t ](#_page14_x70.87_y319.68)∗pRotate, mpp\_flip\_mode\_t flip)
- int(∗**finish<a name="_page29_x70.87_y771.02"></a>** )(gfx\_dev\_t ∗dev)

4. **struct<a name="_page30_x70.87_y70.87"></a> vdec\_dev\_operator\_t**

Operation that needs to be implemented by vdec device.

**Data Fields**

- int(∗**init** )(vdec\_dev\_t ∗dev, void ∗param)
- int(∗**deinit** )(const vdec\_dev\_t ∗dev)
- int(∗ **get\_buf\_desc** )(const vdec\_dev\_t ∗dev, [hw_buf_desc_t](#_page18_x70.87_y473.31) ∗in\_buf, [hw_buf_desc_t](#_page18_x70.87_y473.31) ∗out\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y606.20)∗policy)
- int(∗**decode** )(const vdec\_dev\_t ∗dev, uint8\_t ∗pSrc, uint8\_t ∗pDst, int32\_t jpg\_size, uint32\_t row\_stride)

5. **struct<a name="_page30_x70.87_y255.57"></a><a name="_page30_x70.87_y240.33"></a> vision\_algo\_dev\_operator\_t**

Operation that needs to be implemented by a vision algorithm device.

**Data Fields**

- [hal_valgo_status_t(](#_page22_x70.87_y673.12)∗[init ](#_page30_x212.37_y454.05))(vision\_algo\_dev\_t ∗dev,[ model_param_t ](#_page15_x70.87_y286.04)∗param)
- [hal_valgo_status_t(](#_page22_x70.87_y673.12)∗[deinit ](#_page30_x70.87_y555.71))(vision\_algo\_dev\_t ∗dev)
- [hal_valgo_status_t(](#_page22_x70.87_y673.12)∗[run ](#_page30_x70.87_y657.36))(const vision\_algo\_dev\_t ∗dev, void ∗data)
- [hal_valgo_status_t(](#_page22_x70.87_y673.12)∗[get_buf_desc ](#_page30_x70.87_y759.02))(const vision\_algo\_dev\_t ∗dev,[ hw_buf_desc_t ](#_page18_x70.87_y473.31)∗in\_buf,[ mpp_memory_policy_t ](#_page23_x70.87_y606.20)∗policy)

**Field Documentation <a name="_page30_x212.37_y454.05"></a>**

**init** 

[hal_valgo_status_t](#_page22_x70.87_y673.12)(∗ vision\_algo\_dev\_operator\_t::init) (vision\_algo\_dev\_t ∗dev, [model_param_t](#_page15_x70.87_y286.04) ∗param)

   initialize the dev

**deinit<a name="_page30_x70.87_y555.71"></a>** 

[hal_valgo_status_t](#_page22_x70.87_y673.12)(∗ vision\_algo\_dev\_operator\_t::deinit) (vision\_algo\_dev\_t ∗dev)

   deinitialize the dev

**run<a name="_page30_x70.87_y657.36"></a>** 

[hal_valgo_status_t](#_page22_x70.87_y673.12)(∗ vision\_algo\_dev\_operator\_t::run) (const vision\_algo\_dev\_t ∗dev, void ∗data)

   start<a name="_page30_x70.87_y759.02"></a> the dev

**get\_buf\_desc** 

[hal_valgo_status_t](#_page22_x70.87_y673.12)(∗ vision\_algo\_dev\_operator\_t::get\_buf\_desc) (const vision\_algo\_dev\_t ∗dev, [hw_buf_desc_t](#_page18_x70.87_y473.31) ∗in\_buf, [mpp_memory_policy_t](#_page23_x70.87_y606.20) ∗policy)

   read input parameters

6. **struct<a name="_page31_x70.87_y134.73"></a><a name="_page31_x70.87_y119.49"></a> display\_dev\_operator\_t**

Operation that needs to be implemented by a display device.

**Data Fields**

- [hal_display_status_t(](#_page23_x70.87_y378.76)∗[init](#_page31_x212.37_y371.17))(display\_dev\_t ∗dev, mpp\_display\_params\_t ∗config, [mpp_callback_t ](#_page21_x70.87_y205.33)callback, void ∗user\_data)
- [hal_display_status_t(](#_page23_x70.87_y378.76)∗[deinit ](#_page31_x70.87_y472.83))(const display\_dev\_t ∗dev)
- [hal_display_status_t(](#_page23_x70.87_y378.76)∗[start ](#_page31_x70.87_y574.49))(display\_dev\_t ∗dev)
- [hal_display_status_t(](#_page23_x70.87_y378.76)∗[stop ](#_page31_x70.87_y664.19))(display\_dev\_t ∗dev)
- [hal_display_status_t(](#_page23_x70.87_y378.76)∗[blit](#_page31_x70.87_y753.89) )(const display\_dev\_t ∗dev, void ∗frame, int stripe)
- [hal_display_status_t(](#_page23_x70.87_y378.76)∗[get_buf_desc ](#_page32_x70.87_y127.15))(const display\_dev\_t ∗dev, [hw_buf_desc_t ](#_page18_x70.87_y473.31)∗in\_buf, [mpp_memory_policy_t](#_page23_x70.87_y606.20) ∗policy)

**Field Documentation <a name="_page31_x212.37_y371.17"></a>**

**init**

[hal_display_status_t](#_page23_x70.87_y378.76)(∗ display\_dev\_operator\_t::init) (display\_dev\_t ∗dev, mpp← \_display\_params\_t ∗config, [mpp_callback_t](#_page21_x70.87_y205.33) callback, void ∗user\_data)

   initialize the dev 

**deinit**

[hal_display_status_t](#_page23_x70.87_y378.76)(∗ display\_dev\_operator\_t::deinit) (const display\_dev\_t ∗dev)

   deinitialize the dev

**start**

[hal_display_status_t](#_page23_x70.87_y378.76)(∗ display\_dev\_operator\_t::start) (display\_dev\_t ∗dev) start the dev

**stop**

[hal_display_status_t](#_page23_x70.87_y378.76)(∗ display\_dev\_operator\_t::stop) (display\_dev\_t ∗dev) stop<a name="_page31_x70.87_y753.89"></a> the dev

**blit**

[hal_display_status_t](#_page23_x70.87_y378.76)(∗ display\_dev\_operator\_t::blit) (const display\_dev\_t ∗dev, void ∗frame, int stripe)

   blit a buffer to the dev

**get\_buf\_desc**

[hal_display_status_t](#_page23_x70.87_y378.76)(∗ display\_dev\_operator\_t::get\_buf\_desc) (const display\_dev\_t ∗dev, [hw_buf_desc_t](#_page18_x70.87_y473.31) ∗in\_buf, [mpp_memory_policy_t](#_page23_x70.87_y606.20) ∗policy)

   get buffer descriptors and policy

##### 2.2.1.2 Typedef Documentation

<a name="_page32_x70.87_y297.90"></a>1. **mpp\_callback\_t**

typedef int(∗ mpp\_callback\_t) (mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data) The mpp callback function prototype.

##### 2.2.1.3 Function Documentation

1. **GUI\_DrawText()**

<a name="_page32_x70.87_y574.60"></a>void GUI\_DrawText (
uint16\_t ∗ lcd\_buf,
uint16\_t fcolor,
uint16\_t bcolor,
uint32\_t width,
int x,
int y,
const char ∗ label )

Draws text stored in label pointer to LCD buffer.
This function copy content of data from label text buffer to the LCD.

**Parameters**

|name|description|
| - | - |
|lcd\_buf|LCD buffer address destination for drawing text|
|fcolor|foreground color in rgb565 format|
|bcolor|background color in rgb565 format|
|width|LCD width|
|x|drawing position on X axe|
|y|drawing position on Y axe|
|label|C string pointed by label|

**Returns**

2. **hal\_draw\_pixel565()**

<a name="_page33_x70.87_y305.84"></a><a name="_page33_x70.87_y278.65"></a>static void hal\_draw\_pixel565 (
uint16\_t ∗ pDst,
uint32\_t x,
uint32\_t y,
uint16\_t color,
uint32\_t lcd\_w ) 

Draws pixel with RGB565 color to defined point.

**Parameters**

|name|description|
| - | - |
|pDst|image data address of destination buffer|
|x|drawing position on X axe|
|y|drawing position on Y axe|
|color|RGB565 encoded value|
|lcd_w|lcd width|

3. **hal\_draw\_text565()**

<a name="_page33_x70.87_y619.27"></a><a name="_page33_x70.87_y592.07"></a>static void hal\_draw\_text565 (
uint16\_t ∗ lcd\_buf,
uint16\_t fcolor,
uint16\_t bcolor,
uint32\_t width,
int x,
int y,
const char ∗ label,
int stripe\_top,
int stripe\_bottom ) [static]

Draws text stored in label pointer to LCD buffer.
This function copy content of data from label text buffer to the LCD.

**Parameters**

|name|description|
| - | - |
|lcd\_buf|LCD buffer address destination for drawing text|
|fcolor|foreground color in rgb565 format|
|bcolor|background color in rgb565 format|
|width|LCD width|
|x|drawing position on X axe|
|y|drawing position on Y axe|
|format|C string pointed by format|

**Returns**

The return number of written chars to the buffer

4. **hal\_draw\_rect565()**

<a name="_page34_x70.87_y391.25"></a><a name="_page34_x70.87_y364.05"></a>static void hal\_draw\_rect565 (
uint16\_t ∗ lcd\_buf,
[hal_rect_t ](#_page14_x70.87_y568.99)rect,
mpp\_color\_t rgb,
uint32\_t width,
int stripe\_top,
int stripe\_bottom ) [static]

Draws rectangle.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|lcd\_buf|LCD buffer address destination for drawing rectangle|
|in|color|background color in rgb565 format|
|in|x|drawing position on X axe|
|in|y|drawing position on Y axe|
|in|xsize|rectangle width|
|in|ysize|rectangle height|
|in|r|0-255 red color value|
|in|g|0-255 green color value|
|in|b|0-255 blue color value|
|in|width|LCD width|

**Returns**

N/A

5. **get\_bitpp()<a name="_page35_x70.87_y145.28"></a><a name="_page35_x70.87_y118.09"></a>**static int get\_bitpp (

mpp\_pixel\_format\_t type ) [static]

returns the number of bits per pixel per format, unknown format return 0

6. **swap\_2\_bytes()<a name="_page35_x70.87_y278.79"></a><a name="_page35_x70.87_y251.60"></a>**void swap\_2\_bytes (

uint8\_t ∗ data,

int size )

Swaps a buffer's MSB and LSB bytes..

**Parameters**

|name|description|
| - | - |
|data|pointer to the buffer to be converted(from little endian to big endian and vice-versa).|
|size|buffer size.|

3. **HAL<a name="_page35_x70.87_y473.55"></a> Types**

**Data Structures**

- struct[ virtual_usb_cam_config_msg_t](#_page36_x70.87_y410.10)
- struct[ virtual_usb_cam_req_msg_t](#_page36_x70.87_y574.99)
- struct[ virtual_usb_cam_rsp_msg_t](#_page36_x70.87_y764.50)
- struct[ virtual_usb_cam_msg_t](#_page37_x70.87_y231.18)

**Macros**

- #define[ TARGET_CAMERA0_WIDTH](#_page37_x70.87_y482.54)
- #define **TARGET\_CAMERA0\_HEIGHT**
- #define **TARGET\_CAMERA1\_WIDTH**
- #define **TARGET\_CAMERA1\_HEIGHT**
- #define **TARGET\_CAMERA0\_RESOLUTION**
- #define **TARGET\_CAMERA1\_RESOLUTION**
- #define **TARGET\_CAMERA\_MAX\_RESOLUTION**
- #define **TARGET\_CAMERA\_FPS**
- #define **CAMERA\_DEV\_BUFFER\_ALIGN**
- #define[ CORE1_EPT_ADDRESS](#_page37_x70.87_y604.10)
- #define[ MPP_EPT_ADDRESSS](#_page37_x70.87_y725.66)
- #define[ RTSP_EPT_ADDRESS](#_page38_x369.67_y150.37)
- #define[ APP_EP_READY_EVENT_DATA](#_page38_x70.87_y271.93)

**Enumerations**

- enum[ virtual_usb_cam_msg_type_e ](#_page38_x70.87_y442.57){

  **VIRT\_USB\_CAM\_NOMSG** ,

  **VIRT\_USB\_CAM\_CONFIG**,

  **VIRT\_USB\_CAM\_CONFIG\_ACK**, **VIRT\_USB\_CAM\_CONFIG\_ERR** ,

  **VIRT\_USB\_CAM\_REQRGB** ,

  **VIRT\_USB\_CAM\_REQIR**,

  **VIRT\_USB\_CAM\_REQRGBIR** ,

  **VIRT\_USB\_CAM\_RSPRGB** ,

  **VIRT\_USB\_CAM\_RSPIR** ,

  **VIRT\_USB\_CAM\_RSPRGBIR** ,

  **VIRT\_USB\_CAM\_ERROR** }

- enum **virtual\_usb\_cam\_user\_id\_e** {

  **RTSP\_USER\_ID** ,

  **MPP\_USER\_ID** }

- enum[ virtual_usb_cam_col_format_e ](#_page38_x70.87_y564.13){ **VIRT\_USB\_CAM\_JPEG** }
1. **Detailed<a name="_page36_x70.87_y303.97"></a> Description**

This section provides the detailed documentation for the MPP HAL VIRTUAL CAMERA types.

2. **Data<a name="_page36_x70.87_y378.01"></a> Structure Documentation**
1. **struct<a name="_page36_x70.87_y410.10"></a> virtual\_usb\_cam\_config\_msg\_t**

Structure that characterizes the payload of the camera config message sent from core 0 to core 1. **Data Fields**



|uint32\_t|camera\_width|Width of the camera output in pixels.|
| - | - | - |
|uint32\_t|camera\_height|Height of the camera output in pixels.|
|[virtual_usb_cam_col_format_e](#_page38_x70.87_y564.13)|color\_format|Color format for the camera output (e.g., JPEG)|
|uint32\_t|fps|Frames per second for camera capture rate.|

2. **struct<a name="_page36_x70.87_y590.23"></a><a name="_page36_x70.87_y574.99"></a> virtual\_usb\_cam\_req\_msg\_t**

Structure that characterizes the payload of the camera request message containing frame buffer addresses for RGB and IR data.

**Data Fields**



|uint32\_t|<a name="_page36_x70.87_y764.50"></a>rgb\_frame\_addr|Physical address of the RGB frame buffer.|
| - | - | - |
|uint32\_t|rgb\_max\_frame\_size|Maximum size allocated for RGB frame buffer.|
|uint32\_t|ir\_frame\_addr|Physical address of the IR frame buffer.|
|uint32\_t|ir\_max\_frame\_size|Maximum size allocated for IR frame buffer.|

3. **struct<a name="_page37_x70.87_y70.87"></a> virtual\_usb\_cam\_rsp\_msg\_t**

Structure that characterizes the payload of the camera request message containing frame addresses and sizes for RGB and IR data.

**Data Fields**



|uint32\_t|rgb\_frame\_addr|Physical address of the RGB frame buffer (looped back by the core 1 camera app)|
| - | - | - |
|uint32\_t|rgb\_frame\_size|Size in bytes of the RGB frame data.|
|uint32\_t|ir\_frame\_addr|Physical address of the IR frame buffer (looped back by the core 1 camera app)|
|uint32\_t|ir\_frame\_size|Size in bytes of the IR frame data.|

4. **struct<a name="_page37_x70.87_y246.42"></a><a name="_page37_x70.87_y231.18"></a> virtual\_usb\_cam\_msg\_t**

Structure that characterizes the messages sent between cores. **Data Fields**



|[virtual_usb_cam_msg_type_e](#_page38_x70.87_y442.57)|msg\_type|Type of message being sent (config, request, response, etc.)|
| - | - | - |
|virtual\_usb\_cam\_user\_id\_e|user\_id|Identifier for the user/component sending the message (RTSP or MPP)|
|union[ msg_payload_u](#_page0_x0.00_y841.89)|msg\_payload|Union containing the actual message data based on msg\_type.|

3. **Macro<a name="_page37_x70.87_y435.83"></a> Definition Documentation**
1. **TARGET\_CAMERA0\_WIDTH**

<a name="_page37_x70.87_y509.74"></a><a name="_page37_x70.87_y482.54"></a>#define TARGET\_CAMERA0\_WIDTH

Buffer alignment requirement for camera device buffers in bytes.

2. **CORE1\_EPT\_ADDRESS**

<a name="_page37_x70.87_y631.30"></a><a name="_page37_x70.87_y604.10"></a>#define CORE1\_EPT\_ADDRESS

Endpoint<a name="_page37_x70.87_y725.66"></a> address for Core 1 inter-core communication channel.

3. **MPP\_EPT\_ADDRESSS**

<a name="_page38_x70.87_y70.87"></a>#define MPP\_EPT\_ADDRESSS

Endpoint address for MPP (Media Processing Pipeline) inter-core communication channel. MPP might use a range of endpoints starting with 40 and up to 49 included

4. <a name="_page38_x369.67_y150.37"></a>**RTSP\_EPT\_ADDRESS**

<a name="_page38_x70.87_y179.56"></a>#define RTSP\_EPT\_ADDRESS

Endpoint address for RTSP (Real Time Streaming Protocol) inter-core communication channel.

5. **APP\_EP\_READY\_EVENT\_DATA**

<a name="_page38_x70.87_y299.12"></a><a name="_page38_x70.87_y271.93"></a>#define APP\_EP\_READY\_EVENT\_DATA

Event data value indicating that the application endpoint is ready for communication.

4. **Enumeration<a name="_page38_x70.87_y397.85"></a> Type Documentation**
1. **virtual\_usb\_cam\_msg\_type\_e**

<a name="_page38_x70.87_y469.76"></a><a name="_page38_x70.87_y442.57"></a>enum [virtual_usb_cam_msg_type_e](#_page38_x70.87_y442.57)

Structure that characterizes the exchanged message types between core 0 and core 1.

2. **virtual\_usb\_cam\_col\_format\_e**

<a name="_page38_x70.87_y591.32"></a><a name="_page38_x70.87_y564.13"></a>enum [virtual_usb_cam_col_format_e](#_page38_x70.87_y564.13)

Structure that characterizes the color format for the camera output.

### 2.3 HAL Setup Functions

**Functions**

- int[ hal_label_rectangle ](#_page39_x70.87_y380.73)(uint8\_t ∗frame, int width, int height, mpp\_pixel\_format\_t format, mpp\_labeled\_rect\_t ∗lr, int stripe, int stripe\_max)
- int[ hal_landmark ](#_page40_x70.87_y68.87)(uint8\_t ∗frame, int width, int height, mpp\_pixel\_format\_t format, mpp\_landmark\_t ∗lk, int stripe, int stripe\_max)
- int[ hal_inference_tflite_setup ](#_page40_x70.87_y482.45)(vision\_algo\_dev\_t ∗dev)
- int[ hal_display_setup ](#_page40_x70.87_y738.34)(const char ∗name, display\_dev\_t ∗dev)
- int[ hal_camera_setup ](#_page41_x70.87_y335.15)(const char ∗name, camera\_dev\_t ∗dev)
- int[ hal_gfx_setup ](#_page41_x70.87_y617.05)(const char ∗name, gfx\_dev\_t ∗dev)
- int[ hal_img_decoder_setup ](#_page42_x70.87_y206.81)(const char ∗name, vdec\_dev\_t ∗dev)

#### 2.3.1 Detailed Description

This section provides the detailed documentation for the HAL setup functions that should be defined by each device.

##### 2.3.1.1 Function Documentation

1. **hal\_label\_rectangle()**

<a name="_page39_x70.87_y407.92"></a>int hal\_label\_rectangle (
uint8\_t ∗ frame,
int width,
int height,
mpp\_pixel\_format\_t format,
mpp\_labeled\_rect\_t ∗ lr,
int stripe,
int stripe\_max )

Implementation of hal labeled rectangle component that draws a rectangle and a text on an input image.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|frame|The buffer address|
|in|width|Image width|
|in|height|Image height|
|in|format|Image format|
|in|lr|Labeled rectangle parameters|
|in|stripe|stripe number (0=no stripe)|
|in|stripe\_max|max nb of stripes|

**Returns**

0

2. **hal\_landmark()<a name="_page40_x70.87_y96.07"></a><a name="_page40_x70.87_y68.87"></a>**int hal\_landmark (

uint8\_t ∗ frame,

int width,

int height,

mpp\_pixel\_format\_t format,

mpp\_landmark\_t ∗ lk,

int stripe,

int stripe\_max )

Implementation of hal landmark component that draws a landmark on an input image. **Parameters**



|in/out|name|description|
| - | - | - |
|in|frame|The buffer address|
|in|width|Image width|
|in|height|Image height|
|in|format|Image format|
|in|lk|landmark parameters|
|in|stripe|stripe number (0=no stripe)|
|in|stripe\_max|max nb of stripes|

**Returns**

0

3. **hal\_inference\_tflite\_setup()**

<a name="_page40_x70.87_y509.65"></a><a name="_page40_x70.87_y482.45"></a>int hal\_inference\_tflite\_setup (
vision\_algo\_dev\_t ∗ dev )

Hal setup function for inference engine Tensorflow-Lite Micro.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|dev|vision algo device to register|

**Returns**

error code (0: success, otherwise: failure)

4. **hal\_display\_setup()<a name="_page41_x70.87_y70.87"></a>**

int hal\_display\_setup (
const char ∗ name,
display\_dev\_t ∗ dev )

Register with a display device specified by name. If name is NULL, return error.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|name|display name|
|in|dev|display device to register|

**Returns**

error code (0: success, otherwise: failure)

5. **hal\_camera\_setup()<a name="_page41_x70.87_y362.34"></a><a name="_page41_x70.87_y335.15"></a>**

int hal\_camera\_setup ( const char ∗ name, camera\_dev\_t ∗ dev )

Register with a camera device specified by name. If name is NULL, return error.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|name|camera name|
|in|dev|camera device to register|

**Returns**

error code (0: success, otherwise: failure)

6. **hal\_gfx\_setup()<a name="_page41_x70.87_y643.55"></a><a name="_page41_x70.87_y617.05"></a>**

int hal\_gfx\_setup ( const char ∗ name, gfx\_dev\_t ∗ dev )

Register with a graphic processing device specified by name.

If name is NULL, the first available graphic processing supported by Hw will be selected. The graphic device using CPU operations will be selected if name is not specified and if no graphic processing is available for the Hw.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|name|graphic processing device performing the image conversion|
|in|dev|graphic device to register|

**Returns**

error code (0: success, otherwise: failure)

7. **hal\_img\_decoder\_setup()**

<a name="_page42_x70.87_y234.01"></a><a name="_page42_x70.87_y206.81"></a>int hal\_img\_decoder\_setup ( const char ∗ name, vdec\_dev\_t ∗ dev )

Register with an image decoder device specified by name.

If name is NULL, the first available decoder supported by Hw will be selected. The decoder device using CPU operations will be selected if name is not specified and if no decoder is available for the Hw.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|name|image decoding device name|
|in|dev|decoder device to register|

**Returns**

error code (0: success, otherwise: failure)

