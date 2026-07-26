# 显示画面

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/image-recog/display.html>
> **最后更新**: 2025-04-11

---

# 1 本节介绍 ​

📝本节您将学习如何显示画面到`CanMV IDE K230`，`3.1寸屏幕`和`HDMI屏幕`上。

🏆学习目标

1️⃣如何通过板载USB连接电脑并通过`CanMV IDE K230`中的帧缓冲区来显示画面。

2️⃣如何通过外接[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)，在小屏幕上显示。

3️⃣如何通过外接[立创·MIPI转HDMI扩展板](https://item.szlcsc.com/44319752.html)来外接有HDMI接口的屏幕，在大显示器上显示。

# 2 概述 ​

K230 配备 1 路 MIPI-DSI（1x4 lane），可驱动 MIPI 屏幕或通过接口芯片转换驱动 HDMI 显示器。此外，为了方便调试，还支持虚拟显示器，用户可以选择 `VIRT` 输出设备，即使没有 HDMI 显示器或 LCD 屏幕, 也可在 `CanMV-IDE K230` 中进行图像预览。

简单来说，用IDE的帧缓冲区来显示成本最低，但受限于USB传输速率，他的帧率和画面质量是不可兼得的，不过简单使用绝对是没问题的。

如果使用MIPI来外接我们的[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)，毫无疑问是很方便的，目前这款是支持800x480的分辨率，可以直接和开发板通过铜柱连接起来，将脚本保存到TF卡里面后就可以随身携带，也方便制作一些DIY项目了。自带屏幕可以进行实时的可视化操作，无需再连接连接电脑来查看画面了。

也可以通过MIPI接口通过外接[立创·MIPI转HDMI扩展板](https://item.szlcsc.com/44319752.html)来外接支持HDMI信号的显示器，这个的成本介于用IDE显示和外接3.1寸屏幕之间，支持1080p的分辨率，如果大家家里有HDMI屏幕，其实还是建议用这个方案的，毕竟大屏幕看起来肯定更舒服。

# 3 Display使用指南 ​

## 3.1 init ​

**描述**

初始化 Display 通路，包括 VO 模块、 DSI 模块和 LCD/HDMI。  
**`必须在 MediaManager.init()之前调用`**

**语法**

python
    
    
    init(type=None, width=None, height=None, osd_num=1, to_ide=False, fps=None, quality=90)

1  


**参数**

参数名称| 描述| 输入 / 输出| 说明  
---|---|---|---  
type| 显示设备类型| 输入| 必选  
width| 分辨率宽度| 输入| 默认值根据 `type` 决定  
height| 分辨率高度| 输入| 默认值根据 `type` 决定  
osd_num| 在 show_image 时支持的 LAYER 数量| 输入| 越大占用内存越多  
to_ide| 是否将屏幕显示传输到 IDE 显示| 输入| 开启时占用更多内存  
fps| 显示帧率| 输入| 仅支持 `VIRT` 类型  
quality| 设置 `Jpeg` 压缩质量| 输入| 仅在 `to_ide=True` 时有效，范围 [10-100]  
  
**返回值**

返回值| 描述  
---|---  
无|   
  
## 3.2 show_image ​

**描述**

在屏幕上显示图像。

**语法**

python
    
    
    show_image(img, x=0, y=0, layer=None, alpha=255, flag=0)

1  


**参数**

参数名称| 描述| 输入 / 输出| 说明  
---|---|---|---  
img| 显示的图像| 输入|   
x| 起始坐标的 x 值| 输入|   
y| 起始坐标的 y 值| 输入|   
layer| 显示到 指定层| 输入| 仅支持 `OSD` 层，若需要多层请在 init 中设置 `osd_num`  
alpha| 图层混合 alpha| 输入|   
flag| 显示 标志| 输入|   
  
**返回值**

返回值| 描述  
---|---  
无|   
  
## 2.3 deinit ​

**描述**

执行反初始化， deinit 方法会关闭整个 Display 通路，包括 VO 模块、 DSI 模块和 LCD/HDMI。  
**`必须在 MediaManager.deinit()之前调用`**  
**`必须在 sensor.stop()之后调用`**

**语法**

python
    
    
    deinit()

1  


**返回值**

返回值| 描述  
---|---  
无|   
  
## 2.4 bind_layer ​

**描述**

将 `sensor` 或 `vdec` 模块的输出绑定到屏幕显示。无需用户手动干预即可持续显示图像。  
**`必须在 init 之前调用`**

**语法**

python
    
    
    bind_layer(src=(mod, dev, layer), dstlayer, rect=(x, y, w, h), pix_format, alpha, flag)

1  


**参数**

参数名称| 描述| 输入 / 输出| 说明  
---|---|---|---  
src| `sensor` 或 `vdec` 的输出信息| 输入| 可通过 `sensor.bind_info()` 获取  
dstlayer| 绑定到 Display 的 显示层| 输入| 可绑定到 `video` 或 `osd` 层  
rect| 显示区域| 输入| 可通过 `sensor.bind_info()` 获取  
pix_format| 图像像素格式| 输入| 可通过 `sensor.bind_info()` 获取  
alpha| 图层混合 alpha| 输入|   
flag| 显示 标志| 输入| `LAYER_VIDEO1` 不支持  
  
**返回值**

无

## 3.5 数据结构描述 ​

### 3.5.1 type ​

类型| 分辨率   
(width x height @ fps)| 备注  
---|---|---  
LT9611| 1920x1080@30|  _默认值_  
|  1280x720@30|   
| 640x480@60|   
ST7701| 800x480@30|  _默认值_  
可设置为竖屏 480x800  
| 854x480@30| 可设置为竖屏 480x854  
VIRT| 640x480@90|  _默认值_  
| | `IDE` 调试专用，不在外接屏幕上显示内容   
用户可自定义设置分辨率 (64x64)-(4096x4096) 和帧率 (1-200)  
  
### 3.5.2 layer ​

K230 提供 2 层视频图层支持和 4 层 OSD 图层支持。分列如下：

显示层| 说明| 备注  
---|---|---  
LAYER_VIDEO1| | 仅可在 bind_layer 中使用  
LAYER_VIDEO2| | 仅可在 bind_layer 中使用  
LAYER_OSD0| | 支持 show_image 和 bind_layer 使用  
LAYER_OSD1| | 支持 show_image 和 bind_layer 使用  
LAYER_OSD2| | 支持 show_image 和 bind_layer 使用  
LAYER_OSD3| | 支持 show_image 和 bind_layer 使用  
  
### 3.5.3 flag ​

标志| 说明| 备注  
---|---|---  
FLAG_ROTATION_0| 旋转 `0` 度|   
FLAG_ROTATION_90| 旋转 `90` 度|   
FLAG_ROTATION_180| 旋转 `180` 度|   
FLAG_ROTATION_270| 旋转 `270` 度|   
FLAG_MIRROR_NONE| 不镜像|   
FLAG_MIRROR_HOR| 水平镜像|   
FLAG_MIRROR_VER| 垂直镜像|   
FLAG_MIRROR_BOTH| 水平与垂直镜像|   
  
# 4 在三种显示设备上显示 ​

在IDE缓冲区中显示用3.1寸屏幕来显示用HDMI扩展板连接外置屏幕来显示

python
    
    
    # 导入所需模块
    import time, os, urandom, sys
    from media.display import *
    from media.media import *
    
    # 显示模式选择：可以是 "VIRT"、"LCD" 或 "HDMI"
    DISPLAY_MODE = "VIRT"
    
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
    
    # 显示测试函数
    def display_test():
        print(f"显示测试，当前模式为 {DISPLAY_MODE}")
    
        # 创建用于绘图的图像对象
        img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
    
        # 根据模式初始化显示器
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=60)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        # 初始化媒体管理器
        MediaManager.init()
    
        try:
            while True:
                img.clear()  # 清除图像内容
                for i in range(10):  # 循环绘制10个字符串
                    # 随机生成字符串位置、颜色和大小
                    x = (urandom.getrandbits(11) % img.width())  # 随机X坐标
                    y = (urandom.getrandbits(11) % img.height())  # 随机Y坐标
                    r = urandom.getrandbits(8)  # 红色分量
                    g = urandom.getrandbits(8)  # 绿色分量
                    b = urandom.getrandbits(8)  # 蓝色分量
                    size = (urandom.getrandbits(30) % 64) + 32  # 字体大小（32到96之间）
    
                    # 绘制字符串，支持中文字符
                    img.draw_string_advanced(
                        x, y, size, "Hello World!，你好庐山派！！！", color=(r, g, b),
                    )
    
                # 将绘制结果显示到屏幕
                Display.show_image(img)
    
                time.sleep(1)  # 暂停1秒
                os.exitpoint()  # 可用的退出点
        except KeyboardInterrupt as e:
            print("用户终止：", e)  # 捕获键盘中断异常
        except BaseException as e:
            print(f"异常：{e}")  # 捕获其他异常
        finally:
            # 清理资源
            Display.deinit()
            os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)  # 启用睡眠模式的退出点
            time.sleep_ms(100)  # 延迟100毫秒
            MediaManager.deinit()
    
    # 主程序入口
    if __name__ == "__main__":
        os.exitpoint(os.EXITPOINT_ENABLE)  # 启用退出点
        display_test()  # 调用显示测试函数

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


