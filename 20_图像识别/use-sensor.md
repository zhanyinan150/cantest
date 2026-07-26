# 使用摄像头

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/image-recog/use-sensor.html>
> **最后更新**: 2025-11-05

---

# 1 本节介绍 ​

📝本节您将学习如何调用板子上的摄像头来实时获取图像。

🏆学习目标

1️⃣如何通过默认摄像头获取图像。

2️⃣如何使用其他两个摄像头。

3️⃣如何用按键将图片保存到TF卡中。

# 2 现有摄像头 ​

截止2024年11月21日，我们共有两种摄像头可卖，其内部感光元件都是`GC2093`，内部电路完全一致，只不过镜头不一样。

两个镜头相同的参数:

名称| 参数  
---|---  
感光元件| GC2093  
最大分辨率| 1920x1080  
最大帧率| 60fps@full size  
感光尺寸| 1/2.9  
  
## 2.1 立创·GC2093-200W摄像头-小镜头版-定焦70cm ​

该镜头为购买庐山派标准版即附送，无需额外购买，除非你需要用两个以上的摄像头。

购买链接: <https://item.szlcsc.com/44319753.html>

> TODO：等待拍摄实物图

名称| 参数  
---|---  
焦距（EFL）| 4.3MM  
光圈（F.NO）| 2.0  
视场角（View Angle）| D73°/H65°/V40°  
畸变| <0.5%  
  
机械尺寸及引脚定义： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/use-sensor/use-sensor_20241121_103341.png)

## 2.2 立创·GC2093-200W摄像头-大镜头版-可旋转调焦 ​

该款摄像头配备了大的镜头，并且出厂时未进行焦距固定，也就是说拿到手后可以直接旋转镜头来进行对焦操作。同时，该摄像头采用标准的M12镜头安装接口，支持兼容多种规格的镜头。如果您拥有其他符合M12接口的镜头，也可以自行更换。

购买链接：<https://item.szlcsc.com/44319754.html>

> TODO：等待拍摄实物图

名称| 参数  
---|---  
焦距（EFL）| 3.0MM  
光圈（F.NO）| 2.5  
视场角（View Angle）| D95°/H86°/V55°  
畸变| <1.0%  
  
机械尺寸及引脚定义： ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/use-sensor/use-sensor_20241121_110415.png)

## 2.3 立创·GC2093-200W摄像头-可自动对焦 ​

该款摄像头可以程序设定对焦，也可以自动对焦，在摄像头里面新增了音圈马达，让镜头焦距可以变化了

购买链接：<https://item.szlcsc.com/54973554.html>

机械尺寸及引脚定义： ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/use-sensor/use-sensor_20251027_153246.png)

# 3 庐山派开发板上的摄像头接口 ​

> TODO：放资源标注图，标识处CSI2,CSI0,CSI1.

板子上提供了三路摄像头接口，都是22p-0.5mm的FPC接口，不同的是CSI2（默认摄像头）是立式的可插拔座子，在板子正面；CSI0和CSI1是卧式的翻盖座子，在板子背面，和mipi接口在同一侧。

都兼容树莓派zero和树莓派5的摄像头接口，理论上能给树莓派zero和树莓派5使用的摄像头就可以给我们的板子使用，软件上需要额外适配。目前我们只推荐使用我们定制的GC2093摄像头，动态性能贼棒。

# 4 K230的摄像头架构 ​

在立创·庐山派-K230-CanMV开发板中，Sensor模块的主要作用是负责获取图像数据。这个模块将光信号转化为数字信号，供后续图像处理算法使用。

K230的Sensor模块API提供了对这些硬件的底层控制，模块负责图像采集与数据处理。该模块提供了一套高级 API，开发者可以利用这些接口轻松获取不同格式与尺寸的图像，而无需了解底层硬件的具体实现。其架构如下图所示： ![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/use-sensor/use-sensor_20241122_110342.png)

图中，**sensor 0** 、**sensor 1** 和 **sensor 2** 分别代表三个图像输入传感器设备；这些传感器主要用于将环境中的光信号转化为数字图像信号。在实际系统中，这些传感器可以安装在不同的位置，用来捕获来自不同视角或者区域的图像数据。比如说三路摄像头场景，车辆前后摄像检测各一路+驾驶仓内一路。也可以将CSI0+CSI1转接成一个4lane的接口用来接更高清的摄像头。

**Camera Device 0** 、**Camera Device 1** 和 **Camera Device 2** 是用于处理传感器输入数据的核心单元。每个 `Camera Device` 可以独立完成图像数据捕获，格式转换及预处理等。传感器和Camera Device之间是多对多的关系，也就是说多个传感器输入可以灵活映射到不同的Camera Device。

每个Camera Device支持 **3个输出通道** （`output channel 0`、`output channel 1` 和 `output channel 2`）。这些输出通道的主要功能是将处理后的图像数据并行传输到后续的算法模块或显示设备，同时也支持多种数据格式和尺寸。这样的架构设计，让K230能够支持多路图像数据的高效并行处理，非常适合实时性要求较高的AI视觉任务。

K230 的 `sensor` 模块最多支持三路图像传感器的同时接入，每一路均可独立完成图像数据的采集、捕获和处理。此外，每个视频通道可并行输出三路图像数据供后端模块进行进一步处理。实际应用中，具体支持的传感器数量、输入分辨率和输出通道数将受限于开发板的硬件配置和内存大小，因此需根据项目需求进行综合评估。

**三路图像输入** ：

同时接入3个传感器，适合多摄像头应用场景，比如：

  * 自动驾驶中的多视角检测。
  * 安防监控中的多区域捕获。
  * 工业检测中的多面检测。



**三路图像输出** ：

为每个输入提供并行的多通道输出，便于在不同模块中并发处理，比如：

  * 一路用于实时显示。
  * 一路用于AI算法推理。
  * 一路用于视频存储或回放。



# 5 摄像头使用指南 ​

要使用 摄像头 sensor，首先需要导入该模块：

python
    
    
    from media.sensor import *

1  


## 5.1 构造函数 ​

**描述**

通过 `csi id` 和图像传感器类型构建 `Sensor` 对象。

在图像处理应用中，用户通常需要首先创建一个 `Sensor` 对象。CanMV K230 软件可以自动检测内置的图像传感器，无需用户手动指定具体型号，只需设置传感器的最大输出分辨率和帧率。有关支持的图像传感器信息，请参见图像传感器支持列表。如果设定的分辨率或帧率与当前传感器的默认配置不符，系统会自动调整为最优配置，最终的配置可在日志中查看，例如 `use sensor 23, output 640x480@90`。

