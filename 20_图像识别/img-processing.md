# 图像处理

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/image-recog/img-processing.html>
> **最后更新**: 2025-01-08

---

# 1 本节介绍 ​

📝本节您将学习如何对图像进行处理，例如二值化，边缘检测，轮廓检测等。

🏆学习目标

1️⃣学会从摄像头和TF卡中获取图像。

2️⃣对摄像头获取到的图像进行直方图均衡化，矫正，二值化，边缘检测等操作。

注意

1️⃣大部分图像处理API仅支持`RGB565`或`GRAYSCALE`，使用时需要注意。

2️⃣如无特殊说明，以后所有例程的显示设备均为通过外接立创·3.1寸屏幕扩展板，在3.1寸小屏幕上显示。若用户无3.1寸屏幕扩展板也可以正常在IDE的缓冲区，只是受限于USB带宽，可能会帧率较低或卡顿。

在视觉领域中，图像处理是一项非常重要的工作。通过预处理，我们可以对图像的质量进行提升（如增强对比度、校正畸变等等），来减少一些不必要的噪声或者失真，为后续的图像分析（如目标检测、模式识别等）奠定更好的基础。

# 2 如何获取图像 ​

作为一个AI视觉开发板，我们可以直接使用随庐山派赠送的摄像头进行图像的获取，也可以直接从庐山派运行固件的TF卡中加载图像，同时该图像也可以显示在IDE中。当然以下这两种方式也不是只能二选一的，可以结合使用，比如可以先通过摄像头实时捕获图像进行前期分析，然后再通过TF卡加载标准测试图像进行后续验证与优化。

## 2.1 通过默认摄像头获取（推荐） ​

如果需要**实时处理** 或者**动态场景下的分析** ，建议选择这种方式，但是受限于环境光照，摄像头焦距等原因，可能不同的场景下做的试验会有略微差异。

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
    
    picture_width = 400
    picture_height = 240
    
    
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
        sensor = Sensor(id=sensor_id,width=1920, height=1080)
        # 重置摄像头sensor
        sensor.reset()
    
        # 无需进行镜像和翻转
        # 设置不要水平镜像
        sensor.set_hmirror(False)
        # 设置不要垂直翻转
        sensor.set_vflip(False)
    
        sensor.set_framesize(width=picture_width, height=picture_height, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为RGB565，要注意有些案例只支持GRAYSCALE格式
        sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    
        # 根据模式初始化显示器
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=60)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        # 初始化媒体管理器
        MediaManager.init()
        # 启动传感器
        sensor.run()
    
        fps = time.clock()
    
        while True:
            fps.tick()
            os.exitpoint()
    
            # 捕获通道0的图像
            src_img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            # 在屏幕左上角显示原始图像
            Display.show_image(src_img,x=0,y=0,layer = Display.LAYER_OSD0)
    
            # 图像处理放到这里
            #--------开始--------
    
            # 这里可以插入各种图像处理逻辑，例如二值化、直方图均衡化、滤波等
            # 当前示例仅仅直接显示原图，不做任何操作
    
            #--------结束--------
    
            # 在屏幕右上角显示处理后的图像
            Display.show_image(src_img,x=DISPLAY_WIDTH-picture_width,y=0,layer = Display.LAYER_OSD1)
    
            # 打印帧率到控制台
            print(fps.fps())
    
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


前面的代码在之前的章节都已经介绍过了，这里不再赘述，和之前不同的主要地方是使用了两个层，OSD0和OSD1来分别显示原图和处理过后的图像，原图显示在左上角，处理后的图像显示到右上角。这里选用的分辨率是`400x240`。而我们的3.1寸屏幕的分辨率正好是`800x400`，可以同时展示4个图像，不过我们用这里用两个来进行处理前后的对比就足够了。

## 2.2 通过TF卡来加载 ​

如果环境中**不适合使用摄像头** 或者需要**高质量的图像处理** ，建议使用这种方式。可以重复加载已有的测试图像要验证算法效果。

`Image` 类是机器视觉处理中的基础对象。此类支持从 Micropython GC、MMZ、系统堆、VB 区域等内存区域创建图像对象。此外，还可以通过引用外部内存直接创建图像（ALLOC_REF）。未使用的图像对象会在垃圾回收时自动释放，也可以手动释放内存。

支持的图像格式如下：

  * BINARY
  * GRAYSCALE
  * RGB565
  * BAYER
  * YUV422
  * JPEG
  * PNG
  * ARGB8888（新增）
  * RGB888（新增）
  * RGBP888（新增）
  * YUV420（新增）



