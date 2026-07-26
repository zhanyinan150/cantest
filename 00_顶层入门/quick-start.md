# 立创·庐山派K230 / Lite-K230D CanMV快速上手

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/quick-start.html>
> **最后更新**: 2026-07-17

---

## 一 本节介绍 ​

### 1.1 开发板简介 ​

本章适用开发板

本章同时适用于以下两款开发板：

立创·庐山派K230-CanMV开发板| 立创·庐山派Lite-K230D-CanMV开发板  
---|---  
![立创·庐山派K230-CanMV开发板](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260605_172507.png)| ![立创·庐山派Lite-K230D-CanMV开发板](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260605_172519.png)  
  
本章只讲快速入门流程：准备开发环境、获取对应固件、将固件写入 TF 卡、启动开发板，并通过 Visual Studio Code + CanMV 扩展或立创开发板 CanMV IDE Web 连接开发板、运行示例程序。两款板子的固件镜像不同，【下载和烧录时一定要选择与自己开发板完全匹配的固件】，如果烧错固件有可能导致硬件损坏。

该开发板是立创开发板团队精选打造的一款超高性价比的AI 视觉开发板。以嘉楠科技Kendryte®系列AIoT芯片中的最新一代SoC芯片K230为主控芯片，支持三路摄像头同时输入，典型网络下的推理能力可达K210的13.7倍（算力约为6TOPS）。支持**CanMV** ，可以直接使用Python 编程，简单易用。可作为**AI与边缘计算平台** ，适合物联网；智能家居与消费电子；工业自动化；无人系统等领域的开发者使用。

下面是两款开发板的硬件资源标注图。

立创·庐山派K230-CanMV开发板资源标注| 立创·庐山派Lite-K230D-CanMV开发板资源标注  
---|---  
![立创·庐山派K230-CanMV开发板资源标注图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/open-source-hardware/profile/profile_20241128_213845.jpg)| ![立创·庐山派Lite-K230D-CanMV开发板资源标注图](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260716_094410.png)  
  
### 1.2 K230 与 K230D 的主要区别 ​

**K230 和 K230D 的核心计算性能几乎没有区别** 。它们的 CPU、KPU 等核心计算单元属于同一平台，主要差异不在[算力]，而在 **内存集成方式、封装尺寸、USB / ADC / 引脚引出数量，以及开发板外设配置** 。庐山派 Lite-K230D 【不是】性能低一档的板子，只不过其所用的主控K230D有更小的封装、更高集成度。

从使用 CanMV MicroPython固件的角度看，两款开发板的入门流程是一样的，但固件镜像、板载外设和可用资源不能混在一起看。快速上手阶段大家先记住一句话：**性能基本一致，固件不能混用，资源能力要区分。**

对比项| 立创·庐山派K230-CanMV开发板| 立创·庐山派Lite-K230D-CanMV开发板| 对快速上手的影响  
---|---|---|---  
核心性能| K230 平台，CPU / KPU 核心能力与 K230D 基本一致| K230D 平台，CPU / KPU 核心能力与 K230 基本一致| 不要简单理解为 Lite-K230D 算力更弱，实际差异更多体现在内存容量、外设和引脚资源  
内存| 板卡使用 1GB 外置内存；K230 芯片支持外置 LPDDR3 / LPDDR4，最大支持2G内存| K230D 内置 128MB LPDDR4| 算力接近，但 Lite-K230D 内存更小，运行大模型或多模型时更容易受限制  
USB 资源| K230 芯片 USB 资源更多，庐山派K230板载 USB-A HOST| K230D USB 资源更少，庐山派Lite-K230D无 USB HOST| 本章 Type-C 连接和运行例程流程一致，但 USB HOST 相关例程不能直接照搬到 Lite-K230D  
ADC 资源| K230 最多 6 路 ADC（实际庐山派只引出了4路）| K230D 最多 3 路 ADC（实际庐山派Lite也只引出了3路）| 后续 ADC 教程中要注意可用通道数量不同  
封装与引脚| K230 封装更大，引脚资源更充足| K230D 封装更小，部分引脚没有引出| 庐山派Lite-K230D 外接模块时更要注意 IO 复用和资源占用  
音频与扩展| 标准版 K230 有 3.5mm 耳机口，无板载风扇接口| Lite-K230D 有 HT6872 功放和 PWM 风扇接口| 音频、蜂鸣器、风扇等教程会按板卡硬件能力分别说明  
  
NOTE

