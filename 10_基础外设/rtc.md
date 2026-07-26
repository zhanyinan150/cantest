# 实时时钟【RTC】

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/rtc.html>
> **最后更新**: 2026-07-21

---

## 一 本节介绍 ​

📝本节我们将学习实时时钟，全称 Real-Time Clock（RTC）的基本用法，包括读取日期时间、设置日期时间，以及在到达指定时刻后触发板载绿色 LED。

教程同时适用于以下两款开发板：

  * 立创·庐山派K230-CanMV开发板
  * 立创·庐山派Lite-K230D-CanMV开发板



### 1.1 学习目标 ​

🏆学习目标

1️⃣ 理解 RTC、系统时间和运行时长之间的区别。

2️⃣ 掌握 `RTC()` 与 `RTC.datetime()` 的常用方法。

3️⃣ 能正确读取并格式化 RTC 返回的 8 元组。

4️⃣ 能运行一份通过 `get_board_info()` 自动适配两款开发板的定时触发例程。

5️⃣ 明确两款庐山派均未预留 RTC 后备供电，开发板掉电后 RTC 时间会重置，并了解联网校时或外接 RTC 等解决方法。

### 1.2 重点提示 ​

设置 RTC 会修改系统时间

执行 `rtc.datetime(datetime_tuple)` 会立即修改开发板当前的系统日期和时间。运行示例前，请先检查代码顶部的 `DEMO_DATETIME`，不要在正在记录正式数据的设备上随意执行设置时间操作。

IMPORTANT

两款庐山派在硬件设计时均未预留 RTC 后备供电。一旦开发板供电丢失，RTC 就会停止走时；重新上电后 RTC 时间会重置，必须重新设置或校时。两款开发板的 RTC API 相同，RTC 本身不占用排针 GPIO。本教程为了直观演示定时事件，额外使用板载绿色 LED；两板的 LED 引脚和有效电平不同，完整例程会自动识别并适配。

### 1.3 本节效果 ​

运行完整例程后，串行终端会每秒输出一次当前时间。示例将 RTC 设置为 `2026-07-20 16:30:00`，到达 `16:30:10` 后，板载绿色 LED 点亮；停止脚本时，LED 自动熄灭。

## 二 软硬件准备 ​

名称| 数量| 说明  
---|---|---  
立创·庐山派K230-CanMV开发板或立创·庐山派Lite-K230D-CanMV开发板| 1| 二选一，本教程同时适配  
TF 卡| 1| 已写入与板卡型号匹配的 CanMV MicroPython 固件  
Type-C 数据线| 1| 需要支持数据传输，不能只支持充电  
电脑| 1| 用于连接开发板、运行脚本并查看串行终端  
外接模块| 0| 基础实验直接使用片上 RTC 和板载绿色 LED  
  
**固件要求** ：使用与开发板型号匹配的较新 CanMV MicroPython 固件。K230 与 Lite-K230D 的固件镜像不能混用。

**开发环境** ：可以使用与当前固件配套的 CanMV IDE K230、CanMV IDE Web，或 VS Code 配合 CanMV 扩展。

## 三 双板兼容说明 ​

项目| 立创·庐山派K230-CanMV开发板| 立创·庐山派Lite-K230D-CanMV开发板  
---|---|---  
是否支持本节实验| 支持| 支持  
主控| K230| K230D  
RTC API| `machine.RTC`| `machine.RTC`  
RTC 外部接线| 不需要| 不需要  
示例绿色 LED| GPIO20，低电平点亮| GPIO66，高电平点亮  
内存影响| 可忽略| 可忽略  
掉电后的时间| 未预留 RTC 后备供电；掉电后停止走时，重新上电时间会重置| 未预留 RTC 后备供电；掉电后停止走时，重新上电时间会重置  
板卡适配方式| `get_board_info()` 自动适配| `get_board_info()` 自动适配  
  
IMPORTANT

`get_board_info()` 在本例中只负责适配板载绿色 LED 的引脚和有效电平。RTC 的创建、读取和设置代码在两款开发板上完全相同，用户不需要手动修改 GPIO。

⚠️不要把板载 RTC 当作带电池的高精度时钟

立创·庐山派K230-CanMV开发板和立创·庐山派Lite-K230D-CanMV开发板在硬件设计时均未预留 RTC 后备供电。开发板正常供电时，RTC 可以持续走时；一旦开发板供电丢失，RTC 就会停止走时，重新上电后原有时间不会保留，RTC 时间会重置。

因此，需要准确日历时间的产品必须在每次上电后重新校时：有网络时可使用 NTP，没有网络时可外接带后备电池的高精度 RTC 模块。

