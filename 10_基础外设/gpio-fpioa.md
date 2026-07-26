# GPIO 和 FPIOA

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/gpio-fpioa.html>
> **最后更新**: 2026-07-01

---

  * 灌入电流指的是LED的供电电流是由外部提供电流，将电流灌入我们的MCU。
  * 输出电流指的是由MCU提供电压电流，将电流输出给LED；如果使用 MCU的GPIO 直接驱动 LED，则驱动能力较弱，可能无法提供足够的电流驱动 LED。
  * 需要注意的 是 LED 灯的颜色不同，对应的电压也不同。电流不可过大，通常需要接入220欧姆到10K欧姆左右的限流电阻，限流电阻的阻值越大，LED的亮度越暗。



### 7.4 RGB灯原理图 ​

结合之前介绍，我们已经知道了LED（发光二极管）灯通常只有一个发光二极管，它能产生单一颜色的光。比如，一个红色 LED 只会发出红光，而一个蓝色 LED 只会发出蓝光。而本庐山派开发板和泰山派一样，板载了一个RGB灯，他是红色、绿色和蓝色 LED 灯的组合，可以单独控制每个 LED 的亮灭。

那么我们先来看一下开发板上的RGB灯分别连接的是哪些引脚，看一下原理图：

#### 立创·庐山派K230-CanMV开发板 的 RGB灯原理图 ​

![庐山派 LED1 ](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/gpio-fpioa/gpio-fpioa_20241012_162225.png)

#### 立创·庐山派Lite-K230D-CanMV开发板 的 RGB灯原理图 ​

![庐山派Lite LED](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/gpio-fpioa/gpio-fpioa_20260701_175715.png)

这里的`LED1`就是我们需要控制的用户指示灯，结合左上角的短接符我们可以了解到以下信息：

  * 这是一个共阳级的RGB灯，当对应颜色引脚为低电平时对应引脚就会亮。
  * 这个RGB灯内部有三个不同颜色的灯珠。
  * 立创·庐山派K230-CanMV开发板 上，**红灯** 、**绿灯** 、**蓝灯** 分别接到了 **GPIO62** 、**GPIO20** 和 **GPIO63** 上面。
  * 立创·庐山派Lite-K230D-CanMV开发板的板载 RGB 灯连接不同：**红灯 GPIO65** 、**绿灯 GPIO66** 、**蓝灯 GPIO71** ，并且是**高电平点亮** 。这其实是因为K230D引脚受限，到最后已经没有3.3V的IO可以用了，只能用这几个1.8V电平的IO了，这个RGB灯本身是3.3V供电的，里面的绿灯、蓝灯的正向压降较高，1.8V 基本无法直接点亮，所以这里加入了三个 MOS 管，从而可以用 1.8V GPIO 控制 3.3V LED 电流，从而间接点亮这个RGB灯。



### 7.5 基础点灯试验 ​

接下来我们实现一个让 RGB 灯里面的红灯每 0.5 秒闪烁一次的程序。两块开发板的实验现象相同，但红灯引脚和点亮电平不同，所以这里分别给出完整例程。