**语法**

python
    
    
    sensor = Sensor(id, [width, height, fps])

1  


**参数**

参数名称| 描述| 输入/输出| 说明  
---|---|---|---  
id| `csi` 端口，支持 0，1，2| 输入| 可选，庐山派开发板默认摄像头为CSI2  
width| `sensor` 最大输出图像宽度| 输入| 可选，默认 `1920`  
height| `sensor` 最大输出图像高度| 输入| 可选，默认 `1080`  
fps| `sensor` 最大输出图像帧率| 输入| 可选，默认 `30`  
  
**返回值**

返回值| 描述  
---|---  
Sensor 对象| 传感器对象  
  
**举例**

python
    
    
    sensor = Sensor(id=0)
    sensor = Sensor(id=0, width=1280, height=720, fps=60)
    sensor = Sensor(id=0, width=640, height=480)

1  
2  
3  


庐山派开发板的默认摄像头接口为CSI2，如果Sensor默认内不指定id，则默认为id=2.

也就是说以下两条语句是等效的：

python
    
    
    sensor = Sensor()

1  


python
    
    
    sensor = Sensor(id=2)

1  


## 5.1 sensor.reset ​

**描述**

复位 `sensor` 对象。在构造 `Sensor` 对象后，必须调用此函数以继续执行其他操作。

**语法**

python
    
    
    sensor.reset()

1  


**参数**

参数名称| 描述| 输入/输出  
---|---|---  
无| |   
  
**返回值**

返回值| 描述  
---|---  
无|   
  
**举例**

python
    
    
    # 初始化 sensor 以及传感器 GC2093
    sensor.reset()

1  
2  


## 2.2 sensor.set_framesize ​

**描述**

设置指定通道的输出图像尺寸。用户可以通过 `framesize` 参数或直接指定 `width` 和 `height` 来配置输出图像尺寸。**宽度会自动对齐到 16 像素宽** 。

**语法**

python
    
    
    sensor.set_framesize(framesize=FRAME_SIZE_INVALID, chn=CAM_CHN_ID_0, alignment=0, **kwargs)

1  


注意

`kwargs`是关键词参数（keyword argument）的缩写，目前可以输入的参数有width和height，这两个参数和framesize所设置的分辨率是一个东西，所以是互斥的，只能二选一。

**参数**

参数名称| 描述| 输入/输出  
---|---|---  
framesize| sensor 输出图像尺寸| 输入  
width【**kwargs】| 输出图像宽度， _kw_arg_|  输入  
height 【**kwargs】| 输出图像高度， _kw_arg_|  输入  
chn| sensor 输出通道号| 输入  
  
**参数可选值：**

  * 设置输出图像尺寸，【`framesize`】和【`width` or `height`】二选一

    * `framesize`

      * 图像帧尺寸| **分辨率**  
---|---  
`Sensor.VGA`| 640x480  
`Sensor.HD`| 1280x720  
`Sensor.FHD`| 1920x1080  
更多图像帧尺寸请点击这里图像帧尺寸| **分辨率**  
---|---  
`Sensor.QQCIF`| 88x72  
`Sensor.QCIF`| 176x144  
`Sensor.CIF`| 352x288  
`Sensor.QSIF`| 176x120  
`Sensor.SIF`| 352x240  
`Sensor.QQVGA`| 160x120  
`Sensor.QVGA`| 320x240  
`Sensor.VGA`| 640x480  
`Sensor.HQQVGA`| 120x80  
`Sensor.HQVGA`| 240x160  
`Sensor.HVGA`| 480x320  
`Sensor.B64X64`| 64x64  
`Sensor.B128X64`| 128x64  
`Sensor.B128X128`| 128x128  
`Sensor.B160X160`| 160x160  
`Sensor.B320X320`| 320x320  
`Sensor.QQVGA2`| 128x160  
`Sensor.WVGA`| 720x480  
`Sensor.WVGA2`| 752x480  
`Sensor.SVGA`| 800x600  
`Sensor.XGA`| 1024x768  
`Sensor.WXGA`| 1280x768  
`Sensor.SXGA`| 1280x1024  
`Sensor.SXGAM`| 1280x960  
`Sensor.UXGA`| 1600x1200  
`Sensor.HD`| 1280x720  
`Sensor.FHD`| 1920x1080  
`Sensor.QHD`| 2560x1440  
`Sensor.QXGA`| 2048x1536  
`Sensor.WQXGA`| 2560x1600  
`Sensor.WQXGA2`| 2592x1944  
    * `width` or `height`

      * 这个就是自己填分辨率就行。
  * 设置输出通道号

    * chn 
      * 通道0：`CAM_CHN_ID_0`
      * 通道1：`CAM_CHN_ID_1`
      * 通道2：`CAM_CHN_ID_2`



**返回值**

无

**注意事项**

  * 输出图像尺寸不得超过图像传感器的实际输出能力。
  * 各通道的最大输出图像尺寸受硬件限制。



**举例**

python
    
    
    # 配置 sensor 设备，输出通道 0，输出图尺寸为 640x480
    sensor.set_framesize(chn=CAM_CHN_ID_0, width=640, height=480)
    
    # 配置 sensor 设备，输出通道 1，输出图尺寸为 320x240
    sensor.set_framesize(chn=CAM_CHN_ID_1, width=320, height=240)
    
    # 配置 sensor 设备，输出通道 3，输出图尺寸为 640x480
    sensor.set_framesize(chn=CAM_CHN_ID_3, framesize = sensor.VGA)

1  
2  
3  
4  
5  
6  
7  
8  
9  


## 5.3 sensor.set_pixformat ​

**描述**

配置指定通道的图像传感器输出图像格式。

**语法**

python
    
    
    sensor.set_pixformat(pix_format, chn=CAM_CHN_ID_0)

1  


**参数**

参数名称| 描述| 输入/输出  
---|---|---  
pix_format| 输出图像格式（像素格式）| 输入  
chn| sensor 输出通道号| 输入  
  
**参数可选值：**

  * 设置像素格式（如果想知道这些像素格式的具体定义，请看后续的摄像头基础知识部分）

    * 像素格式| 说明  