## 四 基础知识与名词解释 ​

### 4.1 什么是 RTC ​

RTC 是用来维护“现在是几月几日、几点几分”的时钟模块。可以把它理解成设备内部的一只日历手表：程序读取 RTC 后，就能给日志、照片、传感器数据或告警事件添加时间戳。

名词| 全称| 说明  
---|---|---  
RTC| Real-Time Clock，实时时钟| 维护日期和时间  
系统时间| System Time| 操作系统或运行环境当前使用的日历时间  
时间戳| Timestamp| 用于标记事件发生时刻的数据  
NTP| Network Time Protocol，网络时间协议| 通过网络获取标准时间  
后备电源| Backup Power| 主电源断开后继续给 RTC 供电的电池或超级电容  
  
### 4.2 RTC 不等于运行时间计数器 ​

选择计时接口时，先分清需求：

需求| 推荐接口| 原因  
---|---|---  
显示当前日期和时间| `RTC.datetime()`| 能得到年、月、日、时、分、秒  
给日志添加日历时间| `RTC.datetime()` 或联网校时后的系统时间| 需要绝对时间  
测量一段代码执行了多久| `time.ticks_ms()` / `time.ticks_us()`| 不受手动校时影响，并能处理计数器回绕  
周期执行任务| `Timer` 或基于 `ticks_ms()` 的调度| 比每次比较 RTC 字段更适合周期控制  
  
RTC 适合回答“现在是什么时间”；`ticks_ms()` 更适合回答“过去了多长时间”。如果用户在程序运行期间重新设置 RTC，依赖 RTC 计算的时间差可能突然跳变。

### 4.3 RTC 日期时间元组 ​

`rtc.datetime()` 返回 8 个元素：

text
    
    
    (year, month, day, weekday, hour, minute, second, subsecond)

1  


索引| 字段| 示例| 说明  
---|---|---|---  
`0`| 年| `2026`| 四位年份  
`1`| 月| `7`| `1`～`12`  
`2`| 日| `20`| 取值需要符合当前月份  
`3`| 星期| `0`| 不同固件的取值约定可能存在差异，本教程不依赖此字段  
`4`| 时| `16`| `0`～`23`  
`5`| 分| `30`| `0`～`59`  
`6`| 秒| `10`| `0`～`59`  
`7`| 微秒/子秒| `0`| 含义和精度与硬件、固件实现有关  
  
TIP

进行定时判断时，建议使用字段索引常量或先格式化成明确变量，不要把 `current_time[4]` 这类“魔法数字”散落在实际项目中，不然后面维护会特别麻烦。

### 4.4 精度与掉电保持 ​

“能够走时”“掉电后继续走时”和“长期保持高精度”是三件不同的事：

  1. 开发板正常供电且 RTC 已设置后，可以持续读取递增的日期时间。
  2. 两款庐山派均未预留 RTC 后备供电。一旦开发板掉电，RTC 就会停止走时；重新上电后时间会重置，必须重新设置或校时。
  3. 晶振误差会长期累积。对时间准确性有要求时，应定期通过 NTP 校时，或使用带温度补偿和后备电池的外接 RTC。



## 五 常用 API 说明 ​

### 5.1 `RTC` 类 ​

RTC 类位于 `machine` 模块中：

python
    
    
    from machine import RTC
    
    rtc = RTC()

1  
2  
3  


API| 作用| 参数| 返回值  
---|---|---|---  
`RTC()`| 创建 RTC 对象| 无| RTC 对象  
`rtc.datetime()`| 读取当前日期和时间| 无| 8 元组  
`rtc.datetime(datetime_tuple)`| 设置日期和时间| 8 元组| `None`  
`rtc.init(datetime_tuple)`| 初始化 RTC| 不同 CanMV 固件的元组语义存在历史差异| 本教程不使用  
  
为什么本教程不使用 `RTC.init()`

不同 CanMV 固件版本曾对 `RTC.init()` 的日期时间元组采用过不同字段顺序，旧例程和部分 API 文档也存在不一致。为了避免把“星期”误当成“小时”，本教程统一使用语义明确、符合 MicroPython RTC 通用格式的 `rtc.datetime(8元组)` 设置时间。

最小示例：

python
    
    
    from machine import RTC
    
    rtc = RTC()
    
    # 读取当前 RTC
    print(rtc.datetime())
    
    # 设置为 2026-07-20 16:30:00
    # 第 4 项是星期占位，本例不使用星期字段参与业务判断
    rtc.datetime((2026, 7, 20, 0, 16, 30, 0, 0))
    print(rtc.datetime())

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


