# 模数转换器【ADC】

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/adc.html>
> **最后更新**: 2026-07-17

---

## 一 本节介绍 ​

📝本节我们将学习如何在立创·庐山派K230-CanMV开发板和立创·庐山派Lite-K230D-CanMV开发板上使用 ADC（模数转换器）功能，通过 ADC 引脚读取外部电压值。

### 1.1 学习目标 ​

🏆学习目标

1️⃣ 理解 ADC（模数转换器）的基本概念：分辨率、采样率、参考电压。

2️⃣ 了解开发板上 ADC 的硬件接口位置和接线方式。

3️⃣ 学会使用 `machine.ADC` 模块读取模拟电压值，并在 IDE 终端打印输出。

4️⃣ 知道本章代码如何通过 `get_board_info()` 同时适配两种开发板，不需要手动改通道号。

### 1.2 重点提示 ​

⚠️注意！

  * ADC 输入电压**绝对不能超过 1.8V** ！超压会永久损坏芯片 ADC 模块,甚至会影响主控芯片本身，且不可修复。
  * 接入外部电压前，务必用万用表确认电压在 0~1.8V 范围内。
  * 不要将 3.3V 或 5V 电源直接接入 ADC 引脚。



## 二 软硬件准备 ​

名称| 数量| 说明  
---|---|---  
立创·庐山派K230-CanMV开发板 / 立创·庐山派Lite-K230D-CanMV开发板| 1| 二选一，教程默认同时适配  
Type-C 数据线| 1| 用于供电和连接 IDE  
可调电阻（电位器）| 1| 2K~10K 均可，用于产生可变电压  
FPC-6P 转 2.54 杜邦线转接板| 1| 连接开发板 ADC 座子  
FPC-6P 排线| 1| 连接开发板 FPC 座子到转接板  
万用表（推荐）| 1| 确认输入电压不超过 1.8V  
杜邦线| 若干| 连接电位器与 ADC 通道  
  
**固件要求** ：CanMV 最新版本（如代码报错请升级固件，参考快速入门文档）。

## 三 双板兼容说明 ​

项目| 立创·庐山派K230-CanMV开发板| 立创·庐山派Lite-K230D-CanMV开发板  
---|---|---  
是否支持本节实验| 支持| 支持  
芯片 ADC 通道总数| 6 通道（ADC0~ADC5）| 3 通道（ADC0~ADC2）  
FPC 座子引出通道| ADC0~ADC3（4 通道）| ADC0~ADC2（3 通道）  
ADC 分辨率| 12 位（0~4095）| 12 位（0~4095）  
最大输入电压| 1.8V| 1.8V  
采样率| 1 MHz| 1 MHz  
接口类型| FPC-6P 座子| FPC-6P 座子  
板卡适配方式| `get_board_info()` 自动识别| `get_board_info()` 自动识别  
  
IMPORTANT

两板 FPC-6P ADC 座子的物理线序一致，但引出的有效通道数不同：立创·庐山派K230-CanMV开发板引出 ADC0~ADC3（4 通道），立创·庐山派Lite-K230D-CanMV开发板只引出 ADC0~ADC2（3 通道）。本教程默认使用 **ADC 通道 0** 进行演示，两板通用。

## 四 名词解释 ​

名词| 说明  
---|---  
ADC| **Analog-to-Digital Converter** （模数转换器），将模拟电压信号转换为数字值  
分辨率| ADC 输出的数字位数，K230 为 12 位，即输出范围 0~4095  
采样率| 每秒采样次数，K230 为 1 MHz  
参考电压| ADC 量程上限，K230 的参考电压为 1.8V  
FPC| Flexible Printed Circuit（柔性印刷电路），开发板上用于引出 ADC 的排线接口  
  
INFO

  1. ADC 就像一把精确的【电压尺】——它把连续变化的模拟电压，量化成一个数字告诉你。比如 K230 的 12 位 ADC 把 0~1.8V 切成 4096 份，如果读到的值是 2048，说明当前电压大约是 0.9V（一半量程）。
  2. 你可以把 ADC 想象成一个【电压温度计】：输入端接到被测电压，读数从 0 到 4095 变化，对应 0V 到 1.8V。



## 五 什么是ADC？ ​

