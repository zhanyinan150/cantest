# PWM控制

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/pwm.html>
> **最后更新**: 2026-07-02

---

## 一 本节介绍 ​

📝本节我们将学习如何在立创·庐山派K230-CanMV开发板和立创·庐山派Lite-K230D-CanMV开发板上使用 PWM（脉宽调制）功能。通过 PWM 我们可以驱动板载蜂鸣器播放音调、控制风扇转速、驱动舵机旋转，以及从排针输出 PWM 信号供外部设备使用。

### 1.1 学习目标 ​

🏆学习目标

1️⃣ 理解 PWM 的基本概念（频率、占空比、通道），能把 GPIO 配置为 PWM 输出模式。

2️⃣ 学会驱动板载无源蜂鸣器发声，并能播放简单音调旋律。

3️⃣ 学会用 PWM 控制风扇转速和舵机角度。

4️⃣ 知道本章代码如何通过 `get_board_info()` 同时适配两种开发板，不需要手动改引脚。

### 1.2 重点提示 ​

⚠️注意！

K230/K230D 的 PWM 通道分两组：**PWM0/1/2 共享频率** ，**PWM3/4/5 共享频率** 。同组通道的占空比可以独立设置，但频率会互相影响。蜂鸣器用 PWM1（4kHz）时，同组的 PWM0 和 PWM2 也会被迫使用 4kHz 频率，不适合同时输出 50Hz 舵机信号。

## 二 软硬件准备 ​

名称| 数量| 说明  
---|---|---  
立创·庐山派K230-CanMV开发板 / 立创·庐山派Lite-K230D-CanMV开发板| 1| 二选一，教程默认同时适配  
Type-C 数据线| 1| 用于供电和连接 IDE  
舵机（可选）| 1| 如测试舵机控制，推荐 SG90 或 ES08A II  
5V 外部电源（可选）| 1| 舵机供电用，与开发板共地  
示波器或逻辑分析仪（可选）| 1| 验证 PWM 输出波形  
  
**固件要求** ：CanMV 最新版本（如代码报错请升级固件，参考快速入门文档）。

## 三 双板兼容说明 ​

项目| 立创·庐山派K230-CanMV开发板| 立创·庐山派Lite-K230D-CanMV开发板  
---|---|---  
是否支持本节实验| 支持| 支持  
板载蜂鸣器| GPIO43 → PWM1| GPIO61 → PWM1  
板载风扇接口| 无| GPIO60 → PWM0  
舵机推荐引脚| GPIO47 → PWM3| GPIO59 → PWM5  
屏幕背光 PWM| GPIO25 → PWM5| 以对应原理图为准  
板卡适配方式| `get_board_info()` 自动适配| `get_board_info()` 自动适配  
  
IMPORTANT

本教程所有代码通过 `get_board_info()` 自动适配两种开发板。用户不需要手动修改蜂鸣器、风扇或舵机的 GPIO 编号。

## 四 名词解释 ​

名词| 说明  
---|---  
PWM| **Pulse width modulation** （脉宽调制）  
无源蜂鸣器| 是一种需要外部信号源提供音频频率的电子器件，用于产生声音。  
FPIOA| Flexible Peripheral Input/Output Array，引脚功能复用配置  
PWM 通道| K230/K230D 内部 PWM 外设输出通道，编号 PWM0~PWM5  
占空比| 一个周期内高电平时间的比例  
  
INFO

  1. 脉冲宽度调制（PWM）是一种通过改变脉冲的宽度来控制电信号平均功率的技术。在PWM中，脉冲的频率一般保持恒定，但脉冲的宽度（有效电平的时间）根据需要的模拟信号变化，从而实现对电机速度、LED调光和温度控制等的精确控制。
  2. 无源蜂鸣器是一种电子发声器件，直接供电并不会鸣叫，只有当输入信号的频率变化时，其发出的声音的频率和音调才会发生相应变化。主要用来产生简单的音调来警示用户。



## 五 什么是PWM？ ​

![PWM 波形示意图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/pwm/pwm_20241024_121402.png)

PWM（Pulse Width Modulation，脉宽调制）是一种在嵌入式系统中常用的技术，它可以用来模拟信号，控制设备的功率输出或者实现对设备的精确控制。PWM信号是一种类似于方波的信号，具有固定的频率，但脉冲宽度（占空比）可以调整。在一定频率下，我们可以通过调整这个占空比来改变他的有效电压，在一定程度上可以实现D/A转换（数字量转模拟量，不过一般来说都是用DAC，本开发板K230的DAC已经被连接到了3.5mm耳机孔上面了，可以用来播放音频）。

  * 频率（Frequency）：指PWM信号在一秒内循环的次数。频率是周期的倒数，单位是赫兹（Hz）。
  * 周期（Period）：指一个完整的PWM信号的时间长度，与频率成反比。单位是秒（s）。
  * 脉宽（Pulse Width）：指PWM信号中高电平（通常为1）的时间长度。单位是秒（s）或毫秒（ms）。
  * 占空比(Duty Ratio)：表示在一个完整的PWM信号周期内，高电平（通常为1）所占的时间比例。占空比 = （脉宽 / 周期）x 100%。
  * 上升沿（Rising Edge）：PWM信号从低电平跳变到高电平的瞬间，通常用来作为触发事件。
  * 下降沿（Falling Edge）：PWM信号从高电平跳变到低电平的瞬间，也常被用作触发事件。
  * 正脉冲宽度（Positive Pulse Width）：PWM信号中高电平的持续时间，一般情况下的脉宽指的就是这个。
  * 负脉冲宽度（Negative Pulse Width）：PWM信号中低电平的持续时间。



核心参数速查：

参数| 含义| 单位  
---|---|---  
频率（Frequency）| 一秒内 PWM 信号循环次数| Hz  
周期（Period）| 一个完整波形的时间长度，= 1/频率| s  
占空比（Duty Cycle）| 高电平时间占整个周期的比例| %  
脉宽（Pulse Width）| 高电平持续时间| ms / μs  
  
