# 让代码离线运行

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/ide-usage/offline-run.html>
> **最后更新**: 2024-12-05

---

# 1 前期知识提示 ​

打开IDE后连接开发板，此时点击左下角的**绿色三角** 运行代码，MicroPyhon脚本并不会保存到TF卡中，而是保存到内存中并开始运行，一旦板子断电，程序（也就是脚本）就丢失了。

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ide-usage/offline-run/offline-run_20241130_104753.png)

在实际开发中，当我们通过 IDE 在线运行验证调试好后，就需要将我们的脚本保存到TF卡中，并让庐山派开发板在脱离电脑连接的情况下，上电后自动运行我们的程序。

庐山派开发板的CanMV固件在上电后会自动执行`micropython`，他会在`sdcard`目录中去寻找`boot.py`和`main.py`这两个文件。

**boot.py**

  * **启动文件** ：这是庐山派启动时首先执行的文件，类似于我们嵌入式开发中的初始设置文件。
  * 主要功能： 
    * 配置硬件环境（如I/O引脚、电源管理）。
    * 初始化必要的模块（如串口、网络）。
    * 一般只用作系统配置，不进行主任务逻辑处理。如果代码较为简单的话，一般不需要建立`boot.py`文件
    * 一定要注意不能有死循环，否则他会无法执行 **main.py** 。



**main.py**

  * **主程序文件** ：在 **`boot.py`** 执行完毕后，系统会自动执行 **`main.py`** 。
  * 主要功能： 
    * 实现用户的核心逻辑。
    * 比如图像采集、AI处理、通信或其他任务。



执行流程：

  1. **上电/复位** → 执行 **`boot.py`** ，如果找不到就不执行。
  2. **boot.py 执行完成** → 自动执行 **`main.py`** 。
  3. **main.py 执行完成** → 程序可能结束（如果没有 `while` 循环维持运行，那么主程序会退出）。



# 2 如何操作 ​

## 2.1 用CanMV IDE K230来保存 ​

一般情况下我们只需要用到`main.py`，当你需要用到`boot.py`的时候也不需要再看这篇文档了。

首先打开我们的IDE，连接至我们的开发板后，左下角的绿色三角按钮就会出现了，此时准备好我们的程序，点击菜单栏的**工具** ->`save open script to CanMV board （as main.py）`,就可以把我们当前打开的文件给重命名为main.py，保存到TF卡的`sdcard`根目录下了。

中间会弹出来一个对话框提示我们是否要**去除注释并将空格转化为制表符** ，直接选择是，然后等待保存进度条走完就可以了。

![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ide-usage/offline-run/offline-run_20241130_110903.png)

此时我们就可以断电或者按板子上的复位，检查开发板是否会自动执行你保存的程序了。

## 2.2 手动保存 ​

在之前我们已经知道了，庐山派在上电后，会自动去找`main.py`来执行。所以我们只需要将`main.py`保存到我们的TF卡里面就行了。

开发板连接电脑后会自动被当做一个U盘设备，我们只需要把自己的程序文件名重命名为`main.py`，再把他复制到`sdcard`根目录下就好了，如下图所示：

![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ide-usage/offline-run/offline-run_20241130_111820.png)

此时我们就可以断电或者按板子上的复位，检查开发板是否会自动执行你保存的程序了。

