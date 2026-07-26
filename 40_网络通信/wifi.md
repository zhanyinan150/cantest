# 无线网络

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/network/wifi.html>
> **最后更新**: 2025-02-08

---

# 1 本节介绍 ​

📝本节您将学习如何使用庐山派的WIFI，如何初始化WiFi模块、扫描可用网络、以及连接到指定路由器。

🏆学习目标

1️⃣如何用庐山派去连接2.4G频段的无线路由器（STA模式）。

2️⃣理解STA（站点）和AP（热点）两种工作模式。

警告

目前庐山派上板载的WiFi模块（内部芯片是RTL8189FTV）只支持2.4G频段，不支持5G频段，亦不支持2.4G和5G的混合频段。若路由器采用双频合一模式，请先在路由器设置中分离2.4G信号。如果使用手机来开热点的话也一定要注意将WiFi的频段改为2.4G。

✅ 路由器需单独设置2.4G信号

✅ 手机热点需强制设为2.4G频段

❌ 不支持5G频段和双频混合模式

庐山派上自带板载陶瓷天线，到手使用前无需再连接天线。

如果需要更强的信号质量可以切换天线旁的0欧电阻并从板载的IPEX口插入外置天线。

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/network/wifi/wifi_20250207_092443.png)

大家买到手的板子wifi模块处的实物图如上图所示，默认贴片的0欧电阻直连到板载的陶瓷天线上。

如果需要从IPEX-2代座子处插入外置天线，只需要把陶瓷天线的0欧电阻吹下来，焊接到外置天线的0欧电阻处就可以了。这两个电阻共用其中一个焊盘是为了通过单刀双掷的切换机制实现天线通道的物理隔离。既能避免板载天线与外置天线产生信号串扰，又能确保射频信号路径始终保持完整的50Ω阻抗匹配。

# 2 连接WIFI（STA模式） ​

**STA模式** （Station）：庐山派作为客户端连接到现有无线网络

  * **`network.WLAN(network.STA_IF)`** : 初始化一个 WLAN 对象，并设置为STA 模式。（站模式，可以连接到外部 WiFi 接入点）
  * **`sta.active(bool)`** : 激活或关闭 STA 模式。当传入 `True` 时激活，传入 `False` 时关闭。如果不带参数调用，则返回当前激活状态。
  * **`sta.status()`** : 返回 STA 的当前状态，如是否已连接到 AP。当不传参数时，返回详细的连接信息，`Sta` 模式时，返回`rssi`: 连接信号质量和`ap`: 连接的热点名称
  * **`sta.connect(ssid, password)`** : 尝试连接到指定的 SSID 和密码的 AP。此方法不返回是否连接成功的直接结果，但可以通过检查 `sta.status()` 或 `sta.isconnected()` 来获取连接状态。
  * **`sta.ifconfig()`** : 返回 STA 的 IP 配置信息，如 IP 地址、子网掩码、网关和 DNS 服务器等。
  * **`sta.isconnected()`** : 返回`True`，表示已经成功连接到Wi-Fi；返回`False`，表示未连接。
  * **`sta.disconnect()`** : 断开与当前Wi-Fi接入点的连接。



TIP

详细介绍请查看API文档：