---|---  
`Sensor.RGB565`| 16 位 RGB 格式  
`Sensor.RGB888`| 24 位 RGB 格式  
`Sensor.RGBP888`| 分离的 24 位 RGB  
`Sensor.YUV420SP`| 半平面 YUV  
`Sensor.GRAYSCALE`| 灰度图  
  * 设置输出通道号

    * chn 
      * 通道0：`CAM_CHN_ID_0`
      * 通道1：`CAM_CHN_ID_1`
      * 通道2：`CAM_CHN_ID_2`



**返回值**

无

**举例**

python
    
    
    # 配置 sensor 设备 0，输出通道 0，输出 NV12 格式
    sensor.set_pixformat(Sensor.YUV420SP, chn=CAM_CHN_ID_0)
    
    # 配置 sensor 设备 0，输出通道 1，输出 RGB888 格式
    sensor.set_pixformat(Sensor.RGB888, chn=CAM_CHN_ID_1)

1  
2  
3  
4  
5  


## 5.4 sensor.set_hmirror ​

**描述**

配置图像传感器是否进行水平镜像。

**语法**

python
    
    
    sensor.set_hmirror(enable)

1  


**参数**

参数名称| 描述| 输入/输出  
---|---|---  
enable| `True` 开启水平镜像功能  
`False` 关闭水平镜像功能| 输入  
  
**返回值**

无

**举例**

python
    
    
    sensor.set_hmirror(True)

1  


## 5.5 sensor.set_vflip ​

**描述**

配置图像传感器是否进行垂直翻转。

**语法**

python
    
    
    sensor.set_vflip(enable)

1  


**参数**

参数名称| 描述| 输入/输出  
---|---|---  
enable| `True` 开启垂直翻转功能  
`False` 关闭垂直翻转功能| 输入  
  
**返回值**

无

**举例**

python
    
    
    sensor.set_vflip(True)

1  


## 5.6 sensor.run ​

**描述**

启动图像传感器的输出。**必须在调用`MediaManager.init()` 之前执行此操作。**

**语法**

python
    
    
    sensor.run()

1  


**返回值**

无

**注意事项**

  * 当同时使用多个传感器（最多 3 个）时，仅需其中一个执行 `run` 即可。



**举例**

python
    
    
    # 启动 sensor 设备输出数据流
    sensor.run()

1  
2  


## 5.7 sensor.stop ​

**描述**

停止图像传感器输出。**必须在`MediaManager.deinit()` 之前调用此方法。**

**语法**

python
    
    
    sensor.stop()

1  


**返回值**

返回值| 描述  
---|---  
无|   
  
**注意事项**

  * 如果同时使用多个图像传感器（最多 3 个），**每个传感器都需单独调用`stop`**。



**举例**

python
    
    
    # 停止 sensor 设备 0 的数据流输出
    sensor.stop()

1  
2  


## 5.8 sensor.snapshot ​

**描述**

从指定输出通道中捕获一帧图像数据。

**语法**

python
    
    
    sensor.snapshot(chn=CAM_CHN_ID_0)

1  


**参数**

参数名称| 描述| 输入/输出  
---|---|---  
chn| sensor 输出通道号| 输入  
  
**返回值**

返回值| 描述  
---|---  
image 对象| 捕获的图像数据  
其他| 捕获失败  
  
**举例**

python
    
    
    # 从 sensor 设备 0 的通道 0 捕获一帧图像数据
    sensor.snapshot()

1  
2  


# 6 摄像头基础知识 ​

在开始使用摄像头之前，了解一些基础知识有助于更好地应用和开发。本节将介绍摄像头的图像传感器类型、像素格式和常见参数。

## 6.1 图像传感器 ​

图像传感器是摄像头的核心组件，负责将光信号转换为电信号，这样电子设备才能处理和存储这些信息。可以将其比喻为开发板的“眼睛”。目前常见的图像传感器类型有CCD和CMOS。庐山派开发板使用的`GC2093`摄像头是CMOS类型的传感器，相比OV5647的显示效果提升很大，而且目前OV5647都停产了，只有二手的芯片。

**CMOS和CCD传感器的成像特点和使用场景对比表** ：

**特点**| **CMOS传感器**| **CCD传感器**  
---|---|---  
**信噪比和图像质量**|  \- 现代CMOS噪声已大幅降低，图像质量接近甚至超越CCD  
\- 早期噪声较大，但技术进步已改善| \- 高信噪比，图像质量高  
\- 低噪声表现优秀，适合弱光环境下的高质量成像  
**功耗和集成度**|  \- 低功耗，适合电池供电设备  
\- 高集成度，可在芯片上集成图像处理电路和ADC，简化设计| \- 高功耗，需要高电压驱动  
\- 低集成度，需外部电路支持，如模拟数字转换器（ADC）  
**数据读取速度**|  \- 读取速度快，支持高帧率和全局快门  
\- 可随机访问像素，适合高速成像| \- 读取速度较慢，电荷逐行转移  
\- 帧率和高速成像能力受限  
**制造成本**|  \- 利用标准CMOS工艺制造，成本较低  
\- 成本较低便于大规模生产| \- 制造工艺复杂，生产成本较高  
\- 价格相对昂贵  
**适用场景**|  \- **消费电子产品** ：智能手机、平板电脑、数码相机等  
\- **安防监控** ：监控摄像头、行车记录仪等  
\- **汽车电子** ：倒车影像、驾驶辅助系统等  
\- **物联网和嵌入式系统** ：机器人、无人机等| \- **专业摄影设备** ：高端数码相机、电影摄像机  
\- **科学研究** ：天文学、医学成像等  
\- **工业检测** ：精密测量、质量控制等  
**优点**|  \- 低功耗- 高集成度  
\- 成本低- 读取速度快，支持高帧率| \- 高图像质量- 低噪声，弱光性能优异  
\- 高信噪比  
**缺点**|  \- 早期噪声较大（目前已大幅改善）  
\- 在极端弱光条件下可能不及CCD| \- 高功耗- 成本高  
\- 读取速度慢，帧率受限  
**技术发展趋势**|  \- 技术不断进步，背照式和堆栈式CMOS提升了性能  
\- 全局快门技术改善了图像失真问题| \- 技术成熟，但市场份额逐渐被CMOS取代  
\- 主要应用在特定高端领域  
**选择建议**|  \- **适合对成本、功耗和尺寸敏感的应用**  
\- **需要高速成像和高集成度的设备**|  \- **适合追求极高图像质量的专业领域**  
\- **弱光环境下的高性能成像需求**  
  