在嵌入式系统中，PWM的应用场景非常广泛，例如：

  1. **电机控制** ：通过调整PWM的占空比，可以精确控制直流电机的转速。占空比越高，电机转速越快；占空比越低，电机转速越慢。
  2. **LED亮度调节** ：通过调整PWM的占空比，可以实现对LED灯的亮度调节。占空比越高，LED灯越亮；占空比越低，LED灯越暗。
  3. **蜂鸣器发声** ：通过改变频率产生不同音调。
  4. **舵机角度控制** ：50Hz 频率下，0.5ms~2.5ms 脉宽对应 0°~180°。



举个例子：假设我们有一个LED灯，想实现亮度调节。我们可以生成一个PWM信号，频率为1kHz（周期为1ms），然后通过调整占空比来实现LED灯的亮度调节。如果占空比为100%，那么LED灯将一直处于亮的状态；如果占空比为50%，那么LED灯将以1ms为周期，半亮半暗；如果占空比为0%，那么LED灯将一直处于熄灭状态。通过不断调整占空比，就可以实现LED灯的亮度调节。如果PWM信号的频率过低，我们可能会感觉到它在闪烁。所以，在设计PWM驱动的LED灯时，一般会选择较高的频率来避免可见的闪烁。通常我们是看不到它闪烁的，这主要是因为两个原因：第一个是PWM信号的频率够高，第二个是人眼的视觉暂留效应。即当光源瞬间消失时，我们还能在极短的时间内感知到光源。这种效应导致人眼对快速闪烁的光源产生平滑的感觉。当PWM频率足够高时，视觉暂留效应会使我们感觉到LED灯的亮度是连续的。

## 六 手机屏幕的亮度是怎么调节的？ ​

我们平时一直摸的手机屏幕有一部分就是用PWM来调节的。在智能手机屏幕中，PWM调光和DC调光是两种常见的屏幕亮度调节技术。它们的主要区别在于亮度调节的实现方式。

**PWM调光** ：如之前所说，PWM（脉宽调制）调光是通过调整占空比来控制屏幕亮度的。在这种方法中，屏幕背光源会周期性地开启和关闭。占空比越高，背光源亮的时间越长，屏幕亮度越高；占空比越低，背光源亮的时间越短，屏幕亮度越低。

**DC调光** ：DC（直流）调光是通过调整屏幕背光源的电流来实现亮度调节的。在这种方法中，背光源会一直保持开启状态，但电流的大小会改变，从而调整屏幕亮度。

这两种调光技术各有优缺点：

**PWM调光优缺点** ：

  * 优点：能实现较高的亮度范围和对比度，通常在高亮度下表现更好。
  * 缺点：在低频率下，PWM调光可能导致屏幕闪烁，对部分人来说可能引起眼睛疲劳和不适。此外，对于快速拍照或录像时，PWM调光可能导致出现条纹或闪烁的现象。



**DC调光优缺点** ：

  * 优点：在低亮度下表现更好，因为屏幕背光源一直保持开启状态，不会出现闪烁现象，对眼睛更友好。
  * 缺点：在高亮度下，对比度和亮度范围可能不如PWM调光好。此外，DC调光可能导致背光源的寿命降低和能耗略高。



两种技术对比：

| PWM 调光| DC 调光  
---|---|---  
优点| 亮度范围大、对比度高| 无闪烁、低亮度下对眼睛友好  
缺点| 低频下可能闪烁导致眼疲劳| 高亮度下对比度稍逊  
适用| 高亮度场景| 低亮度场景  
  
各种技术的选择取决于手机制造商的考虑和市场需求。有一些高端手机是用的混合调光，在高亮度的模式下用DC调光，亮度低于一定值后用PWM调光。了解这个背景有助于理解 PWM 在实际产品中的广泛应用。

## 七 开发板上可用的PWM信号 ​

### 7.1 40Pin 排针引出的 PWM ​

【立创·庐山派K230-CanMV开发板 排针脚示意图】 ![40Pin PWM 引脚图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/pwm/pwm_20241024_140755.png)

从上图可以看到，我们排针处有以下PWM可供用户使用。

排针物理引脚号| 芯片 GPIO 号| PWM 通道  
---|---|---  
12| GPIO47| PWM3  
26| GPIO61| PWM1  
32| GPIO46| PWM2  
33| GPIO52| PWM4  
35| GPIO42| PWM0  
  
【立创·庐山派Lite-K230D-CanMV开发板 排针脚示意图】

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/gpio-fpioa/gpio-fpioa_20260622_163005.png)

排针物理引脚号| 芯片 GPIO 号| PWM 通道  
---|---|---  
33| GPIO59| PWM5  
35| GPIO60| PWM0  
  
庐山派lite的GPIO60还同时连接到了板载的风扇接口，所以如果你接了风扇的话，那么排针这里的GPIO60你也是用不了的，没办法，K230D的引脚太少了。

IMPORTANT

排针编号和 GPIO 编号不是一个概念。接线时要看开发板丝印或原理图，不要混淆。

### 7.2 PWM 通道复用表 ​

PWM 通道| 可复用 GPIO| 频率组  
---|---|---  
PWM0| GPIO42, GPIO54, GPIO60| 0-1-2 组（共享频率）  
PWM1| GPIO43, GPIO55, GPIO61| 0-1-2 组（共享频率）  
PWM2| GPIO7, GPIO46, GPIO56| 0-1-2 组（共享频率）  
PWM3| GPIO8, GPIO47, GPIO57| 3-4-5 组（共享频率）  
PWM4| GPIO9, GPIO52, GPIO58| 3-4-5 组（共享频率）  
PWM5| GPIO25, GPIO53, GPIO59| 3-4-5 组（共享频率）  
  
