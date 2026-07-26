# 获取触摸坐标

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/media/touch.html>
> **最后更新**: 2025-02-28

---

# 1 本节介绍 ​

📝本节您将学习如何获取我们3.1寸屏幕的触摸坐标。

🏆学习目标

1️⃣如何获取3.1寸屏幕的触摸坐标。

2️⃣如何做一个简单的手画板。

在开始之前，记得先把我们的庐山派和[3.1寸屏幕](https://item.szlcsc.com/44319751.html)通过排线连接起来：

![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/media/touch/touch_20250228_173237.png)

# 2 电容触摸 VS 电阻触摸 ​

> 我们的[3.1寸屏幕](https://item.szlcsc.com/44319751.html)和我们目前最常用的手机屏幕一样，是由IPS液晶显示屏和触摸板上下粘合组成。触摸板是**电容触摸** 的，其内部驱动是`cst128`，寄存器及驱动程序和`FT5316`是一致的，不过大家使用CanMV固件的话是无需关注这个信息的。

**电容触摸屏** ： 电容触摸屏是基于电容变化原理。当手指接触屏幕时，人体会引起局部电场的变化。触摸屏表面涂有透明的导电材料，屏幕通过检测电容的变化来确定触摸位置。电容触摸屏一般是多点触控的，可以同时响应多个触摸点。

**电阻触摸屏** ： 电阻触摸屏则基于压力感应原理。屏幕由两层透明的导电薄膜构成，中间有微小的间隙。当用户用手指或其他物体（如触笔）施加压力时，上下两层薄膜接触，闭合电路形成电流回路从而改变电流或电压值，屏幕通过这种变化来确定触摸位置。电阻触摸屏一般感应单点触控（有些高端的可以支持多点）。

特性| 电容触摸屏| 电阻触摸屏  
---|---|---  
**工作原理**|  电容变化检测，带了手套就用不了了| 压力感应，可以带手套使用  
**触摸方式**|  轻触操作，支持多点触控| 需要施加压力，一般是单点触控  
**响应速度**|  快，适合多点触控| 较慢，适合单点触控  
**耐用性**|  较高，但受湿度影响| 强，适应恶劣环境  
**精度**|  高，适合手指触摸操作| 高，适合触笔或精细操作  
**价格**|  较贵| 较便宜  
**适用环境**|  消费类电子产品| 工业、医疗、户外设备等  
  
# 3 TOUCH API ​

TOUCH 类位于 `machine` 模块下。

python
    
    
    from machine import TOUCH
    
    # 实例化 TOUCH 设备 0
    tp = TOUCH(0)
    # 获取 TOUCH 数据
    p = tp.read()
    print(p)
    # 打印触摸点坐标
    # print(p[0].x)
    # print(p[0].y)
    # print(p[0].event)

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


## 3.1 **构造函数** ​

python
    
    
    # when index is 0
    touch = TOUCH(index, rotate = -1)

1  
2  
3  


**参数**

  * `index`: `TOUCH` 设备号，目前只支持参数为0.
  * `rotate`: 面板输出坐标与屏幕坐标的旋转，取值范围为 [0-3]。 
    * 0：`ROTATE_0`: 坐标不旋转。
    * 1：`ROTATE_90`: 坐标旋转 90 度。
    * 2：`ROTATE_180`: 坐标旋转 180 度。
    * 3：`ROTATE_270`: 坐标旋转 270 度。



## 3.2 `read` 方法 ​

python
    
    
    TOUCH.read([count])

1  


获取触摸数据。

**参数**

  * `count`: 最多读取的触摸点数量，取值范围为 [0:10]，【默认为 1，只读取一个点】。



**返回值**

返回触摸点数据，类型为元组 `([tp[, tp...]])`，其中每个 `tp` 是一个 `touch_info` 类实例。

## 3.3 `deinit` 方法 ​

python
    
    
    TOUCH.deinit()

1  


当触摸屏不再使用时，可以调用 `deinit` 方法释放 TOUCH 资源。

## 3.4 touch_info类 ​

TOUCH_INFO 类用于存储触摸点的信息，用户可通过相关只读属性访问。

  * `event`: 事件码。【目前还不支持，其内部使用的是k230自行维护的一套】
  * `track_id`: 触点 ID，用于多点触摸。【目前还不支持】
  * `width`: 触点宽度。【目前等效于`track_id`,当前我们3.1寸屏幕使用的触摸芯片是不支持获取这个触点宽度的】
  * `x`: 触点的 x 坐标。
  * `y`: 触点的 y 坐标。
  * `timestamp`: 触点时间戳。



警告

请等待嘉楠后续完善，我们的固件是嘉楠官方在维护的。

数据不匹配算是历史遗留问题 之前嘉楠做适配的时候我们这个3.1寸屏幕调好后mipi的时钟会影响到触摸读取。

后面调了不少时间才搞定 导致现在也不是很完美，但是获取点的坐标是完全没有问题的。

# 4 获取触摸点程序 ​

API

`enumerate()` 是 Python 内置的一个函数，用于遍历可迭代对象（如列表、元组、字符串等）时，同时获取元素的索引和值。它返回的是一个枚举对象，可以在遍历时同时得到每个元素的索引和该元素的值。用这个可以简化我们的代码，避免使用手动创建的计数变量。

**基本语法：**

python
    
    
    enumerate(iterable, start=0)

1  


  * `iterable`：任何可迭代的对象，如列表、元组、字符串等。
  * `start`：指定索引的起始值，默认为 0。可以设置为其他值，指定索引从该值开始。



`enumerate()` 返回一个枚举对象，这个对象可以被 `for` 循环遍历。每次循环中，`enumerate()` 返回一个元组，元组的第一个元素是索引，第二个元素是迭代对象中的元素。

python
    
    
    fruits = ["苹果", "香蕉", "橙子"]
    
    for index, fruit in enumerate(fruits):
        print(f"索引: {index}, 值: {fruit}")

1  
2  
3  
4  


输出：
    
    
    索引: 0, 值: 苹果
    索引: 1, 值: 香蕉
    索引: 2, 值: 橙子

1  
2  
3  


在这个示例中，`enumerate(fruits)` 会返回 `(0, "苹果")`， `(1, "香蕉")` 和 `(2, "橙子")` 这样的元组。

下面这段程序的主要功能是读取触摸屏的触摸点数据，并打印出触摸点的坐标，最多打印5个坐标点（我们的3.1寸屏幕的触摸硬件上就只支持5个触摸点）。

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    import time
    from machine import TOUCH
    
    # 实例化 TOUCH 设备 0
    tp = TOUCH(0)
    
    while True:
        # 获取最多 5 个触摸点数据，默认为1.
        p = tp.read(5)
    
        # 如果返回的 p 为空元组，表示没有触摸
        if p != ():
            print("触摸数据：")
            for idx, point in enumerate(p, start=1):  # 对触摸点进行编号，从 1 开始
                print(f"触摸点 {idx}: X = {point.x}, Y = {point.y}")
    
        time.sleep(0.01)  # 等待 10 毫秒再读取

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


首先，通过 `TOUCH(0)` 实例化触摸屏设备，并进入一个无限循环，不断读取最多 5 个触摸点的数据。若此时你将手指放到我们的屏幕上，程序会遍历每个触摸点，输出其编号及对应的 X、Y 坐标。通过 `enumerate` 方法为每个触摸点加上序号，确保输出有序。最后，程序在每次读取后通过 `time.sleep(0.01)` 等待 10 毫秒，然后重头再来。

在IDE用运行程序并用手指触摸显示屏就能在串行终端处看到输出的坐标值了，在下图中，我放了三根手指，可以看到输出是有三个坐标点的。

![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/media/touch/touch_20250120_194520.png)

# 5 做一个简单的手画板 ​

python
    
    
    import time, os, gc, sys, urandom
    
    from media.display import *
    from media.media import *
    from machine import TOUCH
    
    try:
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
    
        # 根据模式初始化显示器
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=60)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        width = DISPLAY_WIDTH
        height = DISPLAY_HEIGHT
    
        # 初始化媒体管理器
        MediaManager.init()
    
        fps = time.clock()
        # 创建绘制的图像
        img = image.Image(width, height, image.RGB565)
    
        img.clear()
    
        # 设置默认画笔颜色和大小
        current_color = (0, 255, 0)  # 默认绿色
        brush_size = 10  # 默认画笔大小
    
        # 定义画布中的按钮区域
        clear_button_area = (width - 100, 0, 100, 50)  # 清除按钮区域（右上角）
        color_button_area = (0, 0, 130, 50)  # 颜色选择按钮区域（左上角）
    
        # 实例化 TOUCH 设备 0
        tp = TOUCH(0)
    
        last_point = None  # 记录上一个触摸点
    
        def draw_clear_button():
            # 绘制清除按钮
            img.draw_rectangle(clear_button_area[0], clear_button_area[1], clear_button_area[2], clear_button_area[3], color=(255, 0, 0),fill=True)
            img.draw_string_advanced (clear_button_area[0] + 10, clear_button_area[1] + 10,30, "清除", color=(255, 255, 255), scale=2)
    
        def draw_color_buttons():
            # 绘制颜色选择按钮
            img.draw_rectangle(color_button_area[0], color_button_area[1], color_button_area[2], color_button_area[3], color=(255, 255, 0),fill=True)
            img.draw_string_advanced (color_button_area[0] + 10, color_button_area[1] + 10,30, "随机颜色", color=(0, 0, 0), scale=2)
    
        def select_color(x, y):
            global current_color
            # 如果点击了颜色选择区域，则随机更改颜色
            if color_button_area[0] <= x <= color_button_area[0]+color_button_area[2] and color_button_area[1] <= y <= color_button_area[1]+color_button_area[3]:
                current_color = (urandom.getrandbits(8), urandom.getrandbits(8), urandom.getrandbits(8))
                print(f"select_color to {current_color}")
                # 更新触点颜色
                img.draw_circle(color_button_area[0]+200,25,25,color=current_color, thickness=3,fill=True)
    
    
        def check_clear_button(x, y):
            # 检查是否点击了清除按钮
            if clear_button_area[0] <= x <= clear_button_area[0]+clear_button_area[2] and clear_button_area[1] <= y <= clear_button_area[1]+clear_button_area[3]:
                img.clear()  # 清除画布
    
        def draw_line_between_points(last_point, current_point):
            """在两个触摸点之间绘制连线，插入中间点以平滑移动"""
            if last_point is not None:
    
                # 计算两点之间的距离
                dx = current_point.x - last_point.x
                dy = current_point.y - last_point.y
                distance = (dx**2 + dy**2) ** 0.5
    
                # 如果距离大于30，则不进行绘制
                if distance > 30:
                    return
    
                # 设定最小距离，如果两个点之间的距离大于该值，则进行插值
                min_distance = 10  # 可以调整此值来改变插值的密度
                if distance > min_distance:
                    steps = int(distance // min_distance)  # 计算插值的步数
                    for i in range(1, steps + 1):
                        # 插值计算中间点
                        new_x = last_point.x + i * dx / (steps + 1)
                        new_y = last_point.y + i * dy / (steps + 1)
                        # 绘制圆点
                        img.draw_circle(int(new_x), int(new_y), brush_size, color=current_color, thickness=3, fill=True)
    
            # 最后绘制当前点
            img.draw_circle(current_point.x, current_point.y, brush_size, color=current_color, thickness=3, fill=True)
    
    
        while True:
            fps.tick()
            # 检查是否在退出点
            os.exitpoint()
    
    
            # 只读取 1 个触摸点数据
            p = tp.read(1)
    
            # 如果返回的 p 为空元组，表示没有触摸
            if p != ():
                for idx, point in enumerate(p, start=1):  # 对触摸点进行编号，从 1 开始
                    # 如果触摸区域为颜色选择按钮，则随机选择颜色
                    select_color(point.x, point.y)
    
                    # 如果触摸区域为清除按钮，则清除画布
                    check_clear_button(point.x, point.y)
    
                    # 绘制当前点与上一个点之间的线段
                    draw_line_between_points(last_point, point)
    
                    # 更新上一个触摸点和上次触摸的时间
                    last_point = point
    
    
            # 绘制按钮和其他元素
            draw_clear_button()
            draw_color_buttons()
    
            # 显示绘制结果
            Display.show_image(img)
    
            time.sleep_ms(1)
    except KeyboardInterrupt as e:
        print(f"user stop")
    except BaseException as e:
        print(f"Exception '{e}'")
    finally:
        # 销毁 display
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


上面代码是基于之前的图像绘制章节修改而来，可以实现简单画图的绘制。

注意

当前触摸事件在固件中还没有完全实现，因此无法判断手指是否已经离开屏幕(单纯用延时来判断也不合适)。所以在处理两个触摸点之间的插值时存在问题。

当前使用的逻辑是如果两个触摸点之间的距离过远，程序会认为这两个点属于不同的时间段数据，不会进行插值处理。这样做是为了避免错误地将新画线段的起点和旧画线段的终点连接在一起。

首先，程序根据 `DISPLAY_MODE` 变量选择不同的显示模式（IDE帧缓冲区虚拟显示、3.1寸LCD屏幕或 HDMI扩展板），并初始化相应的显示硬件（即使大家没有3.1寸屏幕也可以运行的，只不过没法触摸，但是画面会正常显示到IDE的帧缓冲区里面的）。然后，程序创建一个空白的图像，并设置默认的画笔颜色（绿色）。通过 `TOUCH`类来获取触摸屏的输入，当用户触摸屏幕时，程序会检查触摸位置，如果触摸在随机颜色区域，就会随机更改颜色，如果触摸在清除按钮区域，就清空画布。程序会在触摸点之间绘制连线，并通过插值平滑绘制路径，避免出现跳跃或者只能看到一个一个间断的点的问题。主循环不断的去读取触摸点、更新画布并显示结果，直到程序被手动退出。

在IDE中运行后，点击左上角可以随机改变画笔的颜色，点击右上角可以清除目前画面中的绘制图案，单击除那两个按键的其他位置就可以绘制图案了。

实际运行效果如下图所示：

![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/media/touch/touch_20250120_194751.png)

