# 串口通讯【UART】

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/uart.html>
> **最后更新**: 2026-07-09

---

## 一 本节介绍 ​

📝本节我们将学习如何在立创·庐山派K230-CanMV开发板和立创·庐山派Lite-K230D-CanMV开发板上使用 UART（串口通讯）功能。通过 UART 我们可以与外部设备进行数据收发，实现板间通信、传感器数据读取、调试信息输出等功能。

### 1.1 学习目标 ​

🏆学习目标

1️⃣ 理解串口通讯的基本概念（波特率、数据位、停止位、校验位），知道 UART 的工作原理。

2️⃣ 学会将 K230 的 GPIO 引脚配置为 **UART 模式** ，与外部设备进行数据收发。

3️⃣ 学会使用串口进行基本的数据发送和接收操作。

4️⃣ 掌握串口回环测试方法，能独立验证 UART 硬件连接是否正常。

5️⃣ 知道本章代码如何通过 `get_board_info()` 同时适配两种开发板，不需要手动改引脚。

### 1.2 重点提示 ​

⚠️注意！

  * 串口0（GH1.25座子丝印标号0）被系统（RT-Smart）占用为 `finsh` 控制台，用户**无法** 在 CanMV 固件中调用。
  * 串口通讯时 TX 接对方的 RX，RX 接对方的 TX，千万不要 TX 接 TX。
  * 两个设备之间必须**共地** （GND 互连），否则无法正常通讯。
  * 本教程默认使用开发板上的 **串口2 GH1.25-4P 带锁座** （GPIO11 TX, GPIO12 RX）进行演示。



## 二 软硬件准备 ​

名称| 数量| 说明  
---|---|---  
立创·庐山派K230-CanMV开发板 / 立创·庐山派Lite-K230D-CanMV开发板| 1| 二选一，教程默认同时适配  
Type-C 数据线| 1| 用于供电和连接 IDE  
USB 转 TTL 模块| 1| 用于连接电脑串口助手，务必使用串口IO电平为3.3V的工具，请提前询问你的串口工具购买商家，否则不要乱接。  
GH1.25-4P 转杜邦线| 1| 连接开发板串口座子与 USB 转 TTL 模块  
杜邦线（公对公）| 若干| 回环测试时短接 TX/RX 用  
  
**固件要求** ：CanMV 最新版本（如代码报错请升级固件，参考快速入门文档）。

## 三 双板兼容说明 ​

项目| 立创·庐山派K230-CanMV开发板| 立创·庐山派Lite-K230D-CanMV开发板  
---|---|---  
是否支持本节实验| 支持| 支持  
推荐串口（GH1.25座）| 串口2（GPIO11 TX, GPIO12 RX）| 串口2（GPIO11 TX, GPIO12 RX）  
串口3（GH1.25座）| **GPIO50 TX, GPIO51 RX**| **GPIO32 TX, GPIO33 RX**  
串口0（系统占用）| GPIO38 TX, GPIO39 RX| GPIO38 TX, GPIO39 RX  
40Pin 排针可用串口| 串口1/2/3/4| 根据排针图确认  
板卡适配方式| `get_board_info()` 自动识别| `get_board_info()` 自动识别  
  
立创·庐山派K230-CanMV开发板： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20260709_120508.png) 立创·庐山派Lite-K230D-CanMV开发板： ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20260709_120528.png)

IMPORTANT

本教程默认使用**串口2** 演示，两板串口2引脚完全一致（GPIO11/GPIO12），代码无需做引脚分支。

⚠️ **但串口3两块板子的引脚不一样** ：K230 是 GPIO50(TX)/GPIO51(RX)，Lite K230D 是 GPIO32(TX)/GPIO33(RX)。如果你要改用串口3，务必按自己的板子选对引脚。教程中仍保留 `get_board_info()` 用于识别板卡并适配串口3引脚。

## 四 名词解释 ​

名词| 说明  
---|---  
UART| **Universal Asynchronous Receiver/Transmitter** （通用异步收发传输器），一种串行通信协议  
波特率| Baud Rate，每秒传送的比特（bit）个数，常用 115200、9600 等  
TXD| Transmit Data，数据发送端  
RXD| Receive Data，数据接收端  
全双工| 通信双方可以同时发送和接收数据（TX 和 RX 各一条线）  
GND| Ground，接地点，两设备通讯时必须共地  
FPIOA| Field Programmable Input and Output Array（现场可编程IO阵列），引脚功能复用配置  
  
INFO

  1. UART 是嵌入式系统中最基础、最常用的通讯方式之一。你可以把它想象成两个人打电话——一个人说话（TX），另一个人听（RX），双方约定好 语速 （也就是波特率波特率）就能正常沟通。
  2. FPIOA 可以理解为一个 可重新接线的引脚分配面板 ，我们需要先通过它把 GPIO 配置为 UART 功能，芯片才知道这个引脚要用来做串口通讯。



