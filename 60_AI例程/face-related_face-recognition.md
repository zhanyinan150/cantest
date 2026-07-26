# 人脸识别

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/ai-demo/face-related/face-recognition.html>
> **最后更新**: 2024-12-18

---

# 1 本节介绍 ​

📝想到AI识别，大家最熟悉的应该就是人脸识别了，本节您将学习如何通过庐山派来识别人脸，如无特殊说明，以后所有例程的显示设备均为通过外接[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)，在3.1寸小屏幕上显示。若用户无3.1寸屏幕扩展板也可以正常在IDE的缓冲区，只是受限于USB带宽，可能会帧率较低或卡顿。

🏆学习目标

1️⃣如何用庐山派开发板去识别人脸并用矩形框标注出来。

2️⃣如何获取识别人脸的结果，坐标。

庐山派开发板的固件是存储在TF中的，模型文件已经提前写入到固件中了，所以大家只需要复制下面的代码到IDE，传递到开发板上就可以正常运行了。**无需再额外拷贝** ，至于后面需要拷贝自己训练的模型那就是后话了。

# 2 代码例程 ​

python
    
    
    from libs.PipeLine import PipeLine, ScopedTiming
    from libs.AIBase import AIBase
    from libs.AI2D import Ai2d
    import os
    import ujson
    from media.media import *
    from time import *
    import nncase_runtime as nn
    import ulab.numpy as np
    import time
    import utime
    import image
    import random
    import gc
    import sys
    import aidemo
    
    # 自定义人脸检测类，继承自AIBase基类
    class FaceDetectionApp(AIBase):
        def __init__(self, kmodel_path, model_input_size, anchors, confidence_threshold=0.5, nms_threshold=0.2, rgb888p_size=[224,224], display_size=[1920,1080], debug_mode=0):
            super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)  # 调用基类的构造函数
            self.kmodel_path = kmodel_path  # 模型文件路径
            self.model_input_size = model_input_size  # 模型输入分辨率
            self.confidence_threshold = confidence_threshold  # 置信度阈值
            self.nms_threshold = nms_threshold  # NMS（非极大值抑制）阈值
            self.anchors = anchors  # 锚点数据，用于目标检测
            self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]  # sensor给到AI的图像分辨率，并对宽度进行16的对齐
            self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]  # 显示分辨率，并对宽度进行16的对齐
            self.debug_mode = debug_mode  # 是否开启调试模式
            self.ai2d = Ai2d(debug_mode)  # 实例化Ai2d，用于实现模型预处理
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)  # 设置Ai2d的输入输出格式和类型
    
        # 配置预处理操作，这里使用了pad和resize，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
        def config_preprocess(self, input_image_size=None):
            with ScopedTiming("set preprocess config", self.debug_mode > 0):  # 计时器，如果debug_mode大于0则开启
                ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size  # 初始化ai2d预处理配置，默认为sensor给到AI的尺寸，可以通过设置input_image_size自行修改输入尺寸
                top, bottom, left, right = self.get_padding_param()  # 获取padding参数
                self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [104, 117, 123])  # 填充边缘
                self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)  # 缩放图像
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])  # 构建预处理流程
    
        # 自定义当前任务的后处理，results是模型输出array列表，这里使用了aidemo库的face_det_post_process接口
        def postprocess(self, results):
            with ScopedTiming("postprocess", self.debug_mode > 0):
                post_ret = aidemo.face_det_post_process(self.confidence_threshold, self.nms_threshold, self.model_input_size[1], self.anchors, self.rgb888p_size, results)
                if len(post_ret) == 0:
                    return post_ret
                else:
                    return post_ret[0]
    
        # 绘制检测结果到画面上
        def draw_result(self, pl, dets):
            with ScopedTiming("display_draw", self.debug_mode > 0):
                if dets:
                    pl.osd_img.clear()  # 清除OSD图像
                    for det in dets:
                        # 将检测框的坐标转换为显示分辨率下的坐标
                        x, y, w, h = map(lambda x: int(round(x, 0)), det[:4])
                        x = x * self.display_size[0] // self.rgb888p_size[0]
                        y = y * self.display_size[1] // self.rgb888p_size[1]
                        w = w * self.display_size[0] // self.rgb888p_size[0]
                        h = h * self.display_size[1] // self.rgb888p_size[1]
                        pl.osd_img.draw_rectangle(x, y, w, h, color=(255, 255, 0, 255), thickness=2)  # 绘制矩形框
                else:
                    pl.osd_img.clear()
    
        # 获取padding参数
        def get_padding_param(self):
            dst_w = self.model_input_size[0]  # 模型输入宽度
            dst_h = self.model_input_size[1]  # 模型输入高度
            ratio_w = dst_w / self.rgb888p_size[0]  # 宽度缩放比例
            ratio_h = dst_h / self.rgb888p_size[1]  # 高度缩放比例
            ratio = min(ratio_w, ratio_h)  # 取较小的缩放比例
            new_w = int(ratio * self.rgb888p_size[0])  # 新宽度
            new_h = int(ratio * self.rgb888p_size[1])  # 新高度
            dw = (dst_w - new_w) / 2  # 宽度差
            dh = (dst_h - new_h) / 2  # 高度差
            top = int(round(0))
            bottom = int(round(dh * 2 + 0.1))
            left = int(round(0))
            right = int(round(dw * 2 - 0.1))
            return top, bottom, left, right
    
    if __name__ == "__main__":
        # 显示模式，默认"hdmi",可以选择"hdmi"和"lcd"
        display_mode="lcd"
        # k230保持不变，k230d可调整为[640,360]
        rgb888p_size = [1920, 1080]
    
        if display_mode=="hdmi":
            display_size=[1920,1080]
        else:
            display_size=[800,480]
        # 设置模型路径和其他参数
        kmodel_path = "/sdcard/examples/kmodel/face_detection_320.kmodel"
        # 其它参数
        confidence_threshold = 0.5
        nms_threshold = 0.2
        anchor_len = 4200
        det_dim = 4
        anchors_path = "/sdcard/examples/utils/prior_data_320.bin"
        anchors = np.fromfile(anchors_path, dtype=np.float)
        anchors = anchors.reshape((anchor_len, det_dim))
    
        # 初始化PipeLine，用于图像处理流程
        pl = PipeLine(rgb888p_size=rgb888p_size, display_size=display_size, display_mode=display_mode)
        pl.create()  # 创建PipeLine实例
        # 初始化自定义人脸检测实例
        face_det = FaceDetectionApp(kmodel_path, model_input_size=[320, 320], anchors=anchors, confidence_threshold=confidence_threshold, nms_threshold=nms_threshold, rgb888p_size=rgb888p_size, display_size=display_size, debug_mode=0)
        face_det.config_preprocess()  # 配置预处理
    
        try:
            while True:
                os.exitpoint()                      # 检查是否有退出信号
                with ScopedTiming("total",1):
                    img = pl.get_frame()            # 获取当前帧数据
                    res = face_det.run(img)         # 推理当前帧
                    face_det.draw_result(pl, res)   # 绘制结果
                    pl.show_image()                 # 显示结果
                    gc.collect()                    # 垃圾回收
        except Exception as e:
            sys.print_exception(e)                  # 打印异常信息
        finally:
            face_det.deinit()                       # 反初始化
            pl.destroy()                            # 销毁PipeLine实例

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