嘉楠官方问答中提到，K230D 主要是内置 128MB DDR，并且部分引脚没有引出；其他方面没有明显差别。参考：[K230和K230D有哪些区别？](https://www.kendryte.com/answer/questions/10010000000002293)

### 1.3 K230芯片简介 ​

K230芯片集成了两颗RISC-V处理器核心，双核玄铁C908，12nm 制程工艺，主频高达1.6GHz，是全球首款支持RISC-V Vector 1.0标准的商用SOC，配备第三代KPU处理单元，专为图像、视频、音频处理和AI加速设计，提供强劲的本地AI推理能力。支持三路MIPI CSI视频输入，最大分辨率可达4K。K230支持常见的AI计算框架如TensorFlow和PyTorch。下面是该处理器的框架图：

![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20241011_171509.png)

## 二 开发前准备 ​

序号| 名称| 数量| 说明  
---|---|---|---  
1| 立创·庐山派K230-CanMV开发板 或 立创·庐山派Lite-K230D-CanMV开发板| 1片| 两款开发板均适用本章，固件需要分别选择对应版本  
2| GC2093摄像头| 1块| 庐山派系列的标准套件都会提供  
3| [TF 卡](https://item.szlcsc.com/8422936.html)（2GB 以上，Class 10 等级及以上）| 1片| 建议使用质量稳定的 TF 卡，4GB/8GB 都可以  
4| Type-C 数据线| 1条| 用于给开发板供电并连接电脑  
5| 电脑| 1台| 支持 Windows、macOS 或 Linux；使用网页版时需要 Chrome、Edge 或其他支持 Web Serial API 的 Chromium 内核浏览器  
  
TIP

标准版 **立创·庐山派K230-CanMV开发板** 和 **立创·庐山派Lite-K230D-CanMV开发板** 都可以按照本章流程完成快速上手。摄像头型号以实际套件为准。

TF 卡建议优先选择质量稳定、速度达标的品牌卡。我们目前测试常用的是立创商城的 MK（米客方德）8GB TF 卡（SDSDQAB-008G-MK），购买链接：<https://item.szlcsc.com/mro/58293428.html>

本章只是快速上手，4GB 容量通常已经够用。如果后续要保存较多图片、视频或模型文件，可以再选择更大容量的 TF 卡。

### 2.1 选择 CanMV 开发环境 ​

庐山派目前推荐使用下面两种 CanMV 开发环境。两者都支持立创·庐山派K230-CanMV开发板和立创·庐山派Lite-K230D-CanMV开发板，都可以完成脚本编辑、开发板连接、程序运行、终端输出、图像预览和设备文件管理。

推荐顺序| 开发环境| 适用场景| 主要特点  
---|---|---|---  
**第一推荐**|  Visual Studio Code + CanMV 扩展| 长期开发、正式项目、需要完善代码提示与工程管理| 嘉楠官方推荐；示例和 MicroPython stubs 可与固件匹配；Pylance 提供代码补全和类型提示  
**第二推荐**|  立创开发板 CanMV IDE Web| 第一次体验、临时调试、课堂教学、换电脑快速使用| 打开网页即可使用；支持 AI 助手、浏览器内文件管理和固件烧录  
  
如何选择？

如果各位准备长期学习或开发项目，优先选择 **Visual Studio Code + CanMV 扩展** 。如果暂时不想安装软件，或者需要快速验证开发板，可以选择立创开发板部门制作的 **CanMV IDE Web** 。两套环境运行的都是 CanMV MicroPython 脚本，后续可以随时切换。

IMPORTANT

同一时间只能让一个开发环境占用开发板串口。切换 Visual Studio Code、CanMV IDE Web 或串口工具前，请先在当前工具中断开连接并关闭可能占用串口的页面或软件。

### 2.2 第一推荐：安装 Visual Studio Code 和 CanMV 扩展 ​

嘉楠官方推荐使用 **CanMV for Visual Studio Code** 扩展进行 CanMV K230 开发。它把开发板连接、MicroPython 脚本运行、终端输出、图像预览、设备文件管理和示例浏览等功能集成到了 Visual Studio Code 中。

安装步骤如下：

  1. 安装 [Visual Studio Code](https://code.visualstudio.com/Download) `1.90.0` 或更高版本。
  2. 打开 Visual Studio Code 左侧的“扩展”视图，搜索 `CanMV`。
  3. 找到 CanMV 扩展后点击“安装”。扩展依赖的 Pylance 会自动安装，用于 Python 代码补全和类型提示。
  4. 如果使用离线 VSIX 安装包，可以打开命令面板，运行 `Extensions: Install from VSIX...`，然后选择对应的 VSIX 文件。



![图 19](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260716_103210.png)

关闭可能占用串口的软件

使用 CanMV 扩展连接开发板前，请关闭其他可能占用开发板串口的软件。特别是从旧工具迁移过来的用户，不要让两个开发工具同时连接同一个串口，否则 CanMV 扩展可能无法连接开发板。

更完整的功能和迁移说明请参考嘉楠官方文档：[使用 CanMV Visual Studio Code 扩展](https://www.kendryte.com/k230_canmv/zh/main/userguide/how_to_use_ide.html)。

### 2.3 第二推荐：打开立创开发板 CanMV IDE Web ​

**CanMV IDE Web** 是立创开发板部门制作并部署在立创开发板 Wiki 服务器上的网页版开发环境，不需要安装桌面软件，使用支持网页串口（Web Serial API）的浏览器打开即可使用。

使用前请确认：

  1. 使用最新版 Chrome、Edge 或其他支持 Web Serial API 的 Chromium 内核浏览器，建议版本不低于 `86`。
  2. 使用 `https://` 地址访问在线 IDE；Firefox 和 Safari 目前不支持 Web Serial API，无法连接开发板。
  3. 正常运行脚本时，庐山派使用 USB CDC 虚拟串口。Windows 10/11 通常可以直接识别为“USB 串行设备（COMx）”，不需要安装 CH340 等串口驱动。



在线 IDE 入口：<https://wiki.lckfb.com/storage/html/canmv-web-ide/>

![CanMV IDE Web 主界面](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/canmv-ide-web-doc/canmv-ide-web-doc_20260703_152956.png)

网页版还提供 AI 助手、PWA 桌面安装、浏览器内固件烧录等功能。完整说明请阅读[《CanMV IDE Web 在线使用教程》](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/canmv-ide-web-doc.html)。

网页运行免装串口驱动，不等于网页烧录始终免驱动

开发板正常启动后，通过网页串口连接和运行脚本时，Windows 10/11 通常无需额外驱动；如果使用网页 IDE 的固件烧录功能，让开发板进入 BootROM 模式后，Windows 首次使用仍可能需要通过 Zadig 为 `K230 USB Boot Device` 安装 WinUSB 驱动。

### 2.4 先说清楚：烧录固件和运行脚本不是一回事 ​

庐山派使用的是 CanMV MicroPython 环境。MicroPython 是解释性语言，我们写的 `.py` 文件不是像 STM32 C 工程那样编译成固件再烧录进芯片，而是由开发板里的 MicroPython 解释器读取并执行。

所以这里先把说法统一一下：

操作| 推荐叫法| 说明  
---|---|---  
将 `.img` 固件镜像写入 TF 卡| 烧录固件 / 写入固件镜像| 这是把系统固件写入存储介质  
在任一种 CanMV 开发环境中运行 `.py` 文件| 运行脚本 / 执行程序| 这是让 MicroPython 解释器执行脚本，不是烧录程序  
将 `.py` 文件复制到开发板文件系统| 拷贝脚本 / 保存脚本| 只是文件传输，不是烧录  
  
IMPORTANT

后面我们说【烧录】时，指的是把 `.img` 固件镜像写入 TF 卡；运行官方例程时，请准确理解为【运行脚本】或【执行程序】。

## 三 获取及烧录固件 ​

### 3.1 获取固件 ​

#### 3.1.1 双板固件选择说明 ​

两款开发板的快速上手流程一致，但固件镜像不能混用。下载固件时请先确认自己手上的板卡型号。

![图 20](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260716_121419.png) 这里的图片所展示的版本号是写本文时最新的，大家在实际使用时注意选择用最新的固件即可。

开发板| 固件选择要求| 说明  
---|---|---  
立创·庐山派K230-CanMV开发板| 选择 K230 LCKFB 对应的 CanMV MicroPython 固件| 不要选择 Linux 镜像，也不要选择其他厂商板卡镜像  
立创·庐山派Lite-K230D-CanMV开发板| 选择 Lite-K230D LCKFB 对应的 CanMV MicroPython 固件| 文件名中会体现 Lite / K230D 板型信息，以下载页实际名称为准  
  
DANGER

固件镜像必须和开发板型号完全匹配。烧错固件可能导致系统无法启动，严重时可能造成硬件异常，请不要混烧其他板型或其他系统镜像。

固件获取链接

推荐从[嘉楠开发者社区](https://developer.canaan-creative.com/resource?selected=0-0-3-0)获取最新固件。进入页面后，在 `资料下载` 栏目中依次选择 `K230` -> `Images` -> `CanMV` -> `Micropython`，然后下载与自己开发板匹配、并且文件名带有 `LCKFB` 标识的 CanMV MicroPython 固件。

下载时可以按下面的原则选择版本：

  1. **Daily Build** ：每天自动构建，版本较新。如果你遇到 IDE 连接异常、例程运行失败、外设接口不兼容等问题，可以优先尝试更新到这个版本。
  2. **稳定版本** ：适合正常学习和课堂教学。如果当前教程可以顺利运行，使用稳定版本即可。下图标出来的是[立创·庐山派K230-CanMV开发板]的固件。



![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260605_151340.png)

不建议从旧资料包或不明确来源下载固件。也不要下载 Linux 镜像、其他厂商开发板镜像，或者文件名中没有 `LCKFB` 标识的镜像。上图这里展示的是[立创·庐山派K230-CanMV开发板]的固件，写本文时，如果你用的是[立创·庐山派Lite-K230D-CanMV开发板]，请选择这个开头的固件【CanMV_K230D_Lushanpi_Lite...】

IMPORTANT

固件下载完成后，在烧录前请再检查一次文件名和开发板型号是否匹配。不要因为都是 K230 系列，就把其他板型的镜像写入庐山派开发板。

需要注意，下载下来的文件是压缩包，解压出来的 `.img` 文件才是固件镜像。

⚠️警告：

**.gz** 后缀结尾的是压缩包，**.img** 后缀结尾的才是固件镜像。往 **TF 卡** 中写入固件时，只能选择 **.img 文件** 。如果直接写入 **.gz 文件** ，开发板可能无法正常启动。

如下图所示，`.gz` 是压缩包，解压后得到的 `.img` 才是后续需要写入 TF 卡的固件镜像。

![图 6](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20241011_171749.jpg)

### 3.2 烧录固件 ​

本节提供两种推荐的固件烧录方式。它们最终都是通过开发板的 Type-C 接口把 `.img` 固件镜像写入 TF 卡，选择其中一种即可，不需要重复操作。

推荐顺序| 烧录方式| 适用环境| 推荐场景  
---|---|---|---  
**第一推荐**|  嘉楠 K230BurningTool| Windows| 使用嘉楠官方桌面烧录工具，通过开发板 USB 接口稳定烧录  
**第二推荐**|  立创开发板 CanMV IDE Web| Chrome、Edge 等 Chromium 浏览器| 不想安装烧录软件，希望直接在网页中完成烧录  
  
IMPORTANT

无论使用哪种方式，写入固件都会覆盖 TF 卡中的原有数据。开始前请备份重要文件，并再次确认 `.img` 固件与当前开发板型号完全匹配。

#### 3.2.1 第一推荐：使用嘉楠 K230BurningTool 烧录 ​

首推使用嘉楠官方提供的 **K230BurningTool** ，通过开发板的 Type-C 接口直接把固件写入 TF 卡。该工具的操作步骤明确，适合 Windows 用户首次烧录和后续更新固件。

为了方便演示，本小节以 **立创·庐山派Lite-K230D-CanMV开发板** 为例。标准版 **立创·庐山派K230-CanMV开发板** 也可以使用类似流程，只是下载固件时一定要选择对应板型的镜像。

DANGER

固件镜像只能写入对应的开发板。请务必选择与当前板型完全匹配的 CanMV MicroPython 镜像，不能把 K230、Lite-K230D、Linux 镜像或其他开发板镜像混用，否则可能导致系统无法启动，甚至造成硬件异常。

**第一步：下载并解压固件**

我们先进入 CanMV MicroPython 的每日构建目录，下载适合自己开发板的最新固件。每日构建链接为：[canmv_k230_micropython daily_build](https://kendryte-download.canaan-creative.com/developer/releases/canmv_k230_micropython/daily_build/)。

![图 7](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_142942.png)

下载完成后，先把压缩包解压出来。后面烧录工具需要选择的是解压后的 `.img` 固件镜像，不是 `.gz` 压缩包。

![图 9](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_144215.png)

解压后建议记一下固件所在目录，后面在 K230BurningTool 中选择镜像时需要找到这个文件。

**第二步：让开发板进入 BOOT 模式**

使用官方烧录工具时，需要先让开发板进入 BOOT 模式。这里按照下图的操作方式来做：

  1. 先用 Type-C 数据线把开发板连接到电脑。
  2. 按住开发板上的 **BOOT** 按键不要松开。
  3. 在保持按住 **BOOT** 的同时，短按一下 **RESET** 按键。
  4. 电脑识别到 **K230 USB Boot Device** 后，再松开 **BOOT** 按键。



下图是进入 BOOT 模式的操作示意：

![图 10](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_160856.png)

> 上图由 AI 生成，仅用于说明 BOOT 模式的操作顺序。庐山派K230 和 庐山派Lite-K230D 都有 BOOT 按键，操作思路一致，实际按键位置请以手上的开发板为准。

**第三步：确认电脑是否识别到 BOOT 设备**

开发板进入 BOOT 模式后，可以打开 Windows 设备管理器查看设备状态。如果没有安装驱动，通常会看到一个带感叹号的 **K230 USB Boot Device** 。

![图 6](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_141237.png)

如果出现这种情况，需要先安装 WinUSB 驱动。推荐使用 Zadig 安装：

  1. 下载并打开 Zadig：<https://zadig.akeo.ie/>。
  2. 点击 `Options` -> `List All Devices`，显示全部 USB 设备。
  3. 在设备列表中选择 `K230 USB Boot Device`。
  4. 在 `Driver` 选项中选择 `WinUSB`。
  5. 点击 `Install Driver`，等待安装完成。



如果不熟悉 Zadig，也可以参考这个操作视频：[Zadig 驱动安装视频](https://www.bilibili.com/video/BV1S2QqYoE7r/)。

驱动安装完成后，再次查看设备管理器，应能看到没有感叹号的 **K230 USB Boot Device** 。

![图 11](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_162151.png)

**第四步：下载并打开 K230BurningTool**

嘉楠官方提供了 [K230BurningTool](https://www.kendryte.com/zh/resource/download_tool,k230) 烧录工具，可以用于写入 K230 系列固件。下载工具后先解压。

![图 12](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_162820.png)

解压后找到 `K230BurningTool.exe`，双击打开。

![图 13](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_163235.png)

**第五步：选择固件镜像**

打开工具后，先点击固件路径相关按钮，选择前面已经解压出来的 `.img` 固件镜像。

![图 14](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_163430.png)

在文件选择窗口中找到对应的固件镜像。这里一定要再次确认文件名和开发板型号匹配，例如标准版 K230 选择 K230 对应固件，Lite-K230D 选择 Lite-K230D 对应固件。

![图 15](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_163519.png)

**第六步：选择烧录介质并开始烧录**

确认固件镜像无误后，把目标介质修改为 `SD Card`，然后点击右侧的“开始”按钮。烧录过程中不要断电，也不要拔掉 Type-C 数据线或 TF 卡。

等待进度条走到 100%，并提示下载完成后，就表示固件已经写入完成。此时可以断电重启开发板，进入后续启动和连接步骤。

![图 16](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_163721.png)

如果工具识别不到设备，优先按下面顺序排查：

  1. Type-C 数据线是否支持数据传输，不要使用只能充电的数据线。
  2. 开发板是否确实进入 BOOT 模式。
  3. 设备管理器中是否能看到 `K230 USB Boot Device`。
  4. Zadig 驱动是否已经安装为 `WinUSB`。
  5. 固件镜像是否已经解压为 `.img` 文件。



更详细的避坑说明可以参考：[USB 烧录全攻略](https://www.kendryte.com/answer/questions/10010000000008370)。

#### 3.2.2 第二推荐：使用立创开发板 CanMV IDE Web 烧录 ​

立创开发板部门制作的 **CanMV IDE Web** 也支持通过开发板的 Type-C 接口把固件写入 TF 卡。这种方式不需要安装 K230BurningTool，使用支持相关浏览器设备接口的 Chrome、Edge 等 Chromium 内核浏览器即可操作。

在线 IDE 入口：<https://wiki.lckfb.com/storage/html/canmv-web-ide/>

开始前请准备：

  1. 将 2GB 以上、Class 10 等级及以上的 TF 卡插入开发板。
  2. 使用支持数据传输的 Type-C 数据线连接开发板和电脑。
  3. 准备好已经解压的 `.img` 固件镜像，不要直接选择 `.gz` 压缩包。
  4. Windows 用户首次使用时，如果 `K230 USB Boot Device` 带有感叹号，需要先通过 [Zadig](https://zadig.akeo.ie/) 安装 WinUSB 驱动。



⚠️注意！固件必须与板型匹配

  * 立创·庐山派K230-CanMV开发板：选择 `CanMV_K230_LCKFB_*.img`。
  * 立创·庐山派Lite-K230D-CanMV开发板：选择 `CanMV_K230D_Lushanpi_Lite_*.img`。



网页 IDE 会根据所选板型和固件文件名进行检查，并在发现明显不匹配时给出红色警告。但文件名检查不能代替人工确认，点击开始烧录前仍需核对板型、文件名和镜像来源。

烧录步骤如下：

  1. 打开 CanMV IDE Web，点击“连接设备”旁边的“烧录固件”按钮。
  2. 按照向导确认 TF 卡和数据线已经连接，然后选择当前开发板型号。
  3. 点击“选择文件”，选择前面解压得到的 `.img` 固件镜像。
  4. 按住开发板上的 **BOOT** 按键不要松开，短按一次 **RESET** ，然后松开 **BOOT** ，让开发板进入 BootROM 烧录模式。
  5. 点击“开始烧录”。浏览器弹出设备授权窗口后，选择 `K230 USB Boot Device`。
  6. 按页面提示等待烧录。烧录采用两阶段协议，中途开发板会重启并暂时断开；如果页面提示重新选择设备，请再次授权对应的 K230 设备继续烧录。
  7. 等待进度达到 100% 并出现烧录完成提示。开发板会自动重启，首次启动时还会初始化 TF 卡剩余空间，请继续等待 10～30 秒，不要立即断电或拔卡。



![CanMV IDE Web 烧录固件](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/canmv-ide-web-doc/canmv-ide-web-doc_20260703_160725.gif)

烧录过程中常见问题：

  * **浏览器没有弹出设备窗口** ：点击页面中的“选择设备”，重新触发浏览器设备授权。
  * **找不到设备或设备带感叹号** ：确认已经通过 BOOT + RESET 进入 BootROM，并检查 WinUSB 驱动。
  * **卡在“等待阶段 2”** ：开发板重启后需要浏览器重新授权，按页面提示再次选择设备。
  * **中途提示设备断开** ：两阶段烧录过程中设备会正常重启，不要关闭页面，按提示重新连接即可。



更完整的界面说明和排障步骤请参考[《CanMV IDE Web 在线使用教程》](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/canmv-ide-web-doc.html#九-烧录固件到开发板-可选)。

## 四 启动开发板并连接开发环境 ​

完成以上操作后，我们就可以给开发板上电了。请先确认 TF 卡已经插入开发板，然后用 Type-C 数据线将开发板连接到电脑。

如果正常启动，板子上的状态指示灯会点亮。稍微等待一会后，我们就可以在设备管理器中看到一个新的 **USB 串行设备（COMx）** ，同时电脑上也会出现一个 **CanMV 设备** ，可以像访问 U 盘一样查看开发板中的文件。

![图 10](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20241011_171916.png)

⚠️注意：

在第一次上电时，庐山派开发板会自动将TF 卡除固件镜像外的剩余空间格式化为`fat`，并挂载在`/data`文件夹，格式化会占用一些时间并且会主动进行一次重启。后续上电时不会再进行这个操作，所以第一次上电会稍微慢一点。

开发板启动完成后，可以根据第二章的选择，通过下面任一条路线连接。两条路线完成的是同一件事，不需要同时操作。

### 4.1 路线一：使用 Visual Studio Code + CanMV 扩展连接 ​

打开 Visual Studio Code，通过下面任一种方式连接：

  1. 按 `Ctrl+Shift+P` 打开命令面板，运行 `CanMV: 连接开发板`。
  2. 点击左侧活动栏中的 CanMV 图标，在 `Controls` 视图中点击连接按钮。



CanMV 扩展默认会通过 USB VID/PID `1209:abd1` 自动检测开发板。连接成功后，CanMV 活动栏会显示开发板状态，`Device` 视图会刷新设备文件，底部的 `CanMV Terminal` 面板也会显示开发板输出。

![图 22](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260717_135348.png)

如果自动检测失败，请先在 Windows 设备管理器中确认是否出现 USB 串行设备（COMx），再打开 Visual Studio Code 设置，手动将 `canmv.serialPath` 配置为开发板对应的串口。Linux 常见路径为 `/dev/ttyACM0`，macOS 常见路径为 `/dev/cu.usbmodem*`；手动指定后，扩展会使用 `canmv.baudRate` 设置的波特率连接。

### 4.2 路线二：使用 CanMV IDE Web 连接 ​

  1. 使用 Chrome 或 Edge 打开 [CanMV IDE Web](https://wiki.lckfb.com/storage/html/canmv-web-ide/)。
  2. 点击界面左上角的“连接设备”。
  3. 浏览器弹出串口选择框后，选择开发板对应的“USB 串行设备（COMx）”，再点击“连接”。每台电脑的 COM 号可能不同，请以实际识别结果为准。
  4. 等待在线 IDE 与开发板完成通信。连接成功后，终端会输出开发板信息，设备文件区也会开始显示可访问的目录。



![CanMV IDE Web 连接开发板](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/canmv-ide-web-doc/canmv-ide-web-doc_20260703_153151.gif)

浏览器串口授权

浏览器只有在用户主动点击“连接设备”后，才会显示串口授权窗口。网页不能绕过浏览器直接访问串口，这是 Web Serial API 的安全机制。不要在多个 CanMV IDE Web 标签页中同时连接同一块开发板。

如果串口选择框中没有设备，请确认浏览器支持 Web Serial API、网址以 `https://` 开头，并关闭 Visual Studio Code、串口助手或其他可能占用串口的软件。

## 五 运行 CanMV 示例程序 ​

写入固件后的 TF 卡中会带有嘉楠官方提供的例程，**源码、模型文件、字体文件** 等都在 TF 卡中。开发板连接成功后，可以在 Visual Studio Code 的 `Device` 视图或 CanMV IDE Web 的“设备文件”区域中浏览 `/sdcard`、`/data`、`/udisk`(在K230上面插入了U盘才能用)） 等目录，也可以在电脑的 CanMV 设备中进入 `sdcard` 文件夹查看这些文件。

这里再次强调一下：接下来无论使用 Visual Studio Code 还是 CanMV IDE Web，执行 `.py` 文件都叫**运行脚本** ，不是烧录程序。MicroPython 会读取并执行脚本，运行按钮只是把当前代码发送给开发板执行。

![图 23](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260717_135540.png)

⚠️注意：

  1. Visual Studio Code 用户推荐使用 CanMV 扩展 `Examples` 视图中与当前固件资源匹配的官方示例；网页版用户可以使用左侧“示例”中最新的庐山派示例，也可以使用固件自带例程。不要继续使用来源不明或版本过旧的旧 IDE 示例。
  2. 摄像头相关例程通常默认使用 **CSI2** ，也就是开发板正面的立式 22P 摄像头座。运行摄像头例程前，请确认摄像头排线方向和接口位置。
  3. 如果从开发板文件系统获取例程，建议先将例程下载到电脑本地再运行，避免边读取设备文件边传输图像造成额外的 USB 传输压力。
  4. Lite-K230D 的内存比标准版 K230 小，运行 AI Demo 时建议优先选择较轻量的例程。若出现内存不足、启动慢或画面卡顿，可以先换基础摄像头/显示例程确认环境是否正常。具体能运行哪些AI模型，可以看[这里](https://www.kendryte.com/k230_canmv/zh/main/ai_dev_doc.html#k230-ai)的区别。



其例程`examples`,`libs`,`res`目录结构如表所示：

Details
    
    
    sdcard:
    ├─examples
    │  ├─01-Micropython-Basics
    │  │      demo_crc16.py
    │  │      demo_files.py
    │  │      demo_fs_info.py
    │  │      demo_globals.py
    │  │      demo_json.py
    │  │      demo_logging.py
    │  │      demo_sha256.py
    │  │      demo_sys_info.py
    │  │      demo_thread.py
    │  │      demo_time.py
    │  │      demo_view_mem.py
    │  │      demo_yield.py
    │  │      demo_yield_task.py
    │  │
    │  ├─02-Media
    │  │      acodec.py
    │  │      audio.py
    │  │      mp4muxer.py
    │  │      rtsp_server.py
    │  │      video_decoder.py
    │  │      video_encoder.py
    │  │      video_player.py
    │  │
    │  ├─03-Machine
    │  │  ├─adc
    │  │  │      adc.py
    │  │  │
    │  │  ├─fft
    │  │  │      fft.py
    │  │  │
    │  │  ├─fpioa
    │  │  │      fpioa.py
    │  │  │
    │  │  ├─i2c
    │  │  │      i2c_master.py
    │  │  │      i2c_slave.py
    │  │  │
    │  │  ├─pin
    │  │  │      pin.py
    │  │  │
    │  │  ├─pwm
    │  │  │      pwm.py
    │  │  │
    │  │  ├─rtc
    │  │  │      rtc.py
    │  │  │
    │  │  ├─spi
    │  │  │      spi.py
    │  │  │
    │  │  ├─timer
    │  │  │      timer.py
    │  │  │
    │  │  ├─touch
    │  │  │      touch.py
    │  │  │
    │  │  ├─uart
    │  │  │      uart.py
    │  │  │      uart1.py
    │  │  │
    │  │  └─wdt
    │  │          wdt.py
    │  │
    │  ├─04-Cipher
    │  │      cipher.py
    │  │
    │  ├─05-AI-Demo
    │  │      dynamic_gesture.py
    │  │      eye_gaze.py
    │  │      face_detection.py
    │  │      face_landmark.py
    │  │      face_mesh.py
    │  │      face_parse.py
    │  │      face_pose.py
    │  │      face_recognition.py
    │  │      face_registration.py
    │  │      falldown_detect.py
    │  │      finger_guessing.py
    │  │      hand_detection.py
    │  │      hand_keypoint_class.py
    │  │      hand_keypoint_detection.py
    │  │      hand_recognition.py
    │  │      keyword_spotting.py
    │  │      licence_det.py
    │  │      licence_det_rec.py
    │  │      nanotracker.py
    │  │      object_detect_yolov8n.py
    │  │      ocr_det.py
    │  │      ocr_rec.py
    │  │      person_detection.py
    │  │      person_keypoint_detect.py
    │  │      puzzle_game.py
    │  │      segment_yolov8n.py
    │  │      self_learning.py
    │  │      space_resize.py
    │  │      tts_zh.py
    │  │
    │  ├─06-Display
    │  │      display_hdmi.py
    │  │      display_lcd.py
    │  │      display_virt.py
    │  │
    │  ├─07-April-Tags
    │  │      find_apriltags.py
    │  │      find_apriltags_3d_pose.py
    │  │
    │  ├─08-Codes
    │  │      find_barcodes.py
    │  │      find_datamatrices.py
    │  │      find_qrcodes.py
    │  │
    │  ├─09-Color-Tracking
    │  │      automatic_grayscale_color_tracking.py
    │  │      automatic_rgb565_color_tracking.py
    │  │      black_grayscale_line_following.py
    │  │      image_histogram_info.py
    │  │      image_statistics_info.py
    │  │      multi_color_code_tracking.py
    │  │      single_color_code_tracking.py
    │  │
    │  ├─10-Drawing
    │  │      arrow_drawing.py
    │  │      circle_drawing.py
    │  │      cross_drawing.py
    │  │      ellipse_drawing.py
    │  │      flood_fill.py
    │  │      image_drawing.py
    │  │      image_drawing_advanced.py
    │  │      image_drawing_alpha_blending_test.py
    │  │      keypoints_drawing.py
    │  │      line_drawing.py
    │  │      rectangle_drawing.py
    │  │      text_drawing.py
    │  │
    │  ├─11-Feature-Detection
    │  │      edges.py
    │  │      find_blobs.py
    │  │      find_lines.py
    │  │      find_rects.py
    │  │      hog.py
    │  │      lbp.py
    │  │      linear_regression_fast.py
    │  │
    │  ├─12-Image-Filters
    │  │      adaptive_histogram_equalization.py
    │  │      blur_filter.py
    │  │      color_binary_filter.py
    │  │      color_light_removal.py
    │  │      edge_filter.py
    │  │      erode_and_dilate.py
    │  │      gamma_correction.py
    │  │      grayscale_bilateral_filter.py
    │  │      grayscale_binary_filter.py
    │  │      grayscale_light_removal.py
    │  │      histogram_equalization.py
    │  │      kernel_filters.py
    │  │      lens_correction.py
    │  │      linear_polar.py
    │  │      log_polar.py
    │  │      mean_adaptive_threshold_filter.py
    │  │      mean_filter.py
    │  │      median_adaptive_threshold_filter.py
    │  │      median_filter.py
    │  │      midpoint_adaptive_threshold_filter.py
    │  │      midpoint_filter.py
    │  │      mode_adaptive_threshold_filter.py
    │  │      mode_filter.py
    │  │      negative.py
    │  │      perspective_and_rotation_correction.py
    │  │      perspective_correction.py
    │  │      rotation_correction.py
    │  │      sharpen_filter.py
    │  │      unsharp_filter.py
    │  │      vflip_hmirror_transpose.py
    │  │
    │  ├─14-Socket
    │  │      http_client.py
    │  │      http_server.py
    │  │      iperf3.py
    │  │      network_lan.py
    │  │      network_wlan_ap.py
    │  │      network_wlan_sta.py
    │  │      tcp_client.py
    │  │      tcp_server.py
    │  │      udp_clinet.py
    │  │      udp_server.py
    │  │
    │  ├─15-LVGL
    │  │  │  lvgl_demo.py
    │  │  │  lvgl_touch_demo.py
    │  │  │
    │  │  └─data
    │  │      ├─font
    │  │      │      lv_font_simsun_16_cjk.fnt
    │  │      │      montserrat-16.fnt
    │  │      │
    │  │      └─img
    │  │              animimg001.png
    │  │              animimg002.png
    │  │              animimg003.png
    │  │
    │  ├─16-AI-Cube
    │  │      ClassificationApp.py
    │  │      DetectionApp.py
    │  │      MultiLabelApp.py
    │  │      OCR_Det.py
    │  │      SegmentationApp.py
    │  │      SelfLearningApp.py
    │  │
    │  ├─17-Sensor
    │  │      camera_dual_bind_hdmi.py
    │  │      camera_mirror_flip.py
    │  │      camera_single_bind_hdmi.py
    │  │      camera_single_bind_lcd.py
    │  │      camera_single_show_hdmi.py
    │  │      camera_single_show_lcd.py
    │  │      camera_snapshot_and_save.py
    │  │      camera_triple_bind_hdmi.py
    │  │
    │  ├─18-NNCase
    │  │  │  ai2d+kpu.py
    │  │  │  kpu.py
    │  │  │
    │  │  └─face_detection
    │  │          face_detection_320.kmodel
    │  │          face_detection_ai2d_input.bin
    │  │          face_detection_ai2d_output.bin
    │  │          prior_data_320.bin
    │  │
    │  ├─99-HelloWorld
    │  │      helloworld.py
    │  │
    │  ├─ai_test_kmodel
    │  │      embedding.kmodel
    │  │      insect_det.kmodel
    │  │      landscape_multilabel.kmodel
    │  │      ocr_det_int16.kmodel
    │  │      ocr_rec_int16.kmodel
    │  │      ocular_seg.kmodel
    │  │      veg_cls.kmodel
    │  │
    │  ├─ai_test_utils
    │  │      0.jpg
    │  │      1.jpg
    │  │      2.jpg
    │  │      3.jpg
    │  │      4.jpg
    │  │      5.jpg
    │  │      6.jpg
    │  │      7.jpg
    │  │      8.jpg
    │  │      dict.txt
    │  │
    │  ├─kmodel
    │  │      cropped_test127.kmodel
    │  │      eye_gaze.kmodel
    │  │      face_alignment.kmodel
    │  │      face_alignment_post.kmodel
    │  │      face_detection_320.kmodel
    │  │      face_landmark.kmodel
    │  │      face_parse.kmodel
    │  │      face_pose.kmodel
    │  │      face_recognition.kmodel
    │  │      gesture.kmodel
    │  │      handkp_det.kmodel
    │  │      hand_det.kmodel
    │  │      hand_reco.kmodel
    │  │      hifigan.kmodel
    │  │      kws.kmodel
    │  │      licence_reco.kmodel
    │  │      LPD_640.kmodel
    │  │      nanotracker_head_calib_k230.kmodel
    │  │      nanotrack_backbone_sim.kmodel
    │  │      ocr_det_int16.kmodel
    │  │      ocr_rec_int16.kmodel
    │  │      person_detect_yolov5n.kmodel
    │  │      recognition.kmodel
    │  │      yolov5n-falldown.kmodel
    │  │      yolov8n-pose.kmodel
    │  │      yolov8n_320.kmodel
    │  │      yolov8n_seg_320.kmodel
    │  │      zh_fastspeech_1_f32.kmodel
    │  │      zh_fastspeech_2.kmodel
    │  │
    │  └─utils
    │      │  dict.txt
    │      │  fist.bin
    │      │  five.bin
    │      │  phone_map.txt
    │      │  pinyin.txt
    │      │  prior_data_320.bin
    │      │  shang.bin
    │      │  shear.bin
    │      │  small_pinyin.txt
    │      │  wozai.wav
    │      │  xia.bin
    │      │  you.bin
    │      │  zuo.bin
    │      │
    │      ├─db
    │      │      readme.txt
    │      │
    │      └─db_img
    │              id_1.jpg
    │              id_2.png
    │
    ├─libs
    │      AI2D.py
    │      AIBase.py
    │      PipeLine.py
    │      RtspSever.py
    │
    └─res
        └─font
                LICENSE.txt
                readme.txt
                SourceHanSansSC-Normal-Min.ttf

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
143  
144  
145  
146  
147  
148  
149  
150  
151  
152  
153  
154  
155  
156  
157  
158  
159  
160  
161  
162  
163  
164  
165  
166  
167  
168  
169  
170  
171  
172  
173  
174  
175  
176  
177  
178  
179  
180  
181  
182  
183  
184  
185  
186  
187  
188  
189  
190  
191  
192  
193  
194  
195  
196  
197  
198  
199  
200  
201  
202  
203  
204  
205  
206  
207  
208  
209  
210  
211  
212  
213  
214  
215  
216  
217  
218  
219  
220  
221  
222  
223  
224  
225  
226  
227  
228  
229  
230  
231  
232  
233  
234  
235  
236  
237  
238  
239  
240  
241  
242  
243  
244  
245  
246  
247  
248  
249  
250  
251  
252  
253  
254  
255  
256  
257  
258  
259  
260  
261  
262  
263  
264  
265  
266  
267  
268  
269  
270  
271  
272  
273  
274  
275  
276  
277  
278  
279  
280  
281  
282  
283  
284  
285  
286  
287  
288  
289  
290  
291  
292  
293  
294  
295  
296  
297  
298  
299  
300  
301  
302  
303  
304  
305  
306  
307  
308  
309  
310  
311  
312  
313  
314  
315  
316  
317  
318  
319  


### 5.1 获取与当前固件匹配的示例 ​

#### 5.1.1 Visual Studio Code + CanMV 扩展 ​

CanMV 扩展会根据开发板固件的资源清单下载匹配的官方示例。连接开发板后，打开 CanMV 活动栏中的 `Examples` 视图；如果列表没有刷新，可以打开命令面板并运行 `CanMV: 刷新示例`。

从 `Examples` 视图打开示例时，扩展会把它作为可编辑的未保存文件打开，避免直接改坏本地示例缓存。也可以在 `Device` 视图中找到 `/sdcard/examples` 下的固件内置例程，将它下载到电脑本地后再打开。

![图 23](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260717_135540.png)

#### 5.1.2 CanMV IDE Web ​

网页版 IDE 左侧边栏提供“示例”入口，单击示例即可将代码载入编辑器。网页版内置示例仍在持续更新，优先选择明确标注支持庐山派 K230 / Lite-K230D 的最新示例。

如果需要使用固件自带例程，可以在左侧“设备文件”中打开 `/sdcard/examples`。最新版固件支持直接查看和编辑设备文件；如果无法获取设备文件，请先更新与当前板型匹配的 CanMV MicroPython 固件。

![CanMV IDE Web 界面与示例入口](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/canmv-ide-web-doc/canmv-ide-web-doc_20260703_154702.png)

### 5.2 运行和停止 Python 脚本 ​

这里以 `examples/05-AI-Demo/object_detect_yolov8n.py` 为例。具体目录和文件名可能随固件版本调整，请以开发环境中的示例列表或 `/sdcard/examples` 中的实际内容为准。

⚠️运行前必看：必须先确认并修改显示设备

**请不要打开示例后直接点击运行。**

嘉楠提供的部分官方示例，包括本节使用的 `object_detect_yolov8n.py`，默认将显示设备配置为 **HDMI** 。庐山派开发板本身不能直接输出 HDMI，只有正确连接庐山派 HDMI 扩展板后，才能使用 HDMI 显示模式。

如果没有连接 HDMI 扩展板，运行前必须把示例中的显示设备或显示模式从 **HDMI** 修改为 **LCD** ，否则通常会出现下面的错误：

text
    
    
    RuntimeError: init panel failed

1  


出现这个错误一般是**显示设备配置不匹配** ，不代表摄像头、固件或开发板已经损坏。

**运行前请逐项确认：**

  1. 使用板载 LCD：将代码中的显示设备或显示模式修改为 `LCD`。
  2. 使用 HDMI：必须先正确连接庐山派 HDMI 扩展板，并保持代码为 HDMI 显示模式。
  3. 确认实际显示设备与代码配置一致后，再点击运行。



![图 17](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260612_165659.png)

#### 5.2.1 Visual Studio Code + CanMV 扩展 ​

  1. 在 Visual Studio Code 中打开要运行的 `.py` 文件。
  2. 确认 CanMV 扩展已经连接开发板，并且开发板状态为就绪。
  3. 使用编辑器右上角的运行按钮，或者打开命令面板运行 `CanMV: 运行当前 Python 脚本`。
  4. 也可以在编辑器中右键，选择 `CanMV: 在 K230 上运行当前文件`。
  5. 在 `CanMV Terminal` 中查看脚本输出，同时观察 LCD 画面或摄像头识别效果。
  6. 需要中断脚本时，运行 `CanMV: 停止脚本`，或者在 `CanMV Terminal` 中按 `Ctrl-C`。



![图 24](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/quick-start/quick-start_20260717_140107.gif)

#### 5.2.2 CanMV IDE Web ​

  1. 在编辑器中打开内置示例，或者直接编辑当前代码。
  2. 确认左上角已经显示开发板连接成功。
  3. 点击工具栏的“运行”按钮，或者按 `F5`。
  4. 在下方终端查看脚本输出，同时观察右侧预览画面或开发板 LCD。
  5. 需要中断脚本时，点击“停止”，或者按 `Shift+F5`。



![CanMV IDE Web 运行脚本](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/canmv-ide-web-doc/canmv-ide-web-doc_20260703_153534.gif)

### 5.3 预览摄像头或图像输出 ​

对于会向 IDE 帧缓冲输出图像的脚本，两套环境都可以实时预览。Visual Studio Code 用户可以从 CanMV 活动栏的 `Toolbox` 打开 `Preview`，也可以在命令面板运行 `CanMV: 启用预览`；CanMV IDE Web 用户在右侧切换到“预览”面板即可。

两套环境都支持下面的图像输出方式：

  * `Display.init(..., to_ide=True)`：把显示输出同步到 IDE 帧缓冲。
  * `image.compress_for_ide()`：把指定图像发送到 IDE 帧缓冲。



`Preview` 除了实时画面，还支持适应窗口、原始尺寸、旋转、保存 PNG、像素 RGB 读取、FPS 显示、ROI 直方图和视频录制等功能。

CanMV IDE Web 的右侧预览效果可参考下图。代码中除了 `Display.init(..., to_ide=True)`，还要在循环中调用 `Display.show_image(img)` 或使用对应的 IDE 帧缓冲输出方法刷新图像。

![CanMV IDE Web 摄像头预览](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/canmv-ide-web-doc/canmv-ide-web-doc_20260703_153534.gif)

### 5.4 管理开发板文件 ​

  * **Visual Studio Code** ：在 CanMV 活动栏的 `Device` 视图中管理 `/sdcard`、`/data` 和 `/udisk`，可以上传、下载、新建、重命名、删除或编辑文件。
  * **CanMV IDE Web** ：在左侧“工作区”的“设备文件”中管理开发板文件；打开设备上的文件后，按 `Ctrl+S` 可以直接保存回开发板，也可以拖入本地文件进行上传。



如果希望程序脱离电脑开机自动运行，可以将脚本保存为 `/sdcard/main.py`。详细方法请参考[《让代码离线运行》](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/ide-usage/offline-run.html)。

![CanMV IDE Web 管理设备文件](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/canmv-ide-web-doc/canmv-ide-web-doc_20260703_160135.gif)

## 六 常见问题 ​

### 6.1 运行 `.py` 例程是不是烧录程序？ ​

不是。运行 `.py` 例程是让 MicroPython 解释器执行脚本；只有把 `.img` 固件镜像写入 TF 卡时，才叫烧录固件。

### 6.2 K230 和 Lite-K230D 的固件可以混用吗？ ​

不可以。两款开发板虽然都属于庐山派 CanMV K230 系列，但固件镜像不同。下载和写入固件前，一定要确认文件名和开发板型号匹配。

### 6.3 电脑没有出现 CanMV 设备怎么办？ ​

优先检查 TF 卡是否已经正确写入 `.img` 固件镜像、开发板是否正常上电、Type-C 数据线是否支持数据传输。如果第一次启动时间较长，可以等待开发板完成 TF 卡剩余空间初始化后再观察。

### 6.4 CanMV 扩展无法连接开发板怎么办？ ​

按下面的顺序排查：

  1. 确认 Type-C 数据线支持数据传输，并且开发板已经正常启动。
  2. 关闭串口工具和旧版 IDE 等可能占用开发板串口的软件。
  3. 在 Windows 设备管理器中确认是否出现 USB 串行设备（COMx）。
  4. 重新插拔 Type-C 数据线，然后再次运行 `CanMV: 连接开发板`。
  5. 如果仍然无法自动检测，在 Visual Studio Code 设置中把 `canmv.serialPath` 手动配置为开发板对应的串口。



### 6.5 应该使用哪一套示例？ ​

优先使用 CanMV 扩展 `Examples` 视图中与当前固件资源匹配的官方示例，或者使用固件内置例程。固件内置例程的源码、模型和字体等资源已经放在 TF 卡中，与当前固件的匹配度通常更高。不要直接套用来源不明或版本过旧的示例，否则容易遇到 API、资源路径或模型不匹配的问题。

### 6.6 CanMV Terminal 有输出，但 Preview 没有图像怎么办？ ​

先确认脚本确实会向 IDE 帧缓冲输出图像，例如使用 `Display.init(..., to_ide=True)` 或调用 `compress_for_ide()`。然后从 `Toolbox` 打开 `Preview`，或者运行 `CanMV: 启用预览`。如果仍然没有图像，请查看 CanMV 输出通道中的预览与后端日志，并确认脚本没有因为显示初始化错误而提前退出。

### 6.7 Device 视图没有显示开发板文件怎么办？ ​

先确认开发板状态为就绪，再打开命令面板运行 `CanMV: 刷新资源管理器`。如果仍然无法显示，建议更新到较新的 CanMV K230 固件；过旧固件可能不支持扩展所需的远程文件能力。

### 6.8 CanMV IDE Web 点击“连接设备”后找不到串口怎么办？ ​

按下面的顺序排查：

  1. 确认使用 Chrome、Edge 或其他支持 Web Serial API 的 Chromium 内核浏览器，不要使用 Firefox 或 Safari。
  2. 确认访问的是 `https://wiki.lckfb.com/storage/html/canmv-web-ide/`，浏览器通常不允许普通非安全网页访问串口。
  3. 检查 Type-C 数据线是否支持数据传输，并在设备管理器中确认开发板显示为“USB 串行设备（COMx）”。
  4. 关闭 Visual Studio Code、串口助手、另一个 CanMV IDE Web 标签页等可能占用串口的工具，然后重新点击“连接设备”。



### 6.9 CanMV IDE Web 能否完全代替 Visual Studio Code？ ​

对于快速体验、课堂教学、临时调试、运行示例、图像预览和设备文件管理，CanMV IDE Web 已经可以独立完成。对于长期项目、多文件工程、Git 管理、完善的代码分析和与固件匹配的 stubs，仍然优先推荐 Visual Studio Code + CanMV 扩展。两套环境运行相同的 CanMV MicroPython 脚本，可以根据使用场景随时切换。

### 6.10 CanMV IDE Web 烧录固件也不需要驱动吗？ ​

不能一概而论。开发板正常启动后，网页 IDE 通过 USB CDC 虚拟串口连接，Windows 10/11 通常不需要额外驱动；进入 BootROM 使用网页烧录固件时，Windows 首次可能仍需通过 Zadig 为 `K230 USB Boot Device` 安装 WinUSB 驱动。完整烧录步骤请参考[《CanMV IDE Web 在线使用教程》](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/canmv-ide-web-doc.html#九-烧录固件到开发板-可选)。

## 七 本章小结 ​

本章我们完成了庐山派 K230 / Lite-K230D 的快速上手流程：准备开发板和配件、下载并写入对应固件、启动开发板，以及获取、运行和停止示例程序。开发环境方面，我们提供了两条完整路线：长期开发优先使用 **Visual Studio Code + CanMV 扩展** ，快速体验和临时调试可以使用立创开发板部门制作的 **CanMV IDE Web** 。两套环境都能连接开发板、运行 CanMV MicroPython 脚本、查看终端和图像预览、管理设备文件。

## 八 参考资料 ​

  * [嘉楠官方文档：使用 CanMV Visual Studio Code 扩展](https://www.kendryte.com/k230_canmv/zh/main/userguide/how_to_use_ide.html)
  * [CanMV Visual Studio Code 扩展源码](https://github.com/kendryte/canmv-vscode-extension)
  * [Visual Studio Code 下载页面](https://code.visualstudio.com/Download)
  * [立创开发板 CanMV IDE Web 在线入口](https://wiki.lckfb.com/storage/html/canmv-web-ide/)
  * [CanMV IDE Web 在线使用教程](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/canmv-ide-web-doc.html)