#### 7.5.1 立创·庐山派K230-CanMV开发板完整例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import Pin
    from machine import FPIOA
    import os
    import time
    
    LED_ON_LEVEL = 0   # 立创·庐山派K230-CanMV开发板：低电平点亮
    LED_OFF_LEVEL = 1
    
    fpioa = FPIOA()
    fpioa.set_function(62, FPIOA.GPIO62)  # 红灯
    fpioa.set_function(20, FPIOA.GPIO20)  # 绿灯
    fpioa.set_function(63, FPIOA.GPIO63)  # 蓝灯
    
    LED_R = Pin(62, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    LED_G = Pin(20, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    LED_B = Pin(63, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    
    LED_R.value(LED_OFF_LEVEL)
    LED_G.value(LED_OFF_LEVEL)
    LED_B.value(LED_OFF_LEVEL)
    
    LED = LED_R  # 当前控制红色 LED
    
    try:
        while True:
            os.exitpoint()
            LED.value(LED_ON_LEVEL)
            time.sleep(0.5)
            LED.value(LED_OFF_LEVEL)
            time.sleep(0.5)
    except Exception as e:
        print("程序退出：", e)
    finally:
        LED_R.value(LED_OFF_LEVEL)
        LED_G.value(LED_OFF_LEVEL)
        LED_B.value(LED_OFF_LEVEL)

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


#### 7.5.2 立创·庐山派Lite-K230D-CanMV开发板完整例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import Pin
    from machine import FPIOA
    import os
    import time
    
    LED_ON_LEVEL = 1   # 立创·庐山派Lite-K230D-CanMV开发板：高电平点亮
    LED_OFF_LEVEL = 0
    
    fpioa = FPIOA()
    fpioa.set_function(65, FPIOA.GPIO65)  # 红灯
    fpioa.set_function(66, FPIOA.GPIO66)  # 绿灯
    fpioa.set_function(71, FPIOA.GPIO71)  # 蓝灯
    
    LED_R = Pin(65, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    LED_G = Pin(66, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    LED_B = Pin(71, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    
    LED_R.value(LED_OFF_LEVEL)
    LED_G.value(LED_OFF_LEVEL)
    LED_B.value(LED_OFF_LEVEL)
    
    LED = LED_R  # 当前控制红色 LED
    
    try:
        while True:
            os.exitpoint()
            LED.value(LED_ON_LEVEL)
            time.sleep(0.5)
            LED.value(LED_OFF_LEVEL)
            time.sleep(0.5)
    except Exception as e:
        print("程序退出：", e)
    finally:
        LED_R.value(LED_OFF_LEVEL)
        LED_G.value(LED_OFF_LEVEL)
        LED_B.value(LED_OFF_LEVEL)

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


在上述两段程序中，首先从 `machine` 导入了 `Pin` 和 `FPIOA` 模块，用来初始化和控制 GPIO 引脚；也导入了 `time` 模块，用来调用时间延迟函数。

两块开发板的代码逻辑完全一致，主要差异只有两处：

  1. 板载 RGB 灯连接的 GPIO 不同。
  2. 立创·庐山派K230-CanMV开发板是低电平点亮，立创·庐山派Lite-K230D-CanMV开发板是高电平点亮。



代码里用 `LED_ON_LEVEL` 和 `LED_OFF_LEVEL` 把点亮电平集中起来，后面控制 LED 时只需要写 `LED.value(LED_ON_LEVEL)` 或 `LED.value(LED_OFF_LEVEL)`，初学者看起来会更清楚。

如果要改成闪烁绿色 LED，可以把：

python
    
    
    LED = LED_R

1  


改成：

python
    
    
    LED = LED_G

1  


如果要改成闪烁蓝色 LED，可以改成：

python
    
    
    LED = LED_B

1  


### 7.6 点亮7种不同颜色的RGB灯 ​

接下来我们实现一个让 RGB 灯组合成七种不同颜色并循环闪烁的程序。两块开发板仍然分别给出完整例程，方便各位直接复制运行。

#### 7.6.1 立创·庐山派K230-CanMV开发板完整例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import Pin
    from machine import FPIOA
    import os
    import time
    
    LED_ON_LEVEL = 0
    LED_OFF_LEVEL = 1
    
    fpioa = FPIOA()
    fpioa.set_function(62, FPIOA.GPIO62)  # 红灯
    fpioa.set_function(20, FPIOA.GPIO20)  # 绿灯
    fpioa.set_function(63, FPIOA.GPIO63)  # 蓝灯
    
    LED_R = Pin(62, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    LED_G = Pin(20, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    LED_B = Pin(63, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    
    
    def set_led(pin, on):
        pin.value(LED_ON_LEVEL if on else LED_OFF_LEVEL)
    
    
    def set_color(r_on, g_on, b_on):
        set_led(LED_R, r_on)
        set_led(LED_G, g_on)
        set_led(LED_B, b_on)
    
    
    def blink_color(r_on, g_on, b_on, delay):
        set_color(r_on, g_on, b_on)
        time.sleep(delay)
        set_color(False, False, False)
        time.sleep(delay)
    
    try:
        set_color(False, False, False)
        while True:
            os.exitpoint()
            blink_color(True, False, False, 0.5)   # 红色
            blink_color(False, True, False, 0.5)   # 绿色
            blink_color(False, False, True, 0.5)   # 蓝色
            blink_color(True, True, False, 0.5)    # 黄色
            blink_color(True, False, True, 0.5)    # 紫色
            blink_color(False, True, True, 0.5)    # 青色
            blink_color(True, True, True, 0.5)     # 白色
    except Exception as e:
        print("程序退出：", e)
    finally:
        set_color(False, False, False)

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


#### 7.6.2 立创·庐山派Lite-K230D-CanMV开发板完整例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import Pin
    from machine import FPIOA
    import os
    import time
    
    LED_ON_LEVEL = 1
    LED_OFF_LEVEL = 0
    
    fpioa = FPIOA()
    fpioa.set_function(65, FPIOA.GPIO65)  # 红灯
    fpioa.set_function(66, FPIOA.GPIO66)  # 绿灯
    fpioa.set_function(71, FPIOA.GPIO71)  # 蓝灯
    
    LED_R = Pin(65, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    LED_G = Pin(66, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    LED_B = Pin(71, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    
    
    def set_led(pin, on):
        pin.value(LED_ON_LEVEL if on else LED_OFF_LEVEL)
    
    
    def set_color(r_on, g_on, b_on):
        set_led(LED_R, r_on)
        set_led(LED_G, g_on)
        set_led(LED_B, b_on)
    
    
    def blink_color(r_on, g_on, b_on, delay):
        set_color(r_on, g_on, b_on)
        time.sleep(delay)
        set_color(False, False, False)
        time.sleep(delay)
    
    try:
        set_color(False, False, False)
        while True:
            os.exitpoint()
            blink_color(True, False, False, 0.5)   # 红色
            blink_color(False, True, False, 0.5)   # 绿色
            blink_color(False, False, True, 0.5)   # 蓝色
            blink_color(True, True, False, 0.5)    # 黄色
            blink_color(True, False, True, 0.5)    # 紫色
            blink_color(False, True, True, 0.5)    # 青色
            blink_color(True, True, True, 0.5)     # 白色
    except Exception as e:
        print("程序退出：", e)
    finally:
        set_color(False, False, False)

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


这个例程的核心是 `set_color(r_on, g_on, b_on)` 函数，它分别控制红、绿、蓝三个通道。这里的 `True` 表示点亮某个颜色通道，`False` 表示关闭某个颜色通道。

两块开发板的颜色组合逻辑完全一致，差异仍然集中在 GPIO 编号和 `LED_ON_LEVEL` / `LED_OFF_LEVEL`。这样写的好处是：后面调用 `blink_color(True, False, False, 0.5)` 时，不需要再关心当前开发板到底是高电平点亮还是低电平点亮。

## 八 使用用户按键 ​

> 立创·庐山派K230-CanMV开发板与立创·庐山派Lite-K230D-CanMV开发板的用户按键位置入下图所示。

![图 4](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/gpio-fpioa/gpio-fpioa_20260701_193014.png)

### 8.1 独立按键是什么 ​

独立按键是一种简单的输入设备，广泛应用于各种电子设备中，用于实现基本的用户交互。它们的工作原理通常基于一个简单的机械开关，当按下按键时触发某些操作。独立按键可以有多种尺寸、形状和颜色，便于用户辨识和使用。常见按键展示：

![常见按键](https://wiki.lckfb.com/storage/images/zh-hans/esp32s3r8n8/micropython-beginner/key-led/key-led_20240828_122047.png)

### 8.2 独立按键结构组成 ​

独立按键的主要结构组成包括：按钮、外壳、弹簧、触点、导电片和引脚。由一个弹性体（如弹簧或金属片）和一个按键帽组成。当按键被用户按下时，弹性体会缩短，使按键帽压缩，使按钮顶部变得接近或触摸基底。当用户松开按钮时，弹性体恢复原状，按键返回初始位置。所以当按键未被按下时，通常触点是分开的，电路是断开的。当按下按键时，导电片触碰到触点，从而形成一个闭合电路。常见按键原理图示意如下： ![结构组成](https://wiki.lckfb.com/storage/images/zh-hans/esp32s3r8n8/micropython-beginner/key-led/key-led_20240828_183016.png)

### 8.3 独立按键驱动原理 ​

独立按键驱动是为了让微控制器能识别按键的状态，而微控制器正好可以识别高电平和低电平，所以大多数的按键都是通过给按键的一端接入高电平，一端接入GPIO；或者是给按键的一端接入低电平，一端接入GPIO。通过检测连接按键的引脚有没有发生电平变化，就可以知道按键是否按下。

### 8.4 消抖措施 ​

我们通常用的按键内部都是机械弹性开关，当它按下或者弹起的时候，机械触点会因为弹性作用而在闭合和断开的瞬间伴随着一连串的抖动。这种抖动会导致输入信号在高低电位之间弹跳，产生不正确的输入。这就是按键抖动现象。消抖措施主要分为软件消抖和硬件消抖：

  1. 软件消抖：主要是通过编程的方法，设定一个延迟或计时器，确保不读取按键在抖动状态时的电平，避免抖动对程序的影响。
  2. 硬件RC消抖：在按键电路中加入元器件如电阻、电容组成的RC滤波器，对按键信号进行平滑处理，降低抖动的影响。注意硬件消抖只能改善而不能消除抖动（除非你用比较大的电容，但那样会导致上升沿太缓，也会造成按钮反应时间太长）。同时，如果你只用一个电容直接并联在按键两端，当按键按下时，按键相当于短路了这个电容，但是按钮的电阻又很小，就会有很大的放电电流，造成按钮机械接触点加速老化，从而导致按钮寿命极具减少。所以这个电路的设计需要下一些功夫，一般来说即使你的硬件电路中做了硬件消抖，也是需要软件消抖来配合的。
  3. 硬件RS触发器消抖：成本极高，每个按钮都需要单独的RS电路，这里不过多介绍，想详细理解请从逻辑门电路开始。



![图 7](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/gpio-fpioa/gpio-fpioa_20241023_153647.png)

TIP

在本庐山派开发板中，按键部分的电路很简单(如下面的原理图所示)，没有使用硬件消抖，所以在需要使用按钮来进行状态切换功能时需要进行软件消抖处理。

### 8.5 板载独立按键原理图 ​

![图 4](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/gpio-fpioa/gpio-fpioa_20241012_162250.png)

立创·庐山派-K230-CanMV开发板的原理图中，将按键一端（1号引脚）通过电阻R78接到3.3V的高电平上，另一端（2号引脚）接到K230芯片的引脚GPIO53上，3号引脚和4号引脚是我们板载侧按按钮的固定角，没有电气作用，只是用来固定按键的。这样当按键按下时，1号引脚和2号引脚就会导通，GPIO53的电平就会变为3.3V。

![图 5](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/gpio-fpioa/gpio-fpioa_20260701_193304.png)

立创·庐山派Lite-K230D-CanMV开发板的用户按键连接到 **GPIO64** ，同样是下拉输入，按下为高电平，只不过因为这个IO的电平的1.8V的。

这里面电阻（R78/R84）的作用是限流（害怕初学者不小心给设置成推挽输出了）。

在这里要注意的是要在芯片内部将该GPIO（GPIO53/64）设置为下拉输入模式，这样当按钮没被按下时，引脚为默认的低电平状态。

### 8.6 按键控制板载RGB灯亮灭 ​

当用户按下用户按键时关闭红色 LED，松开按键时点亮红色 LED。两块开发板的按键电平逻辑一致，都是按下为高电平，但按键 GPIO 和红灯 GPIO 不同。

#### 8.6.1 立创·庐山派K230-CanMV开发板完整例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import Pin
    from machine import FPIOA
    import os
    import time
    
    LED_ON_LEVEL = 0
    LED_OFF_LEVEL = 1
    BUTTON_PRESS_LEVEL = 1
    
    fpioa = FPIOA()
    fpioa.set_function(62, FPIOA.GPIO62)  # 红灯
    fpioa.set_function(53, FPIOA.GPIO53)  # 用户按键
    
    LED_R = Pin(62, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    button = Pin(53, Pin.IN, Pin.PULL_DOWN)
    
    try:
        LED_R.value(LED_OFF_LEVEL)
        while True:
            os.exitpoint()
            if button.value() == BUTTON_PRESS_LEVEL:
                LED_R.value(LED_OFF_LEVEL)  # 按下按键，关闭红灯
            else:
                LED_R.value(LED_ON_LEVEL)   # 松开按键，点亮红灯
            time.sleep_ms(10)
    except Exception as e:
        print("程序退出：", e)
    finally:
        LED_R.value(LED_OFF_LEVEL)

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


#### 8.6.2 立创·庐山派Lite-K230D-CanMV开发板完整例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import Pin
    from machine import FPIOA
    import os
    import time
    
    LED_ON_LEVEL = 1
    LED_OFF_LEVEL = 0
    BUTTON_PRESS_LEVEL = 1
    
    fpioa = FPIOA()
    fpioa.set_function(65, FPIOA.GPIO65)  # 红灯
    fpioa.set_function(64, FPIOA.GPIO64)  # 用户按键
    
    LED_R = Pin(65, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    button = Pin(64, Pin.IN, Pin.PULL_DOWN)
    
    try:
        LED_R.value(LED_OFF_LEVEL)
        while True:
            os.exitpoint()
            if button.value() == BUTTON_PRESS_LEVEL:
                LED_R.value(LED_OFF_LEVEL)  # 按下按键，关闭红灯
            else:
                LED_R.value(LED_ON_LEVEL)   # 松开按键，点亮红灯
            time.sleep_ms(10)
    except Exception as e:
        print("程序退出：", e)
    finally:
        LED_R.value(LED_OFF_LEVEL)

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


前半部分和我们在前面学习的点亮 RGB 灯是一样的，都是先通过 FPIOA 把对应引脚配置为 GPIO 功能，再用 `Pin` 创建控制对象。不同的是，这里又增加了一个用户按键输入引脚。

两块开发板的按键都是下拉输入，按下时 `button.value()` 读到 `1`，松开时读到 `0`。本例程关注的是“按键当前是否按下”，不关注“按键状态有没有发生变化”，所以这里暂时不做消抖处理。

### 8.7 用按键切换RGB灯状态 ​

如果希望“按一下切换一次 LED 状态”，就不能只看按键当前是否按下，还要判断按键是否刚刚从松开变为按下，并加入软件消抖。

流程图如下：
    
    
    flowchart TD
        A[开始] --> B[创建 FPIOA 对象]
        B --> C[设置 GPIO 引脚功能]
        C --> D[实例化 LED 和按键引脚]
        D --> E[初始化 LED 状态并设置消抖时间]
    
        E --> F[进入主循环]
        F --> G[获取按键状态和当前时间]
    
        G --> H{按键从 0 变为 1?}
        H -- 是 --> I{时间间隔 > 消抖时间?}
        I -- 是 --> J[切换 LED 状态]
        J --> K{当前 LED 状态?}
        K -- 亮 --> L[熄灭 LED]
        K -- 灭 --> M[点亮 LED]
        L --> N[更新最后按下时间]
        M --> N
    
        N --> O[更新上次按键状态]
        O --> F
    
        H -- 否 --> P[更新上次按键状态]
        P --> F
    

#### 8.7.1 立创·庐山派K230-CanMV开发板完整例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import Pin
    from machine import FPIOA
    import os
    import time
    
    LED_ON_LEVEL = 0
    LED_OFF_LEVEL = 1
    BUTTON_PRESS_LEVEL = 1
    DEBOUNCE_MS = 20
    
    fpioa = FPIOA()
    fpioa.set_function(62, FPIOA.GPIO62)  # 红灯
    fpioa.set_function(53, FPIOA.GPIO53)  # 用户按键
    
    LED_R = Pin(62, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    button = Pin(53, Pin.IN, Pin.PULL_DOWN)
    
    last_press_time = 0
    button_last_state = 0
    led_on = False
    
    
    def set_led(on):
        LED_R.value(LED_ON_LEVEL if on else LED_OFF_LEVEL)
    
    try:
        set_led(False)
        while True:
            os.exitpoint()
            button_state = button.value()
            current_time = time.ticks_ms()
    
            if button_state == BUTTON_PRESS_LEVEL and button_last_state != BUTTON_PRESS_LEVEL:
                if time.ticks_diff(current_time, last_press_time) > DEBOUNCE_MS:
                    led_on = not led_on
                    set_led(led_on)
                    last_press_time = current_time
    
            button_last_state = button_state
            time.sleep_ms(10)
    except Exception as e:
        print("程序退出：", e)
    finally:
        set_led(False)

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


#### 8.7.2 立创·庐山派Lite-K230D-CanMV开发板完整例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import Pin
    from machine import FPIOA
    import os
    import time
    
    LED_ON_LEVEL = 1
    LED_OFF_LEVEL = 0
    BUTTON_PRESS_LEVEL = 1
    DEBOUNCE_MS = 20
    
    fpioa = FPIOA()
    fpioa.set_function(65, FPIOA.GPIO65)  # 红灯
    fpioa.set_function(64, FPIOA.GPIO64)  # 用户按键
    
    LED_R = Pin(65, Pin.OUT, pull=Pin.PULL_NONE, drive=7)
    button = Pin(64, Pin.IN, Pin.PULL_DOWN)
    
    last_press_time = 0
    button_last_state = 0
    led_on = False
    
    
    def set_led(on):
        LED_R.value(LED_ON_LEVEL if on else LED_OFF_LEVEL)
    
    try:
        set_led(False)
        while True:
            os.exitpoint()
            button_state = button.value()
            current_time = time.ticks_ms()
    
            if button_state == BUTTON_PRESS_LEVEL and button_last_state != BUTTON_PRESS_LEVEL:
                if time.ticks_diff(current_time, last_press_time) > DEBOUNCE_MS:
                    led_on = not led_on
                    set_led(led_on)
                    last_press_time = current_time
    
            button_last_state = button_state
            time.sleep_ms(10)
    except Exception as e:
        print("程序退出：", e)
    finally:
        set_led(False)

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


这个程序最重要的改进就是进行了按键消抖，确保每次按钮的状态变化都有效，避免由于机械抖动造成错误触发。

在这个程序中，我们设置按钮的消抖时间为 20ms，然后创建几个变量来辅助进行状态记录：`last_press_time` 用来记录上次按键按下的时间，`led_on` 用来记录 LED 的亮灭状态，`button_last_state` 用来记录上次检查按钮时的按键状态。

两块开发板的按键判断逻辑完全一致，差异仍然集中在红灯 GPIO、用户按键 GPIO 和 LED 点亮电平上。

## 九 常见问题 ​

### 9.1 为什么 立创·庐山派Lite-K230D-CanMV开发板 复制立创·庐山派K230-CanMV开发板 代码后 LED 没反应？ ​

因为两块板的板载 RGB 灯引脚不同，点亮电平也不同。立创·庐山派Lite-K230D-CanMV开发板需要使用 GPIO65、GPIO66、GPIO71，并且是高电平点亮。

### 9.2 为什么按键读取一直不对？ ​

先确认自己使用的开发板型号。立创·庐山派K230-CanMV开发板的用户按键是 GPIO53，立创·庐山派Lite-K230D-CanMV开发板的用户按键是 GPIO64。两块板的按键都是下拉输入，按下为高电平。

### 9.3 为什么外接模块接到 立创·庐山派Lite-K230D-CanMV开发板 排针后没有反应？ ​

立创·庐山派Lite-K230D-CanMV开发板的部分 40Pin IO 与摄像头复位脚、蜂鸣器、功放、风扇等板载功能存在复用关系。外接模块前，请先确认该 IO 当前没有被其他板载功能占用。