## 五 什么是串口？ ​

UART（Universal Asynchronous Receiver-Transmitter，通用异步收发传输器）是嵌入式系统中最常用的串行通信协议之一。从单片机到传感器模块，从 GPS 到蓝牙模块，几乎所有需要低速数据交换的场景都能看到 UART 的身影。甚至以前的电脑都自带串口（老式的 `DB9` 接口），不过随着科技发展在个人 PC 上被 USB 等更现代化的接口替代了。

它是一种**异步** 串行通信协议——发送端和接收端不需要额外的时钟线来同步，只要双方约定好相同的波特率，就能正确解析数据。UART 主要通过以下两条线来进行数据传输：

  * **TXD（Transmit Data）** ：数据发送端，将数据从发送设备发送到接收设备。
  * **RXD（Receive Data）** ：数据接收端，用于接收从发送设备发来的数据。



通信时，UART 会将并行数据（如 8 位数据）通过移位寄存器转换成串行格式，逐位发送；接收端则将收到的比特（bit）数据组装为字节，供系统进一步处理。

### 5.1 串口通讯参数 ​

  * **波特率** ：衡量通信速度的参数，表示每秒钟传送的 bit 个数。常用的波特率有 115200（本文默认使用）、9600 等。两个设备的波特率必须一致才能正常通讯。
  * **数据位** ：一个数据帧中实际数据的位数，常用 8bit。
  * **停止位** ：用于标识一帧数据的结束，典型值为 1 位。停止位给接收端提供了校正时钟同步的机会。
  * **校验位** ：用于简单的错误检测，常见方式包括奇校验（Odd）、偶校验（Even）和无校验（None）。普通通讯中一般不启用。
  * **起始位** ：通常是 1 位逻辑低电平（0），标志着数据帧的开始。



### 5.2 串口工作模式 ​

串口可以工作在单工、半双工和全双工模式下：

模式| 数据线数| 说明  
---|---|---  
单工| 1 根| 信息只能由 A 传到 B，单向  
半双工| 1 根| 信息可双向传输，但同一时刻只能一个方向  
全双工| 2 根| 双方可同时收发数据（UART 默认工作模式）  
  
### 5.3 通讯波形 ​

![UART通讯波形示意图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20241025_150330.svg)

这是一个典型的 UART 数据帧结构示意图，展示了**起始位、数据位、校验位** 和**停止位** 的时序关系：

  1. **空闲状态** ：数据线保持高电平。
  2. **起始位（Start）** ：发出一个低电平，通知接收端"数据要来了"。
  3. **数据位（D0-D7）** ：紧接着发送 8 位数据（如果配置为 8bit），每个位的持续时间是波特率的倒数。例如波特率 115200bps 时，每个位持续约 8.68μs。
  4. **校验位（P）** ：可选，带 `*` 号表示可以不启用。
  5. **停止位（Stop）** ：恢复高电平，标志一帧结束，数据线重新变为空闲状态。



简单总结通讯流程：

  * **发送端** ：拉低 TX → 逐位发送数据 → 可选校验位 → 停止位恢复高电平。
  * **接收端** ：检测到起始位下降沿 → 按波特率采样每一位 → 校验（如有）→ 停止位确认一帧接收完成。



## 六 开发板上可用的串口 ​

K230 内部集成了五个 UART 硬件模块（UART0~UART4）。其中串口0被系统（RT-Smart）占用，剩余的串口1、2、3、4 均可被用户正常调用。

### 6.1 40Pin 排针引出的串口 ​

两块开发板的排针串口引脚绝大多数一致，但仍有个别差异（例如 pin22 在两板上的 UART 功能不同，详见表格下方）。下面按板卡分别给出排针示意图和引脚表，请对照自己手上的板子查看，不要混用另一块板的表。

【立创·庐山派K230-CanMV开发板 排针脚示意图】

![K230 40Pin排针引脚图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/gpio-fpioa/gpio-fpioa_20241024_114820.png)

【立创·庐山派Lite-K230D-CanMV开发板 排针脚示意图】

![Lite K230D 40Pin排针引脚图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20260709_142910.png)

两块板子排针处都能用到串口1、2、3、4，下面分板列出各自可用的 TXD/RXD 收发脚（流控脚 RTS/CTS/DE/RE 基础通讯用不到，这里不列）。两板绝大多数收发脚一致，主要差异见表格下方提示。

【立创·庐山派K230-CanMV开发板 40Pin 可用串口】

