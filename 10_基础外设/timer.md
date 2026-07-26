# 定时器【TIMER】

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/timer.html>
> **最后更新**: 2024-12-12

---

# 1 本节介绍 ​

📝本节您将学习如何通过K230开发板来创建并使用定时器，定时器是嵌入式开发中重要的时间控制手段之一，利用定时器可以在精确定时后触发特定的操作。例如，定时闪烁 LED、定时读取传感器数据、定时执行某些定期维护或统计任务等。

🏆学习目标

1️⃣了解K230内部内部定时器的基本概念

2️⃣掌握软件定时器的初始化与回调函数的设置方法

# 2 名词解释 ​

名词| 说明  
---|---  
定时器【TIMER】| **Timer** 是一种常见的硬件和软件模块，用于执行与时间相关的任务，通常比系统模拟出来的软件定时器更准。  
  
注意：

当前固件只支持软件定时器，K230内部的硬件定时器暂不可用。

# 3 TIMER使用指南 ​

K230 内部集成了 6 个 Timer 硬件模块，最小定时周期为 1 毫秒（ms）。

Timer 类位于 `machine` 模块中。

## 3.1 示例代码 ​

python
    
    
    from machine import Timer
    import time
    
    # 创建一个软件定时器实例，-1 表示使用软件定时器
    tim = Timer(-1)
    
    # 配置定时器，单次模式（ONE_SHOT），定时100毫秒，回调函数打印数字1
    tim.init(period=100, mode=Timer.ONE_SHOT, callback=lambda t: print("Timer Callback: 1"))
    
    # 再次配置定时器，这次为周期模式（PERIODIC），每隔1000毫秒（1秒）打印数字2
    tim.init(period=1000, mode=Timer.PERIODIC, callback=lambda t: print("Timer Callback: 2"))
    
    # 延时5.1秒，让定时有时间执行
    time.sleep(5.1)
    
    # 当不再需要定时器时，可以停用它
    tim.deinit()

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


在上面的代码中，先用一次性定时（ONE_SHOT）回调打印数字1，然后切换为周期定时（PERIODIC）每1秒打印一次数字2。延时5.1秒等待时间生效。最后通过 `deinit()` 方法释放定时器资源。

## 3.2 构造函数 ​

python
    
    
    timer = Timer(index, mode=Timer.PERIODIC, freq=-1, period=-1, callback=None, arg=None)

1  


**参数**

  * `index`: 定时器模块编号，取值范围为 `[-1, 5]`。 
    * `-1` 表示软件定时器。
    * `0~5` 为硬件定时器编号，【但当前不可用】。
  * `mode`: 定时器运行模式，可选 `Timer.ONE_SHOT` 或 `Timer.PERIODIC`。 
    * `ONE_SHOT`：定时器在触发一次回调后自动停止。
    * `PERIODIC`：定时器在触发回调后会自动重置周期，持续周期性触发。
  * `freq`: 定时器频率（Hz），可以为浮点数。如果设置了 `freq`，则定时器的时间间隔由频率决定，`freq` 优先级高于 `period`。例如，`freq=1` 相当于 `period=1000ms`。
  * `period`: 定时器周期，单位为毫秒（ms），如果未设置 `freq`，则以 `period` 为准。
  * `callback`: 超时回调函数。当定时器计数完成后自动调用该函数，函数至少应包含一个参数（接收定时器自身对象或关联参数）。
  * `arg`: 超时回调函数的参数（可选）。如果提供了 `arg`，在回调函数中将可以通过传入的参数获取额外信息。



**注意：** **硬件定时器 [0-5] 暂不可用。**

## 3.3 `init` 方法 ​

python
    
    
    Timer.init(mode=Timer.PERIODIC, freq=-1, period=-1, callback=None, arg=None)

1  


用于初始化或重新配置定时器的参数。参数与构造函数相同。

**参数**

  * `mode`: 定时器运行模式，可以是单次或周期模式（可选参数）。
  * `freq`: 定时器运行频率，支持浮点数，单位为赫兹（Hz），此参数优先级高于 `period`（可选参数）。
  * `period`: 定时器运行周期，单位为毫秒（ms）（可选参数）。
  * `callback`: 超时回调函数，必须设置并应带有一个参数。
  * `arg`: 超时回调函数的参数（可选参数）。



**返回值**

无

## 3.4 `deinit` 方法 ​

python
    
    
    Timer.deinit()

1  


释放定时器资源。

**参数**

无

**返回值**

无

# 4 用软件定时器控制LED灯 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    from machine import RTC
    from machine import Pin
    from machine import FPIOA
    from machine import Timer
    import time
    
    fpioa = FPIOA()
    
    # 将62、20、63号引脚映射为GPIO，用于控制RGB灯的三个颜色通道
    fpioa.set_function(62, FPIOA.GPIO62)
    fpioa.set_function(20, FPIOA.GPIO20)
    fpioa.set_function(63, FPIOA.GPIO63)
    
    LED_R = Pin(62, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 红色LED
    LED_G = Pin(20, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 绿色LED
    LED_B = Pin(63, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 蓝色LED
    
    # 板载RGB灯是共阳结构，高电平=关闭灯，低电平=点亮灯
    # 初始化时先关闭所有LED灯
    LED_R.high()
    LED_G.high()
    LED_B.high()
    
    # 定义一个变量记录当前点亮的是哪一种LED
    color_state = 0  # 0:红, 1:绿, 2:蓝
    
    # 回调函数，用于定期切换LED状态
    def led_toggle(timer):
        global color_state
        # 首先关闭所有LED
        LED_R.high()
        LED_G.high()
        LED_B.high()
    
        # 根据当前状态点亮对应的LED
        if color_state == 0:
            LED_R.low()  # 点亮红灯
        elif color_state == 1:
            LED_G.low()  # 点亮绿灯
        elif color_state == 2:
            LED_B.low()  # 点亮蓝灯
    
        # 切换到下一个颜色状态
        color_state = (color_state + 1) % 3
    
    # 创建软件定时器，index=-1表示软件定时器
    tim = Timer(-1)
    
    # 初始化定时器为周期模式，每隔500ms调用一次led_toggle回调函数
    tim.init(period=500, mode=Timer.PERIODIC, callback=led_toggle)
    
    # 主循环：此处无需手动控制LED，定时器会定期触发回调函数
    while True:
        # 主循环空转，保持程序运行
        time.sleep(1)

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


通过在IDE中运行上面的代码，我们可以直观地看到定时器带来的异步定时执行效果：即使主循环在空转（什么都不做），RGB LED 依然会按照定时器设定的周期不断轮换颜色。