python
    
    
    # 导入所需模块
    import time, os, urandom, sys
    from media.display import *
    from media.media import *
    
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
    
    # 显示测试函数
    def display_test():
        print(f"显示测试，当前模式为 {DISPLAY_MODE}")
    
        # 创建用于绘图的图像对象
        img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
    
        # 根据模式初始化显示器
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=60)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        # 初始化媒体管理器
        MediaManager.init()
    
        try:
            while True:
                img.clear()  # 清除图像内容
                for i in range(10):  # 循环绘制10个字符串
                    # 随机生成字符串位置、颜色和大小
                    x = (urandom.getrandbits(11) % img.width())  # 随机X坐标
                    y = (urandom.getrandbits(11) % img.height())  # 随机Y坐标
                    r = urandom.getrandbits(8)  # 红色分量
                    g = urandom.getrandbits(8)  # 绿色分量
                    b = urandom.getrandbits(8)  # 蓝色分量
                    size = (urandom.getrandbits(30) % 64) + 32  # 字体大小（32到96之间）
    
                    # 绘制字符串，支持中文字符
                    img.draw_string_advanced(
                        x, y, size, "Hello World!，你好庐山派！！！", color=(r, g, b),
                    )
    
                # 将绘制结果显示到屏幕
                Display.show_image(img)
    
                time.sleep(1)  # 暂停1秒
                os.exitpoint()  # 可用的退出点
        except KeyboardInterrupt as e:
            print("用户终止：", e)  # 捕获键盘中断异常
        except BaseException as e:
            print(f"异常：{e}")  # 捕获其他异常
        finally:
            # 清理资源
            Display.deinit()
            os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)  # 启用睡眠模式的退出点
            time.sleep_ms(100)  # 延迟100毫秒
            MediaManager.deinit()
    
    # 主程序入口
    if __name__ == "__main__":
        os.exitpoint(os.EXITPOINT_ENABLE)  # 启用退出点
        display_test()  # 调用显示测试函数

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


