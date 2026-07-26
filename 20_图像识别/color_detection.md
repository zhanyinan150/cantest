# 颜色识别

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/image-recog/color_detection.html>
> **最后更新**: 2025-01-18

---

# 1 本节介绍 ​

📝本节您将学习如何将摄像头采集到的画面根据选定的颜色阈值进行色块寻找，再搭配之前在【图像绘制】章节中介绍的绘制操作就可以在画面中指示出这些色块。

🏆学习目标

1️⃣如何用庐山派去寻找色块。

2️⃣如何调节阈值。

# 2 颜色识别的基本原理 ​

颜色识别的核心是通过分析图像中像素点的颜色信息，从中找到符合特定颜色范围的区域。这个过程一般有以下几个步骤：

  1. **获取图像** ：通过摄像头捕获一帧画面，转换成数字信号，供算法处理。
  2. **颜色空间转换** ：将图像从默认的颜色空间（一般是RGB）转换到适合分析的颜色空间（本节是LAB）。
  3. **阈值匹配** ：根据预先设定的颜色范围，筛选出符合条件的像素。
  4. **区域分析** ：将相邻的符合条件的像素组合成“区域”（Blob），并提取区域的特征（比如位置、大小、形状等）。
  5. **标记与输出** ：对识别出的区域进行标记，并输出相关信息。



在庐山派中，步骤2-4可以是由内置的图像处理库自动完成，我们只需定义颜色的阈值范围，并调用相关函数就可以完成颜色识别。

# 3 什么是LAB色彩空间？ ​

## 3.1 RGB VS LAB ​

LAB是一种基于人眼感知设计的颜色表示方式，由三个通道组成：

  * **L通道** ：表示亮度，范围从黑到白，0表示黑，100表示白。
  * **A通道** ：表示从绿色到红色的颜色范围，范围是-128到127。负值靠近绿色，正值靠近红色。
  * **B通道** ：表示从蓝色到黄色的颜色范围，范围是-128到127，负值靠近蓝色，正值靠近黄色。



相比RGB色彩空间，LAB色彩空间将“亮度”和“颜色”完全分离，更适合颜色分析和处理。

RGB虽然是摄像头捕获图像的默认格式，但在颜色识别中存在以下问题：

  * **光照敏感** ：RGB值会因光照强度的变化而波动，同一颜色在强光或弱光下的RGB值会有显著差异。
  * **颜色混叠** ：RGB将亮度和颜色信息混合在一起，不容易单独分析颜色。
  * **非直观性** ：人眼感知的颜色与RGB值的关系并不直观，例如浅红和深红的RGB值相差较大，但肉眼看起来很接近，在LAB色彩空间中它们的A通道值会更接近。



但是LAB色彩空间克服了这些问题，他有以下优点：

  * **光照鲁棒性** ：颜色（A和B通道）与亮度（L通道）分离，可以忽略光照的影响，只专注于颜色本身。
  * **能直观的进行颜色描述** ：A通道和B通道直接描述颜色范围，更接近人眼对颜色的感知。
  * **更大色域** ：LAB的色域比RGB更大，可以表示更多的颜色，识别效果更精准。



## 3.2 为什么LAB比RGB色域更大？ ​

从数值上来说，RGB通常是 `[0, 255]`，每个通道使用8位存储。LAB是：L通道：`[0, 100]`，A通道：`[-128, 127]`，B通道：`[-128, 127]`。容易让我们感觉LAB的数值范围不如RGB大。但是LAB色彩空间的数值范围设计是为了匹配人眼对颜色的感知，不是简单的线性数值表示。

**RGB有何局限性** ：

RGB的色域范围受设备限制（显示器，摄像头等），sRGB覆盖的颜色范围仅为人眼可感知颜色的35%左右。Adobe RGB覆盖了约50%的可见颜色。因为RGB的原理是将三种特定波长的光（红、绿、蓝）进行混合，而这些波长是人为选择的，并不能完全覆盖所有颜色。

**LAB的理论设计：**

LAB色彩空间是基于整个可见光谱定义的，理论上可以表示人眼能够感知的所有颜色。

