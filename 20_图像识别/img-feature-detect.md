# 特征检测

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/image-recog/img-feature-detect.html>
> **最后更新**: 2025-01-08

---

# 1 本节介绍 ​

📝本节您将学习如何对图像进行特征检测，例如线段检测，矩形检测，圆形检测。

🏆学习目标

1️⃣学会从摄像头获取到的图像中进行线段检测，矩形检测，圆形检测。

注意

1️⃣大部分图像处理API仅支持`RGB565`或`GRAYSCALE`，使用时需要注意。

2️⃣如无特殊说明，以后所有例程的显示设备均为通过外接立创·3.1寸屏幕扩展板，在3.1寸小屏幕上显示。若用户无3.1寸屏幕扩展板也可以正常在IDE的缓冲区，只是受限于USB带宽，可能会帧率较低或卡顿。

在学习目标中提到的这些功能通常基于不同的图像处理算法实现，如霍夫变换、LSD(Line Segment Detector)算法以及 AprilTag 中的四边形检测算法等。在实际工程场景中，特征检测往往需要先经过一些图像预处理来确保检测的准确性与稳定性，例如：二值化、直方图均衡化、边缘检测、亮度/对比度调整等。在下面的例程中，为了演示算法本身，我们直接在原图上进行特征检测。初学者在实际应用中，可根据需求结合上一章介绍的各种预处理方式来提升检测效果。

# 2 线段检测 ​

线段检测是视觉识别中的一个重要的基础功能，常用于识别图像中的线段部分，为更高层次的视觉处理任务提供支持。常使用霍夫变换算法来实现。庐山派-K230-CanMV 开发板所使用的 `find_line_segments` 接口即采用了 LSD 算法，与部分OpenCV版本一致，检测效果准确并且线段不易抖动或跳跃。

线段检测的基本思想有两个：

  1. **空间变换** ：霍夫变换中，会将图像边缘像素映射到一个极坐标或其它累加空间，并在累加空间中找极大值点对应的直线或线段。
  2. **后续合并** ：如果检出很多近似平行并且重叠度较高的线段，会做一个“合并距离”与“角度差”判断，把它们合并成一条线段。在庐山派中就是体现为两个参数，`merge_distance`和`max_theta_difference`。



API

`find_line_segments`
    
    
    image.find_line_segments([roi[, merge_distance=0[, max_theta_difference=15]]])

1  


该函数使用霍夫变换查找图像中的线段，并返回一个 `image.line` 对象的列表。

  * `roi` 为感兴趣区域的矩形元组 `(x, y, w, h)`。若未指定，ROI 默认为整个图像的矩形。操作仅限于该区域内的像素。
  * `merge_distance` 指定两条线段之间的最大像素距离，若小于该值则合并为一条线段。
  * `max_theta_difference` 为需合并的两条线段之间的最大角度差。



该方法使用 LSD 库（OpenCV 亦采用）来查找图像中的线段。虽然速度较慢，但准确性高，且线段不会出现跳跃现象。

**注意：** 此功能不支持压缩图像和 Bayer 图像。

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
    
    picture_width = 400
    picture_height = 240
    
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
    
        while True:
            os.exitpoint()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            # 可以在此处根据需求先做一些预处理，如灰度化、边缘检测、二值化等
    
            # 查找线段（LSD算法）
            #  merge_distance=20         # 两线段中心点相距小于20像素则合并
            #  max_theta_diff=10        # 两线段角度差小于10°则合并
            lines = img.find_line_segments(merge_distance=20, max_theta_diff=10)
            count = 0  # 初始化线段计数器
    
            print("------线段统计开始------")
            for line in lines:
                img.draw_line(line.line(), color=(1, 147, 230), thickness=3)  # 绘制线段
                print(f"Line {count}: {line}")  # 打印线段信息
                count += 1  # 更新计数器
            print("---------END---------")
    
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


准备好我们的图片：

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-feature-detect/img-feature-detect_20250107_192926.png)

在IDE中运行起来后将庐山派的摄像头对准你电脑屏幕上的文档，运行效果如下所示： ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-feature-detect/img-feature-detect_20250107_193913.png)

可以看到左下角的串行终端那里就会打印出检测到的线段信息了，总共检测出5条线段，实图上面就是有5条线段的。大家也可以不用上面的图片，将摄像头对准有明显线段的场景也可以。

# 3 矩形检测 ​

矩形检测一般用于条码检测，可以先定位条码或者二维码，然后再去解码，提高运行速度。也常用来OCR的前置检测，先定位文档中的文字块或段落。CanMV固件中的`find_rects`函数使用了与 AprilTag 中的四边形检测类似的算法，能适应一定程度的平移、旋转和仿射变化（扭曲）。

API

python
    
    
    image.find_rects([roi=Auto, threshold=10000])

1  


此函数使用与 AprilTag 相同的四边形检测算法查找图像中的矩形。该算法最适用于与背景形成鲜明对比的矩形。AprilTag 的四边形检测能够处理任意缩放、旋转和剪切的矩形，并返回一个包含 `image.rect` 对象的列表。

  * `roi` 是用于指定感兴趣区域的矩形元组 `(x, y, w, h)`。若未指定，ROI 默认为整个图像。操作范围仅限于该区域内的像素。