模数转换器（Analog-to-Digital Converter，简称 ADC）是一种将模拟信号（一般为连续的电压信号）转换为数字信号（离散的二进制数据）的外围设备。在嵌入式系统中，ADC 广泛应用于传感器数据采集、音频处理、工业控制等各种场景。

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/adc/adc_20260710_170704.png)

### 5.1 基本工作原理 ​

模拟信号是一个随时间连续变化的量（例如传感器的电压输出），而数字信号只在有限的离散时间点上取值。ADC 的作用就是定时对模拟电压进行采样，并将每个采样点的电压值经过量化和编码后输出一个数字值。

**一般步骤** ：

  1. **采样** ：在每个采样时刻，使用采样保持电路对输入模拟信号进行快速捕捉，获得瞬间电压值。
  2. **量化** ：将采样后的电压值映射到有限离散的电平。例如，K230 的 12 位 ADC 可以将 0~1.8V 分成 2^12 = 4096 个离散电平。
  3. **编码** ：将量化结果转换为对应的数字二进制码。例如，输入电压接近满量程的一半（0.9V）时，ADC 输出约为 2048。



### 5.2 ADC 的关键技术指标 ​

指标| K230/K230D| 说明  
---|---|---  
分辨率| 12 位| 输出 0~4095，位数越高精度越高  
采样速率| 1 MHz| 每秒采样 100 万次，足够绝大多数场景  
参考电压| 1.8V| 输入不可超过此值，由板载 LDO 提供  
通道数| K230 芯片 6 通道 / K230D 芯片 3 通道| FPC 座子：K230 引出 4 通道（ADC0~ADC3），Lite 引出 3 通道（ADC0~ADC2）  
  
⚠️注意！

参考电压由板载 LDO 提供，同时也给芯片 RTC 部分供电。**输入信号必须在 0~1.8V 范围内** ，超出可能导致 ADC 模块甚至芯片永久损坏。

## 六 开发板上的 ADC 接口 ​

K230 处理器内部集成了一个 ADC 硬件模块，提供 6 个独立通道（ADC0~ADC5）。在开发板上通过一个 **FPC-6P 座子** 引出部分通道供用户使用：立创·庐山派K230-CanMV开发板引出 ADC0~ADC3（4 通道），立创·庐山派Lite-K230D-CanMV开发板引出 ADC0~ADC2（3 通道）。

### 6.1 ADC 接口位置 ​

两款开发板的 ADC 原理图和 FPC-6P 座子实际位置如下。每组图片采用统一高度展示，移动端可以左右滑动表格；点击图片可以在当前页面放大查看，再次点击、点击背景或按 `Esc` 即可关闭。

**原理图对比：**

立创·庐山派K230-CanMV开发板 ADC 原理图| 立创·庐山派Lite-K230D-CanMV开发板 ADC 原理图  
---|---  
![立创·庐山派K230-CanMV开发板 ADC 原理图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/adc/adc_20241206_155227.png)| ![立创·庐山派Lite-K230D-CanMV开发板 ADC 原理图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/adc/adc_20260717_144056.png)  
  
**ADC 接口实际位置对比：**

立创·庐山派K230-CanMV开发板 FPC-6P 座子位置| 立创·庐山派Lite-K230D-CanMV开发板 FPC-6P 座子位置  
---|---  
![立创·庐山派K230-CanMV开发板 ADC FPC-6P 座子实际位置](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/adc/adc_20241206_155542.png)| ![立创·庐山派Lite-K230D-CanMV开发板 ADC FPC-6P 座子实际位置](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/adc/adc_20260717_144205.png)  
  
### 6.2 FPC-6P 座子线序 ​

引脚序号| 功能| 说明  
---|---|---  
1| 1.8V| 1.8V 参考电压输出（可作电位器供电）  
2| GND| 接地  
3| ADC0| 通道 0，0~1.8V  
4| ADC1| 通道 1，0~1.8V  
5| ADC2| 通道 2，0~1.8V  
6| ADC3| 通道 3，0~1.8V（仅 K230 板有效，Lite 无此通道）  
  
IMPORTANT

立创·庐山派K230-CanMV开发板的 FPC 座子引出了 **ADC0~ADC3** 共 4 个通道。立创·庐山派Lite-K230D-CanMV开发板的 K230D 芯片只有 3 个 ADC 通道（ADC0~ADC2），FPC 座子也只引出 **ADC0~ADC2** 共 3 个通道。

## 七 常用 API 说明 ​