### 5.2 格式化显示 ​

直接打印元组便于调试，但产品日志通常需要更直观的格式：

python
    
    
    def format_datetime(dt):
        return "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}".format(
            dt[0], dt[1], dt[2], dt[4], dt[5], dt[6]
        )

1  
2  
3  
4  


该函数有意跳过索引 `3` 的星期字段和索引 `7` 的子秒字段，避免不同固件实现对显示结果造成干扰。

## 六 操作步骤 ​

### 6.1 确认板卡和固件 ​

  1. 查看开发板丝印，确认手中是立创·庐山派K230-CanMV开发板还是立创·庐山派Lite-K230D-CanMV开发板。
  2. 确认 TF 卡内固件与板卡型号匹配。
  3. 使用支持数据传输的 Type-C 数据线连接电脑。



⚠️固件不能混用

K230 与 Lite-K230D 使用各自对应的固件镜像。不要为了运行同一份 MicroPython 脚本而混刷另一款板卡的固件。

### 6.2 检查示例参数 ​

完整代码顶部有以下参数：

python
    
    
    SET_RTC_ON_START = True
    DEMO_DATETIME = (2026, 7, 20, 0, 16, 30, 0, 0)
    TRIGGER_TIME = (16, 30, 10)

1  
2  
3  


  * 首次体验时保持 `SET_RTC_ON_START = True`，程序会设置演示时间。
  * `DEMO_DATETIME` 依次表示年、月、日、星期占位、时、分、秒、子秒。
  * `TRIGGER_TIME` 表示时、分、秒，本例在设置 RTC 十秒后触发。
  * 接入实际项目时，建议改为 `SET_RTC_ON_START = False`，并由网络或外接 RTC 提供真实时间。



### 6.3 运行程序 ​

  1. 打开 CanMV 开发环境并连接开发板。
  2. 新建脚本，将“七 代码例程”中的完整代码复制进去。
  3. 点击运行按钮执行脚本。
  4. 观察串行终端中的板卡名称和每秒时间输出。
  5. 到达 `16:30:10` 后，确认板载绿色 LED 点亮。
  6. 点击停止按钮，确认 LED 熄灭。



## 七 代码例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    # 编写者：LCKFB-YZH
    
    from machine import RTC, Pin, FPIOA
    import os
    import time
    
    
    # ==================== 用户可修改参数 ====================
    # True：每次运行都写入演示时间；False：只读取当前 RTC
    SET_RTC_ON_START = True
    
    # (年, 月, 日, 星期占位, 时, 分, 秒, 子秒)
    # 本例不使用星期字段参与业务判断
    DEMO_DATETIME = (2026, 7, 20, 0, 16, 30, 0, 0)
    
    # 到达该时刻后点亮绿色 LED：(时, 分, 秒)
    TRIGGER_TIME = (16, 30, 10)
    
    # RTC 轮询周期。200 ms 可以避免错过目标秒，又不会频繁刷屏
    POLL_INTERVAL_MS = 200
    # ======================================================
    
    
    def get_board_info():
        """自动识别庐山派型号，并返回本例需要的 LED 参数。"""
        board_id = os.uname()[-1]
    
        if board_id == "k230_canmv_lckfb":
            return {
                "board_name": "立创·庐山派K230-CanMV开发板",
                "LED_G": 20,
                "LED_ON_LEVEL": 0,
                "LED_OFF_LEVEL": 1,
            }
    
        return {
            "board_name": "立创·庐山派Lite-K230D-CanMV开发板",
            "LED_G": 66,
            "LED_ON_LEVEL": 1,
            "LED_OFF_LEVEL": 0,
        }
    
    
    def init_green_led(board):
        """根据板卡参数初始化绿色 LED，并确保初始状态为熄灭。"""
        led_pin = board["LED_G"]
        fpioa = FPIOA()
        fpioa.set_function(
            led_pin,
            getattr(FPIOA, "GPIO{}".format(led_pin)),
        )
    
        led = Pin(
            led_pin,
            Pin.OUT,
            pull=Pin.PULL_NONE,
            drive=7,
        )
        led.value(board["LED_OFF_LEVEL"])
        return led
    
    
    def format_datetime(dt):
        """把 RTC 8 元组转换为便于阅读的日期时间字符串。"""
        return "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}".format(
            dt[0], dt[1], dt[2], dt[4], dt[5], dt[6]
        )
    
    
    def hms_to_seconds(hour, minute, second):
        """把一天内的时分秒转换为秒数，便于进行大小比较。"""
        return hour * 3600 + minute * 60 + second
    
    
    BOARD = get_board_info()
    rtc = RTC()
    green_led = None
    
    try:
        green_led = init_green_led(BOARD)
    
        print("当前板卡：{}".format(BOARD["board_name"]))
        print("绿色 LED：GPIO{}".format(BOARD["LED_G"]))
    
        if SET_RTC_ON_START:
            rtc.datetime(DEMO_DATETIME)
            print("已写入演示时间：{}".format(format_datetime(rtc.datetime())))
        else:
            print("保持现有 RTC，不写入演示时间")
    
        target_seconds = hms_to_seconds(
            TRIGGER_TIME[0],
            TRIGGER_TIME[1],
            TRIGGER_TIME[2],
        )
        last_printed_second = -1
        event_triggered = False
    
        while True:
            # 允许用户在 CanMV 开发环境中停止脚本
            os.exitpoint()
    
            current_time = rtc.datetime()
            current_second = current_time[6]
    
            # 轮询周期是 200 ms，但终端只在秒变化时打印一次
            if current_second != last_printed_second:
                print("当前时间：{}".format(format_datetime(current_time)))
                last_printed_second = current_second
    
            current_seconds = hms_to_seconds(
                current_time[4],
                current_time[5],
                current_time[6],
            )
    
            # 使用“大于等于”而不是严格相等，避免系统短暂忙碌时错过目标秒
            if not event_triggered and current_seconds >= target_seconds:
                green_led.value(BOARD["LED_ON_LEVEL"])
                event_triggered = True
                print(
                    "定时事件触发：{}，绿色 LED 已点亮".format(
                        format_datetime(current_time)
                    )
                )
    
            time.sleep_ms(POLL_INTERVAL_MS)
    
    except KeyboardInterrupt:
        print("用户停止运行")
    except Exception as exc:
        print("RTC 示例运行异常：{}".format(exc))
    finally:
        if green_led is not None:
            green_led.value(BOARD["LED_OFF_LEVEL"])
        print("程序结束，绿色 LED 已关闭")

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