排针引脚号| 芯片引脚号| 串口功能号| 备注  
---|---|---|---  
03| GPIO 49| UART4_RXD| 同时连入摄像头0（CSI0）IIC，板内有4.7K上拉至3.3V  
05| GPIO 48| UART4_TXD| 同时连入摄像头0（CSI0）IIC，板内有4.7K上拉至3.3V  
08| GPIO 03| UART1_TXD| —  
10| GPIO 04| UART1_RXD| —  
11| GPIO 05| UART2_TXD| —  
13| GPIO 06| UART2_RXD| —  
27| GPIO 41| UART1_RXD| 同时连入摄像头1（CSI1）IIC，板内有4.7K上拉至3.3V  
28| GPIO 40| UART1_TXD| 同时连入摄像头1（CSI1）IIC，板内有4.7K上拉至3.3V  
29| GPIO 36| UART4_TXD| 复用音频 IIS_D_IN1 / PDM_IN2，用前确认音频未占用  
31| GPIO 37| UART4_RXD| 复用音频 IIS_D_OUT1 / PDM_IN0，用前确认音频未占用  
37| GPIO 32| UART3_TXD| 见下方 GH1.25 说明  
40| GPIO 33| UART3_RXD| 见下方 GH1.25 说明  
  
【立创·庐山派Lite-K230D-CanMV开发板 40Pin 可用串口】

排针引脚号| 芯片引脚号| 串口功能号| 备注  
---|---|---|---  
03| GPIO 49| UART4_RXD| 同时连入摄像头0（CSI0）IIC，板内有4.7K上拉至3.3V  
05| GPIO 48| UART4_TXD| 同时连入摄像头0（CSI0）IIC，板内有4.7K上拉至3.3V  
08| GPIO 03| UART1_TXD| —  
10| GPIO 04| UART1_RXD| —  
11| GPIO 05| UART2_TXD| —  
13| GPIO 06| UART2_RXD| —  
22| GPIO 10| UART1_RXD| ⚠️复用板载 HT6872 功放使能，放音时被占用，慎用  
27| GPIO 41| UART1_RXD| 同时连入摄像头1（CSI1）IIC，板内有4.7K上拉至3.3V  
28| GPIO 40| UART1_TXD| 同时连入摄像头1（CSI1）IIC，板内有4.7K上拉至3.3V  
29| GPIO 36| UART4_TXD| 复用音频 IIS_D_IN1 / PDM_IN2，用前确认音频未占用  
31| GPIO 37| UART4_RXD| 复用音频 IIS_D_OUT1 / PDM_IN0，用前确认音频未占用  
37| GPIO 32| UART3_TXD| 与 GH1.25 座子并联，见下方说明  
40| GPIO 33| UART3_RXD| 与 GH1.25 座子并联，见下方说明  
  
IMPORTANT

排针编号和 GPIO 编号不是一个概念，接线时要看开发板丝印或原理图，不要混淆。两块板子的 UART 收发脚基本一致，主要差异是 **pin22** ：立创·庐山派Lite-K230D-CanMV开发板上是 GPIO10（可作 UART1_RXD，但复用了板载功放使能），立创·庐山派K230-CanMV开发板该位没有 UART 功能。

IMPORTANT

立创·庐山派Lite-K230D-CanMV开发板的部分排针 IO 与板载外设存在复用关系（摄像头 I2C、HDMI、音频 IIS/PDM、功放、风扇等）。连接外部模块前，请先确认该引脚没有被这些功能占用。

### 6.2 GH1.25-4P 带锁座串口 ​

![立创·庐山派K230-CanMV开发板 GH1.25带锁座串口位置](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20241231_153146.png)立创·庐山派K230-CanMV开发板![立创·庐山派Lite-K230D-CanMV开发板 GH1.25带锁座串口位置](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20260709_144029.png)立创·庐山派Lite-K230D-CanMV开发板

> 1️⃣是串口2（亦可复用为IIC，最**重要** 的外接通讯接口），2️⃣是串口3（当前 CanMV 固件中用户可使用），3️⃣是串口0（被RT-Smart系统占用）

开发板上提供了三个 **GH1.25-4P带锁接口** ，具有良好的物理连接稳定性。拔出时必须下压上方卡扣才能顺利拔出，千万不要大力出奇迹，严重时会导致线材断裂甚至接口脱落。

这三个座子的线序统一为：

序号| 简称| 功能  
---|---|---  
1| V| 5V 输入输出口  
2| R| RXD，串口信号接收线  
3| T| TXD，串口信号发送线  
4| G| GND，接地点  
  
IMPORTANT

**串口2 和串口3 各自有 "两套物理引脚"，用户二选一即可，不要同时接两处。**

  * **串口2** ：既可以走 40Pin 排针（pin11=GPIO5 TX，pin13=GPIO6 RX），也可以走 GH1.25 座子（GPIO11 TX，GPIO12 RX）。这是两组不同的物理引脚，但对应同一个 UART2 外设。本教程默认使用 GH1.25 座子（GPIO11/GPIO12）。
  * **串口3** ：同理，排针和 GH1.25 座子各有引脚，且**两块开发板的 GH1.25 串口3 引脚还不一样** （详见下方）。



**1️⃣ 串口2（丝印标号2）—— 本教程推荐使用**