**开头就是导入了一些自定义库** ：`PipeLine` 和 `ScopedTiming`：用于处理图像流和性能计时。`AIBase`：自定义的AI基础类，提供了AI应用的基本功能。`Ai2d`：用于图像预处理的库，如缩放、裁剪等操作。`aidemo`：包含了人脸检测的后处理函数。

导入系统库，`os`, `sys`, `time`, `utime`, `random`, `gc`：提供操作系统接口、时间控制、垃圾回收等功能。

导入第三方库，`nncase_runtime` as `nn`：用于加载和运行神经网络模型。`ulab.numpy` as `np`：他是轻量级的`NumPy`库，提供数组和矩阵运算功能。

**接下来定义了一个人脸检测类** ：

python
    
    
    class FaceDetectionApp(AIBase):
        def __init__(self, kmodel_path, model_input_size, anchors, confidence_threshold=0.5, nms_threshold=0.2, rgb888p_size=[224,224], display_size=[1920,1080], debug_mode=0):
            super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
            ......

1  
2  
3  
4  
5  


`FaceDetectionApp` 类继承自 `AIBase`，复用了AI应用的基本功能。然后就是一些初始化参数了：

`kmodel_path`：模型文件的路径。`model_input_size`：模型输入的尺寸（宽、高）。`anchors`：用于目标检测的锚点框数据。`confidence_threshold`：置信度阈值，用于筛选检测结果。`nms_threshold`：非极大值抑制的阈值，防止重复检测。`rgb888p_size`：摄像头捕获的图像尺寸。`display_size`：显示设备的分辨率。`debug_mode`：调试模式开关，等。这里比较重要的点是对 `rgb888p_size` 和 `display_size` 的宽度进行16字节对齐，来提高内存的访问效率。

