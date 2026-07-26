# 有线网络

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/network/eth.html>
> **最后更新**: 2025-02-08

---

# 1 本节介绍 ​

📝本节您将学习如何使用庐山派的TYPE-A接口连接外部USB转以太网进行有线网通讯。

🏆学习目标

1️⃣如何用庐山派的TYPE-A接口连接外部USB转以太网进行有线网通讯。

警告

**芯片兼容性** ：目前支持的USB转以太网工具仅限于内部使用`RTL8152B`芯片的设备。在购买之前，请务必向商家确认其内部芯片型号是否为`RTL8152B`。

**先连接好USB转以太网** ：为了确保网络设备能够正常初始化，请务必在庐山派上电启动之前插入USB转以太网工具。

在开始之前，大家先将【USB转以太网】插入庐山派的TYPE-A口，然后将能连入互联网的网线插入你的USB转以太网工具。如下图所示： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/network/eth/eth_20250208_100044.png)

# 2 API ​

参考文档: [Micropython LAN](https://docs.micropython.org/en/latest/library/network.LAN.html)

此类为有线网络的配置接口。示例代码如下：

python
    
    
    import network
    nic = network.LAN()
    print(nic.ifconfig())
    
    # 配置完成后，即可像往常一样使用 socket
    ...

1  
2  
3  
4  
5  
6  


**构造函数**

  * **class** `network.LAN()`

创建一个有线以太网对象。




**方法**

  * **LAN.active([state])**

`network(rt_smart) not support set active state`

新版本固件不支持此方法，请忽略。

  * **LAN.isconnected()**

返回 `True` 表示已连接到网络，返回 `False` 表示未连接。

  * **LAN.ifconfig([(ip, subnet, gateway, dns)])**

获取或设置 IP 级别的网络接口参数，包括 IP 地址、子网掩码、网关和 DNS 服务器。无参数调用时，返回一个包含上述信息的四元组；如需设置参数，传入包含 IP 地址、子网掩码、网关和 DNS 的四元组。例如：

python
        
        nic.ifconfig(('192.168.0.4', '255.255.255.0', '192.168.0.1', '8.8.8.8'))

1  


如果你的路由器开启了DHCP，请直接使用下方的代码，**不要进行手动设置IP地址** ，DHCP最大的优点就是**自动化配置** 。当开发板连接到支持DHCP的路由器时，会自动获取可用的IP地址，避免了手动设置时造成的IP地址冲突；同时他也会自动配置子网掩码，默认网关，DNS服务器等。

python
        
        nic.ifconfig("dhcp")

1  


  * **LAN.config()**

获取网络接口参数。返回信息如下：`('192.168.0.103', '255.255.255.0', '192.168.0.1', '192.168.0.1')`




# 3 基础例程 ​

python
    
    
    import network
    
    def nic_config():
        # 创建网络接口
        nic = network.LAN()
    
        # 设置DHCP模式（由你的路由器自动分配）
        nic.ifconfig("dhcp")
    
        # 检查连接状态
        if nic.isconnected():
            print("网络配置成功！")
            print("自动获取的配置：", nic.ifconfig())
        else:
            print("连接失败，请检查网线连接")
    
    nic_config()

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


如果硬件连接正确，运行上面代码后即可在IDE中看到IP地址等信息了：

![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/network/eth/eth_20250208_144619.png)

# 4 进阶例程 ​

随着deepseek等AI大模型的风靡，建议大家在学习、工作中多借助AI助手来提升效率。这里的进阶例程就是在deepseek的辅助下完成的，明显比上面的结构更清晰，对错误的处理和提示信息更全面，程序更健壮。通过AI的帮助，不仅能够优化代码逻辑，还能快速定位问题并提供解决方案，极大节省时间和精力。毫无疑问，以后会使用AI绝对是所有工程师必备的能力。

python
    
    
    import network
    import time
    
    def initialize_network_interface():
        """初始化网络接口并返回实例"""
        try:
            nic = network.LAN()  # 创建有线网络接口实例
            print("✅ 网络接口初始化成功")
            return nic
        except Exception as e:
            print(f"❌ 接口初始化失败: {e}")
            return None
    
    def get_network_config(nic):
        """获取并返回格式化后的网络配置"""
        config = nic.ifconfig()  # 获取配置信息（IP, 子网掩码, 网关, DNS）
        return {
            "IP地址": config[0],
            "子网掩码": config[1],
            "网关": config[2],
            "DNS服务器": config[3]
        }
    
    def print_network_config(config):
        """打印网络配置"""
        print("\n" + "="*40)
        print("📶 当前网络配置：")
        for key, value in config.items():
            print(f"{key:>10}：{value}")
        print("="*40 + "\n")
    
    def check_connection(nic, timeout=10):
        """检查网络连接状态，带有超时机制"""
        print("🔍 正在检测网络连接...")
        start_time = time.time()
    
        while time.time() - start_time < timeout:
            if nic.isconnected():
                print("✅ 网络连接正常")
                return True
            print("⏳ 等待连接...")
            time.sleep(1)
    
        print("❌ 连接超时")
        return False
    
    def nic_config():
        # 初始化网络接口
        nic = initialize_network_interface()
        if not nic:
            return
    
        try:
            # 设置DHCP模式
            nic.ifconfig("dhcp")  # 设置为自动获取IP地址
            print("🔄 已启用DHCP自动配置")
    
            # 检查网络连接
            if check_connection(nic):
                # 获取并打印配置信息
                config = get_network_config(nic)
                print_network_config(config)
            else:
                print("⚠️ 请检查：\n1. 网线是否连接\n2. 路由器是否工作\n3. DHCP服务是否启用")
    
        except Exception as e:
            print(f"❌ 发生异常: {e}")
    
    if __name__ == "__main__":
        nic_config()

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


在IDE中运行结果如下，还使用emoji图标来更清晰的打印出信息。

![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/network/eth/eth_20250208_150753.png)