位于两个侧按按钮中间，既可复用为串口2，也可复用为 IIC2（此时 SCL=GPIO11、SDA=GPIO12，通过 FPIOA 切换功能，IIC 章节会详细介绍）。

丝印简称| 芯片引脚号| 串口功能号| 备注  
---|---|---|---  
V| —| —| 5V 输入输出口  
R| GPIO 12| UART2_RXD| 复用为 IIC2 时是 IIC2_SDA  
T| GPIO 11| UART2_TXD| 复用为 IIC2 时是 IIC2_SCL  
G| —| —| GND，接地点  
  
**2️⃣ 串口3（丝印标号3）**

位于12V电源输入（GH1.25-2P）旁边。

⚠️两块板子的串口3引脚不一样！

串口3 是本节唯一存在双板差异的串口，接线和写代码时务必分清自己用的是哪块板子。

【立创·庐山派K230-CanMV开发板】

丝印简称| 芯片引脚号| 串口功能号| 备注  
---|---|---|---  
V| —| —| 5V 输入输出口  
R| GPIO 51| UART3_RXD| —  
T| GPIO 50| UART3_TXD| —  
G| —| —| GND，接地点  
  
【立创·庐山派Lite-K230D-CanMV开发板】

丝印简称| 芯片引脚号| 串口功能号| 备注  
---|---|---|---  
V| —| —| 5V 输入输出口  
R| GPIO 33| UART3_RXD| 与 40Pin 排针 pin40 并联引出  
T| GPIO 32| UART3_TXD| 与 40Pin 排针 pin37 并联引出  
G| —| —| GND，接地点  
  
IMPORTANT

Lite K230D 的 GH1.25 串口3（GPIO32/33）与 40Pin 排针的 pin37/pin40 是**同一对引脚并联引出** 的，本质是同一个 UART3。请只选其中一处接线，不要在座子和排针上同时接串口3设备。

【立创·庐山派K230-CanMV开发板 的串口3 补充说明】

K230 上情况不同：GH1.25 座子的串口3 走 **GPIO50/GPIO51** ，而 40Pin 排针 pin37/pin40 走的是 **GPIO32/GPIO33** ，二者是**两套不同的物理引脚** 。用户根据接线位置选择对应引脚即可。

**3️⃣ 串口0（丝印标号0）—— 系统占用，用户无法调用**

位于 USB HOST 座子旁边。

丝印简称| 芯片引脚号| 串口功能号| 备注  
---|---|---|---  
V| —| —| 5V 输入输出口  
R| GPIO 39| UART0_RXD| —  
T| GPIO 38| UART0_TXD| —  
G| —| —| GND，接地点  
  
⚠️注意！

串口0（第三个座子）无法在 CanMV 固件中被用户调用，它被系统（RT-Smart）占用为 `finsh` 控制台。如果想查看其输出内容，可以使用 USB 转串口工具连接到电脑的串口终端软件（如 MobaXterm），串口参数为 `115200@8N1`。

### 6.3 背面大触点（可焊排针） ​

![立创·庐山派K230-CanMV开发板 背面大触点串口位置](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20241231_153606.png)立创·庐山派K230-CanMV开发板![立创·庐山派Lite-K230D-CanMV开发板 背面大触点串口位置](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20260709_145651.png)立创·庐山派Lite-K230D-CanMV开发板

> 1️⃣是串口2（亦可复用为IIC），2️⃣是串口3，3️⃣是串口0（系统占用）

这些大触点与上述 GH1.25 座子电气连接完全一致。焊盘间距为 2.54mm，可以直接焊接排针使用杜邦线，也可以用 Pogo Pin 做扩展板连接。

## 七 常用 API 说明 ​

要使用串口功能，首先需要通过 FPIOA 将 GPIO 配置为 UART 模式，然后使用 `machine.UART` 模块进行数据收发。

### 7.1 FPIOA 引脚配置 ​

使用 UART 前必须先通过 FPIOA 把 GPIO 复用为 UART 功能：

python
    
    
    from machine import UART, FPIOA
    
    fpioa = FPIOA()
    # 将 GPIO11 配置为 UART2 发送端
    fpioa.set_function(11, FPIOA.UART2_TXD)
    # 将 GPIO12 配置为 UART2 接收端
    fpioa.set_function(12, FPIOA.UART2_RXD)

1  
2  
3  
4  
5  
6  
7  


### 7.2 UART API 速查表 ​

API| 作用| 常用参数| 说明  
---|---|---|---  
`UART(id, baudrate, bits, parity, stop)`| 创建并初始化 UART 对象| id: UART1~UART4| 先用 FPIOA 配置好引脚  
`init(baudrate, bits, parity, stop)`| 重新配置 UART 参数| 同构造函数| 运行中修改参数  
`read([nbytes])`| 读取数据| nbytes: 可选，最多读取字节数| 无数据时返回 None  
`readline()`| 读取一行（以换行符结束）| —| 适用于文本协议  
`readinto(buf[, nbytes])`| 将数据读入缓冲区| buf: bytearray| 高效读取  
`write(buf)`| 发送数据| buf: 字符串或字节对象| 返回写入字节数  
`deinit()`| 释放 UART 资源| —| 程序结束时必须调用  
  