支持的内存分配区域：

  * **ALLOC_MPGC** ：Micropython 管理的内存
  * **ALLOC_HEAP** ：系统堆内存
  * **ALLOC_MMZ** ：多媒体内存
  * **ALLOC_VB** ：视频缓冲区
  * **ALLOC_REF** ：使用引用对象的内存，不分配新内存



python
    
    
    image.Image(path, alloc=ALLOC_MMZ, cache=True, phyaddr=0, virtaddr=0, poolid=0, data=None)

1  


从文件路径 `path` 创建图像对象，支持 BMP、PGM、PPM、JPG、JPEG 格式。

python
    
    
    image.Image(w, h, format, alloc=ALLOC_MMZ, cache=True, phyaddr=0, virtaddr=0, poolid=0, data=None)

1  


创建指定大小和格式的图像对象。

  * **w** ：图像宽度
  * **h** ：图像高度
  * **format** ：图像格式
  * **alloc** ：内存分配方式（默认 ALLOC_MMZ）
  * **cache** ：是否启用内存缓存（默认启用）
  * **phyaddr** ：物理内存地址，仅适用于 VB 区域
  * **virtaddr** ：虚拟内存地址，仅适用于 VB 区域
  * **poolid** ：VB 区域的池 ID，仅适用于 VB 区域
  * **data** ：引用外部数据对象（可选）



示例：

python
    
    
    # 在 MMZ 区域创建 ARGB8888 格式的 640x480 图像
    img = image.Image(640, 480, image.ARGB8888)
    
    # 在 VB 区域创建 YUV420 格式的 640x480 图像
    img = image.Image(640, 480, image.YUV420, alloc=image.ALLOC_VB, phyaddr=xxx, virtaddr=xxx, poolid=xxx)
    
    # 使用外部引用创建 RGB888 格式的 640x480 图像
    img = image.Image(640, 480, image.RGB888, alloc=image.ALLOC_REF, data=buffer_obj)

1  
2  
3  
4  
5  
6  
7  
8  


> 这里再另外提一嘴如何把庐山派摄像头拍摄到的照片保存到TF卡中，当然您可以可直接在连接电脑后，像往U盘放照片一样存放到CanMV设备（庐山派开发板）中。

用下面这个函数就可以把摄像头拍摄到的保存到TF卡中。

python
    
    
    image.save(path[, roi[, quality=50]])

1  


将图像保存到指定路径 `path`，支持指定感兴趣区域 `roi` 及JPEG压缩质量 `quality`。

保存一个bmp图像到TF卡中从TF卡中加载该图像

Python
    
    
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
    
    picture_width = 400
    picture_height = 240
    
    
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
        sensor = Sensor(id=sensor_id,width=1920, height=1080)
        # 重置摄像头sensor
        sensor.reset()
    
        # 无需进行镜像和翻转
        # 设置不要水平镜像
        sensor.set_hmirror(False)
        # 设置不要垂直翻转
        sensor.set_vflip(False)
    
        sensor.set_framesize(width=picture_width, height=picture_height, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为RGB565
        sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    
        # 根据模式初始化显示器
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=60)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        # 初始化媒体管理器
        MediaManager.init()
        # 启动传感器
        sensor.run()
    
        # 丢掉前面100帧数据，防止摄像头还不稳定
        for i in range(100):
            sensor.snapshot()
    
        fps = time.clock()
    
        while True:
            fps.tick()
            os.exitpoint()
    
            # 捕获通道0的图像
            src_img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            # 在屏幕左上角显示原始图像
            Display.show_image(src_img,x=0,y=0,layer = Display.LAYER_OSD0)
    
            # 图像处理放到这里
            #--------开始--------
    
            # 这里可以插入各种图像处理逻辑，例如二值化、直方图均衡化、滤波等
            # 当前示例仅仅直接显示原图，不做任何操作
    
            # 保存图像到data目录下
            src_img.save("/data/src_img.bmp")
    
            break
    
            #--------结束--------
    
            # 在屏幕右上角显示处理后的图像
            Display.show_image(src_img,x=DISPLAY_WIDTH-picture_width,y=0,layer = Display.LAYER_OSD1)
    
            # 打印帧率到控制台
            print(fps.fps())
    
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