⚠️通道频率耦合

同组通道（PWM0/1/2 或 PWM3/4/5）共享频率。如果蜂鸣器（PWM1）正在以 4kHz 播放音调，此时 PWM0（风扇 20kHz）或 PWM2 想输出不同频率会互相干扰。

解决办法：不要在同一时间让同组通道输出不同频率。本教程中蜂鸣器用 PWM1（0-1-2 组），舵机用 PWM3 或 PWM5（3-4-5 组），背光用 PWM5（3-4-5 组），风扇用 PWM0（0-1-2 组）。注意风扇和蜂鸣器在同一组，不要同时运行。

### 7.3 两板板载 PWM 资源差异 ​

开发板| 板载资源| GPIO| PWM 通道  
---|---|---|---  
立创·庐山派K230-CanMV开发板| 无源蜂鸣器| GPIO43| PWM1  
立创·庐山派Lite-K230D-CanMV开发板| 无源蜂鸣器| GPIO61| PWM1  
立创·庐山派Lite-K230D-CanMV开发板| 风扇接口| GPIO60| PWM0  
两板| 屏幕背光| GPIO25| PWM5  
  
### 7.4 板载蜂鸣器电路 ​

【立创·庐山派K230-CanMV开发板 蜂鸣器驱动电路原理图】

![蜂鸣器驱动电路原理图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/pwm/pwm_20241024_145314.png)

【立创·庐山派Lite-K230D-CanMV开发板 蜂鸣器驱动电路原理图】

![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/pwm/pwm_20260702_155331.png)

庐山派使用的是无源电磁式贴片蜂鸣器，工作电压 2.5~4.5V，最佳驱动频率 4000Hz（在此频率下声音最响）。大家可以用这个蜂鸣器来做人机交互的提示，也可以用不同的PWM来驱动这个蜂鸣器来播放简单的纯音调音乐。

电路说明：

  * **D15/D20（续流二极管）** ：保护驱动 MOS 管。蜂鸣器是感性元件，停止供电时会产生反向感应电动势，续流二极管为其提供泄放路径。如果没有这个续流二极管，停止给蜂鸣器供电的时候在蜂鸣器两端会有反向感应电动势，产生高达几十V的尖峰电压，很有可能损坏驱动电路。
  * **R89/R105（限流电阻）** ：防止电流过大损坏芯片 PWM 输出引脚。
  * **R90/R106（下拉电阻）** ：确保无 PWM 信号时 MOS 管截止，蜂鸣器不会误响。



两块开发板的蜂鸣器都使用 PWM1 通道，但 GPIO 不同：

开发板| 蜂鸣器 GPIO| PWM 通道  
---|---|---  
立创·庐山派K230-CanMV开发板| GPIO43| PWM1  
立创·庐山派Lite-K230D-CanMV开发板| GPIO61| PWM1  
  
### 7.5 屏幕背光驱动电路 ​

【立创·庐山派K230-CanMV开发板 屏幕背光驱动电路】 ![屏幕背光驱动电路](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/pwm/pwm_20241024_145440.png)

【立创·庐山派Lite-K230D-CanMV开发板 屏幕背光驱动电路】

【和上面的 立创·庐山派K230-CanMV开发板 屏幕背光驱动电路 完全一样，这里就不再展示了】

屏幕背光驱动本质上是一个恒流源。`LCD_EN`（GPIO25 → PWM5）既可以作为使能电平控制屏幕亮灭，也可以输入 PWM 信号调节背光亮度。

芯片 FB 脚通过采样电阻检测 LED 电流并反馈，维持恒流输出。默认两个 3.6Ω 电阻设定最大电流约 111mA，可兼容大小尺寸屏幕。

背光驱动引脚| PWM 通道  
---|---  
GPIO25（两个板子都是这个引脚）| PWM5  
  
> 蜂鸣器用 PWM1，背光用 PWM5，两者不在同一频率组，可以独立调整互不影响。

## 八 PWM使用指南 ​

### 8.1 FPIOA（引脚功能复用） ​

使用 PWM 前必须先通过 FPIOA 把 GPIO 复用为 PWM 功能：

API| 作用| 示例  
---|---|---  
`FPIOA()`| 创建 FPIOA 对象| `fpioa = FPIOA()`  
`set_function(pin, func)`| 将 GPIO 复用为指定功能| `fpioa.set_function(43, FPIOA.PWM1)`  
  
代码中用 `getattr(FPIOA, f"PWM{ch}")` 动态获取 `FPIOA.PWM0`~`FPIOA.PWM5` 常量，避免硬编码。

### 8.2 PWM API ​

API| 作用| 常用参数| 说明  
---|---|---|---  
`PWM(channel)`| 创建 PWM 对象| channel: 0~5| 先用 FPIOA 配置好引脚  
`freq([freq])`| 获取/设置频率| freq: Hz| 同组通道频率联动  
`duty([duty])`| 获取/设置占空比| duty: 0~100| 百分比形式  
`duty_u16([val])`| 获取/设置占空比| val: 0~65535| 推荐使用，精度高  
`duty_ns([ns])`| 获取/设置高电平时间| ns: 纳秒| 精确脉宽控制  
`deinit()`| 释放 PWM 资源| —| 程序结束时必须调用  
  
> `duty`、`duty_u16`、`duty_ns` 三者只能选一个设置，不要混用。初学推荐 `duty_u16()`。

最小示例：

python
    
    
    from machine import PWM, FPIOA
    
    fpioa = FPIOA()
    fpioa.set_function(43, FPIOA.PWM1)  # GPIO43 → PWM1
    
    beep = PWM(1)
    beep.freq(4000)       # 4kHz
    beep.duty_u16(32768)  # 50% 占空比
    # beep.duty_u16(0)    # 停止输出
    # beep.deinit()       # 释放资源

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


### 8.3 用户可修改的参数 ​