[2.2 network 模块 API 手册 — CanMV K230](https://developer.canaan-creative.com/k230_canmv/zh/main/zh/api/extmod/K230_CanMV_network%E6%A8%A1%E5%9D%97API%E6%89%8B%E5%86%8C.html)

## 例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    import network
    import time
    
    SSID = "lckfb"        # 路由器名称
    PASSWORD = "123456781" # 路由器密码
    
    def sta_test():
        # 初始化STA模式（客户端模式）
        sta = network.WLAN(network.STA_IF)
    
        # 激活WiFi模块（相当于打开手机WIFI开关）
        if not sta.active():  # 判断是否已激活
            sta.active(True)
        print("WiFi模块激活状态:", sta.active())
    
        # 查看初始连接状态
        print("初始连接状态:", sta.status())
    
        # 扫描当前环境中的WIFI
        wifi_list = sta.scan()  # 扫描周围WiFi
        # 打印每个Wi-Fi信息
        for wifi in wifi_list:
            # 访问 rt_wlan_info 对象的属性
            ssid = wifi.ssid       # ssid 属性
            rssi = wifi.rssi       # rssi 属性
            print(f"SSID: {ssid}, 信号强度: {rssi}dBm")
    
        # 尝试连接路由器
        print(f"正在连接 {SSID}...")
        sta.connect(SSID, PASSWORD)
    
        # 等待连接结果（最多尝试5次）
        max_wait = 5
        while max_wait > 0:
            if sta.isconnected():  # 检查是否连接成功
                break
            max_wait -= 1
            time.sleep(1)  # 失败了就线休息一秒再说
            sta.connect(SSID, PASSWORD)
            print("剩余等待次数：", max_wait, "次")
    
        # 如果获取不到IP地址就一直在这等待
        while sta.ifconfig()[0] == '0.0.0.0':
            pass
    
        if sta.isconnected():
            print("\n连接成功！")
            # 重新获取并打印网络配置
            ip_info = sta.ifconfig()
            print(f"IP地址: {ip_info[0]}")
            print(f"子网掩码: {ip_info[1]}")
            print(f"网关: {ip_info[2]}")
            print(f"DNS服务器: {ip_info[3]}")
        else:
            print("连接失败，请检查密码或信号强度")
    
    sta_test()
    
    while True:
        # 持续死循环，等待用户打断并退出该循环
        time.sleep(0.5)  # 等待0.5秒

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


上面的代码就是实现了庐山派去连接2.4G WIFI。

导入了`network`库和`time`库，使得下面的代码可以使用网络和时间相关函数，在开始用`SSID`来确定待连接路由器的名称；用`PASSWORD`：确定待连接Wi-Fi所需要的密码，**在连接你自己的路由器或手机热点前一定要记得先修改这里** 。为了方便大家后续将代码方便的用于其他项目，所以独立成一个连接WIFI的函数了，（`sta_test()`），它可以实现对指定路由器的连接和信息打印,会自动尝试连接5次，如果能成功连接WIFI，在获取到IP地址后就会在IDE的串行终端处把各种信息打印出来。

实际运行效果如下图所示： ![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/network/wifi/wifi_20250207_111838.png)

# 3 创建热点（AP模式） ​

**AP模式** （Access Point）：庐山派作为热点供其他设备连接,不过庐山派自己也没有连接如网络，所以其他设备连入庐山派的热点也无法上网，但是可以和庐山派互相通讯。

  * **`network.WLAN(network.AP_IF)`** : 初始化一个 WLAN 对象，并设置为 AP 模式。
  * **`ap.active(bool)`** : 激活或关闭 AP 模式。当传入 `True` 时激活，传入 `False` 时关闭。如果不带参数调用，则返回当前激活状态。
  * **`ap.config(ssid=None, key=None ...)`** : 配置 AP 的参数，如 SSID、密码、频道等。如果不带任何参数调用，则返回当前配置。
  * **`ap.status()`** : 返回 AP 的当前状态。



## 例程 ​

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    import network
    import time
    
    AP_SSID = 'LushanPi-AP'  # 热点名称
    AP_KEY = '123456781'  # 至少8位密码
    
    def ap_test():
        # 初始化AP模式
        ap = network.WLAN(network.AP_IF)
    
        # 激活AP模式
        if not ap.active():
            ap.active(True)
        print("AP模式激活状态:", ap.active())
    
        # 配置热点参数
        ap.config(ssid=AP_SSID,key=AP_KEY)
        print("\n热点已创建:")
        print(f"SSID: {AP_SSID}")
        print(f"Channel: {AP_KEY}")
    
        # 等待热点启动（暂定3秒）
        time.sleep(3)
    
        # 获取并打印IP信息
        ip_info = ap.ifconfig()
        print("\nAP网络配置:")
        print(f"IP地址: {ip_info[0]}")
        print(f"子网掩码: {ip_info[1]}")
        print(f"网关: {ip_info[2]}")
        print(f"DNS服务器: {ip_info[3]}")
    
        # 持续监控连接设备
        while True:
            clients = ap.status('stations')
            print(f"\n已连接设备数: {len(clients)}")
    
            time.sleep(1)
    
    ap_test()

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


运行以上代码后，大家可以打开手机的WIFI，去搜索名为`LushanPi_AP`的热点，输入密码`12345678`进行连接，此时如果手机成功连接庐山派，即可以在IDE的串行终端处看到有一个设备连接了。

![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/network/wifi/wifi_20250207_171106.png)