Python
    
    
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
    
    
    picture_width = 400
    picture_height = 240
    
    
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
        # 根据模式初始化显示器
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=60)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        # 初始化媒体管理器
        MediaManager.init()
    
        fps = time.clock()
    
        # 从TF卡中加载之前保存的图像
    
        src_img = image.Image("/data/src_img.bmp")
    
        # 在屏幕左上角显示原始图像
        Display.show_image(src_img,x=0,y=0,layer = Display.LAYER_OSD0)
    
        while True:
            fps.tick()
            os.exitpoint()
    
            # 图像处理放到这里
            #--------开始--------
    
            #这里以直方图均衡化作为案例，做简单的亮度/对比度增强操作。
            src_img.histeq()
    
            #--------结束--------
    
            # 在屏幕右上角显示处理后的图像
            Display.show_image(src_img,x=DISPLAY_WIDTH-picture_width,y=0,layer = Display.LAYER_OSD1)
    
            # 打印帧率到控制台
            print(fps.fps())
    
    except KeyboardInterrupt as e:
        print("用户停止: ", e)
    except BaseException as e:
        print(f"异常: {e}")
    finally:
    
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


我们首先执行第一个程序`保存一个bmp图像到TF卡中`，找到一个比较暗的场景，做一个图片的保存。程序运行成功后我们就可以在我们`CanMV`设备的`data`目录下看到新保存的图像。 ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250103_152234.png)  
实际保存到庐山派中的照片如下图所示，大家可以自行拍摄自己需要的照片，或者直接使用网上的图片： ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250103_152709.png)

既然有了图片，我们就可以运行第二个程序了，从TF卡中加载该图像。在IDE中执行效果如下图所示。

![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250103_153433.png)

在这个图片中，我们可以看到原图和直方图均衡话后图像的对比信息，均衡化后的图像比原图更清晰，更多内容会在本文后续内容中继续介绍。

注意！

在后续的案例中，我们都不使用TF卡来加载图片，而是直接使用上面推荐的方式【通过默认摄像头】来获取图像。

# 3 直方图均衡化 ​

其主要功能是增强图像的对比度，使图像的直方图分布更均匀。直方图是一种统计图表，用来显示图像中不同亮度值的分布情况。横轴表示亮度值（0-255），纵轴表示对应亮度值的像素数量。原图中，亮度可能分布不均，导致对比度不足或细节丢失。直方图均衡化通过调整像素亮度分布，让图像的亮度范围更均匀，增强对比度和细节。

基础的框架代码代码已经在前面提供，可以同时显示处理前和处理后的图像，方便大家进行原图和处理过后的图的对比。我们只需要把`2.1 通过默认摄像头获取（推荐）`中的代码复制到K230 IDE中，然后在代码的图像处理中间插入对图像进行的函数就可以了。

API