**注释：**

  * **CMOS传感器** 在大多数应用中已成为主流选择，适合消费电子和需要高集成度的设备。是目前绝大多数的数码单反相机所用的图像传感器。
  * **CCD传感器** 仍在专业摄影、科学研究等需要卓越图像质量的领域占有重要地位。做智能车的用户可能会比较熟悉，应该用过那种线性CCD摄像头来进行巡线。



## 6.2 分辨率和帧率 ​

  * **分辨率** ：简单来说，分辨率就是图像的清晰度。它表示为图像的宽度和高度的像素数，例如1920x1080。这意味着图像有1920个像素的宽度和1080个像素的高度，我家里以及在公司使用的显示屏也就这个分辨率，就是平常所说的1080P屏幕。像素数越高，图像就越清晰，能展示的细节就越多。
  * **帧率（FPS）** ：帧率表示每秒钟摄像头可以捕捉并传输的图像数量。帧率越高，视频就越流畅。比如，电影通常是24fps，打游戏的话一般都得144Hz以上。我们的GC2093摄像头支持最高60fps的帧率，能提供非常流畅的图像，至少在嵌入式领域算是比较高的帧率了，毕竟嵌入式会落后计算机20年嘛。



## 6.3 焦距和视场角 ​

  * **焦距（EFL）** ：焦距决定了摄像头能看到的范围，就像是人眼的视野。焦距越短，摄像头能看到的范围就越广，但看到的物体会显得较小。焦距越长，摄像头能看到的范围就越窄，但可以看得更远、更清楚。
  * **视场角（View Angle）** ：视场角是用角度来表示摄像头能看到的范围，包括： 
    * **对角线视角（D）** ：从图像的一个角到对角线另一端的视角。
    * **水平视角（H）** ：摄像头水平方向能看到的范围。
    * **垂直视角（V）** ：摄像头垂直方向能看到的范围。



我们在板子上线的时候也会同步上架一款大镜头，可以旋转调焦的镜头，他就可以更换m12规格的其他镜头。如果我们使用广角镜头的话，视场角会比较大，能够拍摄到范围更广的场景，也会引入一定的摄像头畸变。

## 6.4 像素格式 ​

像素格式是指图像数据的存储方式，它描述了每个像素如何表示颜色或亮度信息。选择合适的像素格式不仅影响图像的质量，还直接影响存储空间、数据传输速度和处理效率。

### **6.4.1 像素格式的分析** ​

#### **RGB565** ​

  * 定义：每个像素用16位表示，RGB通道分别分配5位、6位和5位。 
    * 红色 (R)：5位，32级灰度。
    * 绿色 (G)：6位，64级灰度（由于人眼对绿色更敏感，绿色多分配了一位）。
    * 蓝色 (B)：5位，32级灰度。
  * 特点： 
    * 数据量较小，仅16位（2字节）每像素。
    * 支持基本的颜色表现，但颜色精度较低。
  * 应用场景： 
    * 低功耗嵌入式设备，如手持设备、工业屏幕显示。
    * 对图像质量要求不高但注重内存占用的场景。



#### **RGB888** ​

  * 定义：每个像素用24位表示，RGB通道分别分配8位（即每种颜色256级灰度）。 
    * 红色 (R)：8位。
    * 绿色 (G)：8位。
    * 蓝色 (B)：8位。
  * 特点： 
    * 高颜色精度，适合高质量图像显示。
    * 数据量较大，每像素3字节。
  * 应用场景： 
    * 高分辨率图像处理。
    * 用于显示要求高的场合，如智能手机屏幕、高清显示器。



#### **RGBP888** ​

  * 定义：也叫分离的24位RGB格式，R、G、B通道数据分别存储在独立的内存区域中。 
    * 第一个内存块存储所有像素的R通道值。
    * 第二个内存块存储所有像素的G通道值。
    * 第三个内存块存储所有像素的B通道值。
  * 特点： 
    * 更适合某些图像处理算法（如颜色分离、特征提取）。
    * 数据访问更高效，但存储方式较复杂。
  * 应用场景： 
    * 用于图像算法开发，如深度学习中的特征提取。
    * 专业图像处理器的内部数据格式。



#### **YUV420SP** ​

  * 定义：一种半平面 YUV 格式，主要用于视频压缩和处理。 
    * Y：表示亮度（Luminance）。
    * U、V：表示色度（Chrominance），分别存储蓝色和红色的差异。
    * 420：表示UV分量的分辨率为Y的1/4。
    * 半平面：Y存储在一块连续内存中，UV分量交替存储在另一块连续内存中。
  * 特点： 
    * 数据量小，压缩比高。
    * 亮度和色度分开，便于色彩调整。
  * 应用场景： 
    * 视频流处理和压缩，如H.264、H.265视频编码。
    * 用于需要节省存储和带宽的实时视频传输场景。



#### **GRAYSCALE** ​

  * 定义：灰度图，只有亮度信息，每像素用8位表示（256级灰度）。 
    * 无颜色信息，只有从黑到白的亮度变化。
  * 特点： 
    * 数据量小，仅1字节每像素。
    * 不包含色彩信息，但对亮度变化敏感。
  * 应用场景： 
    * 图像识别和机器视觉，如边缘检测、特征提取。
    * 黑白图像存储和处理。



* * *

### **6.4.2 优缺点简要对比** ​

**像素格式**| **每像素数据量**| **优点**| **缺点**| **典型应用**  
---|---|---|---|---  
RGB565| 2字节| 数据量小，适合低功耗设备| 颜色精度低| 嵌入式显示屏、工业显示  
RGB888| 3字节| 颜色精度高，适合高清显示| 数据量大，占用更多内存| 高清显示器、手机屏幕  
RGBP888| 3字节| 更高效的通道访问| 存储和处理较复杂| 深度学习、专业图像处理  
YUV420SP| 1.5字节| 压缩比高，适合视频传输| 色彩精度不如RGB| 视频流处理、实时视频传输  
GRAYSCALE| 1字节| 数据量小，专注亮度处理| 无颜色信息| 机器视觉、图像识别  
  
* * *