ADC 类属于 `machine` 模块，使用前直接导入即可，**不需要** FPIOA 引脚配置（ADC 是专用引脚，不是 GPIO 复用）。

### 7.1 API 速查表 ​

API| 作用| 参数| 返回值  
---|---|---|---  
`ADC(channel)`| 创建 ADC 对象| channel: 0~5（K230）/ 0~2（Lite）| ADC 实例  
`read_u16()`| 读取采样原始值| 无| 0~4095（12位）  
`read_uv()`| 读取电压值（微伏）| 无| 0~1800000（即 0~1.8V）  
  
最小示例：

python
    
    
    from machine import ADC
    
    adc = ADC(0)              # 使用通道 0
    print(adc.read_u16())     # 打印原始采样值（0~4095）
    print(adc.read_uv())      # 打印电压值（微伏）

1  
2  
3  
4  
5  


### 7.2 用户可修改的参数 ​

参数| 位置| 可调范围| 说明  
---|---|---|---  
通道号| `ADC(0)`| 0~3（FPC引出）| K230 板芯片实际支持 0~5，Lite 只有 0~2  
采样间隔| `time.sleep_ms(100)`| 10~1000 ms| 越小采样越快，CPU 占用越高  
电压转换| `read_uv() / 1000000`| —| 微伏转伏特  
  
## 八 操作步骤 ​

### 8.1 硬件接线 ​

我们用一个可调电阻（电位器）来产生一个 0~1.8V 的可变电压，接入 ADC 通道 0 进行采样。

**接线方式：**

将电位器的三个引脚分别接入：

  * **引脚 1（一端）** → FPC 座子的 **1.8V** （第 1 脚）
  * **引脚 3（另一端）** → FPC 座子的 **GND** （第 2 脚）
  * **引脚 2（中间/滑动端）** → FPC 座子的 **ADC0** （第 3 脚）



这样旋转电位器，中间端输出的电压就会在 0~1.8V 之间变化。

![ADC 接线示意图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/adc/adc_20241206_165731.jpg)

注意

  * 电位器的两端接 **1.8V 和 GND** ，不是 3.3V 和 GND！用 3.3V 供电会导致中间端输出超过 1.8V，损坏 ADC。
  * 如果手头没有电位器，也可以用两个阻串联分压，只要确保分压后电压不超过 1.8V。
  * 建议接好线后先用万用表测量中间端电压，确认在安全范围内再接入开发板。



### 8.2 连接 IDE ​

  1. Type-C 数据线连接开发板和电脑。
  2. 打开 CanMV IDE K230，等待连接成功。
  3. 将下方代码复制到 IDE 中运行。



## 九 代码例程 ​

### 9.1 统一版：持续读取 ADC 电压 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    ADC 持续读取电压（统一版）
    - 自动适配 K230 和 Lite K230D
    - 读取 ADC 通道 0 的采样值和电压值
    - 每 100ms 采样一次，在 IDE 终端持续打印
    """
    
    import time, os
    from machine import ADC
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "ADC_CH_MAX": 5}
        else:
            return {"board": "lushan_lite_k230d", "ADC_CH_MAX": 2}
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}, ADC 通道范围: 0~{BOARD['ADC_CH_MAX']}")
    
    # 实例化 ADC 通道 0
    adc = ADC(0)
    
    try:
        while True:
            os.exitpoint()
    
            # 获取原始采样值（0~4095）
            adc_value = adc.read_u16()
    
            # 获取电压值（微伏）
            adc_voltage_uv = adc.read_uv()
    
            # 转换为伏特
            adc_voltage_v = adc_voltage_uv / 1000000
    
            print("ADC Value: %d, Voltage: %d uV, %.4f V" % (adc_value, adc_voltage_uv, adc_voltage_v))
    
            time.sleep_ms(100)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")

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


### 9.2 持续读取 ADC 电压（分板代码） ​

立创·庐山派K230-CanMV开发板立创·庐山派Lite-K230D-CanMV开发板

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time, os
    from machine import ADC
    
    # K230 支持 ADC(0)~ADC(5)，FPC 座子引出 ADC0~ADC3
    adc = ADC(0)
    
    try:
        while True:
            os.exitpoint()
            adc_value = adc.read_u16()
            adc_voltage_uv = adc.read_uv()
            adc_voltage_v = adc_voltage_uv / 1000000
            print("ADC Value: %d, Voltage: %d uV, %.4f V" % (adc_value, adc_voltage_uv, adc_voltage_v))
            time.sleep_ms(100)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")

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


python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time, os
    from machine import ADC
    
    # Lite K230D 只有 ADC(0)~ADC(2)，FPC 座子引出 ADC0~ADC2
    adc = ADC(0)
    
    try:
        while True:
            os.exitpoint()
            adc_value = adc.read_u16()
            adc_voltage_uv = adc.read_uv()
            adc_voltage_v = adc_voltage_uv / 1000000
            print("ADC Value: %d, Voltage: %d uV, %.4f V" % (adc_value, adc_voltage_uv, adc_voltage_v))
            time.sleep_ms(100)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")

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


### 9.3 统一版：多通道扫描 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    ADC 多通道扫描（统一版）
    - 自动适配 K230（扫描 ADC0~ADC3）和 Lite K230D（扫描 ADC0~ADC2）
    - 每秒打印一次所有可用通道的电压值
    """
    
    import time, os
    from machine import ADC
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "ADC_CH_MAX": 3}  # FPC 引出 0~3
        else:
            return {"board": "lushan_lite_k230d", "ADC_CH_MAX": 2}  # 芯片只有 0~2
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}, 扫描通道: ADC0~ADC{BOARD['ADC_CH_MAX']}")
    
    # 创建所有可用通道的 ADC 对象
    channels = []
    for ch in range(BOARD["ADC_CH_MAX"] + 1):
        channels.append(ADC(ch))
    
    try:
        while True:
            os.exitpoint()
            print("--- ADC 扫描 ---")
            for i, adc in enumerate(channels):
                voltage_v = adc.read_uv() / 1000000
                print(f"  CH{i}: {adc.read_u16():4d} ({voltage_v:.4f} V)")
            print("")
            time.sleep(1)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")

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


