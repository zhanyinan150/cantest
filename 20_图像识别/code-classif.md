# 码类识别

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/image-recog/code-classif.html>
> **最后更新**: 2025-01-18

---

# 1 本节介绍 ​

📝本节您将学习如何将进行码类识别。

🏆学习目标

1️⃣如何用庐山派去识别一维码和二维码

2️⃣如何用庐山派去识别机器码（AprilTag）

如今，手机扫码条形码的应用几乎无处不在。从购物支付到物流追踪、从图书管理到停车支付，二维码已经成为我们日常生活中不可或缺的一部分。

在码类识别的世界中，常见的有一维码、二维码、机器码（比如AprilTag等，它们都是通过不同的编码方式来表示数据的。每种码都有不同的特点和应用场景。

**一维码** ：最常见的条形码，通常由黑白条纹组成，信息只能按一行排列。应用于商品管理，比如超市的商品扫描、库存管理等。如果你仔细观察的话会发现其实微信和支付宝虽然最常用的是二维码，但是上面也有一个一维码，也可以起到支付作用。

**二维码** ：二维码通过黑白方块组成一个矩阵，信息可以在二维空间中排列，容量比一维码大，能够存储更多的信息。二维码广泛应用于支付、车票、名片、网址等场景，如支付宝、微信支付扫码支付。

**机器码（AprilTag）** ：这种二维码特别适用于计算机视觉和机器人领域，它通过精确的定位和标识来帮助机器识别和定位物体。常用在机器人导航、增强现实（AR）等领域。

**DataMatrix码** ：类似于二维码，形状通常是正方形，且能够在较小的空间内存储更多的信息。它常用于小型物品的标识，适合空间有限但需要编码大量信息的场景。

一维码适合简单商品信息存储，二维码适合大容量信息存储和快速扫描，机器码适合机器人定位和视觉识别，DataMatrix码则适用于空间小且需要大量数据编码的场景。

码类识别通常使用灰度图而不适用彩色图，主要是为了简化计算来提升效率，而且本来这些码类通常是黑白或单一颜色的对比图形，识别过程主要依赖于黑白色块之间的对比度。灰度图保留了图像的亮度信息（就是黑白的对比度），不需要关注颜色差异。

# 2 识别一维码 ​

生活中最常见的一维码就是你购买各种商品上的条形码了，它是一种通过不同宽度的黑白条纹来表示信息的图形标识符。条形码通常是由不同宽度的黑条和白条交替排列（黑色吸收光中的所有颜色，而白色反射光中的所有颜色，对比度很高，最常用），并按照特定的编码规则设计。商品上的条形码能够携带商品的各种信息，当然我们也可以直接将字节信息直接编入一维码中。

除了特定的商用场景会用到激光扫描器，在日常生活中我们最常用的就是用手机来扫描一维码，手机通过摄像头拍摄条形码，并将其图像输入到条码识别软件中。软件首先会对图像进行处理，识别出其中的黑条和白条。由于黑条吸光而白条反射光，条形码的扫描器（在手机中由摄像头和软件共同完成）通过光电转换将光信号转化为电信号。这些电信号的强度差异反映了条形码中每个黑条和白条的不同宽度。

通过对这些信号进行分析，手机的应用程序能够根据条形码的编码规则识别出对应的数字或字母信息。识别过程不仅仅是转换图形，还会根据条形码标准来对数据进行解码。你可以把庐山派的识别过程按照上面这个理解，我们随板子附赠的gc2093摄像头动态性能和成像效果都是很不错的。

提示