python
    
    
    # 导入所需模块
    import time, os, urandom, sys
    from media.display import *
    from media.media import *
    
    # 显示模式选择：可以是 "VIRT"、"LCD" 或 "HDMI"
    DISPLAY_MODE = "HDMI"
    
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
    
    # 显示测试函数
    def display_test():
        print(f"显示测试，当前模式为 {DISPLAY_MODE}")
    
        # 创建用于绘图的图像对象
        img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
    
        # 根据模式初始化显示器
        if DISPLAY_MODE == "VIRT":
            Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=60)
        elif DISPLAY_MODE == "LCD":
            Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        elif DISPLAY_MODE == "HDMI":
            Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    
        # 初始化媒体管理器
        MediaManager.init()
    
        try:
            while True:
                img.clear()  # 清除图像内容
                for i in range(10):  # 循环绘制10个字符串
                    # 随机生成字符串位置、颜色和大小
                    x = (urandom.getrandbits(11) % img.width())  # 随机X坐标
                    y = (urandom.getrandbits(11) % img.height())  # 随机Y坐标
                    r = urandom.getrandbits(8)  # 红色分量
                    g = urandom.getrandbits(8)  # 绿色分量
                    b = urandom.getrandbits(8)  # 蓝色分量
                    size = (urandom.getrandbits(30) % 64) + 32  # 字体大小（32到96之间）
    
                    # 绘制字符串，支持中文字符
                    img.draw_string_advanced(
                        x, y, size, "Hello World!，你好庐山派！！！", color=(r, g, b),
                    )
    
                # 将绘制结果显示到屏幕
                Display.show_image(img)
    
                time.sleep(1)  # 暂停1秒
                os.exitpoint()  # 可用的退出点
        except KeyboardInterrupt as e:
            print("用户终止：", e)  # 捕获键盘中断异常
        except BaseException as e:
            print(f"异常：{e}")  # 捕获其他异常
        finally:
            # 清理资源
            Display.deinit()
            os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)  # 启用睡眠模式的退出点
            time.sleep_ms(100)  # 延迟100毫秒
            MediaManager.deinit()
    
    # 主程序入口
    if __name__ == "__main__":
        os.exitpoint(os.EXITPOINT_ENABLE)  # 启用退出点
        display_test()  # 调用显示测试函数

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