参数| 位置| 可调范围| 说明  
---|---|---|---  
蜂鸣器频率| `beep.freq(4000)`| 200~20000 Hz| 4000Hz 最响，改变频率可演奏不同音调  
蜂鸣器时长| `time.sleep_ms(50)`| 10~5000 ms| 越大声音持续越久  
风扇频率| `fan.freq(20000)`| 1000~25000 Hz| 低于 20kHz 可能有噪声  
舵机角度范围| `ANGLE_MIN/MAX`| 0~180| 根据舵机实际行程调整  
舵机摆动速度| `SWING_DELAY`| 0.005~0.1 s| 越小摆动越快  
  
### 8.4 资源释放 ​

所有例程都使用 `try/finally` 确保退出时调用 `duty_u16(0)` \+ `deinit()`：

  * `duty_u16(0)` 停止 PWM 输出，避免蜂鸣器持续响或风扇不停
  * `deinit()` 释放通道资源，避免下次运行时通道被占用



## 九 代码例程 ​

本节所有代码均通过 `get_board_info()` 自动适配两种开发板，复制即可运行，无需手动修改引脚。

### 9.1 统一版：蜂鸣器鸣叫一声（自动适配两板） ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    蜂鸣器鸣叫一声（统一版）
    - 自动适配 K230（GPIO43）和 Lite K230D（GPIO61）
    - PWM1 通道，4kHz，50% 占空比，持续 50ms
    """
    
    import time, os
    from machine import PWM, FPIOA
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "BUZZER_PIN": 43, "BUZZER_PWM_CH": 1}
        else:
            return {"board": "lushan_lite_k230d", "BUZZER_PIN": 61, "BUZZER_PWM_CH": 1}
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}, 蜂鸣器: GPIO{BOARD['BUZZER_PIN']} -> PWM{BOARD['BUZZER_PWM_CH']}")
    
    fpioa = FPIOA()
    fpioa.set_function(BOARD["BUZZER_PIN"], getattr(FPIOA, f"PWM{BOARD['BUZZER_PWM_CH']}"))
    beep = PWM(BOARD["BUZZER_PWM_CH"])
    
    try:
        os.exitpoint()
        beep.freq(4000)
        beep.duty_u16(32768)
        time.sleep_ms(50)
        print("[INFO] 蜂鸣器已鸣叫一声")
    finally:
        beep.duty_u16(0)
        beep.deinit()

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


### 9.2 蜂鸣器鸣叫一声（分板代码） ​

立创·庐山派K230-CanMV开发板立创·庐山派Lite-K230D-CanMV开发板

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time
    from machine import PWM, FPIOA
    
    # 配置蜂鸣器IO口功能
    beep_io = FPIOA()
    beep_io.set_function(43, FPIOA.PWM1)
    
    # 初始化蜂鸣器PWM通道
    beep_pwm = PWM(1)  # 默认频率4kHz,占空比50%
    
    # 调整通道0频率为4000Hz
    beep_pwm.freq(4000)
    
    # 调整通道0的占空比为 50% (32768 / 65535)
    beep_pwm.duty_u16(32768)
    
    # 使能PWM通道输出
    beep_pwm.duty_u16(32768)
    # 延时50ms
    time.sleep_ms(50)
    # 关闭PWM输出 防止蜂鸣器吵闹
    beep_pwm.duty_u16(0)
    # 叫完了就释放PWM
    beep_pwm.deinit()

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


python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time
    from machine import PWM, FPIOA
    
    # 配置蜂鸣器IO口功能（Lite K230D 使用 GPIO61）
    beep_io = FPIOA()
    beep_io.set_function(61, FPIOA.PWM1)
    
    # 初始化蜂鸣器PWM通道
    beep_pwm = PWM(1)
    
    # 调整通道频率为4000Hz
    beep_pwm.freq(4000)
    
    # 调整占空比为 50% (32768 / 65535)
    beep_pwm.duty_u16(32768)
    
    # 延时50ms
    time.sleep_ms(50)
    # 关闭PWM输出 防止蜂鸣器吵闹
    beep_pwm.duty_u16(0)
    # 叫完了就释放PWM
    beep_pwm.deinit()

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


开头就是导入`time`库用来延时，PWM用来控制引脚输出PWM信号，FPIOA用来将引脚复用为PWM功能。

接下来实例化FPIOA，设置蜂鸣器的驱动脚（K230 为 `GPIO43`，Lite K230D 为 `GPIO61`）为PWM通道1输出模式。设置频率为4KHz，占空比为50%。接下来让蜂鸣器开始发出4Khz的声音，延时50ms后关闭PWM输出来停止蜂鸣器的发声，最后释放一下PWM通道资源，防止在不断电的情况下继续运行其他程序造成的资源占用。

### 9.3 统一版：播放《一闪一闪亮晶晶》 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    蜂鸣器播放《一闪一闪亮晶晶》（统一版）
    - 自动适配 K230（GPIO43）和 Lite K230D（GPIO61）
    """
    
    import time, os
    from machine import PWM, FPIOA
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "BUZZER_PIN": 43, "BUZZER_PWM_CH": 1}
        else:
            return {"board": "lushan_lite_k230d", "BUZZER_PIN": 61, "BUZZER_PWM_CH": 1}
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}, 蜂鸣器: GPIO{BOARD['BUZZER_PIN']}")
    
    fpioa = FPIOA()
    fpioa.set_function(BOARD["BUZZER_PIN"], getattr(FPIOA, f"PWM{BOARD['BUZZER_PWM_CH']}"))
    beep = PWM(BOARD["BUZZER_PWM_CH"])
    beep.freq(4000)
    beep.duty_u16(0)
    
    NOTES = {
        'C4': 261, 'D4': 293, 'E4': 329, 'F4': 349,
        'G4': 392, 'A4': 440, 'B4': 493, 'C5': 523,
    }
    
    MELODY = [
        ('C4', 500), ('C4', 500), ('G4', 500), ('G4', 500),
        ('A4', 500), ('A4', 500), ('G4', 1000),
        ('F4', 500), ('F4', 500), ('E4', 500), ('E4', 500),
        ('D4', 500), ('D4', 500), ('C4', 1000),
    ]
    
    def play_tone(note, duration):
        freq = NOTES.get(note, 0)
        if freq > 0:
            beep.freq(freq)
            beep.duty_u16(32768)
            time.sleep_ms(duration)
        beep.duty_u16(0)
        time.sleep_ms(50)
    
    try:
        print("[INFO] 开始播放《一闪一闪亮晶晶》")
        for note, duration in MELODY:
            os.exitpoint()
            play_tone(note, duration)
        print("[INFO] 播放完成")
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        beep.duty_u16(0)
        beep.deinit()

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