也就是说LAB是设备无关的颜色空间，表示的是理论上的颜色，不受硬件限制。RGB色彩空间是设备相关的，受限于具体硬件的能力。

提示

其实上面这些说的挺好，但都是理论，实际使用中给你就会发现进行显示生活中的颜色识别时，环境光亮对实际效果的影响还是特别大的，在进行颜色阈值调节时，需要尽量保证环境光亮的一致性，否则会严重影响颜色识别的效果。

# 4 find_blobs（寻找图像中色块） ​
    
    
    image.find_blobs(thresholds[, invert=False[, roi[, x_stride=2[, y_stride=1[, area_threshold=10[, pixels_threshold=10[, merge=False[, margin=0[, threshold_cb=None[, merge_cb=None]]]]]]]]]])

1  


此函数用于查找图像中的所有色块，并返回一个包含每个色块对象的列表。有关 `image.blob` 对象的更多信息，请参阅相关文档。

参数 `thresholds` 必须为元组列表，形式为 `[(lo, hi), (lo, hi), ...]`，用于定义需要追踪的颜色范围。

  * 对于灰度图像，每个元组应包含两个值：最小灰度值和最大灰度值。函数将仅考虑落在这些阈值之间的像素区域。
  * 对于 RGB565 图像（彩色图像），每个元组需要包含六个值 `(l_lo, l_hi, a_lo, a_hi, b_lo, b_hi)`，分别对应 LAB 色彩空间中的 L、A 和 B 通道的最小和最大值。该函数会自动纠正最小值和最大值的交换情况。如果元组包含超过六个值，则其余值将被忽略；若元组不足，则假定缺失的阈值为最大范围。



**注释：**

要获取目标对象的阈值，只需在 IDE 帧缓冲区内选择（单击并拖动）要跟踪的对象，直方图将实时更新。然后，记录下每个直方图通道中颜色分布的起始与结束位置，这些值将作为 `thresholds` 的低值和高值。建议手动确定阈值，以避免上下四分位数的微小差异。

您还可以通过进入 CanMV IDE K230 的“工具” -> “机器视觉” -> “阈值编辑器”，并通过 GUI 界面拖动滑块来确定颜色阈值。

  * `invert` 参数用于反转阈值操作，使得仅在已知颜色范围之外的像素被匹配。
  * `roi` 参数为感兴趣区域的矩形元组 `(x, y, w, h)`。若未指定，ROI 将默认为整个图像的矩形。操作仅限于该区域内的像素。
  * `x_stride` 为查找色块时需要跳过的 x 像素数量。在找到色块后，直线填充算法将精确处理该区域。如果已知色块较大，可以增加 `x_stride` 以提高查找速度。
  * `y_stride` 为查找色块时需要跳过的 y 像素数量。在找到色块后，直线填充算法将精确处理该区域。如果已知色块较大，可以增加 `y_stride` 以提高查找速度。
  * `area_threshold` 用于过滤掉边界框区域小于此值的色块。
  * `pixels_threshold` 用于过滤掉像素数量少于此值的色块。
  * `merge` 若为 `True`，则合并所有未被过滤的色块，这些色块的边界矩形互相重叠。`margin` 可用于在合并测试中增大或减小色块边界矩形的大小。例如，边缘为 1 的两个重叠色块将被合并。



合并色块可实现颜色代码的追踪。每个色块对象具有一个 `code` 值，该值为一个位向量。例如，若在 `image.find_blobs` 中输入两个颜色阈值，则第一个阈值对应的代码为 1，第二个为 2（第三个代码为 4，第四个代码为 8，以此类推）。合并色块时，所有的 `code` 通过逻辑或运算进行合并，以指示产生它们的颜色。这使得同时追踪两种颜色成为可能，若两个颜色得到同一个色块对象，则可能对应于某一种颜色代码。

