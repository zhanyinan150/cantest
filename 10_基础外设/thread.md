# 线程（Thread）

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/thread.html>
> **最后更新**: 2025-07-18

---

# 1 本节介绍 ​

📝本节学习MicroPython中_thread模块的使用，实现多线程编程。

🏆学习目标

1️⃣ 理解线程概念及适用场景

2️⃣ 使用start_new_thread创建线程

3️⃣ 使用锁（Lock）避免资源冲突

# 2 名词解释 ​

名词| 说明  
---|---  
**线程**|  程序执行的最小单元，可并行运行多个任务  
**锁（Lock）**|  同步机制，确保同一时间只有一个线程访问共享资源  
**线程标识**|  唯一整数ID，用于区分不同线程  
  
注意：

MicroPython线程**非抢占式** 需要​​主动休息（让出cpu）​​才会切换（比如使用time.sleep()）

# 3 线程操作 ​

## 3.1 创建基础线程 ​

▶ **创建新线程**

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    import _thread
    import time
    
    def task(thread_id, delay):
        for i in range(3):
            time.sleep(delay)
            print(f"线程{thread_id}执行第{i+1}次")
    
    # 启动新线程：参数为(函数, (参数元组))
    _thread.start_new_thread(task, (1, 1.0))  # 线程1，每秒执行
    _thread.start_new_thread(task, (2, 0.5))  # 线程2，每0.5秒执行

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


其执行效果如下图所示：

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/basic/thread/thread_20250718_115016.png)

## 3.2 使用线程锁（Lock） ​

▶ **为什么需要锁？**

当多个线程同时访问**共享资源** （如全局变量、外设）时，会导致**数据混乱或损坏** 。锁可以确保关键代码段的**原子性执行** 。

▶ **锁的基本用法**

python
    
    
    import _thread
    
    lock = _thread.allocate_lock()  # 创建锁对象
    
    lock.acquire()  # 获取锁（如果锁已被占用，则等待）
    # 临界区代码（操作共享资源）
    lock.release()  # 释放锁

1  
2  
3  
4  
5  
6  
7  


▶ **带锁的计数器示例**

python
    
    
    import _thread
    import time
    
    # 共享资源
    counter = 0
    # 创建锁对象
    lock = _thread.allocate_lock()
    
    def increment(thread_id):
        global counter
        for _ in range(5):
            lock.acquire()  # 获取锁
            # === 临界区开始 ===
            temp = counter
            time.sleep(0.01)  # 模拟处理延时
            counter = temp + 1
            print(f"线程{thread_id}增加计数, 当前值: {counter}")
            # === 临界区结束 ===
            lock.release()  # 必须释放锁
            time.sleep(0.1)
    
    # 启动两个线程
    _thread.start_new_thread(increment, (1,))
    _thread.start_new_thread(increment, (2,))
    
    # 主线程等待足够时间（生产中应使用更可靠的同步机制）
    time.sleep(2)
    print("最终计数器值:", counter)  # 正确结果应为10

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


**执行结果分析** ：

  * 若**不加锁** ：可能输出`最终计数器值: 5-9`（因数据竞争导致结果不确定）大家可以自行注释掉`lock.acquire() # 获取锁`和`lock.release() # 必须释放锁`自行试试
  * **加锁后** ：始终输出`最终计数器值: 10`（保证操作的原子性）



最佳实践

1️⃣ **临界区尽量短** ：减少锁占用时间

2️⃣ **确保释放锁** ：使用`try-finally`保证异常时也能释放

python
    
    
    lock.acquire()
    try:
        # 临界区代码
    finally:
        lock.release()

1  
2  
3  
4  
5  


3️⃣ **避免嵌套锁** ：MicroPython锁**不可重入** （同一线程不能多次获取）

# 4 实际案例 ​