庐山派标配的是[立创·GC2093-200W摄像头-小镜头版](https://item.szlcsc.com/44319753.html)，是固定焦距的，无法自动对焦。

如果需要对近距离或者远距离的图像进行识别，请购买[立创·GC2093-200W摄像头-大镜头版-可旋转调焦](https://item.szlcsc.com/44319754.html)进行手动旋转对焦。

或者如果你确实不喜欢小镜头的定焦，直接大力出奇迹进行旋转把固定镜头螺纹用的胶水破坏掉(有可能需要借助外部工具)，你就可以用小镜头来调焦了。

API

**find_barcodes**
    
    
     image.find_barcodes([roi])

1  


该函数查找指定 ROI 内的所有一维条形码，并返回一个包含 `image.barcode` 对象的列表。有关更多信息，请参考 `image.barcode` 对象的相关文档。

为了获得最佳效果，建议使用长为 640 像素、宽为 40/80/160 像素的窗口。窗口的垂直程度越低，运行速度越快。由于条形码是线性一维图像，因此在一个方向上需具有较高分辨率，而在另一个方向上可具有较低分辨率。请注意，该函数会进行水平和垂直扫描，因此您可以使用宽为 40/80/160 像素、长为 480 像素的窗口。请务必调整镜头，使条形码位于焦距最清晰的区域。模糊的条形码无法解码。

该函数支持以下所有一维条形码：

  * `image.EAN2`
  * `image.EAN5`
  * `image.EAN8`
  * `image.UPCE`
  * `image.ISBN10`
  * `image.UPCA`
  * `image.EAN13`
  * `image.ISBN13`
  * `image.I25`
  * `image.DATABAR (RSS-14)`
  * `image.DATABAR_EXP (RSS-Expanded)`
  * `image.CODABAR`
  * `image.CODE39`
  * `image.PDF417`
  * `image.CODE93`
  * `image.CODE128`
  * `roi` 是用于指定感兴趣区域的矩形元组 `(x, y, w, h)`。若未指定，ROI 默认为整个图像。操作范围仅限于该区域内的像素。



**注意：** 不支持压缩图像和 Bayer 图像。

代码如下：

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
    import time, math, os, gc, sys
    
    picture_width = 640
    picture_height = 480
    
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
    
    # 自定义函数，用于返回条形码类型名称
    def barcode_name(code):
        # 判断条形码类型并返回对应的名称
        if(code.type() == image.EAN2):
            return "EAN2"
        if(code.type() == image.EAN5):
            return "EAN5"
        if(code.type() == image.EAN8):
            return "EAN8"
        if(code.type() == image.UPCE):
            return "UPCE"
        if(code.type() == image.ISBN10):
            return "ISBN10"
        if(code.type() == image.UPCA):
            return "UPCA"
        if(code.type() == image.EAN13):
            return "EAN13"
        if(code.type() == image.ISBN13):
            return "ISBN13"
        if(code.type() == image.I25):
            return "I25"
        if(code.type() == image.DATABAR):
            return "DATABAR"
        if(code.type() == image.DATABAR_EXP):
            return "DATABAR_EXP"
        if(code.type() == image.CODABAR):
            return "CODABAR"
        if(code.type() == image.CODE39):
            return "CODE39"
        if(code.type() == image.PDF417):
            return "PDF417"
        if(code.type() == image.CODE93):
            return "CODE93"
        if(code.type() == image.CODE128):
            return "CODE128"
    
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
    
        # 设置通道0的输出尺寸
        sensor.set_framesize(width=picture_width, height=picture_height, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为GRAYSCALE(灰度)
        sensor.set_pixformat(Sensor.GRAYSCALE, chn=CAM_CHN_ID_0)
        #sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    
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
    
        # 创建一个FPS计时器，用于实时计算每秒帧数
        fps = time.clock()
    
        while True:
            os.exitpoint()
            # 更新FPS计时
            fps.tick()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            # 遍历捕获到的所有条形码
            for code in img.find_barcodes():
                # 在图像上绘制条形码矩形框
                img.draw_rectangle([v for v in code.rect()], color=(255, 0, 0),thickness=5)
                # 打印条形码信息，包括类型、内容、旋转角度、质量和FPS
                print_args = (barcode_name(code), code.payload(), (180 * code.rotation()) / math.pi, code.quality(), fps.fps())
                img.draw_string_advanced(10,0,32,code.payload())
                print("Barcode %s, Payload \"%s\", rotation %f (degrees), quality %d, FPS %f" % print_args)
    
            # 显示捕获的图像，中心对齐，居中显示
            Display.show_image(img, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))
    
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


还是准备好图片，你也可以自行去找一个一维码： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/code-classif/code-classif_20250117_142823.png)  
运行效果如下图所示： ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/code-classif/code-classif_20250117_143235.png)