### 9.4 播放《一闪一闪亮晶晶》（分板代码） ​

下述程序的流程图如下：
    
    
    graph TD
        A[程序开始] --> B[配置蜂鸣器IO]
        B --> C[初始化蜂鸣器PWM]
        C --> D{遍历旋律}
        D -- 还有音符未播放--> E[获取音符频率]
        E --> F{频率 > 0?}
        F -- 是 --> G[设置频率并播放音符]
        G --> H[音符持续时间]
        H --> I[停止蜂鸣器并暂停50ms]
        F -- 否 --> I
        I --> D
        D -- 遍历结束--> J[释放PWM资源]
        J --> K[程序结束]
    

立创·庐山派K230-CanMV开发板立创·庐山派Lite-K230D-CanMV开发板

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time
    from machine import PWM, FPIOA
    
    # 配置蜂鸣器IO口功能
    beep_io = FPIOA()
    beep_io.set_function(43, FPIOA.PWM1)
    
    # 初始化蜂鸣器
    beep = PWM(1)
    
    # 调整通道0频率为4000Hz
    beep.freq(4000)
    
    # 调整通道0的占空比为 50% (32768 / 65535)
    beep.duty_u16(32768)
    
    # 定义音符频率（以Hz为单位）
    notes = {
        'C4': 261,
        'D4': 293,
        'E4': 329,
        'F4': 349,
        'G4': 392,
        'A4': 440,
        'B4': 493,
        'C5': 523
    }
    
    # 定义《一闪一闪亮晶晶》旋律和节奏 (音符, 时长ms)
    melody = [
        ('C4', 500), ('C4', 500), ('G4', 500), ('G4', 500),
        ('A4', 500), ('A4', 500), ('G4', 1000),
        ('F4', 500), ('F4', 500), ('E4', 500), ('E4', 500),
        ('D4', 500), ('D4', 500), ('C4', 1000)
    ]
    
    def play_tone(note, duration):
        """播放指定音符"""
        frequency = notes.get(note, 0)
        if frequency > 0:
            beep.freq(frequency)
            beep.duty_u16(32768)
            time.sleep_ms(duration)
            beep.duty_u16(0)
            time.sleep_ms(50)
    
    # 播放旋律
    for note, duration in melody:
        play_tone(note, duration)
    
    # 释放PWM资源
    beep.deinit()

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


python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time
    from machine import PWM, FPIOA
    
    # 配置蜂鸣器IO口功能（Lite K230D 使用 GPIO61）
    beep_io = FPIOA()
    beep_io.set_function(61, FPIOA.PWM1)
    
    # 初始化蜂鸣器
    beep = PWM(1)
    
    # 调整通道频率为4000Hz
    beep.freq(4000)
    
    # 调整占空比为 50% (32768 / 65535)
    beep.duty_u16(32768)
    
    # 定义音符频率（以Hz为单位）
    notes = {
        'C4': 261,
        'D4': 293,
        'E4': 329,
        'F4': 349,
        'G4': 392,
        'A4': 440,
        'B4': 493,
        'C5': 523
    }
    
    # 定义《一闪一闪亮晶晶》旋律和节奏 (音符, 时长ms)
    melody = [
        ('C4', 500), ('C4', 500), ('G4', 500), ('G4', 500),
        ('A4', 500), ('A4', 500), ('G4', 1000),
        ('F4', 500), ('F4', 500), ('E4', 500), ('E4', 500),
        ('D4', 500), ('D4', 500), ('C4', 1000)
    ]
    
    def play_tone(note, duration):
        """播放指定音符"""
        frequency = notes.get(note, 0)
        if frequency > 0:
            beep.freq(frequency)
            beep.duty_u16(32768)
            time.sleep_ms(duration)
            beep.duty_u16(0)
            time.sleep_ms(50)
    
    # 播放旋律
    for note, duration in melody:
        play_tone(note, duration)
    
    # 释放PWM资源
    beep.deinit()

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


上面这个程序定义了一个音符频率表`notes`，单位是Hz，对应了标准的音符。然后定义了旋律和节奏`melody`，包含具体的音符和持续时间的列表，每个元组表示一个音符和它的时长（单位是毫秒）。例如，`('C4', 500)`表示音符`C4`将持续500毫秒。

接下来定义了一个播放单个音符的函数`play_tone`，接受两个参数：音符`note`和持续时间`duration`。在这个函数中，根据音符名称从音符表中查找对应的频率。如果找不到对应的音符，默认返回0；然后设置蜂鸣器的频率为指定的音符频率；开启PWM信号并持续`duration`毫秒，最后停止发声在两个音符之间短暂暂停50ms，避免连续播放时音符之间没有间隔造成的失真。

之后就到了播放旋律了，使用`for`循环遍历`melody`列表，依次播放每个音符。每次循环会调用`play_tone`函数，传入音符和它的持续时间。最后等播放完毕后，释放PWM资源。

### 9.5 风扇 PWM 调速 ​

注意供电

