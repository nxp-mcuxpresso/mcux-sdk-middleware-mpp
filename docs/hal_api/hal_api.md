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
Graphics:
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

- struct [camera_dev_static_config_t](#_page12_x70.87_y161.91)
- struct [camera_dev_private_capability_t](#_page12_x70.87_y466.87)
- struct [camera_dev_t](#_page12_x70.87_y615.19)
- struct [static_image_static_config_t](#_page13_x70.87_y202.98)
- struct [static_image_t](#_page13_x70.87_y439.48)
- struct [gfx_surface_t](#_page13_x70.87_y633.89)
- struct [gfx_rotate_config_t](#_page14_x70.87_y235.94)
- struct [gfx_dev_t](#_page14_x70.87_y386.90)
- struct [hal_rect_t](#_page14_x70.87_y568.99)
- struct [model_param_t](#_page14_x70.87_y731.01)
- struct [valgo_dev_private_capability_t](#_page16_x70.87_y702.04)
- struct [vision_frame_t](#_page17_x70.87_y121.40)
- struct [vision_algo_dev_t](#_page17_x70.87_y314.97)
- struct [display_dev_private_capability_t](#_page17_x70.87_y543.04)
- struct [display_dev_t](#_page18_x70.87_y193.84)
- struct [hw_buf_desc_t](#_page18_x70.87_y458.65)
- struct [hal_img_decoder_setup_t](#_page18_x70.87_y709.18)
- struct [hal_graphics_setup_t](#_page18_x70.87_y679.87)
- struct [hal_display_setup_t](#_page19_x70.87_y70.87)
- struct [hal_camera_setup_t](#_page19_x70.87_y166.55)
- struct [checksum_data_t](#_page19_x70.87_y276.28)

**Macros**

- #define [HAL_GFX_DEV_CPU_NAME](#_page19_x70.87_y458.22)
- #define [GUI_PRINTF_BUF_SIZE](#_page19_x397.39_y572.15)
- #define [GUI_PRINTF_BUF_SIZE](#_page19_x397.39_y572.15)
- #define [MAX_INPUT_PORTS](#_page19_x70.87_y771.13)
- #define MAX\_OUTPUT\_PORTS
- #define [HAL_DEVICE_NAME_MAX_LENGTH](#_page20_x247.59_y150.37)

**Typedefs**

- typedef int(∗ [camera_dev_callback_t) (const](#_page20_x70.87_y310.38) camera\_dev\_t ∗dev, [camera_event_t ev](#_page21_x70.87_y469.44)ent, void ∗param, uint8\_t fromISR)
- typedef void ∗**vision\_algo\_private\_data\_t**
- typedef int(∗[mpp_callback_t) ](#_page20_x70.87_y569.51)(mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data)
- typedef int(∗[img_decoder_setup_func_t) (vdec_de](#_page21_x70.87_y213.17)v\_t ∗)
- typedef int(∗[graphic_setup_func_t) (gfx_de](#_page20_x70.87_y666.24)v\_t ∗)
- typedef int(∗[display_setup_func_t) (displa](#_page20_x70.87_y763.09)y\_dev\_t ∗)
- typedef int(∗[camera_setup_func_t) (const](#_page21_x70.87_y139.39) char ∗, camera\_dev\_t ∗)

**Enumerations**

- enum [hal_camera_status_t](#_page21_x70.87_y285.55) {

  [kStatus_HAL_CameraSuccess](#_page21_x93.51_y392.29),
  
  [kStatus_HAL_CameraBusy](#_page21_x107.46_y406.03),
  
  [kStatus_HAL_CameraNonBlocking](#_page21_x77.24_y420.58),
  
  [kStatus_HAL_CameraError](#_page21_x107.47_y435.24) }
 
- enum [camera_event_t](#_page21_x70.87_y469.44) {

  [kCameraEvent_SendFrame](#_page21_x101.78_y595.45),
  
  [kCameraEvent_CameraDeviceInit](#_page21_x77.24_y609.19) }

- enum [hal_image_status_t](#_page21_x70.87_y644.20) {

  [MPP_kStatus_HAL_ImageSuccess](#_page22_x77.24_y86.20),
  
  [MPP_kStatus_HAL_ImageError](#_page22_x91.20_y100.86) }

- enum [gfx_rotate_target_t](#_page22_x70.87_y135.97) {

  **kGFXRotateTarget\_None**, 
  
  **kGFXRotate\_SRCSurface**, 
  
  **kGFXRotate\_DSTSurface** }

- enum [hal_valgo_status_t](#_page22_x70.87_y234.70) {

  [kStatus_HAL_ValgoSuccess](#_page22_x89.19_y358.72),
  
  [kStatus_HAL_ValgoMallocError](#_page22_x77.24_y373.38),
  
  [kStatus_HAL_ValgoInitError](#_page22_x91.19_y388.04),
  
  [kStatus_HAL_ValgoError](#_page22_x103.15_y402.70),
  
  [kStatus_HAL_ValgoStop](#_page22_x104.63_y417.35) }

- enum [display_event_t](#_page22_x70.87_y452.47) { 

  [kDisplayEvent_RequestFrame](#_page22_x77.24_y578.49) }

- enum [hal_display_status_t](#_page22_x70.87_y613.60) {

  [kStatus_HAL_DisplaySuccess](#_page22_x93.51_y739.51),
  
  [kStatus_HAL_DisplayTxBusy](#_page22_x97.50_y754.06),
  
  [kStatus_HAL_DisplayNonBlocking](#_page22_x77.24_y768.61),
  
  [kStatus_HAL_DisplayError](#_page22_x107.47_y783.27) }

- enum [mpp_memory_policy_t](#_page23_x70.87_y70.87) {
  
  [HAL_MEM_ALLOC_NONE ](#_page23_x88.20_y215.41), 
  
  [HAL_MEM_ALLOC_INPUT ](#_page23_x87.20_y229.96), 
  
  [HAL_MEM_ALLOC_OUTPUT ](#_page23_x77.24_y256.46), 
  
  [HAL_MEM_ALLOC_BOTH ](#_page23_x89.55_y282.97) }

- enum [checksum_type_t](#_page23_x70.87_y317.98) {

  [CHECKSUM_TYPE_PISANO](#_page23_x99.65_y442.10),
  
  [CHECKSUM_TYPE_CRC_ELCDIF](#_page23_x77.24_y456.76) }

**Functions**

- int[ HAL_GfxDev_CPU_Register ](#_page23_x70.87_y540.99)(gfx\_dev\_t ∗dev)
- int[ HAL_GfxDev_GPU_Register ](#_page23_x70.87_y758.56)(gfx\_dev\_t ∗dev)
- int **setup\_static\_image\_elt** (static\_image\_t ∗elt)
- uint32\_t **calc\_checksum** (int size\_b, void ∗pbuf)

#### 2.1.1 Detailed Description

This section provides the detailed documentation for the MPP HAL types.

##### 2.1.1.1 Data Structure Documentation

1. **struct<a name="_page12_x70.87_y161.91"></a> camera\_dev\_static\_config\_t**

Structure that characterizes the camera device. 

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
|mpp\_rotate\_degree\_t|rotate|rotate degree|
|mpp\_flip\_mode\_t|flip|flip|
|int|swapByte|swap byte per two bytes|
|mpp\_pixel\_format\_t|format|pixel format|
|int|framerate|frame rate|
|int|stripe\_size|stripe size in bytes|
|bool|stripe|stripe mode|

2. **struct<a name="_page12_x70.87_y482.11"></a><a name="_page12_x70.87_y466.87"></a> camera\_dev\_private\_capability\_t**

camera device private capability. 

**Data Fields**

|type|name|description|
|-|-|-|
|[camera_dev_callback_t](#_page20_x70.87_y310.38)|callback|callback|
|void ∗|param|parameter for the callback|

3. **struct<a name="_page12_x70.87_y630.07"></a><a name="_page12_x70.87_y615.19"></a> \_camera\_dev**

Attributes of a camera device. hal camera device declaration.

Camera devices can enqueue and dequeue frames as well as react to events from input devices via the "input← Notify" function. Camera devices can use any number of interfaces, including MIPI and CSI as long as the HAL driver implements the necessary functions found in [camera_dev_operator_t. Examples](#_page25_x70.87_y88.00) of camera devices include the Orbbec U1S 3D SLM camera module and the OnSemi MT9M114 camera module.

**Data Fields**

|type|name|description|
|-|-|-|
|int|id|unique id which is assigned by camera manager during registration|
|char|name[[HAL_DEVICE_NAME_MAX_L](#_page20_x247.59_y150.37)|[ENGTHname of\]](#_page20_x247.59_y150.37) the device|
|const [camera_dev_operator_t ](#_page25_x70.87_y88.00)∗|ops|operations|
|[camera_dev_static_config_t](#_page12_x70.87_y161.91)|config|static configurations|
|[camera_dev_private_capability_t](#_page12_x70.87_y466.87)|cap|private capability|

4. **struct<a name="_page13_x70.87_y218.22"></a><a name="_page13_x70.87_y202.98"></a> static\_image\_static\_config\_t**

Structure that characterize the image element. 

**Data Fields**

|type|name|description|
|-|-|-|
|int|height|buffer height|
|int|width|buffer width|
|int|left|left position|
|int|top|top position|
|int|right|right position|
|int|bottom|bottom position|
|mpp\_pixel\_format\_t|format|pixel format|
|bool|stripe|stripe mode|
|int|compressed\_size|compressed size in bytes|

5. **struct<a name="_page13_x70.87_y454.72"></a><a name="_page13_x70.87_y439.48"></a> \_static\_image**

Attributes of an image element.

**Data Fields**

|type|name|description|
|-|-|-|
|int|id|unique id which is assigned by image manager|
|const [static_image_operator_t ](#_page26_x70.87_y356.88)∗|ops|operations|
|[static_image_static_config_t](#_page13_x70.87_y202.98)|config|static configs|
|int|stripe\_idx|the current stripe index|
|uint8\_t ∗|buffer|static image buffer|

6. **struct<a name="_page13_x70.87_y649.13"></a><a name="_page13_x70.87_y633.89"></a> gfx\_surface\_t**

Gfx surface parameters.

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
|int|swapByte|swap byte per two bytes|
|mpp\_pixel\_format\_t|format|pixel format|
|void ∗|buf|buffer|
|void ∗|lock|the structure is determined by hal and set to null if not use in hal|

7. **struct<a name="_page14_x70.87_y251.19"></a><a name="_page14_x70.87_y235.94"></a> gfx\_rotate\_config\_t**

gfx rotate configuration 

**Data Fields**

|type|name|description|
|-|-|-|
|[gfx_rotate_target_t](#_page22_x70.87_y135.97)|target||
|mpp\_rotate\_degree\_t|degree||

8. **struct<a name="_page14_x70.87_y402.14"></a><a name="_page14_x70.87_y386.90"></a> \_gfx\_dev**

**Data Fields**

|type|name|description|
|-|-|-|
|int|id||
|const [gfx_dev_operator_t ](#_page26_x70.87_y757.75)∗|ops||
|[gfx_surface_t](#_page13_x70.87_y633.89)|src||
|[gfx_surface_t](#_page13_x70.87_y633.89)|dst||
|[mpp_callback_t](#_page29_x70.87_y406.20)|callback||
|void ∗|user\_data||

9. **struct<a name="_page14_x70.87_y568.99"></a> hal\_rect\_t**

rectangle positions.

**Data Fields**

|type|name|description|
|-|-|-|
|int|<a name="_page14_x70.87_y731.01"></a>top||
|int|left||
|int|bottom||
|int|right||

10. **struct<a name="_page15_x70.87_y70.87"></a> model\_param\_t**

Structure passed to HAL as description of the binary model provided by user.

**Data Fields**

- const void ∗[model_data](#_page15_x70.87_y369.23)
- int[ model_size](#_page15_x70.87_y452.50)
- float [model_input_mean](#_page15_x70.87_y549.24)
- float [model_input_std](#_page15_x70.87_y645.97)
- mpp\_inference\_params\_t [inference_params](#_page15_x70.87_y742.71)
- int[ height](#_page16_x70.87_y127.74)
- int[ width](#_page16_x70.87_y206.48)
- mpp\_pixel\_format\_t f[ormat](#_page16_x70.87_y295.06)
- mpp\_tensor\_type\_t [inputType](#_page16_x70.87_y375.58)
- mpp\_tensor\_order\_t [tensor_order](#_page16_x70.87_y454.20)
- int(∗[evt_callback_f ](#_page16_x70.87_y531.05))(mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data)
- void ∗[cb_userdata](#_page16_x70.87_y623.41)

**Field Documentation** 

<a name="_page15_x70.87_y369.23"></a>**model\_data**

const void∗ model\_param\_t::model\_data pointer to model binary

<a name="_page15_x70.87_y452.50"></a>**model\_size**

int model\_param\_t::model\_size model binary size

<a name="_page15_x70.87_y549.24"></a>**model\_input\_mean**

float model\_param\_t::model\_input\_mean

model 'mean' of input values, used for normalization

<a name="_page15_x70.87_y645.97"></a>**model\_input\_std**

float model\_param\_t::model\_input\_std

model<a name="_page15_x70.87_y742.71"></a> 'standard deviation' of input values, used for normalization

**inference\_params**

mpp\_inference\_params\_t model\_param\_t::inference\_params inference parameters

<a name="_page16_x70.87_y127.74"></a>**height**

int model\_param\_t::height frame height

<a name="_page16_x70.87_y206.48"></a>**width**

int model\_param\_t::width frame width

<a name="_page16_x70.87_y295.06"></a>**format**

mpp\_pixel\_format\_t model\_param\_t::format pixel format

<a name="_page16_x70.87_y375.58"></a>**inputType**

mpp\_tensor\_type\_t model\_param\_t::inputType input type

<a name="_page16_x70.87_y454.20"></a>**tensor\_order**

mpp\_tensor\_order\_t model\_param\_t::tensor\_order <a name="_page16_x70.87_y531.05"></a>tensor order

**evt\_callback\_f**

int(∗ model\_param\_t::evt\_callback\_f) (mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data)

the callback to be called when model output is ready

<a name="_page16_x70.87_y623.41"></a>**cb\_userdata**

void∗ model\_param\_t::cb\_userdata

pointer to user data, should be passed by callback

11. **struct<a name="_page16_x70.87_y716.64"></a><a name="_page16_x70.87_y702.04"></a> valgo\_dev\_private\_capability\_t** 

Valgo devices private capability.

**Data Fields**

|type|name|description|
|-|-|-|
|void ∗|param|param for the callback|

12. **struct<a name="_page17_x70.87_y136.64"></a><a name="_page17_x70.87_y121.40"></a> vision\_frame\_t**

Characteristics that need to be defined by a vision algo.

**Data Fields**

|type|name|description|
|-|-|-|
|int|height|frame height|
|int|width|frame width|
|int|pitch|frame pitch|
|mpp\_pixel\_format\_t|format|pixel format|
|void ∗|input\_buf|pixel input buffer|

13. **struct<a name="_page17_x70.87_y330.21"></a><a name="_page17_x70.87_y314.97"></a> \_vision\_algo\_dev**

Attributes of a vision algo device.

**Data Fields**

|type|name|description|
|-|-|-|
|int|id|unique id which is assigned by algorithm manager during the registration|
|char|name[[HAL_DEVICE_NAME_MAX_L](#_page20_x247.59_y150.37)|[ENGTHname to\]](#_page20_x247.59_y150.37) identify|
|[valgo_dev_private_capability_t](#_page16_x70.87_y702.04)|cap|private capability|
|<p>const [vision_algo_dev_operator_t](#_page27_x70.87_y341.95)</p><p>∗</p>|ops|operations|
|vision\_algo\_private\_data\_t|priv\_data|private data|

14. **struct<a name="_page17_x70.87_y558.28"></a><a name="_page17_x70.87_y543.04"></a> \_display\_dev\_private\_capability**

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

15. **struct<a name="_page18_x70.87_y209.08"></a><a name="_page18_x70.87_y193.84"></a> \_display\_dev**

Attributes of a display device. hal display device declaration.

Display devices can be used to display images, GUI overlays, etc. Examples of display devices include display panels like the RK024hh298 display, and external displays like UVC (video over USB).

**Data Fields**

|type|name|description|
|-|-|-|
|int|id|unique id which is assigned by the display manager during the registration|
|char|name[[HAL_DEVICE_NAME_MAX_L](#_page20_x247.59_y150.37)|[ENGTHname of\]](#_page20_x247.59_y150.37) the device|
|const [display_dev_operator_t ](#_page28_x70.87_y247.77)∗|ops|operations|
|display\_dev\_private\_capability\_t|cap|private capability|

16. **struct<a name="_page18_x70.87_y473.89"></a><a name="_page18_x70.87_y458.65"></a> hw\_buf\_desc\_t**

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

17. **struct<a name="_page18_x70.87_y724.42"></a><a name="_page18_x70.87_y709.18"></a> hal\_img\_decoder\_setup\_t!**

**Data Fields**

|type|name|description|
|-|-|-|
|const char ∗|vdec\_dev\_name||
|[img_decoder_setup_func_t](#_page21_x70.87_y213.17)|decoder\_setup\_func||

18. **struct<a name="_page18_x70.87_y695.11"></a><a name="_page18_x70.87_y679.87"></a> hal\_graphics\_setup\_t**

**Data Fields**

|type|name|description|
|-|-|-|
|const char ∗|gfx\_dev\_name||
|[graphic_setup_func_t](#_page21_x70.87_y150.37)|gfx\_setup\_func||

19. **struct<a name="_page19_x70.87_y70.87"></a> hal\_display\_setup\_t**

**Data Fields**

|type|name|description|
|-|-|-|
|const char ∗|display\_name||
|[display_setup_func_t](#_page20_x70.87_y763.09)|display\_setup\_func||

20. **struct<a name="_page19_x70.87_y166.55"></a> hal\_camera\_setup\_t**

**Data Fields**

|type|name|description|
|-|-|-|
|const char ∗|camera\_name||
|[camera_setup_func_t](#_page21_x70.87_y139.39)|camera\_setup\_func||

21. **struct<a name="_page19_x70.87_y276.28"></a> checksum\_data\_t**

computed checksum

**Data Fields**

|type|name|description|
|-|-|-|
|[checksum_type_t](#_page23_x70.87_y317.98)|type|checksum calculation method|
|uint32\_t|value|checksum value|

##### 2.1.1.2 Macro Definition Documentation

1. **HAL\_GFX\_DEV\_CPU\_NAME**

<a name="_page19_x70.87_y458.22"></a>#define HAL\_GFX\_DEV\_CPU\_NAME hal graphics (gfx) device declaration.

Graphics processing devices can be used to perform conversion from one image format to another, resize images and compose images on top of one another. Examples of graphics devices include<a name="_page19_x397.39_y572.15"></a> the PXP (pixel pipeline) found on many i.MXRT series MCUs. Name of the graphic device using CPU operations

2. **GUI\_PRINTF\_BUF\_SIZE<a name="_page19_x70.87_y601.22"></a> [1/2]**

#define GUI\_PRINTF\_BUF\_SIZE Local text buffer size.

3. **GUI\_PRINTF\_BUF\_SIZE<a name="_page19_x70.87_y692.79"></a> [2/2]**

#define GUI\_PRINTF\_BUF\_SIZE <a name="_page19_x70.87_y771.13"></a>Local text buffer size.

4. **MAX\_INPUT\_PORTS**

<a name="_page20_x70.87_y70.87"></a>#define MAX\_INPUT\_PORTS

HAL public types header.

maximum number of element inputs/outputs

5. <a name="_page20_x247.59_y150.37"></a>**HAL\_DEVICE\_NAME\_MAX\_LENGTH**

<a name="_page20_x70.87_y179.45"></a>#define HAL\_DEVICE\_NAME\_MAX\_LENGTH maximum length of device name

##### 2.1.1.3 Typedef Documentation

1. **camera\_dev\_callback\_t**

<a name="_page20_x70.87_y310.38"></a>typedef int(∗ camera\_dev\_callback\_t) (const camera\_dev\_t ∗dev, [camera_event_t event, void ](#_page21_x70.87_y469.44)∗param, uint8\_t fromISR)

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

<a name="_page20_x70.87_y584.75"></a><a name="_page20_x70.87_y569.51"></a>typedef int(∗ mpp\_callback\_t) (mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data) The mpp callback function prototype.

3. **img\_decoder\_setup\_func\_t**

<a name="_page21_x70.87_y239.85"></a><a name="_page21_x70.87_y213.17"></a>typedef int(∗ img\_decoder\_setup\_func\_t) (vdec\_dev\_t ∗) video decoder setup

4. **graphic\_setup\_func\_t**

<a name="_page20_x70.87_y681.48"></a><a name="_page20_x70.87_y666.24"></a>typedef int(∗ graphic\_setup\_func\_t) (gfx\_dev\_t ∗) graphics<a name="_page20_x70.87_y763.09"></a> setup

5. **display\_setup\_func\_t**

<a name="_page21_x70.87_y70.87"></a>typedef int(∗ display\_setup\_func\_t) (display\_dev\_t ∗) display setup

6. **camera\_setup\_func\_t**

<a name="_page21_x70.87_y154.63"></a><a name="_page21_x70.87_y139.39"></a>typedef int(∗ camera\_setup\_func\_t) (const char ∗, camera\_dev\_t ∗) camera setup

##### 2.1.1.4 Enumeration Type Documentation

1. **hal\_camera\_status\_t**

<a name="_page21_x70.87_y285.55"></a>enum [hal_camera_status_t ](#_page21_x70.87_y285.55)Camera return status.

**Enumerator**

|label|description|
|-|-|
|<a name="_page21_x93.51_y392.29"></a>kStatus\_HAL\_CameraSuccess|HAL camera successful.|
|<a name="_page21_x107.46_y406.03"></a>kStatus\_HAL\_CameraBusy|Camera is busy.|
|<a name="_page21_x77.24_y420.58"></a>kStatus\_HAL\_CameraNonBlocking|Camera will return immediately.|
|<a name="_page21_x107.47_y435.24"></a>kStatus\_HAL\_CameraError|Error occurs on HAL Camera.|

2. **camera\_event\_t**

<a name="_page21_x70.87_y484.68"></a><a name="_page21_x70.87_y469.44"></a>enum [camera_event_t](#_page21_x70.87_y469.44)

Type of events that are supported by calling the callback function.

**Enumerator**

|label|description|
|-|-|
|<a name="_page21_x101.78_y595.45"></a>kCameraEvent\_SendFrame|Camera new frame is available.|
|<a name="_page21_x77.24_y609.19"></a>kCameraEvent\_CameraDeviceInit|Camera device finished the initialization process.|

3. **hal\_image\_status\_t**

<a name="_page21_x70.87_y659.44"></a><a name="_page21_x70.87_y644.20"></a>enum [hal_image_status_t ](#_page21_x70.87_y644.20)static image return status

**Enumerator**

|label|description|
|-|-|
|<a name="_page22_x77.24_y86.20"></a>MPP\_kStatus\_HAL\_ImageSuccess|Successfully.|
|<a name="_page22_x91.20_y100.86"></a>MPP\_kStatus\_HAL\_ImageError|Error occurs on HAL Image.|

4. **gfx\_rotate\_target\_t**

<a name="_page22_x70.87_y151.21"></a><a name="_page22_x70.87_y135.97"></a>enum [gfx_rotate_target_t ](#_page22_x70.87_y135.97)gfx rotate target

5. **hal\_valgo\_status\_t**

<a name="_page22_x70.87_y249.94"></a><a name="_page22_x70.87_y234.70"></a>enum [hal_valgo_status_t](#_page22_x70.87_y234.70)

Valgo Error codes for hal operations.

**Enumerator**

|label|description|
|-|-|
|<a name="_page22_x89.19_y358.72"></a>kStatus\_HAL\_ValgoSuccess|Successfully.|
|<a name="_page22_x77.24_y373.38"></a>kStatus\_HAL\_ValgoMallocError|memory allocation failed for HAL algorithm|
|<a name="_page22_x91.19_y388.04"></a>kStatus\_HAL\_ValgoInitError|algorithm initialization error|
|<a name="_page22_x103.15_y402.70"></a>kStatus\_HAL\_ValgoError|Error occurs in HAL algorithm|
|<a name="_page22_x104.63_y417.35"></a>kStatus\_HAL\_ValgoStop|HAL algorithm stop|

6. **display\_event\_t**

<a name="_page22_x70.87_y467.71"></a><a name="_page22_x70.87_y452.47"></a>enum [display_event_t](#_page22_x70.87_y452.47)

Type of events that are supported by calling the callback function.

**Enumerator**

|label|description|
|-|-|
|<a name="_page22_x77.24_y578.49"></a>kDisplayEvent\_RequestFrame|Display finished sending the frame asynchronously, provide another frame.|

7. **hal\_display\_status\_t**

<a name="_page22_x70.87_y628.84"></a><a name="_page22_x70.87_y613.60"></a>enum [hal_display_status_t ](#_page22_x70.87_y613.60)Error codes for display hal devices.

**Enumerator**

|label|description|
|-|-|
|<a name="_page22_x93.51_y739.51"></a>kStatus\_HAL\_DisplaySuccess|HAL display successful.|
|<a name="_page22_x97.50_y754.06"></a>kStatus\_HAL\_DisplayTxBusy|Display tx is busy.|
|<a name="_page22_x77.24_y768.61"></a>kStatus\_HAL\_DisplayNonBlocking|Display will return immediately.|
|<a name="_page22_x107.47_y783.27"></a>kStatus\_HAL\_DisplayError|Error occurs on HAL Display.|

8. **mpp\_memory\_policy\_t**

<a name="_page23_x70.87_y70.87"></a>enum [mpp_memory_policy_t](#_page23_x70.87_y70.87)

The memory allocation policy of an element's hal.

During the pipeline construction, the HAL uses this enum to tell the pipeline if it already owns input/ouput buffers. Before the pipeline starts, the memory manager will map the existing buffers to elements and allocate missing buffers from the heap.

**Enumerator**

|label|description|
|-|-|
|<a name="_page23_x88.20_y215.41"></a>HAL\_MEM\_ALLOC\_NONE|element requires buffers to be provided by other elements, or by the pipeline|
|<a name="_page23_x87.20_y229.96"></a>HAL\_MEM\_ALLOC\_INPUT|element allocates its input buffer, it may require output buffers to be provided by other elements, or by the pipeline|
|<a name="_page23_x77.24_y256.46"></a>HAL\_MEM\_ALLOC\_OUTPUT|element allocates its output buffer, it may require input buffers to be provided by other elements, or by the pipeline|
|<a name="_page23_x89.55_y282.97"></a>HAL\_MEM\_ALLOC\_BOTH|element allocates both its input and output buffers|

9. **checksum\_type\_t**

<a name="_page23_x70.87_y333.22"></a><a name="_page23_x70.87_y317.98"></a>enum [checksum_type_t ](#_page23_x70.87_y317.98)checksum calculation method

**Enumerator**

|label|description|
|-|-|
|<a name="_page23_x99.65_y442.10"></a>CHECKSUM\_TYPE\_PISANO|checksum computed using Pisano|
|<a name="_page23_x77.24_y456.76"></a>CHECKSUM\_TYPE\_CRC\_ELCDIF|checksum computed CRC from ELCDIF|

##### 2.1.1.5 Function Documentation

1. **HAL\_GfxDev\_CPU\_Register()**

<a name="_page23_x70.87_y540.99"></a>int HAL\_GfxDev\_CPU\_Register ( gfx\_dev\_t ∗ dev )

Register the graphic device with the CPU operations.

**Parameters**

|in/out|name|description|
|-|-|-|
|in|dev|graphic device to register|

**Returns**

error<a name="_page23_x70.87_y758.56"></a> code (0: success, otherwise: failure)

2. **HAL\_GfxDev\_GPU\_Register()**

<a name="_page24_x70.87_y70.87"></a>int HAL\_GfxDev\_GPU\_Register ( gfx\_dev\_t ∗ dev )

Register the graphic device with the GPU operations.

**Parameters**

|in/out|name|description|
|-|-|-|
|in|dev|graphic device to register|

**Returns**

error code (0: success, otherwise: failure)

### 2.2 HAL OPERATIONS

**Data Structures**

- struct [camera_dev_operator_t](#_page25_x70.87_y88.00)
- struct [static_image_operator_t](#_page26_x70.87_y356.88)
- struct [gfx_dev_operator_t](#_page26_x70.87_y757.75)
- struct [vision_algo_dev_operator_t](#_page27_x70.87_y341.95)
- struct [display_dev_operator_t](#_page28_x70.87_y247.77)

**Typedefs**

- typedef int(∗[mpp_callback_t) ](#_page29_x70.87_y406.20)(mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data)

**Functions**

- void [GUI_DrawText ](#_page29_x70.87_y534.69)(uint16\_t ∗lcd\_buf, uint16\_t fcolor, uint16\_t bcolor, uint32\_t width, int x, int y, const char ∗label)
- static void [hal_draw_pixel565 ](#_page30_x70.87_y105.33)(uint16\_t ∗pDst, uint32\_t x, uint32\_t y, uint16\_t color, uint32\_t lcd\_w)
- static void [hal_draw_text565 ](#_page30_x70.87_y393.83)(uint16\_t ∗lcd\_buf, uint16\_t fcolor, uint16\_t bcolor, uint32\_t width, int x, int y, const char ∗label, int stripe\_top, int stripe\_bottom)
- static void [hal_draw_rect565 (uint16_t](#_page31_x70.87_y105.33) ∗lcd\_buf, [hal_rect_t rect,](#_page14_x70.87_y568.99) mpp\_color\_t rgb, uint32\_t width, int stripe← \_top, int stripe\_bottom)
- static int [get_bitpp ](#_page31_x70.87_y523.19)(mpp\_pixel\_format\_t type)
- void [swap_2_bytes ](#_page31_x70.87_y633.59)(uint8\_t ∗data, int size)

#### 2.2.1 Detailed Description

This section provides the detailed documentation for the MPP HAL operations that needs to be implemented for each component.

##### 2.2.1.1 Data Structure Documentation

1. **struct<a name="_page25_x70.87_y88.00"></a> camera\_dev\_operator\_t**

Operation that needs to be implemented by a camera device.

**Data Fields**

- [hal_camera_status_t](#_page21_x70.87_y285.55)([∗init](#_page25_x70.87_y360.24))(camera\_dev\_t ∗dev, mpp\_camera\_params\_t ∗config, [camera_dev_callback_t ](#_page20_x70.87_y310.38)callback, void ∗param)
- [hal_camera_status_t](#_page21_x70.87_y285.55)([∗deinit](#_page25_x70.87_y453.68))(camera\_dev\_t ∗dev)
- [hal_camera_status_t](#_page21_x70.87_y285.55)([∗start](#_page25_x70.87_y550.42))(const camera\_dev\_t ∗dev)
- [hal_camera_status_t](#_page21_x70.87_y285.55)([∗stop](#_page25_x70.87_y647.15))(const camera\_dev\_t ∗dev)
- [hal_camera_status_t](#_page21_x70.87_y285.55)([∗enqueue](#_page25_x70.87_y745.67))(const camera\_dev\_t ∗dev, void ∗data)
- [hal_camera_status_t](#_page21_x70.87_y285.55)([∗dequeue](#_page26_x70.87_y139.39))(const camera\_dev\_t ∗dev, void ∗∗data, int ∗stripe)
- [hal_camera_status_t](#_page21_x70.87_y285.55)([∗get_buf_desc](#_page26_x70.87_y248.19))(const camera\_dev\_t ∗dev, [hw_buf_desc_t ∗](#_page18_x70.87_y458.65)out\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y70.87)∗policy)

**Field Documentation <a name="_page25_x70.87_y360.24"></a>**

**init**

[hal_camera_status_t](#_page21_x70.87_y285.55)(∗ camera\_dev\_operator\_t::init) (camera\_dev\_t ∗dev, mpp\_camera\_params\_t ∗config, [camera_dev_callback_t](#_page20_x70.87_y310.38) callback, void ∗param)

<a name="_page25_x70.87_y453.68"></a>initialize the dev 

**deinit**

[hal_camera_status_t](#_page21_x70.87_y285.55)(∗ camera\_dev\_operator\_t::deinit) (camera\_dev\_t ∗dev)

<a name="_page25_x70.87_y550.42"></a>deinitialize the dev

**start**

[hal_camera_status_t](#_page21_x70.87_y285.55)(∗ camera\_dev\_operator\_t::start) (const camera\_dev\_t ∗dev)

<a name="_page25_x70.87_y647.15"></a>start the dev

**stop**

[hal_camera_status_t](#_page21_x70.87_y285.55)(∗ camera\_dev\_operator\_t::stop) (const camera\_dev\_t ∗dev)

stop<a name="_page25_x70.87_y745.67"></a> the dev

**enqueue**

[hal_camera_status_t(∗ ](#_page21_x70.87_y285.55)camera\_dev\_operator\_t::enqueue) (const camera\_dev\_t ∗dev, void ∗data) enqueue a buffer to the dev

**dequeue**

[hal_camera_status_t(∗ ](#_page21_x70.87_y285.55)camera\_dev\_operator\_t::dequeue) (const camera\_dev\_t ∗dev, void ∗∗data, int ∗stripe)

dequeue a buffer from the dev (blocking) <a name="_page26_x70.87_y248.19"></a>

**get\_buf\_desc**

[hal_camera_status_t(∗ ](#_page21_x70.87_y285.55)camera\_dev\_operator\_t::get\_buf\_desc) (const camera\_dev\_t ∗dev, [hw_buf_desc_t ](#_page18_x70.87_y458.65)∗out\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y70.87)∗policy)

get buffer descriptors and policy

2. **struct<a name="_page26_x70.87_y372.12"></a><a name="_page26_x70.87_y356.88"></a> static\_image\_operator\_t**

Operation that needs to be implemented by an image element.

**Data Fields**

- [hal_image_status_t(∗](#_page21_x70.87_y644.20)[init )(static_image_t](#_page26_x70.87_y553.83) ∗elt, mpp\_img\_params\_t ∗config, void ∗param)
- [hal_image_status_t(∗](#_page21_x70.87_y644.20)[dequeue )(static_image_t](#_page26_x70.87_y647.28) ∗elt, [hw_buf_desc_t ∗](#_page18_x70.87_y458.65)out\_buf, int ∗stripe\_num)

**Field Documentation <a name="_page26_x70.87_y553.83"></a>**

**init**

[hal_image_status_t(∗ ](#_page21_x70.87_y644.20)static\_image\_operator\_t::init) (static\_image\_t ∗elt, mpp\_img\_params\_t ∗config, void ∗param)

<a name="_page26_x70.87_y647.28"></a>initialize the elt 

**dequeue**

[hal_image_status_t(∗ ](#_page21_x70.87_y644.20)static\_image\_operator\_t::dequeue) (static\_image\_t ∗elt, [hw_buf_desc_t ](#_page18_x70.87_y458.65)∗out\_buf, int ∗stripe\_num)

dequeue<a name="_page26_x70.87_y757.75"></a> a buffer from the elt

3. **struct<a name="_page27_x70.87_y70.87"></a> gfx\_dev\_operator\_t**

Operation that needs to be implemented by gfx device.

**Data Fields**

- int(∗**init** )(const gfx\_dev\_t ∗dev, void ∗param)
- int(∗**deinit** )(const gfx\_dev\_t ∗dev)
- int(∗**get\_buf\_desc** )(const gfx\_dev\_t ∗dev, [hw_buf_desc_t ](#_page18_x70.87_y458.65)∗in\_buf, [hw_buf_desc_t ](#_page18_x70.87_y458.65)∗out\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y70.87)∗policy)
- int(∗ **blit** )(const gfx\_dev\_t ∗dev, const [gfx_surface_t ](#_page13_x70.87_y633.89)∗pSrc, const [gfx_surface_t ](#_page13_x70.87_y633.89)∗pDst, const [gfx_rotate_config_t ](#_page14_x70.87_y235.94)∗pRotate, mpp\_flip\_mode\_t flip)
- int(∗**drawRect** )(const gfx\_dev\_t ∗dev, [gfx_surface_t ](#_page13_x70.87_y633.89)∗pOverlay, int x, int y, int w, int h, int color)
- int(∗**drawPicture** )(const gfx\_dev\_t ∗dev, [gfx_surface_t ](#_page13_x70.87_y633.89)∗pOverlay, int x, int y, int w, int h, int alpha, const char ∗pIcon)
- int(∗**drawText** )(const gfx\_dev\_t ∗dev, [gfx_surface_t ](#_page13_x70.87_y633.89)∗pOverlay, int x, int y, int textColor, int bgColor, int type, const char ∗pText)
- int(∗**compose** )(const gfx\_dev\_t ∗dev, [gfx_surface_t ](#_page13_x70.87_y633.89)∗pSrc, [gfx_surface_t ](#_page13_x70.87_y633.89)∗pOverlay, [gfx_surface_t ](#_page13_x70.87_y633.89)∗pDst, [gfx_rotate_config_t ](#_page14_x70.87_y235.94)∗pRotate, mpp\_flip\_mode\_t flip)

4. **struct<a name="_page27_x70.87_y357.19"></a><a name="_page27_x70.87_y341.95"></a> vision\_algo\_dev\_operator\_t**

Operation that needs to be implemented by a vision algorithm device.

**Data Fields**

- [hal_valgo_status_t(∗](#_page22_x70.87_y234.70)[init )(vision_algo_de](#_page27_x70.87_y576.76)v\_t ∗dev, [model_param_t ∗](#_page14_x70.87_y731.01)param)
- [hal_valgo_status_t(∗](#_page22_x70.87_y234.70)[deinit )(vision_algo_de](#_page27_x70.87_y670.20)v\_t ∗dev)
- [hal_valgo_status_t(∗](#_page22_x70.87_y234.70)r[un )(const](#_page27_x70.87_y766.94) vision\_algo\_dev\_t ∗dev, void ∗data)
- [hal_valgo_status_t(∗](#_page22_x70.87_y234.70)[get_buf_desc )(const](#_page28_x70.87_y144.76) vision\_algo\_dev\_t ∗dev, [hw_buf_desc_t ∗](#_page18_x70.87_y458.65)in\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y70.87)∗policy)

**Field Documentation <a name="_page27_x70.87_y576.76"></a>**

**init**

[hal_valgo_status_t(∗ ](#_page22_x70.87_y234.70)vision\_algo\_dev\_operator\_t::init) (vision\_algo\_dev\_t ∗dev, [model_param_t ](#_page14_x70.87_y731.01)∗param)

<a name="_page27_x70.87_y670.20"></a>initialize the dev 

**deinit**

[hal_valgo_status_t(∗ ](#_page22_x70.87_y234.70)vision\_algo\_dev\_operator\_t::deinit) (vision\_algo\_dev\_t ∗dev) <a name="_page27_x70.87_y766.94"></a>deinitialize the dev

**run**

[hal_valgo_status_t(∗ ](#_page22_x70.87_y234.70)vision\_algo\_dev\_operator\_t::run) (const vision\_algo\_dev\_t ∗dev, void ∗data)

<a name="_page28_x70.87_y144.76"></a>start the dev 

**get\_buf\_desc**

[hal_valgo_status_t(∗ ](#_page22_x70.87_y234.70)vision\_algo\_dev\_operator\_t::get\_buf\_desc) (const vision\_algo\_dev\_t ∗dev, [hw_buf_desc_t ](#_page18_x70.87_y458.65)∗in\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y70.87)∗policy)

read input parameters

5. **struct<a name="_page28_x70.87_y262.75"></a><a name="_page28_x70.87_y247.77"></a> display\_dev\_operator\_t**

Operation that needs to be implemented by a display device.

**Data Fields**

- [hal_display_status_t(](#_page22_x70.87_y613.60)∗[init](#_page28_x70.87_y506.35))(display\_dev\_t ∗dev, mpp\_display\_params\_t ∗config, [mpp_callback_t ](#_page20_x70.87_y569.51)callback, void ∗user\_data)
- [hal_display_status_t(](#_page22_x70.87_y613.60)∗[deinit ](#_page28_x70.87_y592.59))(const display\_dev\_t ∗dev)
- [hal_display_status_t(](#_page22_x70.87_y613.60)∗[start ](#_page28_x70.87_y681.86))(display\_dev\_t ∗dev)
- [hal_display_status_t(](#_page22_x70.87_y613.60)∗[stop ](#_page28_x70.87_y771.13))(display\_dev\_t ∗dev)
- [hal_display_status_t(](#_page22_x70.87_y613.60)∗[blit](#_page29_x70.87_y139.39) )(const display\_dev\_t ∗dev, void ∗frame, int stripe)
- [hal_display_status_t(](#_page22_x70.87_y613.60)∗[get_buf_desc ](#_page29_x70.87_y246.30))(const display\_dev\_t ∗dev, [hw_buf_desc_t ](#_page18_x70.87_y458.65)∗in\_buf, [mpp_memory_policy_t](#_page23_x70.87_y70.87) ∗policy)

**Field Documentation <a name="_page28_x70.87_y506.35"></a>**

**init**

[hal_display_status_t(∗ ](#_page22_x70.87_y613.60)display\_dev\_operator\_t::init) (display\_dev\_t ∗dev, mpp\_display\_params\_t ∗config, [mpp_callback_t callback,](#_page20_x70.87_y569.51) void ∗user\_data)

<a name="_page28_x70.87_y592.59"></a>initialize the dev 

**deinit**

[hal_display_status_t(∗ ](#_page22_x70.87_y613.60)display\_dev\_operator\_t::deinit) (const display\_dev\_t ∗dev) <a name="_page28_x70.87_y681.86"></a>deinitialize the dev

**start**

[hal_display_status_t(∗ ](#_page22_x70.87_y613.60)display\_dev\_operator\_t::start) (display\_dev\_t ∗dev) <a name="_page28_x70.87_y771.13"></a>start the dev

**stop**

[hal_display_status_t(∗ ](#_page22_x70.87_y613.60)display\_dev\_operator\_t::stop) (display\_dev\_t ∗dev) stop the dev

<a name="_page29_x70.87_y139.39"></a>**blit**

[hal_display_status_t(∗ ](#_page22_x70.87_y613.60)display\_dev\_operator\_t::blit) (const display\_dev\_t ∗dev, void ∗frame, int stripe)

<a name="_page29_x70.87_y246.30"></a>blit a buffer to the dev 

**get\_buf\_desc**

[hal_display_status_t(∗ ](#_page22_x70.87_y613.60)display\_dev\_operator\_t::get\_buf\_desc) (const display\_dev\_t ∗dev, [hw_buf_desc_t ](#_page18_x70.87_y458.65)∗in\_buf, [mpp_memory_policy_t ](#_page23_x70.87_y70.87)∗policy)

get buffer descriptors and policy

##### 2.2.1.2 Typedef Documentation

<a name="_page29_x70.87_y406.20"></a>1. **mpp\_callback\_t**

typedef int(∗ mpp\_callback\_t) (mpp\_t mpp, mpp\_evt\_t evt, void ∗evt\_data, void ∗user\_data) The mpp callback function prototype.

##### 2.2.1.3 Function Documentation

1. **GUI\_DrawText()**

<a name="_page29_x70.87_y534.69"></a>void GUI\_DrawText (
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

<a name="_page30_x70.87_y120.57"></a><a name="_page30_x70.87_y105.33"></a>static void hal\_draw\_pixel565 (
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

<a name="_page30_x70.87_y409.07"></a><a name="_page30_x70.87_y393.83"></a>static void hal\_draw\_text565 (
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

<a name="_page31_x70.87_y120.57"></a><a name="_page31_x70.87_y105.33"></a>static void hal\_draw\_rect565 (
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

5. **get\_bitpp()<a name="_page31_x70.87_y538.43"></a><a name="_page31_x70.87_y523.19"></a>**

static int get\_bitpp ( mpp\_pixel\_format\_t type ) [static]

returns the number of bits per pixel per format, unknown format return 0

6. **swap\_2\_bytes()<a name="_page31_x70.87_y648.83"></a><a name="_page31_x70.87_y633.59"></a>**

void swap\_2\_bytes ( uint8\_t ∗ data, int size )

Swaps a buffer's MSB and LSB bytes.

**Parameters**

|name|description|
| - | - |
|data|pointer to the buffer to be converted(from little endian to big endian and vice-versa).|
|size|buffer size.|

### 2.3 HAL Setup Functions

**Functions**

- int[ hal_label_rectangle ](#_page32_x70.87_y429.26)(uint8\_t ∗frame, int width, int height, mpp\_pixel\_format\_t format, mpp\_labeled\_rect\_t ∗lr, int stripe, int stripe\_max)
- int[ hal_inference_tflite_setup ](#_page33_x70.87_y105.41)(vision\_algo\_dev\_t ∗dev)
- int[ hal_display_setup (const](#_page33_x70.87_y338.17) char ∗name, display\_dev\_t ∗dev)
- int[ hal_camera_setup (const](#_page33_x70.87_y617.67) char ∗name, camera\_dev\_t ∗dev)
- int[ hal_gfx_setup ](#_page34_x70.87_y194.37)(const char ∗name, gfx\_dev\_t ∗dev)
- int[ hal_img_decoder_setup (const](#_page36_x70.87_y572.68) char ∗name, vdec\_dev\_t ∗dev)
- int[ hal_landmark](#_page36_x70.87_y573.68)(uint8\_t *frame, int width, int height, mpp\_pixel\_format\_t format, mpp\_landmark\_t *lk, int stripe, int stripe\_max)

#### 2.3.1 Detailed Description

This section provides the detailed documentation for the HAL setup functions that should be defined by each device.

##### 2.3.1.1 Function Documentation

1. **hal\_label\_rectangle()**

<a name="_page32_x70.87_y429.26"></a>int hal\_label\_rectangle (
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

2. **hal\_inference\_tflite\_setup()**

<a name="_page33_x70.87_y120.65"></a><a name="_page33_x70.87_y105.41"></a>int hal\_inference\_tflite\_setup (
vision\_algo\_dev\_t ∗ dev )

Hal setup function for inference engine Tensorflow-Lite Micro.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|dev|vision algo device to register|

**Returns**

error code (0: success, otherwise: failure)

3. **hal\_display\_setup()<a name="_page33_x70.87_y353.41"></a><a name="_page33_x70.87_y338.17"></a>**

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

4. **hal\_camera\_setup()<a name="_page33_x70.87_y632.83"></a><a name="_page33_x70.87_y617.67"></a>**

int hal\_camera\_setup ( const char ∗ name, camera\_dev\_t ∗ dev )

Register with a camera device specified by name. If name is NULL, return error.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|name|camera name|
|in|dev|camera device to register|

**Returns**

error code (0: success, otherwise: failure)

5. **hal\_gfx\_setup()<a name="_page34_x70.87_y209.61"></a><a name="_page34_x70.87_y194.37"></a>**

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

6. **hal\_img\_decoder\_setup()**

<a name="_page36_x70.87_y599.87"></a><a name="_page36_x70.87_y572.68"></a>int hal\_img\_decoder\_setup ( const char ∗ name, vdec\_dev\_t ∗ dev )

Register with an image decoder device specified by name.

If name is NULL, the first available decoder supported by Hw will be selected. The decoder device using CPU operations will be selected if name is not specified and if no decoder is available for the Hw.

**Parameters**

|in/out|name|description|
| - | - | - |
|in|name|image decoding device name|
|in|dev|decoder device to register|

**Returns**

error code (0: success, otherwise: failure)

7. **hal\_landmark()**

<a name="_page36_x70.87_y573.68"></a>int hal\_landmark(uint8\_t *frame, int width, int height, mpp\_pixel\_format\_t format, mpp\_landmark\_t *lk, int stripe, int stripe\_max)