### **6.4.3 如何选择像素格式** ​

  1. **性能与图像质量的平衡** ： 
     * RGB565 和 YUV420SP 更注重性能，适合嵌入式场景。
     * RGB888 和 RGBP888 更注重图像质量，适合高清显示或复杂算法。
  2. **存储和带宽限制** ： 
     * 嵌入式设备通常内存和带宽有限，选择合适的像素格式可以降低资源占用。
  3. **理解格式的用途** ： 
     * 如果需要处理彩色图像，优先选择 RGB 格式。
     * 如果主要关注亮度变化，比如说巡线，可以选择 GRAYSCALE 格式。
  4. **实际开发经验** ： 
     * 在图像处理或视频传输项目中，YUV420SP 是视频压缩的主流格式。
     * 在AI模型中，经常会将RGB888转换为RGBP888格式，便于模型高效处理。



理解以上这些像素格式，我们就可以根据项目需求选择最合适的格式，提高项目的性能和效率。

## 6.5 镜像和翻转 ​

在实际应用中，摄像头可能会因为安装方向的不同，导致拍摄的图像是倒置的或者是镜像的。为了让图像呈现正确的方向，我们就需要对图像进行镜像或翻转处理：

  * **水平镜像（H-Mirror）** ：将图像左右颠倒，就像看镜子里的自己。
  * **垂直翻转（V-Flip）** ：将图像上下颠倒，使上方的物体显示在下方。



通过镜像和翻转，就可以确保图像以正确的方向显示，方便观看和处理。

## 6.6 曝光和白平衡 ​

  * **曝光** ：曝光控制着传感器接收光线的时间，直接影响图像的亮度。曝光时间越长，传感器接收到的光线就越多，图像就会越亮；曝光时间越短，图像就会变暗。在光线不足的环境下，可以增加曝光时间来提高亮度。
  * **白平衡** ：白平衡用于调整图像的色彩，使其看起来更加自然。不同的光源（如日光、白炽灯、荧光灯）会有不同的色温，影响图像的颜色表现。通过调整白平衡，可以纠正颜色偏差，使白色看起来真正是白色，从而使整个图像的颜色更加准确。



注意！

K230芯片内部自带ISP，如果需要调节摄像头的亮度，对比度，曝光时间等就需要将ISP的AE模块关掉，会导致成像效果变差，所以目前还没有能调节曝光和白平衡的API接口，目前用户无法直接操作。

# 7 基础案例 ​

## 7.1 获取默认摄像头的图像 ​