**构造函数参数详解：**

参数| 可选值| 默认值| 说明  
---|---|---|---  
`id`| `UART.UART1` ~ `UART.UART4`| —| UART 模块编号  
`baudrate`| 任意整数| 115200| 波特率  
`bits`| `UART.FIVEBITS` / `SIXBITS` / `SEVENBITS` / `EIGHTBITS`| `EIGHTBITS`| 数据位数  
`parity`| `UART.PARITY_NONE` / `PARITY_ODD` / `PARITY_EVEN`| `PARITY_NONE`| 校验方式  
`stop`| `UART.STOPBITS_ONE` / `STOPBITS_TWO`| `STOPBITS_ONE`| 停止位数  
  
最小示例：

python
    
    
    from machine import UART, FPIOA
    
    fpioa = FPIOA()
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    uart.write("Hello!\n")  # 发送数据
    # uart.deinit()         # 释放资源

1  
2  
3  
4  
5  
6  
7  
8  
9  


### 7.3 用户可修改的参数 ​

参数| 位置| 可调范围| 说明  
---|---|---|---  
波特率| `baudrate=115200`| 9600~1500000| 两端必须一致  
串口号| `UART.UART2`| UART1~UART4| 同时修改 FPIOA 配置  
发送内容| `uart.write(...)`| 字符串/字节| 根据实际需求修改  
读取超时| 循环中 `time.sleep()`| 0.01~1.0 s| 越小响应越快，CPU占用越高  
  
## 八 操作步骤 ​

### 8.1 硬件接线（串口收发测试） ​

我们使用开发板的 **串口2 GH1.25-4P 带锁座** 连接 USB 转 TTL 模块到电脑：

开发板串口2座子| USB 转 TTL 模块| 说明  
---|---|---  
T（GPIO11 TXD）| RXD| 开发板发送 → 模块接收  
R（GPIO12 RXD）| TXD| 模块发送 → 开发板接收  
G（GND）| GND| 必须共地  
V（5V）| 不接| USB 转 TTL 模块自带供电，不需要额外接 5V  
  
注意

  * TX 接对方 RX，RX 接对方 TX，交叉连接！
  * 两设备必须共地（GND 互连），否则通讯异常。
  * V（5V）口不要随意接入外部电源，除非你明确知道自己在做什么。



### 8.2 电脑端串口助手配置 ​

  1. 将 USB 转 TTL 模块插入电脑 USB 口。
  2. 打开串口助手软件（推荐：MobaXterm、PuTTY、SSCOM 等）。
  3. 选择对应的 COM 口，设置参数为 **115200, 8N1** （115200波特率，8位数据位，无校验，1位停止位）。
  4. 点击"打开串口"。



### 8.3 硬件接线（回环测试） ​

回环测试不需要 USB 转 TTL 模块，只需要一根杜邦线：

  * 使用 GH1.25 转杜邦线，将串口2座子的 **T（TXD）** 和 **R（RXD）** 短接在一起即可。
  * 这样发送的数据会立即被自己接收回来，用于验证 UART 硬件和软件配置是否正确。



注意

回环测试时确保没有其他设备连接到 UART 引脚上，以避免信号冲突。

## 九 代码例程 ​

本节所有代码均通过 `get_board_info()` 自动识别板卡型号，复制即可运行，无需手动修改。