在返回的矩形列表中，边界大小（通过在矩形边缘的所有像素上滑动索贝尔算子并累加其值）小于 `threshold` 的矩形将被过滤。适当的 `threshold` 值取决于具体的应用场景。

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
    
    picture_width = 400
    picture_height = 240
    
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
    
        while True:
            os.exitpoint()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            # 查找线段并绘制
            rects = img.find_rects(threshold=5000)
            count = 0  # 初始化线段计数器
    
            print("------矩形统计开始------")
            for rect in rects:
                 # 若想获取更详细的四个顶点，可使用 rect.corners()，该函数会返回一个有四个元祖的列表，每个元组代表矩形的四个顶点，从左上角开始，按照顺时针排序。
                img.draw_rectangle(rect.rect(), color=(1, 147, 230), thickness=3)  # 绘制线段
                print(f"Rect {count}: {rect}")  # 打印线段信息
                count += 1  # 更新计数器
            print("---------END---------")
    
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


准备好我们的图片： ![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-feature-detect/img-feature-detect_20250107_201032.png)

在IDE中运行起来后将庐山派的摄像头对准你电脑屏幕上的文档图片，运行效果如下所示：

![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-feature-detect/img-feature-detect_20250107_201852.png)

可以看到左下角的串行终端那里就会打印出检测到的矩形信息了。

# 4 圆形检测 ​

可以用来快速定位圆心与半径，可以用来检测圆环标记，交通标志等，也可以用于物体检测（如五子棋棋子定位）。

API

`find_circles`

python
    
    
    image.find_circles([roi[, x_stride=2[, y_stride=1[, threshold=2000[, x_margin=10[, y_margin=10[, r_margin=10]]]]]]])

1  


该函数使用霍夫变换在图像中查找圆形，并返回一个 `image.circle` 对象的列表。

  * `roi` 为感兴趣区域的矩形元组 `(x, y, w, h)`。若未指定，ROI 默认为整个图像的矩形。操作仅限于该区域内的像素。
  * `x_stride` 为霍夫变换过程中需要跳过的 x 像素数量。如果已知圆较大，可增加 `x_stride`。
  * `y_stride` 为霍夫变换过程中需要跳过的 y 像素数量。如果已知圆较大，可增加 `y_stride`。
  * `threshold` 控制检测到的圆的大小，仅返回大于或等于该阈值的圆。合适的阈值取决于图像内容。请注意，圆的大小（magnitude）是构成圆的所有索贝尔滤波像素



大小的总和。

  * `x_margin` 为对 x 坐标进行合并时允许的最大像素偏差。
  * `y_margin` 为对 y 坐标进行合并时允许的最大像素偏差。
  * `r_margin` 为对半径进行合并时允许的最大像素偏差。



该方法通过在图像上应用索贝尔滤波器，并利用其幅值和梯度响应执行霍夫变换。无需对图像进行任何预处理，尽管图像的清理和过滤将会产生更为稳定的结果。

**注意：** 此功能不支持压缩图像和 Bayer 图像。

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
    
    picture_width = 400
    picture_height = 240
    
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
    
        while True:
            os.exitpoint()
    
            # 捕获通道0的图像
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
    
            # 查找线段并绘制
            circles = img.find_circles(threshold=6000)
            count = 0  # 初始化线段计数器
    
            print("------圆形统计开始------")
            for circle in circles:
                 # 若想获取更详细的四个顶点，可使用 rect.corners()，该函数会返回一个有四个元祖的列表，每个元组代表圆形的四个顶点，从左上角开始，按照顺时针排序。
                img.draw_circle(circle.circle(), color=(1, 147, 230), thickness=3)  # 绘制线段
                print(f"Circle {count}: {circle}")  # 打印线段信息
                count += 1  # 更新计数器
            print("---------END---------")
    
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


准备好我们的图片： ![图 5](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-feature-detect/img-feature-detect_20250108_114926.png)

在IDE中运行起来后将庐山派的摄像头对准你电脑屏幕上的文档图片，运行效果如下所示：

![图 4](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/img-feature-detect/img-feature-detect_20250108_114922.png)

可以看到左下角的串行终端那里就会打印出检测到的圆形信息了。

# 5 重点提示 ​

大家在使用中可能会觉得这些检测效果不是很好，可能识别效果不好或者帧数不满意，可以进行以下这些调节。

## 进行多角度及多距离测试 ​

特征检测算法的稳定性受到光照、分辨率、拍摄角度、距离远近等多方面影响。为了获得更好的检测效果，可以从不同角度、距离、光照条件、距离远近下采集图像进行测试，并尝试相应的预处理（边缘检测、滤波、二值化等）。

当物体距离摄像头较远时，在图像中的特征尺寸变小，检测算法更容易漏检；而当物体过近时，可能出现局部过曝或画面裁切。根据实际应用需求，多做不同距离下的测试，尤其是当前庐山派随板子附送摄像头是定焦的，距离过近就会导致画面模糊，如果确实需要调整对焦，可以去购买一个旋转镜头的工具，大力一点就能旋转调焦了。

## 进行阈值与参数调优 ​

  * 对于线段检测，`merge_distance` 与 `max_theta_difference` 参数可防止过多重复线段，过小可能会导致同向线段无法合并，过大可能会把一些不相干的线段合并在一起。
  * 对于矩形检测，`threshold` 参数太低会导致噪点（误识别）较多，太高又可能漏检，可以根据矩形边缘在图像中的对比度逐步试验。
  * 对于圆形检测，`threshold` 过大或过小都会直接影响检测结果的精度。`x_margin`, `y_margin`, `r_margin`：用于合并相似的检测结果。当拍摄物体抖动或分辨率较低时，可能检测到位置和大小相近的多个圆，适当增大这三个参数可避免多重检测。



## 确定ROI (感兴趣区域) ​

大部分 API 中都可以通过 `roi=(x, y, w, h)` 设置仅对部分图像区域进行检测，一是可以**减少干扰** ：若在画面其它区域存在大量不需要的特征，设置 ROI 能避免算法被无关特征干扰或误检。二是可**提升效率** ：只对局部区域进行检测可以节省计算量，提高处理速度。