风扇、电机、舵机这类负载不要直接从普通 GPIO 取电。PWM 引脚只负责输出控制信号，负载电源要按开发板接口或外部电源要求连接，并且外部电源和开发板需要共地。

**统一版（自动检测板卡，仅 Lite K230D 有风扇接口）：**

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    风扇 PWM 调速（统一版）
    - 仅 Lite K230D 有风扇接口（GPIO60 -> PWM0）
    - K230 运行时会提示无风扇接口
    - 20kHz PWM，每 2 秒切换占空比
    """
    
    import time, os
    from machine import PWM, FPIOA
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "FAN_PIN": None, "FAN_PWM_CH": None}
        else:
            return {"board": "lushan_lite_k230d", "FAN_PIN": 60, "FAN_PWM_CH": 0}
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}")
    
    if BOARD["FAN_PIN"] is None:
        print("[提示] 当前板卡（立创·庐山派K230-CanMV开发板）没有板载风扇接口。")
        print("[提示] 如需测试 PWM 输出，可改用排针上的其他 PWM 引脚接外部风扇或示波器。")
    else:
        fpioa = FPIOA()
        fpioa.set_function(BOARD["FAN_PIN"], getattr(FPIOA, f"PWM{BOARD['FAN_PWM_CH']}"))
        fan = PWM(BOARD["FAN_PWM_CH"])
        fan.freq(20000)
    
        DUTY_LEVELS = [19660, 39321, 58981]
    
        try:
            print("[INFO] 风扇调速启动（20kHz），每 2 秒切换档位")
            while True:
                os.exitpoint()
                for duty in DUTY_LEVELS:
                    fan.duty_u16(duty)
                    percent = round(duty / 65535 * 100)
                    print(f"[INFO] 当前占空比: {percent}%")
                    time.sleep(2)
        except KeyboardInterrupt:
            print("[INFO] 用户停止")
        finally:
            fan.duty_u16(0)
            fan.deinit()
            print("[INFO] 风扇已停止，PWM 资源已释放")

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


**Lite K230D 专用版：**

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    风扇 PWM 调速示例（仅 Lite K230D）
    - GPIO60 -> PWM0，20kHz
    - 每 2 秒切换占空比（30% -> 60% -> 90%）
    """
    
    import time, os
    from machine import PWM, FPIOA
    
    fpioa = FPIOA()
    fpioa.set_function(60, FPIOA.PWM0)
    
    fan = PWM(0)
    fan.freq(20000)  # 20kHz，避开可闻噪声
    
    DUTY_LEVELS = [19660, 39321, 58981]  # 约 30%、60%、90%
    
    try:
        print("[INFO] 风扇调速启动（20kHz），每 2 秒切换档位")
        while True:
            os.exitpoint()
            for duty in DUTY_LEVELS:
                fan.duty_u16(duty)
                percent = round(duty / 65535 * 100)
                print(f"[INFO] 当前占空比: {percent}%")
                time.sleep(2)
    
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    
    finally:
        fan.duty_u16(0)
        fan.deinit()
        print("[INFO] 风扇已停止，PWM 资源已释放")

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


### 9.6 排针 PWM 输出 ​

如果你有示波器或逻辑分析仪，可以用下面的代码测试任意排针 PWM 输出。以 GPIO47/PWM3 为例，输出 2kHz、50% 占空比：

python
    
    
    from machine import PWM, FPIOA
    
    fpioa = FPIOA()
    fpioa.set_function(47, FPIOA.PWM3)
    
    pwm = PWM(3)
    pwm.freq(2000)
    pwm.duty_u16(32768)
    print(f"PWM3 输出: {pwm.freq()}Hz, duty_u16={pwm.duty_u16()}")

1  
2  
3  
4  
5  
6  
7  
8  
9  


示波器实测波形：

![示波器 2kHz 50% PWM](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/pwm/pwm_20241024_172344.png)

横坐标为时间，每一个格子为250微秒，也就是0.25ms，正好对应2kHz的频率。从图中可以看到，信号的高电平持续时间是250微秒，占整个周期的一半，也就是占空比为50%。建议大家在代码中调整PWM参数，然后去观察信号的变化。

其他排针 PWM 通道测试（替换 GPIO 和通道号即可）：

排针物理引脚| GPIO| PWM 通道| FPIOA 配置  
---|---|---|---  
12| GPIO47| PWM3| `set_function(47, FPIOA.PWM3)`  
26| GPIO61| PWM1| `set_function(61, FPIOA.PWM1)`  
32| GPIO46| PWM2| `set_function(46, FPIOA.PWM2)`  
33| GPIO52| PWM4| `set_function(52, FPIOA.PWM4)`  
35| GPIO42| PWM0| `set_function(42, FPIOA.PWM0)`  
  