## 十 代码说明 ​

### 10.1 单通道读取解析 ​

代码核心逻辑非常简单：

  1. **导入 ADC** ：`from machine import ADC`，不需要 FPIOA 配置（ADC 是专用引脚，不走 GPIO 复用）。
  2. **实例化通道** ：`adc = ADC(0)` 创建通道 0 的对象。
  3. **读取数据** ：循环中交替调用 `read_u16()`（原始值）和 `read_uv()`（微伏值），两种形式二选一都可以，本教程两种都演示了。
  4. **单位转换** ：`read_uv()` 返回微伏（0~1800000），除以 1000000 就是伏特。



**为什么用`os.exitpoint()`？**

和前面 UART/PWM 章节一样，这是 CanMV IDE 的"停止检查点"。循环中加上它，点击 IDE 的停止按钮就能立即中断脚本，不然可能要等很久或强制断电。

**能改哪里？**

  * 把 `ADC(0)` 的 `0` 改成 `1`/`2`/`3`（K230 还可以用 `4`/`5`），对应不同通道
  * 把 `time.sleep_ms(100)` 改小，采样更快（最快可以去掉延时，靠 ADC 本身 1MHz 速率）
  * 如果只关心电压不关心原始值，直接用 `read_uv()` 即可



### 10.2 多通道扫描解析 ​

多通道扫描的关键设计：

  1. **通过`get_board_info()` 获取可用通道上限**：K230 的 FPC 引出 ADC0~ADC3，Lite 的 FPC 引出 ADC0~ADC2。
  2. **用循环创建 ADC 对象列表** ：`for ch in range(max+1)` 自动适配通道数，不需要硬编码。
  3. **遍历打印** ：每秒扫描一次所有通道，打印原始值和对应电压。



这个例程适合用来快速检测哪个通道接了东西、哪个通道是空悬的（空悬通道读数会有随机波动）。

## 十一 实际运行效果 ​

运行 持续读取 ADC 电压 例程，旋转电位器到大约 0.65V 的位置，CanMV IDE 终端会持续打印类似如下内容：
    
    
    [INFO] 板卡: lushan_k230, ADC 通道范围: 0~5
    ADC Value: 1479, Voltage: 650127 uV, 0.6501 V
    ADC Value: 1480, Voltage: 650565 uV, 0.6506 V
    ADC Value: 1478, Voltage: 649689 uV, 0.6497 V
    ADC Value: 1481, Voltage: 651003 uV, 0.6510 V
    ...

1  
2  
3  
4  
5  
6  