## 八 代码说明 ​

### 8.1 板卡自动检测 ​

`get_board_info()` 通过 `os.uname()[-1]` 判断当前板卡。标准版使用 GPIO20 控制绿色 LED，且低电平点亮；Lite-K230D 使用 GPIO66，且高电平点亮。

这两个差异同时存在，因此不能只替换 GPIO 而忽略有效电平。完整例程把引脚号、点亮电平和熄灭电平放在同一个板卡配置字典中，后续逻辑无需出现板型判断。

### 8.2 设置与读取 RTC ​

python
    
    
    rtc = RTC()
    rtc.datetime(DEMO_DATETIME)
    current_time = rtc.datetime()

1  
2  
3  


  * 带 8 元组参数时，`datetime()` 设置 RTC。
  * 不带参数时，`datetime()` 读取 RTC。
  * 业务代码只使用年、月、日、时、分、秒，不依赖星期和子秒字段。



### 8.3 定时判断为什么使用“大于等于” ​

如果只在 `current_seconds == target_seconds` 时触发，一旦系统在目标秒内执行了耗时操作，下一次读取可能已经超过目标时刻，事件将永远无法触发。

本例先将时、分、秒转换为当天秒数，再使用：

python
    
    
    if not event_triggered and current_seconds >= target_seconds:
        ...

1  
2  


`event_triggered` 保证事件只触发一次，`>=` 则提高了面对短时阻塞时的可靠性。

WARNING

这个简化判断适用于“同一天内、目标时间晚于启动时间”的演示。需要跨午夜、跨日期或周期任务时，应比较完整日期时间，或使用调度器、`Timer`、`ticks_ms()` 实现。

### 8.4 主循环与退出清理 ​

  * `os.exitpoint()` 让 CanMV 开发环境能够中断主循环。
  * 轮询周期为 200 ms，但只有秒字段变化时才输出，避免终端刷屏。
  * `try/except/finally` 捕获异常并确保脚本停止时关闭 LED。
  * 本例没有创建需要 `deinit()` 的定时器或总线对象；`Pin` 退出前恢复到 LED 熄灭电平即可。



### 8.5 常改参数 ​

参数| 作用| 推荐做法  
---|---|---  
`SET_RTC_ON_START`| 是否写入演示时间| 学习时为 `True`，正式项目通常为 `False`  
`DEMO_DATETIME`| 手动设置的日期时间| 按实际日期时间修改，并确保日期合法  
`TRIGGER_TIME`| LED 触发时刻| 设置为当天且晚于当前时间  
`POLL_INTERVAL_MS`| RTC 轮询周期| 一般使用 `100`～`1000` ms  
  