## 4.1 基础版：无同步独立闪烁 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    from machine import Pin
    from machine import FPIOA
    import time
    import _thread
    
    # 创建FPIOA对象，用于初始化引脚功能配置
    fpioa = FPIOA()
    
    # 设置引脚功能，将指定的引脚配置为普通GPIO功能,
    fpioa.set_function(62,FPIOA.GPIO62)
    fpioa.set_function(20,FPIOA.GPIO20)
    fpioa.set_function(63,FPIOA.GPIO63)
    
    # 实例化Pin62, Pin20, Pin63为输出，分别用于控制红、绿、蓝三个LED灯
    LED_R = Pin(62, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 红灯
    LED_G = Pin(20, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 绿灯
    LED_B = Pin(63, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 蓝灯
    
    # 板载RGB灯是共阳结构，设置引脚为高电平时关闭灯，低电平时点亮灯
    # 初始化时先关闭所有LED灯
    LED_R.high()  # 关闭红灯
    LED_G.high()  # 关闭绿灯
    LED_B.high()  # 关闭蓝灯
    
    def led_task(led, interval, thread_id):
        """独立LED闪烁任务"""
        while True:
            led.low()  # 点亮LED
            print(f"线程{thread_id}：LED亮")
            time.sleep(interval)
            led.high()  # 熄灭LED
            print(f"线程{thread_id}：LED灭")
            time.sleep(interval)
    
    # 启动两个独立LED线程
    _thread.start_new_thread(led_task, (LED_R, 0.3, 1))
    _thread.start_new_thread(led_task, (LED_B, 0.5, 2))
    
    # 主线程持续运行
    while True:
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


**执行效果** ：

  * LED0以0.3秒间隔闪烁
  * LED1以0.5秒间隔闪烁
  * 两灯闪烁**不同步** ，亮灭状态随机组合



## 4.2 进阶版：同步交替闪烁 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    from machine import Pin, FPIOA
    import time
    import _thread
    
    # 创建FPIOA对象
    fpioa = FPIOA()
    
    # 配置引脚功能
    fpioa.set_function(62, FPIOA.GPIO62)
    fpioa.set_function(20, FPIOA.GPIO20)
    fpioa.set_function(63, FPIOA.GPIO63)
    
    # 实例化LED控制引脚（共阳结构）
    LED_R = Pin(62, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 红灯
    LED_G = Pin(20, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 绿灯
    LED_B = Pin(63, Pin.OUT, pull=Pin.PULL_NONE, drive=7)  # 蓝灯
    
    # 初始化关闭所有LED
    LED_R.high()  # 高电平关闭
    LED_G.high()
    LED_B.high()
    
    # 创建锁对象控制交替同步
    lock = _thread.allocate_lock()
    # 共享状态变量 (0:红灯亮，1:蓝灯亮)
    state = 0
    
    def red_led_task():
        """红灯控制线程"""
        global state
        while True:
            # 获取锁进行操作
            lock.acquire()
            try:
                if state == 0:
                    LED_R.low()   # 点亮红灯
                    LED_B.high()  # 关闭蓝灯
                    # 更新状态并保持0.5秒
                    state = 1
                    time.sleep(0.5)
            finally:
                # 确保释放锁
                lock.release()
            # 让出CPU控制权
            time.sleep(0.01)  # 短暂休眠避免忙等待
    
    def blue_led_task():
        """蓝灯控制线程"""
        global state
        while True:
            lock.acquire()
            try:
                if state == 1:
                    LED_B.low()   # 点亮蓝灯
                    LED_R.high()  # 关闭红灯
                    # 更新状态并保持0.5秒
                    state = 0
                    time.sleep(0.5)
            finally:
                lock.release()
            time.sleep(0.01)
    
    # 启动线程（绿灯保持常灭）
    _thread.start_new_thread(red_led_task, ())
    _thread.start_new_thread(blue_led_task, ())
    
    # 主线程维持程序运行
    while True:
        time.sleep(1)  # 保持主线程不退出

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


**工作原理详解** ：

  1. **共享状态变量** ： 
     * `state = 0`初始状态为红灯亮
     * `state = 1`表示蓝灯亮
     * 每次亮灯操作后会切换状态值
  2. **线程分工** ： 
     * `red_led_task()`：专门检查红灯状态 
       * 当`state==0`时点亮红灯并关闭蓝灯
       * 点亮后切换状态为`state=1`
     * `blue_led_task()`：专门检查蓝灯状态 
       * 当`state==1`时点亮蓝灯并关闭红灯
       * 点亮后切换状态为`state=0`
  3. **锁机制** ： 
     * `lock.acquire()`确保同一时间只有一个线程操作LED
     * `finally`块保证无论是否出错都会释放锁
     * 临界区内包含`time.sleep(0.5)`保持亮灯状态
  4. **非阻塞设计** ： 
     * 线程在非临界区使用`time.sleep(0.01)`
     * 短暂让出CPU避免忙等待
     * 保持系统响应性



**执行效果** ：

  1. 程序启动：红灯亮起0.5秒（蓝灯灭）
  2. 0.5秒后：蓝灯亮起0.5秒（红灯灭）
  3. 0.5秒后：红灯再次亮起0.5秒
  4. 持续循环交替（绿灯始终关闭）



**注意事项** ：

  1. 通过`state`变量传递控制权，避免状态冲突
  2. 锁保护了状态切换和LED操作的原子性
  3. 每个线程只关心自身状态，职责单一
  4. 线程间通过共享变量协同工作，实现同步