旋转电位器，数值会实时跟随变化：

  * 旋到最小（GND 端）：接近 `ADC Value: 0, Voltage: 0 uV, 0.0000 V`
  * 旋到最大（1.8V 端）：接近 `ADC Value: 4095, Voltage: 1800000 uV, 1.8000 V`



## 十二 常见问题 ​

Q1：读出来的电压值一直是 0 或始终不变？

**可能原因** ：

  1. 电位器没有正确接线（中间端没接到 ADC 通道）。
  2. FPC 线没插好，或者 FPC 转接板方向反了。
  3. 使用了超出板卡范围的 ADC 通道号（Lite K230D 只支持 ADC0~ADC2）。



**解决办法** ：

  1. 用万用表直接测量 FPC 转接板上 ADC0 引脚的电压，确认有电压变化。
  2. 检查 FPC 线插入方向（金手指朝向一致）。
  3. Lite K230D 用户请使用 ADC(0)、ADC(1) 或 ADC(2)，FPC 座子上只引出了这 3 个通道。

Q2：读数波动很大、不稳定？

**可能原因** ：

  1. ADC 引脚悬空（没接任何东西），悬空引脚会拾取环境噪声。
  2. 电位器接触不良，或杜邦线接触氧化。
  3. 采样间隔太短，没有给 ADC 足够的建立时间。



**解决办法** ：

  1. 确认 ADC 引脚确实连接了电压源（不要悬空测试）。
  2. 更换杜邦线或重新焊接电位器引脚。
  3. 可以在代码中多次采样取平均值来滤波：



python
    
    
    samples = [adc.read_u16() for _ in range(10)]
    avg = sum(samples) // len(samples)

1  
2  


Q3：超过 1.8V 会怎样？ADC 坏了能修吗？

超过 1.8V 的输入电压会击穿 ADC 模块的输入保护结构，**永久损坏** 该通道甚至整个 ADC 模块。损坏后无法维修，只能更换芯片（开发板级别相当于报废）。

**预防措施** ：

  1. 使用 FPC 座子上引出的 1.8V 给电位器供电（而不是 3.3V 或 5V）。
  2. 接入任何外部传感器前，先用万用表确认其输出电压范围在 0~1.8V 内。
  3. 如果传感器输出超过 1.8V，必须在外部做分压（电阻分压）或稳压（LDO）处理后再接入。

Q4：Lite K230D 为什么只有 3 个 ADC 通道？

这是 K230D 芯片本身的设计——K230D 相比 K230 裁剪了部分外设资源以降低成本和封装面积（SiP 集成方案），ADC 通道从 6 个减为 3 个（ADC0~ADC2）。因此立创·庐山派Lite-K230D-CanMV开发板的 FPC 座子也只引出了 ADC0~ADC2 共 3 个通道，比 K230 板少一个 ADC3。

## 十三 总结 ​

本节我们学习了 ADC 模数转换的基础知识和实际使用方法：

  1. 理解了 ADC 的工作原理——把连续的模拟电压"量化"成离散的数字值。
  2. 掌握了开发板 FPC-6P 座子的接线方式和通道分配。
  3. 学会了通过 `machine.ADC` 模块读取电压，两种形式（原始值 / 微伏值）按需选用。
  4. 了解了两板的差异：K230 芯片 6 通道、Lite K230D 芯片 3 通道，FPC 座子 K230 引出 ADC0~ADC3、Lite 引出 ADC0~ADC2。



**后续可以尝试的扩展方向：**

  * 接入光敏电阻，做一个简易光照强度检测器
  * 接入热敏电阻（NTC），做一个温度采集器
  * 接入电位器摇杆模块，做一个双轴模拟摇杆输入设备
  * 接入土壤湿度传感器（注意分压到 1.8V 以内）
  * 多通道同时采集，做简易数据记录器



## 十四 参考资料 ​

  1. [ADC 模块 API 手册 — CanMV K230](https://developer.canaan-creative.com/k230_canmv/zh/main/zh/api/machine/K230_CanMV_ADC%E6%A8%A1%E5%9D%97API%E6%89%8B%E5%86%8C.html)
  2. [MicroPython machine.ADC 官方文档](https://docs.micropython.org/en/latest/library/machine.ADC.html)
  3. [模数转换器 - 维基百科](https://zh.wikipedia.org/wiki/%E9%A1%9E%E6%AF%94%E6%95%B8%E4%BD%8D%E8%BD%89%E6%8F%9B%E5%99%A8)