## 九 实际运行效果 ​

以 Lite-K230D 为例，终端输出形式如下；标准版只有板卡名称和绿色 LED GPIO 不同：

text
    
    
    当前板卡：立创·庐山派Lite-K230D-CanMV开发板
    绿色 LED：GPIO66
    已写入演示时间：2026-07-20 16:30:00
    当前时间：2026-07-20 16:30:00
    当前时间：2026-07-20 16:30:01
    当前时间：2026-07-20 16:30:02
    当前时间：2026-07-20 16:30:03
    ...
    当前时间：2026-07-20 16:30:10
    定时事件触发：2026-07-20 16:30:10，绿色 LED 已点亮

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


## 十 常见问题 ​

### 10.1 为什么断电再上电后，RTC 时间会重置？ ​

**现象** ：开发板断电后重新上电，读取到重置后的时间，而不是断电前的日期和时间。

**原因** ：两款庐山派在硬件设计时均未预留 RTC 后备供电。开发板供电丢失后，RTC 会停止走时且无法保存原有时间；重新上电后 RTC 时间会重置。这是硬件供电设计决定的正常现象，不是程序故障。

**解决方法** ：

  1. 每次上电后通过网络同步时间，确认校时成功后再开始记录数据。
  2. 无网络产品可外接带后备电池的 RTC 模块，例如 SD3078、DS3231 等。
  3. 不要只在首次部署时设置一次 RTC；每次完整掉电重启后都需要重新校时。



### 10.2 时间正常输出，但绿色 LED 不亮？ ​

**现象** ：终端已经打印“定时事件触发”，板载绿色 LED 仍未点亮。

**原因** ：常见原因是固件与板卡型号不匹配、板卡识别结果异常，或复制代码时改错了 LED 电平。

**解决方法** ：

  1. 查看终端打印的板卡名称和 GPIO。
  2. 标准版应使用 GPIO20、低电平点亮；Lite-K230D 应使用 GPIO66、高电平点亮。
  3. 确认代码中的 `get_board_info()` 和 `BOARD["LED_ON_LEVEL"]` 未被删改。
  4. 确认 TF 卡内固件与手中板卡型号一致。



### 10.3 设置后小时、星期或秒的位置不符合预期？ ​

**现象** ：使用旧教程中的 `RTC.init()` 后，时间字段发生错位；或者星期字段与预期不一致。

**原因** ：不同 CanMV 固件版本中，`RTC.init()` 的元组语义曾存在差异；星期字段的约定也不适合直接作为跨固件业务逻辑。

**解决方法** ：

  1. 使用本教程的 `rtc.datetime((年, 月, 日, 星期占位, 时, 分, 秒, 子秒))` 设置方式。
  2. 读取后以索引 `4`、`5`、`6` 分别取得时、分、秒。
  3. 星期敏感应用应在应用层根据年月日计算星期，并在目标固件上验证。



### 10.4 联网校时后相差 8 小时？ ​

**现象** ：同步网络时间后，显示值与本地时间相差一个时区。

**原因** ：网络时间源、RTC 和应用层可能分别使用 UTC 或本地时间。如果重复增加时区偏移，就会出现多加 8 小时；完全未转换则可能少 8 小时。

**解决方法** ：

  1. 明确项目内部统一使用 UTC 还是北京时间。
  2. 只在一个层级执行时区转换，不要在网络同步和显示格式化中重复转换。
  3. 同步后同时打印原始 RTC 元组和格式化结果进行核对。



### 10.5 能否使用 RTC 实现毫秒级精确定时？ ​

不建议。RTC 主要用于日历时间，子秒字段的含义和精度依赖硬件与固件。测量毫秒、微秒时间间隔时使用 `time.ticks_ms()`、`time.ticks_us()`；周期控制优先使用 `Timer`。

## 十一 总结 ​

本节完成了 RTC 的读取、设置、格式化显示和定时事件演示。核心要点如下：

  * 两款庐山派都支持相同的 `machine.RTC` API。
  * 为兼容不同 CanMV 固件，本教程使用 `RTC.datetime()` 设置时间，不使用存在历史语义差异的 `RTC.init()`。
  * RTC 示例中的绿色 LED 通过 `get_board_info()` 自动适配两板的 GPIO 和有效电平。
  * RTC 适合维护日历时间，测量时间间隔应使用 `ticks_ms()`、`ticks_us()` 或 `Timer`。
  * 两款庐山派均未预留 RTC 后备供电；开发板掉电后 RTC 停止走时，重新上电时间会重置，正式产品必须在每次上电后重新校时，或外接带后备电池的 RTC。