# 3 识别二维码 ​

二维码的出现是为了解决传统一维条形码信息量有限的问题，二维码具有存储更多信息的优势。它的应用范围从商品追踪到支付系统，尤其在移动支付中得到了广泛的应用。现在中国如此发达的交易支付体系离不开二维码的普及，尤其是在扫码支付上，二维码几乎成为了每个商户的标配。二维码通过黑白模块组成矩阵，可以存储更大量的信息。它的优势在于能够容纳的字符远多于传统的一维条形码，且识别速度快，扫描精度高，极大地方便了日常生活中的支付、社交、信息交换等。

二维码由多个黑白方块组成，每个模块代表一个数据单元，排列成一个正方形的网格结构。二维码不仅可以存储数字信息，还可以存储字母、汉字甚至网址等信息。二维码的解析不仅要识别出二维码的位置和方向，还需要解码其中包含的数据。所以在识别二维码时就需要首先对**图像处理** （本章不涉及，直接使用一条函数解决处理和识别），比如边缘检测，二值化等；然后要**定位二维码** 的位置，一般的二维码都有三个定位位置；最后就是解码了。

提示

庐山派标配的是[立创·GC2093-200W摄像头-小镜头版](https://item.szlcsc.com/44319753.html)，是固定焦距的，无法自动对焦。

如果需要对近距离或者远距离的图像进行识别，请购买[立创·GC2093-200W摄像头-大镜头版-可旋转调焦](https://item.szlcsc.com/44319754.html)进行手动旋转对焦。

或者如果你确实不喜欢小镜头的定焦，直接大力出奇迹进行旋转把固定镜头螺纹用的胶水破坏掉(有可能需要借助外部工具)，你就可以用小镜头来调焦了。

API

**find_qrcodes**

python
    
    
    image.find_qrcodes([roi])

1  


该函数查找指定 ROI 内的所有二维码，并返回一个包含 `image.qrcode` 对象的列表。有关更多信息，请参考 `image.qrcode` 对象的相关文档。

为了确保该方法成功运行，图像上的二维码需尽量平展。可以通过使用 `sensor.set_windowing` 函数在镜头中心放大、使用 `image.lens_corr` 函数消除镜头的桶形畸变，或更换视野较窄的镜头，获得不受镜头畸变影响的平展二维码。部分机器视觉镜头不产生桶形失真，但其成本高于 OpenMV 提供的标准镜头，这些镜头为无畸变镜头。

  * `roi` 是用于指定感兴趣区域的矩形元组 `(x, y, w, h)`。若未指定，ROI 默认为整个图像。操作范围仅限于该区域内的像素。



**注意：** 不支持压缩图像和 Bayer 图像。

我们的庐山派在下单时选择了嘉立创二维码，在这里介绍一下嘉立创二维码：

> 嘉立创利用字符喷印技术及后台数据管理能力，在**每一片PCB板上都打上唯一序列号的二维码** 。此二维码不仅可以用于产品溯源，而且终端客户还可以通过微信扫码来查看电子产品的使用说明书。我们为客户提供了全方位的后台编辑功能，并决定将此功能命名为 “嘉立创二维码”。

所以现在各位手里的庐山派其实都是唯一的，每个人板子上的二维码虽然都指向了同一个网址，但是每个人的后缀是不一样的，大家可以自行扫扫看。但是该二维码尺寸较小且庐山派送的这个镜头还不支持变焦。所以目前庐山派用自己的摄像头还识别不了庐山派板子上的这个二维码。

代码如下：

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
    import time, math, os, gc, sys
    
    picture_width = 800
    picture_height = 480
    
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
    
    # 自定义函数，用于返回条形码类型名称
    def barcode_name(code):
        # 判断条形码类型并返回对应的名称
        if(code.type() == image.EAN2):
            return "EAN2"
        if(code.type() == image.EAN5):
            return "EAN5"
        if(code.type() == image.EAN8):
            return "EAN8"
        if(code.type() == image.UPCE):
            return "UPCE"
        if(code.type() == image.ISBN10):
            return "ISBN10"
        if(code.type() == image.UPCA):
            return "UPCA"
        if(code.type() == image.EAN13):
            return "EAN13"
        if(code.type() == image.ISBN13):
            return "ISBN13"
        if(code.type() == image.I25):
            return "I25"
        if(code.type() == image.DATABAR):
            return "DATABAR"
        if(code.type() == image.DATABAR_EXP):
            return "DATABAR_EXP"
        if(code.type() == image.CODABAR):
            return "CODABAR"
        if(code.type() == image.CODE39):
            return "CODE39"
        if(code.type() == image.PDF417):
            return "PDF417"
        if(code.type() == image.CODE93):
            return "CODE93"
        if(code.type() == image.CODE128):
            return "CODE128"
    
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
    
        # 设置通道0的输出尺寸
        sensor.set_framesize(width=picture_width, height=picture_height, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为GRAYSCALE(灰度)
        sensor.set_pixformat(Sensor.GRAYSCALE, chn=CAM_CHN_ID_0)
        #sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    
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
    
        # 创建一个FPS计时器，用于实时计算每秒帧数
        fps = time.clock()
    
        while True:
            os.exitpoint()
            # 更新FPS计时
            fps.tick()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            # 遍历捕获到的所有条形码
            for code in img.find_qrcodes():
                # 在图像上绘制条形码矩形框
                rect = code.rect()
                img.draw_rectangle([v for v in rect], color=(255, 0, 0), thickness = 5)
                # 打印条形码信息
                img.draw_string_advanced(rect[0], rect[1], 32, code.payload())
                print(code)
            # 显示捕获的图像，中心对齐，居中显示
            Display.show_image(img, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))
    
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


图片如下：

![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/code-classif/code-classif_20250117_145653.png)

实际运行效果如下图所示: ![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/code-classif/code-classif_20250117_155332.png)

# 4 识别机器码（AprilTag） ​

AprilTag是一种基于二维码的视觉标记系统，最早是由麻省理工学院（MIT）在2008年开发的。AprilTag的设计目标是为机器人视觉系统提供快速、可靠的视觉标记，特别适用于定位和跟踪应用。相比二维码和一维码，AprilTag有着更高的精度和更强的鲁棒性。

AprilTag通过在正方形的图案中使用独特的黑白图案来进行信息编码。与二维码不同，AprilTag的特点是它通过**不同的标记ID** 来标识不同的标签，本身还包括误差修正能力，能够在一定程度上适应环境噪声和图像变形。一个AprilTag一般由一个黑色的边框和一组内部的二进制编码区域组成。每个AprilTag的编码方式是不同的，也就是说每个标记都有唯一的ID。

提示

庐山派标配的是[立创·GC2093-200W摄像头-小镜头版](https://item.szlcsc.com/44319753.html)，是固定焦距的，无法自动对焦。

如果需要对近距离或者远距离的图像进行识别，请购买[立创·GC2093-200W摄像头-大镜头版-可旋转调焦](https://item.szlcsc.com/44319754.html)进行手动旋转对焦。

或者如果你确实不喜欢小镜头的定焦，直接大力出奇迹进行旋转把固定镜头螺纹用的胶水破坏掉(有可能需要借助外部工具)，你就可以用小镜头来调焦了。

API

**find_apriltags**
    
    
     image.find_apriltags([roi[, families=image.TAG36H11[, fx[, fy[, cx[, cy]]]]]])

1  


该函数查找指定 ROI 内的所有 AprilTag，并返回一个包含 `image.apriltag` 对象的列表。有关更多信息，请参考 `image.apriltag` 对象的相关文档。

与二维码相比，AprilTags 可以在更远的距离、较差的光照条件和更扭曲的图像环境下被有效检测。AprilTags 能够应对各种图像失真问题，而二维码则不能。因此，AprilTags 仅将数字 ID 编码作为其有效载荷。

此外，AprilTags 还可用于定位。每个 `image.apriltag` 对象将返回其三维位置信息和旋转角度。位置信息由 `fx`、`fy`、`cx` 和 `cy` 决定，分别表示图像在 X 和 Y 方向上的焦距和中心点。

> 可以使用 OpenMV IDE 内置的标签生成器工具创建 AprilTags。该工具可生成可打印的 8.5”x11” 格式的 AprilTags。

  * `roi` 是用于指定感兴趣区域的矩形元组 `(x, y, w, h)`。若未指定，ROI 默认为整个图像。操作范围仅限于该区域内的像素。
  * `families` 是要解码的标签家族的位掩码，以逻辑或形式表示： 
    * `image.TAG16H5`
    * `image.TAG25H7`
    * `image.TAG25H9`
    * `image.TAG36H10`
    * `image.TAG36H11`
    * `image.ARTOOLKIT`



默认设置为最常用的 `image.TAG36H11` 标签家族。请注意，启用每个标签家族都会稍微降低 `find_apriltags` 的速度。

  * `fx` 是以像素为单位的相机 X 方向的焦距。标准 OpenMV Cam 的值为 ((2.8 / 3.984) \times 656)，该值通过毫米计的焦距值除以 X 方向上感光元件的长度，再乘以 X 方向上感光元件的像素数量（针对 OV7725 感光元件）。
  * `fy` 是以像素为单位的相机 Y 方向的焦距。标准 OpenMV Cam 的值为 ((2.8 / 2.952) \times 488)，该值通过毫米计的焦距值除以 Y 方向上感光元件的长度，再乘以 Y 方向上感光元件的像素数量（针对 OV7725 感光元件）。
  * `cx` 是图像的中心，即 `image.width()/2`，而非 `roi.w()/2`。
  * `cy` 是图像的中心，即 `image.height()/2`，而非 `roi.h()/2`。



**注意：** 不支持压缩图像和 Bayer 图像。

目前庐山派支持以下这六种Tags：

  * **image.TAG16H5** ：【30个标签】这种标签使用16个模块（16x5的阵列），是一个小型的二维码标签格式，适合于中等复杂度的应用。它的编码密度较低，但识别相对简单。

  * **image.TAG25H7** ：【242个标签】该标签由25个模块组成，阵列大小为25x7。由于模块数更多，它可以存储更多信息，适合需要高存储容量的应用。

  * **image.TAG25H9** ：【35个标签】类似于TAG25H7，但它的阵列大小为25x9，相比之下，能存储更多的信息，适用于需要较高编码密度的应用场景。

  * **image.TAG36H10** ：【2320个标签】这种标签是36x10的阵列，拥有更多的模块，使得它可以存储更复杂的数据，适用于信息量较大的应用场景。

  * （建议大家优先用这个）**image.TAG36H11** ：【0-586】这种标签类似于TAG36H10，但它是36x11阵列，具有更高的编码密度和更大的存储容量，适合于需要非常高精度的识别或更多数据存储的应用。

  * **image.ARTOOLKIT** ：【512个标签】这个是指使用ARToolKit框架下的标签，这些标签一般是特定的增强现实标记，专门用于AR应用的定位和识别。




* * *

**如何生成标签？** 打开IDE,单击【工具】-》【机器视觉】-》【AprilTag生成器】-》【TAG36H11家族】 ![图 6](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/code-classif/code-classif_20250117_172051.png)  
在弹出来的对话框中你就可以选择具体需要几张标签图像了，获取后可以放在屏幕上识别，也可以打印出来贴在你需要识别的地方。 ![图 7](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/code-classif/code-classif_20250117_172218.png)  
单击OK后会需要选择要生成的文件夹路径，自行选择并记得就好。

代码如下：

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
    import time, math, os, gc, sys
    
    picture_width = 800
    picture_height = 480
    
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
    
    # 注意！与find_qrcodes不同，find_apriltags方法无需对图像进行镜像校正。
    
    # apriltag代码支持最多6个标签族，可以同时处理多个标签。
    # 返回的标签对象将包含标签族和标签族内的标签ID。
    
    tag_families = 0
    #tag_families |= image.TAG16H5 # comment out to disable this family
    #tag_families |= image.TAG25H7 # comment out to disable this family
    #tag_families |= image.TAG25H9 # comment out to disable this family
    #tag_families |= image.TAG36H10 # comment out to disable this family
    tag_families |= image.TAG36H11 # comment out to disable this family (default family)
    #tag_families |= image.ARTOOLKIT # comment out to disable this family
    
    # 标签族之间有什么区别？例如，TAG16H5标签族是一个4x4的正方形标签，
    # 这意味着它可以在较远的距离检测到，而TAG36H11标签族是6x6的正方形标签。
    # 然而，较低的H值（H5相比H11）意味着4x4标签的误识别率要比6x6标签高得多。
    # 所以，除非有特定需要，否则使用默认的TAG36H11标签族。
    
    def family_name(tag):
        if(tag.family() == image.TAG16H5):
            return "TAG16H5"
        if(tag.family() == image.TAG25H7):
            return "TAG25H7"
        if(tag.family() == image.TAG25H9):
            return "TAG25H9"
        if(tag.family() == image.TAG36H10):
            return "TAG36H10"
        if(tag.family() == image.TAG36H11):
            return "TAG36H11"
        if(tag.family() == image.ARTOOLKIT):
            return "ARTOOLKIT"
    
    
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
    
        # 设置通道0的输出尺寸
        sensor.set_framesize(width=picture_width, height=picture_height, chn=CAM_CHN_ID_0)
        # 设置通道0的输出像素格式为GRAYSCALE(灰度)
        sensor.set_pixformat(Sensor.GRAYSCALE, chn=CAM_CHN_ID_0)
        #sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    
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
    
        # 创建一个FPS计时器，用于实时计算每秒帧数
        fps = time.clock()
    
        while True:
            os.exitpoint()
            # 更新FPS计时
            fps.tick()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            # 查找并处理AprilTag标签
            for tag in img.find_apriltags(families=tag_families):
                # 在图像中绘制标签的矩形框
                img.draw_rectangle([v for v in tag.rect()], color=(255, 0, 0))
                # 在标签中心绘制十字
                img.draw_cross(tag.cx(), tag.cy(), color=(0, 255, 0))
                # 显示标签ID
                img.draw_string_advanced(tag.cx(), tag.cy(), 32, str(tag.id()))
                print_args = (family_name(tag), tag.id(), (180 * tag.rotation()) / math.pi)
                print("Tag Family %s, Tag ID %d, rotation %f (degrees)" % print_args)
    
            # 显示捕获的图像，中心对齐，居中显示
            Display.show_image(img, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))
    
            gc.collect()
    
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


还是先准备好图片: ![图 5](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/code-classif/code-classif_20250117_162430.png)

运行效果如下： ![图 4](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/code-classif/code-classif_20250117_162322.png)

识别速度比较慢，会比较卡，如果需要更高帧率的只能降低我们的摄像头分辨率了。