在使用严格的颜色范围时，可能无法完全追踪目标对象的所有像素，此时可考虑合并色块。若希望合并色块但不希望不同阈值的色块被合并，可以分别调用两次 `image.find_blobs`。

  * `threshold_cb` 可设置为在每个色块经过阈值筛选后调用的回调函数，以从即将合并的色块列表中过滤出特定色块。回调函数将接收一个参数：待筛选的色块对象。若希望保留色块，回调函数应返回 `True`，否则返回 `False`。
  * `merge_cb` 可设置为在两个即将合并的色块间调用的回调函数，以控制合并的批准或禁止。回调函数将接收两个参数，即两个待合并的色块对象。若希望合并色块，应返回 `True`，否则返回 `False`。



**注意：** 此功能不支持压缩图像和 Bayer 图像。

# 5 寻找特定颜色色块 ​

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
    
        # 设置通道0的输出尺寸为显示分辨率
        sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, chn=CAM_CHN_ID_0)
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
    
        # 指定颜色阈值
        # 格式：[min_L, max_L, min_A, max_A, min_B, max_B]
        color_threshold = [(0, 79, 31, 67, 26, 60)]
    
        while True:
            os.exitpoint()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            #color_threshold 是要寻找的颜色的阈值，area_threshold 表示过滤掉小于此面积的色块。
            blobs = img.find_blobs(color_threshold,area_threshold = 2000)
    
            # 如果检测到颜色块
            if blobs:
                # 遍历每个检测到的颜色块
                for blob in blobs:
                    # 绘制颜色块的外接矩形
                    # blob[0:4] 表示颜色块的矩形框 [x, y, w, h]，
                    img.draw_rectangle(blob[0:4])
    
                    # 在颜色块的中心绘制一个十字
                    # blob[5] 和 blob[6] 分别是颜色块的中心坐标 (cx, cy)
                    img.draw_cross(blob[5], blob[6])
    
                    # 在控制台输出颜色块的中心坐标
                    print("Blob Center: X={}, Y={}".format(blob[5], blob[6]))
    
    
            # 显示捕获的图像，中心对齐，居中显示
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


还是准备好图片： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/color_detection/color_detection_20250116_193018.png)

在上面的代码中，我设置的颜色阈值，也就是color_threshold是我在写这篇文档时自行设定的红色阈值，将庐山派对准屏幕显示效果如下：

![图 8](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/color_detection/color_detection_20250116_211021.png)

# 6 如何进行阈值调节 ​

首先运行一个能够在帧缓冲区显示图像的程序，将庐山派的摄像头对准你需要调节颜色阈值的对象，然后点击帧缓冲区右上角的【禁用】，先把图像固定，方便后续步骤。 位置如下图所示： ![图 7](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/color_detection/color_detection_20250116_210823.png)

注意

调节完阈值之后注意需要单击这个【禁用】来取消，否则即使你重新运行程序，帧缓冲区也会一直不更新图像。

单击**CanMV IDE K230** 上方菜单栏中的【工具】->【机器视觉】->【阈值编辑器】 ![图 4](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/color_detection/color_detection_20250116_202952.png)  
在新弹出来的对话框中单击【帧缓冲区】，设定图像来源为帧缓冲区。 ![图 5](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/color_detection/color_detection_20250116_203104.png)  
弹出来的框就是阈值编辑器，在下面的六个通道中来回拖动选定，需要注意的是左边是原图像，右边图像中展示为白色的就是我们要跟踪的图像。 ![图 6](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/color_detection/color_detection_20250116_203548.png)

首先需要确定你需要提取的颜色阈值范围，我这里选定的是图中的红色，也就是左边图像中从上往下数的第三个方块。上图中的阈值范围是我已经调节好的，我们如果要寻找红色，其实就是让右边这个二进制图像中显示白色的位置和左边的源图像中的红色位置是一致的。

在调节的过程中，要实时观察右边的二进制图像。从默认的LAB值范围开始，逐步缩小或扩大值的范围，直到目标区域完全呈现为白色，背景为黑色。然后记录最下面的LAB阈值，在这里是(0, 79, 31, 67, 26, 60)，就是当前环境下我的屏幕上红色的阈值。具体调节的效果可以看下面这个动图： ![图 9](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/color_detection/color_detection_20250116_211955.gif)