### 9.1 统一版：串口发送数据 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    串口发送数据（统一版）
    - 自动适配 K230 和 Lite K230D
    - 使用串口2（GPIO11 TX, GPIO12 RX），115200@8N1
    - 每隔 500ms 发送一次递增计数
    """
    
    import time, os
    from machine import UART, FPIOA
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "UART_TX": 11, "UART_RX": 12, "UART_ID": 2}
        else:
            return {"board": "lushan_lite_k230d", "UART_TX": 11, "UART_RX": 12, "UART_ID": 2}
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}, 串口2: GPIO{BOARD['UART_TX']}(TX), GPIO{BOARD['UART_RX']}(RX)")
    
    # 配置引脚为 UART 功能
    fpioa = FPIOA()
    fpioa.set_function(BOARD["UART_TX"], FPIOA.UART2_TXD)
    fpioa.set_function(BOARD["UART_RX"], FPIOA.UART2_RXD)
    
    # 初始化 UART2
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    count = 0
    
    try:
        while True:
            os.exitpoint()
            message = "Hello LuShan-Pi! Count: {}\n".format(count)
            uart.write(message)
            print(f"[发送] {message.strip()}")
            count += 1
            time.sleep_ms(500)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        uart.deinit()
        print("[INFO] UART 资源已释放")

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


### 9.2 串口发送数据（分板代码） ​

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
    from machine import UART, FPIOA
    
    # 配置引脚
    fpioa = FPIOA()
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    
    # 初始化 UART2，115200@8N1
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    count = 0
    
    try:
        while True:
            os.exitpoint()
            message = "Hello LuShan-Pi! Count: {}\n".format(count)
            uart.write(message)
            print(f"[发送] {message.strip()}")
            count += 1
            time.sleep_ms(500)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        uart.deinit()
        print("[INFO] UART 资源已释放")

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


python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time, os
    from machine import UART, FPIOA
    
    # 配置引脚（Lite K230D 串口2引脚与 K230 一致）
    fpioa = FPIOA()
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    
    # 初始化 UART2，115200@8N1
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    count = 0
    
    try:
        while True:
            os.exitpoint()
            message = "Hello LuShan-Pi! Count: {}\n".format(count)
            uart.write(message)
            print(f"[发送] {message.strip()}")
            count += 1
            time.sleep_ms(500)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        uart.deinit()
        print("[INFO] UART 资源已释放")

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


![图 5](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20260709_154336.png)

### 9.3 统一版：串口接收并回传 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    串口接收并回传（统一版）
    - 自动适配 K230 和 Lite K230D
    - 持续监听串口2，收到数据后在 IDE 终端打印，并原样回传给发送方
    - 适用场景：与电脑串口助手进行双向通讯测试
    """
    
    import time, os
    from machine import UART, FPIOA
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "UART_TX": 11, "UART_RX": 12, "UART_ID": 2}
        else:
            return {"board": "lushan_lite_k230d", "UART_TX": 11, "UART_RX": 12, "UART_ID": 2}
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}, 串口2: GPIO{BOARD['UART_TX']}(TX), GPIO{BOARD['UART_RX']}(RX)")
    
    # 配置引脚为 UART 功能
    fpioa = FPIOA()
    fpioa.set_function(BOARD["UART_TX"], FPIOA.UART2_TXD)
    fpioa.set_function(BOARD["UART_RX"], FPIOA.UART2_RXD)
    
    # 初始化 UART2
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    print("[INFO] 等待接收数据... 请通过串口助手发送数据到开发板")
    
    try:
        while True:
            os.exitpoint()
            data = uart.read()
            if data:
                print(f"[接收] {data}")
                # 原样回传，并加前缀标识
                uart.write("UART2 Received: {}\n".format(data))
            time.sleep_ms(50)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        uart.deinit()
        print("[INFO] UART 资源已释放")

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


### 9.4 串口接收并回传（分板代码） ​

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
    from machine import UART, FPIOA
    
    # 配置引脚
    fpioa = FPIOA()
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    
    # 初始化 UART2，115200@8N1
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    print("[INFO] 等待接收数据...")
    
    try:
        while True:
            os.exitpoint()
            data = uart.read()
            if data:
                print(f"[接收] {data}")
                uart.write("UART2 Received: {}\n".format(data))
            time.sleep_ms(50)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        uart.deinit()

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


python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time, os
    from machine import UART, FPIOA
    
    # 配置引脚（Lite K230D 串口2引脚与 K230 一致）
    fpioa = FPIOA()
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    
    # 初始化 UART2，115200@8N1
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    print("[INFO] 等待接收数据...")
    
    try:
        while True:
            os.exitpoint()
            data = uart.read()
            if data:
                print(f"[接收] {data}")
                uart.write("UART2 Received: {}\n".format(data))
            time.sleep_ms(50)
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        uart.deinit()

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


![图 6](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20260709_154510.png)