[`histeq`](https://developer.canaan-creative.com/k230_canmv/zh/main/zh/api/openmv/image.html#histeq)

python
    
    
    image.histeq([adaptive=False[, clip_limit=-1[, mask=None]]])

1  


对图像执行直方图均衡化，使图像的对比度和亮度标准化。 若 `adaptive=True`，则启用自适应直方图均衡化方法，通常比非自适应方法效果更好，但处理速度较慢。 `clip_limit` 用于控制自适应直方图均衡化的对比度，较小的值（如 10）可以生成对比度受限的图像。 `mask` 是用于像素级操作的掩码图像，必须是黑白图像，且大小需与当前图像相同。仅掩码中白色像素对应的区域会被修改。 返回处理后的图像对象，允许后续调用其他方法。 不支持压缩图像和 Bayer 格式图像。

python
    
    
    src_img.histeq()

1  


代码的添加位置及在IDE中的运行效果如下图所示： ![图 4](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250103_160226.png)

从上图IDE中的帧缓冲区可以看到，左上角是原图，右上角是经过直方图均衡化后的图像，大家可以自行试试，用鼠标把原图和处理后的图像分别框选起来，帧缓冲区下方的直方图就能清晰的显示出他们的直方图了。 ![图 5](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250103_170909.gif)

从这个动图里面就能看出来，原图（左侧）的直方图分布较为不均，许多像素值集中在特定的亮度范围内，有一些细节由于光纤不足被影藏，特别是暗部区域。均衡话后的图（右侧）像素值分布变得更加均匀。红、绿、蓝三通道的分布被拉伸到更大的范围，使得图像的整体亮度更高、对比度更强。

# 4 图像矫正 ​

## 4.1 Gamma 校正 ​

Gamma 校正是为了调整图像的亮度和对比度而引入的非线性变换。简单来说，当 Gamma 值 **> 1** 时，图像的中间灰度变亮，高亮区域对比度变强；当 Gamma 值 **< 1** 时，图像的中间灰度变暗，整体对比会变弱。我们的人眼对亮度和颜色的感知并不是线性的，这个Gamma 校正可以根据人眼视觉特性来调整图像，看起来会更贴近我们想要的视觉效果。

python
    
    
    src_img.gamma_corr(3)

1  


实际运行效果如下，还是和之前一样，在在代码的图像处理中间插入对图像进行的函数就可以了。 ![图 6](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250103_173956.png) 这个截图中，我把下面直方图的色彩空间改为了LAB色彩空间，可以看到经过Gamma矫正后的图像中的L(也就是亮度)变得高了。

## 4.2 旋转校正 ​

旋转校正的主要作用是对图像中因为拍摄角度或姿态变化带来的旋转进行矫正，以使后续的识别和测量更加准确。一般只需要设定一个大概的额旋转矫正角度（弧度值）就行。

API

[`rotation_corr`](https://developer.canaan-creative.com/k230_canmv/zh/main/zh/api/openmv/image.html#rotation-corr)

python
    
    
    img.rotation_corr([x_rotation=0.0[, y_rotation=0.0[, z_rotation=0.0[, x_translation=0.0[, y_translation=0.0[, zoom=1.0[, fov=60.0[, corners]]]]]]]])

1  


通过对帧缓冲区进行 3D 旋转，来校正图像中的透视问题。

  * **参数说明** ： 
    * `x_rotation`：图像绕 x 轴旋转的角度（上下旋转）。
    * `y_rotation`：图像绕 y 轴旋转的角度（左右旋转）。
    * `z_rotation`：图像绕 z 轴旋转的角度（图像朝向的调整）。
    * `x_translation`：图像旋转后沿 x 轴移动的单位，单位为 3D 空间的单位。
    * `y_translation`：图像旋转后沿 y 轴移动的单位，单位为 3D 空间的单位。
    * `zoom`：图像缩放倍数，默认为 1.0。
    * `fov`：用于 2D->3D 投影的视场。当此值接近 0 时，图像位于视口无限远处；当此值接近 180 时，图像位于视口中。通常情况下不建议更改此值，但可以通过调整它来改变 2D->3D 映射效果。
    * `corners`：包含四个 `(x, y)` 元组的列表，表示四个角点，用于创建四点对应单应性，将第一个角点映射到 (0,0)，第二个角点映射到 (image_width-1, 0)，第三个角点映射到 (image_width-1, image_height-1)，第四个角点映射到 (0, image_height-1)。此参数允许用户使用 `rotation_corr` 实现鸟瞰图转换。



该方法返回图像对象，以便用户可以继续调用其他方法。

python
    
    
    src_img.rotation_corr(0.5)

1  


实际运行效果如下，还是和之前一样，在在代码的图像处理中间插入对图像进行的函数就可以了。 ![图 7](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250103_180126.png)  
当然，我这里的图像本来就是正确的，做了这个旋转校正后反而变得更不正确了。在实际应用中，比如在检测条码、二维码或标志物时，一定角度的旋转会影响识别效果，我们就可以在预处理环节进行旋转校正。

## 4.3 镜头畸变校正 ​

在摄像头拍摄过程中，由于镜头的光学特性，一些超广角镜头或低成本镜头常常会带来“桶形”或“枕形”等畸变（**我们庐山派随板子附赠的摄像头都是无畸变的摄像头，用不到这个函数，如果各位更换了广角镜头就需要使用这个函数来处理了** ），从而导致图像在边缘部分看起来被拉伸或压缩。通过镜头畸变校正函数，可以在一定程度上减小这种畸变带来的影响，让图像更为接近真实的效果。

API

[`lens_corr`](https://developer.canaan-creative.com/k230_canmv/zh/main/zh/api/openmv/image.html#lens-corr)
    
    
    image.lens_corr([strength=1.8[, zoom=1.0]])

1  


该方法用于进行镜头畸变校正，以消除镜头导致的鱼眼效果。

  * **参数说明** ： 
    * `strength`：浮点数，决定去除鱼眼效果的程度。默认值为 1.8，用户可以根据图像效果进行调整。
    * `zoom`：浮点数，用于图像缩放，默认值为 1.0。



该方法返回图像对象，以便用户可以继续调用其他方法。

python
    
    
    src_img.lens_corr(3.0)

1  


实际运行效果如下，还是和之前一样，在在代码的图像处理中间插入对图像进行的函数就可以了。实际不应该这样处理，因为随我们板子附送的摄像头的镜头是无畸变的，可以看到右边矫正后的图像反而不正常了，后面的箱子都变形倾斜了。

![图 8](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250106_152813.png)

# 5 二值化 ​

二值化是图像处理和计算机视觉中最基础、最常见的操作之一。它的目标是将彩色或灰度图像简化为仅有黑白两种像素的图像，便于后续提取轮廓、检测目标形状、做OCR文本识别等。比如在识别二维码、条码场景里面，二值化能极大地简化处理难度，降低处理器压力，提高识别速度。

最常见的二值化方法就是我们手动指定一个阈值，低于该阈值的像素变为黑，高于该阈值的像素变为白（或者反过来）。其他自定义阈值等属于高级的用法，这里暂不介绍。

API

[`binary`](https://developer.canaan-creative.com/k230_canmv/zh/main/zh/api/openmv/image.html#binar)

python
    
    
    image.binary(thresholds[, invert=False[, zero=False[, mask=None]]])

1  


根据指定的阈值列表 `thresholds`，将图像中的所有像素转换为黑白二值图像。

  * `thresholds`：为一个元组列表，格式为 `[(lo, hi), ...]`。对于灰度图像，每个元组定义一个灰度值范围（最低值和最高值）；对于 RGB565 图像，每个元组包含六个值，分别表示 LAB 空间中 L、A 和 B 通道的范围。
  * `invert`：如果设为 True，则反转阈值操作，将阈值之外的像素转换为白色。
  * `zero`：如果设为 True，则将匹配阈值的像素设置为零，而保留其余像素。
  * `mask`：应用于二值化操作的掩码图像。掩码应为二值图像，且尺寸与目标图像相同。



返回图像对象，以便后续方法可以链式调用。

该方法不支持压缩图像和 Bayer 格式图像。

我们实际设定的阈值如下：

python
    
    
    src_img.binary([(120, 255)],invert=False)  # 二值化，阈值范围为 (100, 255),不进行像素值反转

1  


同时，本案例使用的图片格式为灰度格式，需要设置摄像头的输出像素格式为`GRAYSCALE`，实际运行效果如下图：

![图 9](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250106_161823.png)

# 6 边缘检测 ​

边缘检测是对图像进行高通滤波或微分运算，以突出像素值变化剧烈的区域，从而获取目标物体的边缘信息。通常是后续的形状分析、轮廓检测或特征提取的关键一步。在不需要识别颜色的场景下，往往先做灰度化，然后再做边缘检测。

6.1`Canny 边缘检测算法`和6.2`简单的阈值高通滤波算法`使用同一个API，只是参数不一样而已。记得设置摄像头的输出像素格式为`GRAYSCALE`。

API

`find_edges`

python
    
    
    image.find_edges(edge_type[, threshold])

1  


该函数将图像转换为黑白图像，仅保留边缘为白色像素。

  * `edge_type` 可选值包括：

    * `image.EDGE_SIMPLE` \- 简单的阈值高通滤波算法
    * `image.EDGE_CANNY` \- Canny 边缘检测算法
  * `threshold` 是包含低阈值和高阈值的二元元组。您可以通过调整该值来控制边缘质量，默认设置为 `(100, 200)`。




**注意：** 此方法仅支持灰度图像。

## 6.1 Canny 边缘检测算法 ​

python
    
    
    src_img.find_edges(image.EDGE_CANNY, threshold=(50, 80))

1  


实际运行效果如下，还是和之前一样，在在代码的图像处理中间插入对图像进行的函数就可以了。 ![图 11](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250106_171305.png)

## 6.2 简单的阈值高通滤波算法 ​

python
    
    
    src_img.find_edges(image.EDGE_SIMPLE, threshold=(50, 255))

1  


实际运行效果如下，还是和之前一样，在在代码的图像处理中间插入对图像进行的函数就可以了。这个的效果就没有Canny算法好了。 ![图 12](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250106_171454.png)

## 6.3 拉普拉斯核 ​

API

`laplacian`

python
    
    
    image.laplacian(size[, sharpen=False[, mul[, add=0[, threshold=False[, offset=0[, invert=False[, mask=None]]]]]]])

1  


通过拉普拉斯核进行边缘检测，对图像进行卷积。

**参数说明：**

  * **size** ：内核的大小，取值为1（3x3内核）、2（5x5内核）或更高。
  * **sharpen** ：若设置为`True`，则对图像进行锐化，而非仅输出未经过阈值处理的边缘图像。
  * **mul** ：用于与卷积结果相乘的数字，若不设置，则使用默认值以防止卷积输出缩放。
  * **add** ：用于与每个像素的卷积结果相加的数值。



`mul`可用于全局对比度调整，`add`可用于全局亮度调整。

**返回值** ：返回图像对象，以便进一步调用其他方法。

**注意** ：不支持压缩图像和Bayer图像。

python
    
    
    src_img.laplacian(2,mul=0.2)  # 拉普拉斯边缘检测，窗口大小为 3

1  


实际运行效果如下图所示：

![图 10](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-processing/img-processing_20250106_170919.png)

