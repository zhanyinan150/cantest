# 3.1寸屏幕扩展板

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/open-source-hardware/lckfb-mipi-3.1inch-screen.html>
> **最后更新**: 2025-02-28

---

# 原理图介绍 ​

本屏幕扩展板自带3.1寸屏幕和板载背光芯片，为了降低大家学习成本，尽量物尽其用。设计上同时支持【泰山派】和【庐山派】，原理图设计如下： ![图 5](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/open-source-hardware/lckfb-mipi-3.1inch-screen/lckfb-mipi-3.1inch-screen_20250228_162417.png)

实物图标注如下： ![图 6](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/open-source-hardware/lckfb-mipi-3.1inch-screen/lckfb-mipi-3.1inch-screen_20250228_162600.png)

3.1寸屏幕参数如下：

[屏幕购买链接](https://item.taobao.com/item.htm?abbucket=3&id=602291522189&ns=1&priceTId=2147835a17407102895104226ebdef&skuId=5902231318748&spm=a21n57.1.hoverItem.2&utparam=%7B%22aplus_abtest%22%3A%22adebf0a48f4af58adfa92232c0af2a3b%22%7D&xxc=taobaoSearch)

![图 7](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/open-source-hardware/lckfb-mipi-3.1inch-screen/lckfb-mipi-3.1inch-screen_20250228_163233.png)

结合原理图，左上角的【泰山派MIPI接口】部分就是3.1寸扩展板通过**31p MIPI线** （用嘉立创FPC生产的）连接庐山派或泰山派。

接下来的【麦克风&喇叭】中，上面是板载的麦克风和POGO PIN（MIC2和MIC1），下面是驱动喇叭的POGO PIN。图中右下角的【SPK】是泰山派通过那两个POGO PIN，连接外部喇叭的接口。

【泰山派触摸接口】是3.1寸屏幕扩展板通过6P 触摸线（用嘉立创FPC生产的）连接庐山派或泰山派。【3.1寸触摸】和【3.1寸MIPI】是扩展板连接3.1寸屏幕的接口。

【背光选择】有R1-R4，四个电阻，生产中默认不贴R1和R2，贴R3和R4。R3和R4贴片后就将3.1寸屏幕的背光电路连接到了扩展板的板载背光芯片（U5-SY7201ABC）的输出上，如果用户自行将R3和R4处的电阻移动到R1和R2上，则3.1寸屏幕的驱动电流由庐山派或泰山派上面板载的背光电路通过FPC传递过来。

【板子背光调节】这个其实就是一个IIC转PWM的芯片，这个PWM通道接入到扩展板的板载背光驱动芯片（SY7201ABC）的使能脚，这个脚默认被上拉，所以果没有进行I2C配置，屏幕背光是会上电就会亮，如果需要控制屏幕的亮度，只需要控制这个PWM的占空比就行了。

【固定孔】就是那三个M2.5的贴片螺母了，至于为什么不是四个，主要是因为3.1寸屏幕的FPC座子把哪个固定孔的位置占了，导致放不下，所以就只能由三个贴片螺母了。

# 重点问题答疑 ​

❓为什么庐山派和泰山派本身板子上就自带了背光驱动，还要在3.1寸屏幕上加一个背光驱动，还得再来一个IIC转PWM来驱动这个背光驱动芯片？

泰山派的MIPI接口在设计之初，主要定位是用来驱动大屏幕的，所以原电路预设的屏幕背光驱动电流是111mA，所以庐山派为了兼容泰山派，沿用了同样的背光驱动电路。但是当前我们这个扩展板适配的屏幕是3.1寸的，很小，111mA的驱动电流会导致屏幕发热严重，时间长了有可能会烧毁。

这个时候就只有两条路可选了，一条是直接让用户自行更换开发板上板载背光驱动电路的反馈电阻，但是这样一是难度有点高，不方便，二是会破坏开发板原本的兼容性。另外一条就是通过外接扩展板新增一个可调节的小电流背光电路。为了保持优雅，我们选择了后者。

借用触摸的IIC通道，先利用一个I2C转PWM芯片（GP7101），然后将这个PWM通道接入到扩展板的板载背光驱动芯片（SY7201ABC）的使能脚，该脚默认被R7（100kΩ）的电阻上拉，所以如果没有进行I2C配置，屏幕背光是会上电即亮的。

简图如下：

jsx
    
    
    [庐山派或泰山派主板]
        │
        ├── 原背光电路（默认驱动电流配置为111mA，不做修改）
        │
        └── I2C接口（触摸那条线）
             │
             ▼
    [扩展板]
        │
        ├── I2C转PWM芯片（生成PWM波，用来调节屏幕的亮度）
        │
        └── 扩展板板载背光电路（默认驱动电流配置为20mA）

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


* * *

❓扩展板的POGO PIN（弹簧探针）对不上庐山派的焊盘啊，是不是设计错了？

首先我们打开泰山派和庐山派的开源工程，对比一下他们的pogo pin 焊盘位置，链接如下：

泰山派：<https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-tai-shan-pai-kai-fa-ban>

庐山派：<https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-lu-shan-pai-k230-canmv-kai-fa-ban>

页面打开后，记得单击打开设计图按钮来打开工程，我们用3D预览图来对比一下他们俩焊盘的间距。如下图所示： ![图 9](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/open-source-hardware/lckfb-mipi-3.1inch-screen/lckfb-mipi-3.1inch-screen_20250228_163614.png)

可以看到，扩展板上引出来的三个pogo pin ，在设计上就和庐山派的触电进行了错位，如果安装正确，泰山派和庐山派的扩展板是不会互相干涉的。需要注意的是，我们板子的固定孔是按照M3的螺栓来设计的，其内部直径为**Φ3.2mm，** 而屏幕扩展板的的贴片螺母为按照M2.5设计的（因为M3的贴片螺母放不下）。这就导致M2.5的螺栓在拧的时候会有虚位，导致会有错位，所以大家在安装时，在拧紧前先对好位置。