### 9.5 统一版：串口回环测试 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    """
    串口回环测试（统一版）
    - 自动适配 K230 和 Lite K230D
    - 将串口2的 TX 和 RX 用杜邦线短接
    - 发送一条数据，然后读取回来，对比是否一致
    - 用于验证 UART 硬件和软件配置是否正确
    """
    
    import time, os
    from machine import UART, FPIOA
    
    def get_board_info():
        board_id = os.uname()[-1]
        if board_id == "k230_canmv_lckfb":
            return {"board": "lushan_k230", "UART_TX": 11, "UART_RX": 12, "UART_ID": 2}
        else:
            return {"board": "lushan_lite_k230d", "UART_TX": 11, "UART_RX": 12, "UART_ID": 2}
    
    BOARD = get_board_info()
    print(f"[INFO] 板卡: {BOARD['board']}, 串口2: GPIO{BOARD['UART_TX']}(TX), GPIO{BOARD['UART_RX']}(RX)")
    
    # 配置引脚为 UART 功能
    fpioa = FPIOA()
    fpioa.set_function(BOARD["UART_TX"], FPIOA.UART2_TXD)
    fpioa.set_function(BOARD["UART_RX"], FPIOA.UART2_RXD)
    
    # 初始化 UART2
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    # 要测试的消息
    test_message = b'UART Loopback Test from LuShan-Pi!'
    
    try:
        print("[INFO] 开始回环测试...")
        print(f"[发送] {test_message}")
    
        # 发送数据
        uart.write(test_message)
    
        # 等待数据通过回环线路返回
        time.sleep_ms(100)
    
        # 读取接收到的数据
        received_data = uart.read()
    
        if received_data:
            print(f"[接收] {received_data}")
            if received_data == test_message:
                print("✅ 回环测试通过！发送和接收数据完全一致。")
            else:
                print("❌ 回环测试失败：数据不匹配！")
                print(f"   期望: {test_message}")
                print(f"   实际: {received_data}")
        else:
            print("❌ 回环测试失败：未收到任何数据！")
            print("   请检查：TX 和 RX 是否已用杜邦线短接？")
    
    except KeyboardInterrupt:
        print("[INFO] 用户停止")
    finally:
        uart.deinit()
        print("[INFO] UART 资源已释放")

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


### 9.6 串口回环测试（分板代码） ​

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
    from machine import UART, FPIOA
    
    # 配置引脚
    fpioa = FPIOA()
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    
    # 初始化 UART2，115200@8N1
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    test_message = b'UART Loopback Test!'
    
    try:
        print("[INFO] 开始回环测试...")
        uart.write(test_message)
        time.sleep_ms(100)
    
        received_data = uart.read()
    
        if received_data:
            print(f"[接收] {received_data}")
            if received_data == test_message:
                print("✅ 回环测试通过！")
            else:
                print("❌ 回环测试失败：数据不匹配")
        else:
            print("❌ 回环测试失败：未收到数据，请检查 TX/RX 是否短接")
    finally:
        uart.deinit()

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


python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    import time
    from machine import UART, FPIOA
    
    # 配置引脚（Lite K230D 串口2引脚与 K230 一致）
    fpioa = FPIOA()
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    
    # 初始化 UART2，115200@8N1
    uart = UART(UART.UART2, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)
    
    test_message = b'UART Loopback Test!'
    
    try:
        print("[INFO] 开始回环测试...")
        uart.write(test_message)
        time.sleep_ms(100)
    
        received_data = uart.read()
    
        if received_data:
            print(f"[接收] {received_data}")
            if received_data == test_message:
                print("✅ 回环测试通过！")
            else:
                print("❌ 回环测试失败：数据不匹配")
        else:
            print("❌ 回环测试失败：未收到数据，请检查 TX/RX 是否短接")
    finally:
        uart.deinit()

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


![图 7](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/uart/uart_20260709_154800.png)

## 十 代码说明 ​

### 10.1 串口发送例程解析 ​

发送例程的核心流程非常简单：

  1. **引脚配置** ：通过 FPIOA 将 GPIO11 复用为 UART2_TXD，GPIO12 复用为 UART2_RXD。这一步告诉芯片【这两个引脚要用来做串口通讯】。
  2. **初始化 UART** ：创建 UART 对象，设定波特率 115200、8位数据位、无校验、1位停止位（简称 `115200@8N1`，这是最通用的配置）。
  3. **循环发送** ：在 `while True` 中不断调用 `uart.write()` 发送格式化字符串，每次发送后计数器加一。
  4. **资源释放** ：`try/finally` 确保无论是正常退出还是被 IDE 中断，都会调用 `uart.deinit()` 释放硬件资源。



**为什么要用`os.exitpoint()`？**

在 CanMV IDE 中运行脚本时，如果你点击了 停止 按钮，IDE 会向脚本发送中断信号。`os.exitpoint()` 就是一个检查点——每次循环执行到这里时，会检查是否收到了停止信号，如果收到就会抛出异常退出循环。没有它的话，点击停止按钮可能不会立即生效。

**能改哪里？**

  * 把 `time.sleep_ms(500)` 改小，发送频率就更快
  * 把 `message` 的内容换成你实际要发送的数据
  * 把 `baudrate=115200` 改为其他波特率（记得同步修改接收端）
  * 如果要发送二进制数据，用 `uart.write(bytes([0x01, 0x02, 0x03]))` 代替字符串



### 10.2 串口接收例程解析 ​

接收例程的核心逻辑：

  1. 初始化过程与发送例程相同。
  2. 进入 `while True` 循环后，每隔 50ms 调用一次 `uart.read()` 尝试读取数据。
  3. `uart.read()` 如果没有收到数据会返回 `None`，用 `if data:` 判断是否收到了有效数据。
  4. 收到数据后做两件事：在 IDE 终端打印（方便调试），以及通过串口原样回传（方便验证通讯是否正常）。



**为什么要加`time.sleep_ms(50)`？**

如果不加延时，CPU 会满载运行读取循环，白白浪费处理能力。50ms 的延时对于 115200 波特率来说完全够用（115200bps 下每秒最多传输约 11520 字节，50ms 内最多接收约 576 字节）。如果你的应用对响应延迟敏感，可以减小这个值。

### 10.3 串口回环测试解析 ​

回环测试的原理非常直观：

  1. 用杜邦线把 TX 和 RX 短接——自己发出去的数据立刻从自己的 RX 接收回来。
  2. 发送一段已知数据 → 等待一小段时间（让数据传完）→ 读取接收缓冲区 → 对比收发是否一致。
  3. 如果一致，说明 UART 的发送、接收功能和引脚配置都正确；如果不一致或收不到，则需要检查硬件连接或配置。



**为什么发送后要`time.sleep_ms(100)`？**

串口是逐位发送的。以 115200bps 发送 34 字节的测试消息，理论耗时约 3ms。但考虑到系统调度和缓冲延迟，预留 100ms 确保万无一失。

## 十一 常见问题 ​

Q1：串口助手收到的是乱码怎么办？

**原因** ：发送端和接收端的波特率不一致，或者数据位/校验位/停止位设置不匹配。

**解决办法** ：

  1. 确认开发板代码中的 `baudrate=115200` 和电脑串口助手设置的波特率一致。
  2. 确认双方都是 8N1（8位数据位、无校验、1位停止位）。
  3. 如果串口助手显示的是十六进制而你期望看到文字，检查显示模式是否设为"字符串/ASCII"。

Q2：回环测试收不到数据？

**原因** ：TX 和 RX 没有正确短接，或者引脚配置错误。

**解决办法** ：

  1. 检查杜邦线是否牢固连接在 GH1.25 座子的 T（TXD）和 R（RXD）之间。
  2. 确认代码中 `set_function` 配置的 GPIO 编号正确（GPIO11 为 TXD，GPIO12 为 RXD）。
  3. 确认使用的是串口2的 GH1.25 座子（丝印标号为2的那个），不要接错到串口0（系统占用）。
  4. 尝试增大 `time.sleep_ms(100)` 的等待时间到 200ms。

Q3：运行脚本后 IDE 终端没有任何输出？

**原因** ：可能是固件版本过旧，或者脚本运行出错但异常被吞了。

**解决办法** ：

  1. 确认固件是最新版本的 CanMV 固件，旧版本可能不支持部分 API。
  2. 在代码开头加一行 `print("程序启动")` 确认脚本确实在运行。
  3. 检查 IDE 是否连接成功（左下角状态显示"已连接"）。

Q4：能不能同时使用多个串口？

可以。K230 有 4 个用户可用串口（UART1~UART4），只要分别配置好对应的 GPIO 和 UART 编号，就可以同时使用多个串口进行通讯。例如同时使用串口2和串口3：

立创·庐山派K230-CanMV开发板立创·庐山派Lite-K230D-CanMV开发板

python
    
    
    # 串口2：GPIO11/GPIO12（两板一致）
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    # 串口3：K230 用 GPIO50/GPIO51
    fpioa.set_function(50, FPIOA.UART3_TXD)
    fpioa.set_function(51, FPIOA.UART3_RXD)
    
    uart2 = UART(UART.UART2, baudrate=115200)
    uart3 = UART(UART.UART3, baudrate=9600)

1  
2  
3  
4  
5  
6  
7  
8  
9  


python
    
    
    # 串口2：GPIO11/GPIO12（两板一致）
    fpioa.set_function(11, FPIOA.UART2_TXD)
    fpioa.set_function(12, FPIOA.UART2_RXD)
    # 串口3：Lite K230D 用 GPIO32/GPIO33（与 K230 不同！）
    fpioa.set_function(32, FPIOA.UART3_TXD)
    fpioa.set_function(33, FPIOA.UART3_RXD)
    
    uart2 = UART(UART.UART2, baudrate=115200)
    uart3 = UART(UART.UART3, baudrate=9600)

1  
2  
3  
4  
5  
6  
7  
8  
9  


注意不同串口可以设置不同的波特率，互不影响。同时也要留意：**串口3 两块板子的引脚不一样** ，K230 是 GPIO50/51，Lite K230D 是 GPIO32/33。 :::

## 十二 总结 ​

本节我们学习了 UART 串口通讯的基础知识和实际使用方法：

  1. 了解了 UART 协议的工作原理——异步、全双工、通过波特率约定 语速 。
  2. 掌握了开发板上可用的串口资源，知道推荐使用串口2（GH1.25-4P带锁座）进行外部设备通讯。
  3. 学会了通过 FPIOA 配置引脚 + UART API 实现数据收发。
  4. 通过回环测试掌握了自行验证串口功能的方法。



**后续可以尝试的扩展方向：**

  * 连接 GPS 模块，解析 NMEA 定位数据
  * 连接蓝牙模块（HC-05/HC-06），实现手机与开发板的无线串口通讯
  * 与 STM32 等单片机（比如说天空星开发板）进行板间通讯，实现多设备协作
  * 自定义通讯协议（帧头+数据+校验），实现可靠的数据传输
  * 连接语音识别模块或 AI 语音模块进行交互