我们先来将默认摄像头CSI2接口上接的摄像头获取到的图像显示到IDE的帧缓冲区。首先需要将摄像头连接好，再上电并运行下面的程序：

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    import time, os, sys
    
    from media.sensor import *
    from media.display import *
    from media.media import *
    
    sensor_id = 2
    sensor = None
    
    try:
        # 构造一个具有默认配置的摄像头对象
        sensor = Sensor(id=sensor_id)
        # 重置摄像头sensor
        sensor.reset()
    
        # 无需进行镜像翻转
        # 设置水平镜像
        # sensor.set_hmirror(False)
        # 设置垂直翻转
        # sensor.set_vflip(False)
    
        # 设置通道0的输出尺寸为1920x1080
        sensor.set_framesize(Sensor.FHD, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为RGB888
        sensor.set_pixformat(Sensor.RGB888, chn=CAM_CHN_ID_0)
    
        # 使用IDE的帧缓冲区作为显示输出
        Display.init(Display.VIRT, width=1920, height=1080, to_ide=True)
        # 初始化媒体管理器
        MediaManager.init()
        # 启动传感器
        sensor.run()
    
        while True:
            os.exitpoint()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
            # 显示捕获的图像
            Display.show_image(img)
    
    except KeyboardInterrupt as e:
        print("用户停止: ", e)
    except BaseException as e:
        print(f"异常: {e}")
    finally:
        # 停止传感器运行
        if isinstance(sensor, Sensor):
            sensor.stop()
        # 反初始化显示模块
        Display.deinit()
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        # 释放媒体缓冲区
        MediaManager.deinit()

1  
2  
3  
4  
5  
6  
7  
8  
9  
10  
11  
12  
13  
14  
15  
16  
17  
18  
19  
20  
21  
22  
23  
24  
25  
26  
27  
28  
29  
30  
31  
32  
33  
34  
35  
36  
37  
38  
39  
40  
41  
42  
43  
44  
45  
46  
47  
48  
49  
50  
51  
52  
53  
54  
55  
56  
57  
58  
59  
60  
61  
62  
63  


以上代码主要用于初始化并配置摄像头传感器，设置其输出格式和分辨率，然后在主循环中不断捕获图像并显示。首先，创建并重置传感器对象，设置输出分辨率为1920x1080（全高清），像素格式为RGB888。然后，初始化显示模块，将输出定向到IDE的虚拟帧缓冲区。接着，启动传感器，在无限循环中持续捕获图像并显示。程序在异常或中断时，会正确地停止传感器和显示模块，并释放相关资源。

将以上代码复制进CanMV IDE K230中，连接开发板后运行就能看到帧缓冲区里面的图像了，如下图所示：

![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/use-sensor/use-sensor_20241122_171621.png)

## 7.2 打印当前帧率 ​

我们当前的固件是基于micropython的，所以兼容其标准微库。

### 7.2.1 clock类 ​

构造函数

python
    
    
    utime.clock()

1  


#### `tick` ​

python
    
    
    clock.tick()

1  


记录当前时间（毫秒），可用于 FPS 计算。

#### `fps` ​

python
    
    
    clock.fps()

1  


根据上一次 `clock.tick()` 调用后的时间间隔，计算帧率（FPS）。

示例：

python
    
    
    import utime
    clock = utime.clock()
    while True:
        clock.tick()
        utime.sleep(0.1)
        print("fps = ", clock.fps())

1  
2  
3  
4  
5  
6  


#### `reset` ​

python
    
    
    clock.reset()

1  


重置所有计时标记。

#### `avg` ​

python
    
    
    clock.avg()

1  


计算每帧的平均时间消耗。

* * *

在基础案例7.1章节中，我们获取了默认摄像头的图像，我们接下来去获取另外两个摄像头接口的图像并在IDE的串口控制台中打印出当前帧率。

首先将默认摄像头座子（CSI2）上的摄像头拆下来，将它安装到CSI0或CSI1上面。【当然，你预算充足的话可以到我们商城直接再买两个，把这三个摄像头接口接满，就不需要来回更换了】

CSI0接摄像头CSI1接摄像头

python
    
    
    import time, os, sys
    
    import utime
    from media.sensor import *
    from media.display import *
    from media.media import *
    
    #用CSI0接口的摄像头
    sensor_id = 0
    sensor = None
    
    try:
        # 构造一个具有默认配置的摄像头对象
        sensor = Sensor(id=sensor_id)
        # 重置摄像头sensor
        sensor.reset()
    
        # 无需进行镜像翻转
        # 设置水平镜像
        # sensor.set_hmirror(False)
        # 设置垂直翻转
        # sensor.set_vflip(False)
    
        # 设置通道0的输出尺寸为1920x1080
        sensor.set_framesize(Sensor.FHD, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为RGB888
        sensor.set_pixformat(Sensor.RGB888, chn=CAM_CHN_ID_0)
    
        # 使用IDE的帧缓冲区作为显示输出
        Display.init(Display.VIRT, width=1920, height=1080, to_ide=True)
        # 初始化媒体管理器
        MediaManager.init()
        # 启动传感器
        sensor.run()
    
        #构造clock
        clock = utime.clock()
    
        while True:
            os.exitpoint()
    
            #更新当前时间（毫秒）
            clock.tick()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
            # 显示捕获的图像
            Display.show_image(img)
    
            #打印当前fps
            print("fps = ", clock.fps())
    
    except KeyboardInterrupt as e:
        print("用户停止: ", e)
    except BaseException as e:
        print(f"异常: {e}")
    finally:
        # 停止传感器运行
        if isinstance(sensor, Sensor):
            sensor.stop()
        # 反初始化显示模块
        Display.deinit()
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        # 释放媒体缓冲区
        MediaManager.deinit()

1  
2  
3  
4  
5  
6  
7  
8  
9  
10  
11  
12  
13  
14  
15  
16  
17  
18  
19  
20  
21  
22  
23  
24  
25  
26  
27  
28  
29  
30  
31  
32  
33  
34  
35  
36  
37  
38  
39  
40  
41  
42  
43  
44  
45  
46  
47  
48  
49  
50  
51  
52  
53  
54  
55  
56  
57  
58  
59  
60  
61  
62  
63  
64  
65  
66  
67  


python
    
    
    import time, os, sys
    
    import utime
    from media.sensor import *
    from media.display import *
    from media.media import *
    
    #用CSI1接口的摄像头
    sensor_id = 1
    sensor = None
    
    try:
        # 构造一个具有默认配置的摄像头对象
        sensor = Sensor(id=sensor_id)
        # 重置摄像头sensor
        sensor.reset()
    
        # 无需进行镜像翻转
        # 设置水平镜像
        # sensor.set_hmirror(False)
        # 设置垂直翻转
        # sensor.set_vflip(False)
    
        # 设置通道0的输出尺寸为1920x1080
        sensor.set_framesize(Sensor.FHD, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为RGB888
        sensor.set_pixformat(Sensor.RGB888, chn=CAM_CHN_ID_0)
    
        # 使用IDE的帧缓冲区作为显示输出
        Display.init(Display.VIRT, width=1920, height=1080, to_ide=True)
        # 初始化媒体管理器
        MediaManager.init()
        # 启动传感器
        sensor.run()
    
        #构造clock
        clock = utime.clock()
    
        while True:
            os.exitpoint()
    
            #更新当前时间（毫秒）
            clock.tick()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
            # 显示捕获的图像
            Display.show_image(img)
    
            #打印当前fps
            print("fps = ", clock.fps())
    
    except KeyboardInterrupt as e:
        print("用户停止: ", e)
    except BaseException as e:
        print(f"异常: {e}")
    finally:
        # 停止传感器运行
        if isinstance(sensor, Sensor):
            sensor.stop()
        # 反初始化显示模块
        Display.deinit()
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        # 释放媒体缓冲区
        MediaManager.deinit()

1  
2  
3  
4  
5  
6  
7  
8  
9  
10  
11  
12  
13  
14  
15  
16  
17  
18  
19  
20  
21  
22  
23  
24  
25  
26  
27  
28  
29  
30  
31  
32  
33  
34  
35  
36  
37  
38  
39  
40  
41  
42  
43  
44  
45  
46  
47  
48  
49  
50  
51  
52  
53  
54  
55  
56  
57  
58  
59  
60  
61  
62  
63  
64  
65  
66  
67  


和之前的代码基本一致，只是引入了`clock.tick()`来更新时钟，然后在循环末尾打印当前的帧率。

运行时IDE界面如下所示：

![图 4](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/use-sensor/use-sensor_20241122_175423.png)

## 7.3 使用对焦摄像头 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    import time, os, sys
    
    from media.sensor import *
    from media.display import *
    from media.media import *
    
    sensor_id = 2
    sensor = None
    
    # 显示模式选择：可以是 "VIRT"、"LCD" 或 "HDMI"
    DISPLAY_MODE = "LCD"
    
    # 根据模式设置显示宽高
    if DISPLAY_MODE == "VIRT":
        # 虚拟显示器模式
        DISPLAY_WIDTH = ALIGN_UP(1920, 16)
        DISPLAY_HEIGHT = 1080
    elif DISPLAY_MODE == "LCD":
        # 3.1寸屏幕模式
        DISPLAY_WIDTH = 800
        DISPLAY_HEIGHT = 480
    elif DISPLAY_MODE == "HDMI":
        # HDMI扩展板模式
        DISPLAY_WIDTH = 1920
        DISPLAY_HEIGHT = 1080
    else:
        raise ValueError("未知的 DISPLAY_MODE，请选择 'VIRT', 'LCD' 或 'HDMI'")
    
    try:
        # 构造一个具有默认配置的摄像头对象
        sensor = Sensor(id=sensor_id)
        # 重置摄像头sensor
        sensor.reset()
    
        # 无需进行镜像翻转
        # 设置水平镜像
        # sensor.set_hmirror(False)
        # 设置垂直翻转
        # sensor.set_vflip(False)
    
        # 设置通道0的输出尺寸为1920x1080
        sensor.set_framesize(Sensor.FHD, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为RGB888
        sensor.set_pixformat(Sensor.RGB888, chn=CAM_CHN_ID_0)
    
        # 根据模式初始化显示器
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=60)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, chn=CAM_CHN_ID_0)
        sensor.set_pixformat(Sensor.RGB888, chn=CAM_CHN_ID_0)
        sensor.auto_focus(1)
    
        # 初始化媒体管理器
        MediaManager.init()
        # 启动传感器
        sensor.run()
    
        while True:
            os.exitpoint()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
            # 显示捕获的图像
            Display.show_image(img)
    
            current_focus_pos = sensor.focus_pos()
            print("当前对焦位置:", current_focus_pos)
    
    except KeyboardInterrupt as e:
        print("用户停止: ", e)
    except BaseException as e:
        print(f"异常: {e}")
    finally:
        # 停止传感器运行
        if isinstance(sensor, Sensor):
            sensor.stop()
        # 反初始化显示模块
        Display.deinit()
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        # 释放媒体缓冲区
        MediaManager.deinit()

1  
2  
3  
4  
5  
6  
7  
8  
9  
10  
11  
12  
13  
14  
15  
16  
17  
18  
19  
20  
21  
22  
23  
24  
25  
26  
27  
28  
29  
30  
31  
32  
33  
34  
35  
36  
37  
38  
39  
40  
41  
42  
43  
44  
45  
46  
47  
48  
49  
50  
51  
52  
53  
54  
55  
56  
57  
58  
59  
60  
61  
62  
63  
64  
65  
66  
67  
68  
69  
70  
71  
72  
73  
74  
75  
76  
77  
78  
79  
80  
81  
82  
83  
84  
85  
86  
87  
88  
89  
90  
91  
92  
93  
94  


运行上述程序，就可以进行对焦测试了，更多内容可以看这个API文档，也可以手动调节焦距： [Sensor 模块 API 手册 — CanMV K230](https://www.kendryte.com/k230_canmv/zh/main/zh/api/mpp/K230_CanMV_Sensor%E6%A8%A1%E5%9D%97API%E6%89%8B%E5%86%8C.html#sensor-focus-pos)

# 8 用按键将图片保存到TF卡中 ​

代码如下：

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    import time, os, sys
    
    #使用默认摄像头，可选参数:0,1,2.
    sensor_id = 2
    
    # ========== 多媒体/图像相关模块 ==========
    from media.sensor import Sensor, CAM_CHN_ID_0
    from media.display import Display
    from media.media import MediaManager
    import image
    
    # ========== GPIO/按键/LED相关模块 ==========
    from machine import Pin
    from machine import FPIOA
    
    # ========== 创建FPIOA对象并为引脚功能分配 ==========
    fpioa = FPIOA()
    fpioa.set_function(62, FPIOA.GPIO62)   # 红灯
    fpioa.set_function(20, FPIOA.GPIO20)   # 绿灯
    fpioa.set_function(63, FPIOA.GPIO63)   # 蓝灯
    fpioa.set_function(53, FPIOA.GPIO53)   # 按键
    
    # ========== 初始化LED (共阳：高电平熄灭，低电平亮) ==========
    LED_R = Pin(62, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 红灯
    LED_G = Pin(20, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 绿灯
    LED_B = Pin(63, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 蓝灯
    
    # 默认熄灭所有LED
    LED_R.high()
    LED_G.high()
    LED_B.high()
    
    # 选一个LED用来拍照提示
    PHOTO_LED = LED_G
    
    # ========== 初始化按键：按下时高电平 ==========
    button = Pin(53, Pin.IN, Pin.PULL_DOWN)
    debounce_delay = 200  # 按键消抖时长(ms)
    last_press_time = 0
    button_last_state = 0
    
    # ========== 显示配置 ==========
    DISPLAY_MODE = "LCD"   # 可选："VIRT","LCD","HDMI"
    if DISPLAY_MODE == "VIRT":
        DISPLAY_WIDTH = 1920
        DISPLAY_HEIGHT = 1080
        FPS = 30
    elif DISPLAY_MODE == "LCD":
        DISPLAY_WIDTH = 800
        DISPLAY_HEIGHT = 480
        FPS = 60
    elif DISPLAY_MODE == "HDMI":
        DISPLAY_WIDTH = 1920
        DISPLAY_HEIGHT = 1080
        FPS = 30
    else:
        raise ValueError("未知的 DISPLAY_MODE，请选择 'VIRT', 'LCD' 或 'HDMI'")
    
    def lckfb_save_jpg(img, filename, quality=95):
        """
        将图像压缩成JPEG后写入文件 (不依赖第一段 save_jpg/MediaManager.convert_to_jpeg 的写法)
        :param img:    传入的图像对象 (Sensor.snapshot() 得到)
        :param filename: 保存的目标文件名 (含路径)
        :param quality:  压缩质量 (1-100)
        """
        compressed_data = img.compress(quality=quality)
    
        with open(filename, "wb") as f:
            f.write(compressed_data)
    
        print(f"[INFO] 使用 lckfb_save_jpg() 保存完毕: {filename}")
    
    # ========== 自动创建图片保存文件夹 & 计算已有图片数量 ==========
    image_folder = "/sdcard/images"
    
    # 若不存在该目录则创建
    try:
        os.stat(image_folder)  # 尝试获取目录信息
    except OSError:
        os.mkdir(image_folder)  # 若失败则创建该目录
    
    # 统计当前目录下以 “lckfb_XX.jpg” 命名的文件数量，自动从最大编号继续
    image_count = 0
    existing_images = [fname for fname in os.listdir(image_folder)
                       if fname.startswith("lckfb_") and fname.endswith(".jpg")]
    
    if existing_images:
        # 提取编号并找出最大值
        numbers = []
        for fname in existing_images:
            # 假设文件名格式为 "lckfb_XX.jpg"
            # 取中间 XX 部分转为数字
            try:
                num_part = fname[6:11]  # "lckfb_" 长度为6，取到 ".jpg" 前还要注意下标
                numbers.append(int(num_part))
            except:
                pass
        if numbers:
            image_count = max(numbers)
    
    try:
        print("[INFO] 初始化摄像头 ...")
        sensor = Sensor(id=sensor_id)
        sensor.reset()
    
        # 在本示例中使用 VGA (640x480) 做演示
        sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, chn=CAM_CHN_ID_0)
        sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    
        # ========== 初始化显示 ==========
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=FPS)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        # ========== 初始化媒体管理器 ==========
        MediaManager.init()
    
        # ========== 启动摄像头 ==========
        sensor.run()
        print("[INFO] 摄像头已启动，进入主循环 ...")
    
        fps = time.clock()
    
        while True:
            fps.tick()
            os.exitpoint()
    
            #抓取通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            #按键处理（检测上升沿）
            current_time = time.ticks_ms()
            button_state = button.value()
    
            if button_state == 1 and button_last_state == 0:  # 上升沿
                if current_time - last_press_time > debounce_delay:
                    # LED闪烁提示
                    PHOTO_LED.low()   # 点亮LED
                    time.sleep_ms(20)
                    PHOTO_LED.high()  # 熄灭LED
    
                    # 拍照并保存
                    image_count += 1
                    filename = f"{image_folder}/lckfb_{image_count:05d}_{img.width()}x{img.height()}.jpg"
                    print(f"[INFO] 拍照保存 -> {filename}")
    
                    # 直接调用自定义的 lckfb_save_jpg() 函数
                    lckfb_save_jpg(img, filename, quality=95)
    
                    last_press_time = current_time
    
            button_last_state = button_state
    
            img.draw_string_advanced(0, 0, 32, str(image_count), color=(255, 0, 0))
            img.draw_string_advanced(0, DISPLAY_HEIGHT-32, 32, str(fps.fps()), color=(255, 0, 0))
    
            Display.show_image(img)
    
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    except BaseException as e:
        print(f"[ERROR] 出现异常: {e}")
    finally:
        if 'sensor' in locals() and isinstance(sensor, Sensor):
            sensor.stop()
        Display.deinit()
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        MediaManager.deinit()

1  
2  
3  
4  
5  
6  
7  
8  
9  
10  
11  
12  
13  
14  
15  
16  
17  
18  
19  
20  
21  
22  
23  
24  
25  
26  
27  
28  
29  
30  
31  
32  
33  
34  
35  
36  
37  
38  
39  
40  
41  
42  
43  
44  
45  
46  
47  
48  
49  
50  
51  
52  
53  
54  
55  
56  
57  
58  
59  
60  
61  
62  
63  
64  
65  
66  
67  
68  
69  
70  
71  
72  
73  
74  
75  
76  
77  
78  
79  
80  
81  
82  
83  
84  
85  
86  
87  
88  
89  
90  
91  
92  
93  
94  
95  
96  
97  
98  
99  
100  
101  
102  
103  
104  
105  
106  
107  
108  
109  
110  
111  
112  
113  
114  
115  
116  
117  
118  
119  
120  
121  
122  
123  
124  
125  
126  
127  
128  
129  
130  
131  
132  
133  
134  
135  
136  
137  
138  
139  
140  
141  
142  
143  
144  
145  
146  
147  
148  
149  
150  
151  
152  
153  
154  
155  
156  
157  
158  
159  
160  
161  
162  
163  
164  
165  
166  
167  
168  
169  
170  
171  
172  
173  
174  
175  
176  
177  
178  
179  
180  


上面代码的主要步骤是，先进行硬件的初始化；然后就开始循环采集图像并显示到3.1寸屏幕上；当按下按键后，绿灯亮一下做提醒，然后拍摄当前画面并以 JPEG 图片保存到 TF 卡中。

该代码在运行后会一直处于主循环，等待按键动作。一旦按键按下，就保存当前图像到指定路径（image_folder）下，这个路径你也可以自行修改，并继续循环拍摄显示。

前面关于LED灯，按键，摄像头在前面的章节都说过了，这里就不再重复。改变`DISPLAY_MODE` 可以改变显示设备，默认使用3.1寸屏幕。

**自定义了一个图像的保存函数** ：

python
    
    
    def lckfb_save_jpg(img, filename, quality=95):
        compressed_data = img.compress(quality=quality)
        with open(filename, "wb") as f:
            f.write(compressed_data)
        print(f"[INFO] 使用 lckfb_save_jpg() 保存完毕: {filename}")

1  
2  
3  
4  
5  


  * 从 `img` 对象中调用 `compress()` 方法，将图像压缩为 JPEG 格式数据。
  * 将压缩后的数据写入到文件中（此时已经是标准 JPEG 字节流）。
  * `quality=95` 表示压缩质量越高，图片越清晰，文件体积越大，这个也会导致明明都是800*480分辨率的图片，有的图片大，有的图片小。



**自动创建图片文件夹 以及 计算已有图片数量** ：

python
    
    
    image_folder = "/sdcard/images"
    
    # 若不存在该目录则创建
    try:
        os.stat(image_folder)
    except OSError:
        os.mkdir(image_folder)

1  
2  
3  
4  
5  
6  
7  


首先image_folder是可以自行修改的，你们也可以选择自己想要保存的路径，不需要事先创建文件夹，如果没有这个文件夹就会自动创建。`os.stat(image_folder)` 会尝试获取该路径的信息，如果抛出异常，就说明目录不存在，需要使用 `os.mkdir(image_folder)` 先创建。

python
    
    
    image_count = 0
    existing_images = [fname for fname in os.listdir(image_folder)
                       if fname.startswith("lckfb_") and fname.endswith(".jpg")]
    if existing_images:
        numbers = []
        for fname in existing_images:
            try:
                num_part = fname[6:11]  # "lckfb_" 长度为6，这里尝试取后续数字部分
                numbers.append(int(num_part))
            except:
                pass
        if numbers:
            image_count = max(numbers)

1  
2  
3  
4  
5  
6  
7  
8  
9  
10  
11  
12  
13  


  * 进入指定文件夹后，找出所有符合 `lckfb_XXXX---.jpg` 命名规则的文件，提取它们的编号（XXXXX）。这里我设定的是最多存10W张图片，在上面的案例中，第465张图片的文件名是这样的：`lckfb_00465_800x480.jpg`。
  * 将编号进行比较，找到最大值，作为本次运行的起始编号（`image_count`）。
  * 这样可以“继续”编号，避免重复覆盖之前的图片。当你重新上电时，新拍的图片不会覆盖旧的图片。



结合以上的设计，程序每次启动时，就会会自动找到之前保存的图片中最大的编号，后续拍照时继续往后递增编号，最大可以到99999。

**主循环：**

这里的实现思路和GPIO章节的按钮一样，使用 `button.value()` 获取按键状态，如果上一次是低电平，这一次是高电平，就判断为一个**按键按下** ，利用 `current_time - last_press_time > debounce_delay` 做消抖，确保按键抖动不会导致重复触发。

当确认用户按键（板子上有USR丝印的按钮）被按下时，RGB里面的绿灯会闪烁一下作为拍照的提示。生成文件名（包含编号和当前图像分辨率），然后调用自定义的 `lckfb_save_jpg` 函数来保存 JPEG图像。

之后在图像左上角绘制出当前已拍照数量。在左下脚显示当前帧率。虽然这些数字是直接在img图像上叠加的，但是并不会保存到图片文件中，因为保存图片的操作是在叠加这些数字之前的。