这三个代码都是一样的，只是在开头改变了DISPLAY_MODE从而改变了显示模式，"VIRT"、"LCD" 或 "HDMI"分别表示使用CanMV IDE K230帧缓冲区显示，3.1寸屏幕扩展板和HDMI扩展板模式 。然后根据显示模式来确定分辨率，其中`VIRT`和`HDMI`是1080P的分辨率，3.1寸屏幕是800x480，因为我们当前生产的这个屏幕的物理分辨率就是800x480，大分辨率也显示不出来。

接下来创建了一个用于绘图的图像对象，然后根据显示模式给对应的显示设备做初始化，配置了分辨率，帧率及显示方式。

到了循环之后就开始对图形内容的动态绘制，每个循环会生成字符串内容（"Hello World!，你好庐山派！！！"），同时这些字符串的位置，颜色，大小是随机的，每次都会显示10条。

然后调用`Display.show_image(img)`将绘制好的图像对象刷新到屏幕，后面再用`time.sleep(1)` 来暂停1秒，控制一下刷新间隔。

## 4.1 在Canmv IDE K230上显示 ​

这种方式很简单，只需要将开发板用USB线连接至电脑，打开软件后运行上面的第一个程序就在IDE的真缓冲去里面能看到以下画面了。 ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/display/display_20241127_112541.png)

## 4.2 在立创·3.1寸屏幕扩展板上显示 ​

准备好你的屏幕扩展板，按下图所示和庐山派连接起来，建议两条线都连接，免得后面找不到。如果你用不到触摸的话就只需要连接31p MIPI线就好了，触摸6P线可以不连。

![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/display/display_20241127_114505.png)

运行上面的第二个程序就能在屏幕上看到信息了。

## 4.3 用HDMI扩展板在外部显示器上显示 ​

![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/display/display_20241127_113557.png) 装好后如下图所示，运行上面的第三个程序再用HDMI扩展板接入自己的屏幕就能看到画面了。 ![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/image-recog/display/display_20241127_114633.png)