### 9.7 统一版：舵机控制 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    舵机控制（统一版，50Hz PWM）
    - 自动适配 K230（GPIO47 -> PWM3）和 Lite K230D（GPIO59 -> PWM5）
    - 舵机在 10~170 度之间来回摆动
    - 注意：舵机需外部 5V 供电，与开发板共地
    """
    
    import time, os
    from machine import PWM, FPIOA
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "SERVO_PIN": 47, "SERVO_PWM_CH": 3}
        else:
            return {"board": "lushan_lite_k230d", "SERVO_PIN": 59, "SERVO_PWM_CH": 5}
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}, 舵机: GPIO{BOARD['SERVO_PIN']} -> PWM{BOARD['SERVO_PWM_CH']}")
    
    ANGLE_MIN = 10
    ANGLE_MAX = 170
    SWING_DELAY = 0.02
    
    fpioa = FPIOA()
    fpioa.set_function(BOARD["SERVO_PIN"], getattr(FPIOA, f"PWM{BOARD['SERVO_PWM_CH']}"))
    servo = PWM(BOARD["SERVO_PWM_CH"])
    servo.freq(50)
    
    def angle_to_duty(angle):
        pulse_ms = 0.5 + (angle / 180.0) * 2.0
        return int((pulse_ms / 20.0) * 65535)
    
    def move_servo(angle):
        servo.duty_u16(angle_to_duty(angle))
    
    print(f"[INFO] 舵机摆动启动 ({ANGLE_MIN}~{ANGLE_MAX} 度)")
    
    try:
        while True:
            os.exitpoint()
            for angle in range(ANGLE_MIN, ANGLE_MAX + 1):
                move_servo(angle)
                time.sleep(SWING_DELAY)
            for angle in range(ANGLE_MAX, ANGLE_MIN - 1, -1):
                move_servo(angle)
                time.sleep(SWING_DELAY)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        move_servo(90)
        time.sleep(0.5)
        servo.duty_u16(0)
        servo.deinit()
        print("[INFO] 舵机已归中，PWM 资源已释放")

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


### 9.8 舵机控制（分板代码） ​

立创·庐山派K230-CanMV开发板立创·庐山派Lite-K230D-CanMV开发板

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    import time
    from machine import PWM, FPIOA
    
    # 配置排针引脚号12，芯片引脚号为47的排针复用为PWM通道3输出
    pwm_io = FPIOA()
    pwm_io.set_function(47, FPIOA.PWM3)
    
    # 初始化PWM参数
    pwm = PWM(3)
    
    # 调整通道频率为50Hz
    pwm.freq(50)
    
    # ---- 用户可配置参数 ----
    ANGLE_RANGE = (10, 170)  # 舵机摆动角度范围(最小角度, 最大角度)
    # ----------------------
    
    # 计算角度对应的脉冲宽度(单位：毫秒)
    def angle_to_pulse(angle):
        return 0.5 + (angle / 180) * 2.0
    
    # 计算脉冲宽度对应的duty值
    def pulse_to_duty(pulse_ms):
        return int((pulse_ms / 20) * 65535)
    
    # 移动舵机到指定角度
    def move_servo(angle):
        pulse = angle_to_pulse(angle)
        duty = pulse_to_duty(pulse)
        pwm.duty_u16(duty)
    
    # 角度变动时的等待时间
    DELAY_PER_DEGREE = 0.02
    
    print("舵机摆动启动...")
    
    try:
        while True:
            # 从最小角度移动到最大角度
            for angle in range(ANGLE_RANGE[0], ANGLE_RANGE[1] + 1, 1):
                move_servo(angle)
                time.sleep(DELAY_PER_DEGREE)
    
            # 从最大角度移动到最小角度
            for angle in range(ANGLE_RANGE[1], ANGLE_RANGE[0] - 1, -1):
                move_servo(angle)
                time.sleep(DELAY_PER_DEGREE)
    
    except KeyboardInterrupt:
        print("程序终止")
        # 归中后关闭PWM
        move_servo(90)
        time.sleep(1)
        pwm.deinit()

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


python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    import time
    from machine import PWM, FPIOA
    
    # Lite K230D 使用 GPIO59 -> PWM5（3-4-5 组，不与蜂鸣器冲突）
    pwm_io = FPIOA()
    pwm_io.set_function(59, FPIOA.PWM5)
    
    # 初始化PWM参数
    pwm = PWM(5)
    
    # 调整通道频率为50Hz
    pwm.freq(50)
    
    # ---- 用户可配置参数 ----
    ANGLE_RANGE = (10, 170)  # 舵机摆动角度范围(最小角度, 最大角度)
    # ----------------------
    
    # 计算角度对应的脉冲宽度(单位：毫秒)
    def angle_to_pulse(angle):
        return 0.5 + (angle / 180) * 2.0
    
    # 计算脉冲宽度对应的duty值
    def pulse_to_duty(pulse_ms):
        return int((pulse_ms / 20) * 65535)
    
    # 移动舵机到指定角度
    def move_servo(angle):
        pulse = angle_to_pulse(angle)
        duty = pulse_to_duty(pulse)
        pwm.duty_u16(duty)
    
    # 角度变动时的等待时间
    DELAY_PER_DEGREE = 0.02
    
    print("舵机摆动启动...")
    
    try:
        while True:
            # 从最小角度移动到最大角度
            for angle in range(ANGLE_RANGE[0], ANGLE_RANGE[1] + 1, 1):
                move_servo(angle)
                time.sleep(DELAY_PER_DEGREE)
    
            # 从最大角度移动到最小角度
            for angle in range(ANGLE_RANGE[1], ANGLE_RANGE[0] - 1, -1):
                move_servo(angle)
                time.sleep(DELAY_PER_DEGREE)
    
    except KeyboardInterrupt:
        print("程序终止")
        # 归中后关闭PWM
        move_servo(90)
        time.sleep(1)
        pwm.deinit()

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


舵机接线实物图（立创·庐山派K230-CanMV开发板，GPIO47/PWM3）：

![舵机接线实物图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/pwm/pwm_20250723_150628.png)

舵机和开发板分别供电，让两者之间共地，将舵机 PWM 信号线接入排针物理引脚 12（GPIO47）。

## 十 什么是蜂鸣器 ​

知道了啥是PWM，以及如何控制PWM后，我们就可以驱动板载蜂鸣器来播放声音了。在此之前，先来了解一下蜂鸣器的基本知识。

大家可以先去立创商城看看蜂鸣器都有哪些，点这个[链接](https://list.szlcsc.com/catalog/386.html)。

![蜂鸣器种类](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/pwm/pwm_20241024_175111.png)

它是一种常见的声音输出设备，可以发出各种声音或音调。它们广泛应用于家用电器、电子设备、汽车、安全系统等领域。以下是蜂鸣器的主要种类及其使用场景。

蜂鸣器从构造类型上有电磁式和电压式两种：

**电磁式蜂鸣器** ：

它是利用电磁原理来产生声音，内部由线圈、电磁铁和振动膜片组成。电流通过线圈时，产生的磁场驱动振动膜片振动，从而发出声音。声音较大，但能耗高，容易受到磁场干扰，且对工作电压要求较为严格。适用于低频应用、需要较大音量的场合，适合工业环境或报警器等。

**压电式蜂鸣器** ：

它是基于压电效应原理，利用压电陶瓷材料在外加电场作用下发生形变，驱动振动膜片发出声音。其功耗低，体积小，工作电压范围很广。但低频性能较差，声音是比较小的。适用于对体积和功耗有要求的电子设备。

从驱动方式来说蜂鸣器主要分为两大类：有源蜂鸣器和无源蜂鸣器。

**有源蜂鸣器（Active Buzzer）** ：

有源蜂鸣器内部集成了一个振荡电路，当直接接入电源时就可以发出声音。由于内部已经集成了振荡电路，有源蜂鸣器的控制相对简单，只需要提供一个恒定的电源电压即可。但是，这种蜂鸣器的音调和音量调节较为有限。

**无源蜂鸣器（Passive Buzzer）** ：

与有源蜂鸣器不同，无源蜂鸣器没有内置振荡电路。要使无源蜂鸣器发声，需要提供一个外部的交流信号（PWM信号）。这种蜂鸣器的优点是可以通过调整外部信号的频率和占空比来实现更丰富的音调和音量控制。

我这里选用的无源蜂鸣器的C编号是[C95297](https://item.szlcsc.com/96499.html)，是一个贴片**无源电磁式蜂鸣器** ，他的驱动频率是4000Hz，也就是说我们的PWM频率在4KHz时声音是最响的。

## 十一 控制舵机基础知识 ​

舵机（Servo Motor）是一种特殊的电机，它可以精确地控制旋转角度。舵机广泛应用于模型船、航空模型、遥控车和其他需要精确角度控制的领域。

舵机的工作原理主要基于脉冲宽度调制（PWM）信号。PWM信号由高电平和低电平组成，其中高电平的时长称为脉冲宽度。通过调节脉冲宽度，可以控制舵机的旋转角度。也有通过串口通讯来控制的舵机，会比较贵。

舵机通常由电机、减速器、电子电路和输出轴组成。舵机的基础知识：

  1. **工作原理** ：舵机内部有一个电机驱动减速器转动，减速器的输出轴与电位器相连，电位器会随着输出轴的转动而发生变化，变化的电位器信号被反馈给驱动电路，电路会根据电位器的信号调整电机的转速，使输出轴停在设定的位置上。
  2. **控制信号** ：舵机的控制信号通常是一个 PWM 脉冲信号，信号的周期为 20ms（即频率为50Hz），脉宽为 0.5ms 到 2.5ms 之间，其中 0.5ms 表示输出角度为 0°，2.5ms 表示输出角度为 180°，1.5ms 表示输出角度为中间值 90°。
  3. **输出角度** ：舵机的输出角度通常为 0 到 180 度之间，不同型号的舵机有不同的输出角度范围。
  4. **扭矩** ：舵机的扭矩是指舵机输出轴能够承受的最大力矩，扭矩的大小具体要看所选舵机的参数。
  5. **电源电压** ：小功率舵机的电源电压一般为 4.8V 到 6V 之间，不同的舵机有不同的电源电压范围，我这次用的ES08A II舵机就是5V供电的。



IMPORTANT

舵机需要外部 5V 供电，不能直接从开发板 GPIO 取电！舵机的 GND 必须与开发板的 GND 连接（共地），否则 PWM 信号没有参考电位，舵机不会响应。

## 十二 常见问题 ​

### 12.1 蜂鸣器代码能运行，但蜂鸣器不响？ ​

**原因** ：GPIO 编号写错了。K230 是 GPIO43，Lite K230D 是 GPIO61。如果写反，程序不会报错但信号没输出到蜂鸣器。

**解决** ：使用本教程的统一代码（含 `get_board_info()`），不需要手动改 GPIO。

### 12.2 蜂鸣器和风扇/舵机一起用时互相影响？ ​

**原因** ：PWM 通道频率耦合。PWM0/1/2 共享频率，PWM3/4/5 共享频率。

**解决** ：蜂鸣器（PWM1，4kHz）和风扇（PWM0，20kHz）在同一组，不要同时运行。如需同时使用，把其中一个改到另一组通道。

### 12.3 舵机抖动或不响应？ ​

**原因** ：

  1. 舵机电源不足（直接从 GPIO 取电）
  2. 没有共地（PWM 信号没有参考电位）
  3. 频率不对（不是 50Hz）



**解决** ：

  1. 舵机单独 5V 供电
  2. 舵机 GND 与开发板 GND 连接
  3. 确认 `servo.freq(50)` 设置正确



### 12.4 运行代码后下次 PWM 不工作了？ ​

**原因** ：上次运行没有正常释放 PWM 资源（比如直接断开连接而不是点停止）。

**解决** ：在 IDE 中先点停止按钮，再重新运行。本教程所有例程都有 `finally: deinit()` 保护。

## 十三 总结 ​

本节我们完成了：

  * 理解 PWM 的基本概念（频率、占空比、通道分组）
  * 用 PWM 驱动板载无源蜂鸣器发声和播放旋律
  * 用 PWM 控制 Lite K230D 板载风扇的转速
  * 用 PWM 控制舵机角度（50Hz，0.5~2.5ms 脉宽）
  * 从排针输出 PWM 信号并用示波器验证
  * 了解了蜂鸣器的种类（有源/无源、电磁/压电）
  * 了解了舵机的工作原理和接线要求



后续可以尝试：

  * 用蜂鸣器播放更复杂的旋律（添加更多音符到 NOTES 表）
  * 结合按键实现按一下响一声的交互
  * 用 PWM 驱动 LED 实现呼吸灯效果
  * 结合 AI 推理结果控制舵机（如人脸跟踪云台）