**然后开始配置图像预处理** ：

python
    
    
    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ...

1  
2  
3  
4  


这段代码配置图像的预处理操作，包括填充和缩放，以适应模型的输入要求。当调试开关打开后，去统计预处理配置所需的时间，方便性能优化。然后开始确定

输入尺寸，如果没有指定`input_image_size`，就使用默认的 `rgb888p_size`。调用 `get_padding_param()` 函数，计算图像在缩放到模型输入尺寸时需要的填充大小。使用 `self.ai2d.pad()` 进行图像边缘填充，填充值为 `[104, 117, 123]`，这是常用的图像均值，便于模型处理。接下来使用 `self.ai2d.resize()` 进行图像缩放，调用 `self.ai2d.build()`，指定输入和输出的图像尺寸，完成预处理配置。

再将图像输入深度学习模型前，一般是需要对图像进行预处理的，主要是为了匹配模型的输入要求。上面有个填充步骤是因为我们再缩放图片的时候，为了不改变图像的纵横比，有时就需要在图像的边缘添加填充。

**后处理检测结果** ：

python
    
    
    def postprocess(self, results):
        with ScopedTiming("postprocess", self.debug_mode > 0):
            post_ret = aidemo.face_det_post_process(...)
            ......

1  
2  
3  
4  
5  


这段代码的主要功能是对模型的输出结果进行后处理，得到人脸检测的最终结果。`ScopedTiming`：主要作用是统计后处理所需的时间。然后使用 `aidemo.face_det_post_process()`，传入模型的输出、阈值和相关参数，进行后处理。如果能检测到人脸，就返回结果，没有就返回空结果。

**接下来就到绘制检测结果了：**

python
    
    
    def draw_result(self, pl, dets):
        with ScopedTiming("display_draw", self.debug_mode > 0):
            if dets:
                pl.osd_img.clear()
                for det in dets:
                    # 将检测框的坐标转换为显示分辨率下的坐标
                    ......
                    pl.osd_img.draw_rectangle(x, y, w, h, color=(255, 255, 0, 255), thickness=2)
            else:
                pl.osd_img.clear()

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


就是把模型检测到的人脸给框出来，进来的时候先调用 `pl.osd_img.clear()` 清空之前的绘图层。把检测框的坐标从模型的输入尺寸映射到我们的显示设备的尺寸（分辨率），使用 `pl.osd_img.draw_rectangle()`，在人脸位置绘制矩形框，来方便我们查看。这里是在OSD层绘制，他只是在图像上叠加来显示文字或者图像，并不会修改原始的图像数据。

`get_padding_param`是用来计算填充参数的，重点是为了保持图像的横纵比，对不满足的地方进行填充。这里不再赘述。

**然后就到主函数了** ：

首先通过字符串选择显示设备，可以是HDMI阔这LCD，我们以后默认都是使用LCD，因为比较方便。选好后根据显示设备来设置图像尺寸，然后根据模型文件的路径来加载文件和锚点数据（anchors）。初始化pipline，初始化人脸检测应用。

之后的死循环就是不断的获取摄像头帧来进行人脸检测，并且显示结果。注释都够全，这里不再赘述。

# 3 运行效果 ​

首先我们准备一张人脸的照片： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-recognition/face-recognition_20241130_165831.png)  
将上面代码复制进IDE里面并运行，此时在IDE的帧缓冲区和3.1寸屏幕上就都能看到结果了。

![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-recognition/face-recognition_20241130_170235.png)

在3.1寸屏幕上实际运行效果： ![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-recognition/face-recognition_20241130_170523.png)

# 4 打印获取人脸识别结果和坐标 ​

前面我们提到了 `res = face_det.run(img) # 推理当前帧`，这句函数就能获取到结果和坐标，并将结果传递给draw_result后才进行的识别框绘制，我们在`res = face_det.run(img)`后面再加一下下面的语句：
    
    
                    if res:
                        print(res)

1  
2  


然后再次在IDE中运行代码，就能在串行终端中看到人脸的坐标了，我们这里识别到了四个人脸的坐标。再结合之前我们学过的uart章节，就可以把这些信息传递给外部设备了。

![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-recognition/face-recognition_20241130_172350.png)

`det = [x, y, w, h, score]` 函数中`x, y, w, h`是人脸检测框的坐标和尺寸，`score`是检测的置信度得分。需要注意的是，这里获取到的坐标也不是屏幕上的真实坐标，想要获得屏幕上的真实坐标，还需要用`draw_result`函数里面的公式来将这个坐标转换为对应显示分辨率下的坐标。

