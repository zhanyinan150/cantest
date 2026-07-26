# 人手检测

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/ai-demo/hand-related/hand-detect.html>
> **最后更新**: 2024-12-27

---

# 1 本节介绍 ​

📝本节您将学习如何通过庐山派来进行人手检测，如无特殊说明，以后所有例程的显示设备均为通过外接[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)，在3.1寸小屏幕上显示。若用户无3.1寸屏幕扩展板也可以正常在IDE的缓冲区，只是受限于USB带宽，可能会帧率较低或卡顿。

🏆学习目标

1️⃣如何用庐山派开发板去进行人手检测。

庐山派开发板的固件是存储在TF中的，模型文件已经提前写入到固件中了，所以大家只需要复制下面的代码到IDE，传递到开发板上就可以正常运行了。**无需再额外拷贝** ，至于后面需要拷贝自己训练的模型那就是后话了。

本文主要介绍如何在庐山派开发板上进行人手检测。可以用于手势控制，如VR，体感游戏，虚拟演奏，行为监控等场景。

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
    import aicube
    
    # 自定义手掌检测类，继承自AIBase基类
    class HandDetectionApp(AIBase):
        def __init__(self, kmodel_path, model_input_size, labels, anchors, confidence_threshold=0.2, nms_threshold=0.5, nms_option=False, strides=[8,16,32], rgb888p_size=[224,224], display_size=[1920,1080], debug_mode=0):
            super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)  # 调用基类的构造函数，初始化模型文件路径、模型输入分辨率、RGB图像分辨率和调试模式
            self.kmodel_path = kmodel_path  # 模型文件路径
            self.model_input_size = model_input_size  # 模型输入分辨率
            self.labels = labels  # 模型输出的类别标签列表
            self.anchors = anchors  # 用于目标检测的锚点尺寸列表
            self.strides = strides  # 特征下采样倍数
            self.confidence_threshold = confidence_threshold  # 置信度阈值，用于过滤低置信度的检测结果
            self.nms_threshold = nms_threshold  # NMS（非极大值抑制）阈值，用于去除重叠的检测框
            self.nms_option = nms_option  # NMS选项，可能影响NMS的具体实现
            self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]  # sensor给到AI的图像分辨率，对齐到最近的16的倍数
            self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]  # 显示分辨率，对齐到最近的16的倍数
            self.debug_mode = debug_mode  # 调试模式，用于输出调试信息
            self.ai2d = Ai2d(debug_mode)  # 实例化Ai2d类，用于实现模型预处理
            # 设置Ai2d的输入输出格式和类型，这里使用NCHW格式，数据类型为uint8
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)
    
        # 配置预处理操作，这里使用了pad和resize
        def config_preprocess(self, input_image_size=None):
            with ScopedTiming("set preprocess config", self.debug_mode > 0):  # 使用ScopedTiming装饰器来测量预处理配置的时间
                # 初始化ai2d预处理配置，默认为sensor给到AI的尺寸，可以通过设置input_image_size自行修改输入尺寸
                ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
                # 计算padding参数并应用pad操作，以确保输入图像尺寸与模型输入尺寸匹配
                top, bottom, left, right = self.get_padding_param()
                self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [0, 0, 0])
                # 使用双线性插值进行resize操作，调整图像尺寸以符合模型输入要求
                self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
                # 构建预处理流程
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])
    
        # 自定义当前任务的后处理，用于处理模型输出结果
        def postprocess(self, results):
            with ScopedTiming("postprocess", self.debug_mode > 0):  # 使用ScopedTiming装饰器来测量后处理的时间
                # 使用aicube库的函数进行后处理，得到最终的检测结果
                dets = aicube.anchorbasedet_post_process(results[0], results[1], results[2], self.model_input_size, self.rgb888p_size, self.strides, len(self.labels), self.confidence_threshold, self.nms_threshold, self.anchors, self.nms_option)
                return dets
    
        # 绘制检测结果到屏幕上
        def draw_result(self, pl, dets):
            with ScopedTiming("display_draw", self.debug_mode > 0):  # 使用ScopedTiming装饰器来测量绘制结果的时间
                if dets:  # 如果存在检测结果
                    pl.osd_img.clear()  # 清除屏幕上的旧内容
                    for det_box in dets:  # 遍历每个检测框
                        # 根据模型输出计算检测框的像素坐标，并调整大小以适应显示分辨率
                        x1, y1, x2, y2 = det_box[2], det_box[3], det_box[4], det_box[5]
                        w = float(x2 - x1) * self.display_size[0] // self.rgb888p_size[0]
                        h = float(y2 - y1) * self.display_size[1] // self.rgb888p_size[1]
                        x1 = int(x1 * self.display_size[0] // self.rgb888p_size[0])
                        y1 = int(y1 * self.display_size[1] // self.rgb888p_size[1])
                        x2 = int(x2 * self.display_size[0] // self.rgb888p_size[0])
                        y2 = int(y2 * self.display_size[1] // self.rgb888p_size[1])
                        # 过滤掉太小或者位置不合理的检测框
                        if (h < (0.1 * self.display_size[0])):
                            continue
                        if (w < (0.25 * self.display_size[0]) and ((x1 < (0.03 * self.display_size[0])) or (x2 > (0.97 * self.display_size[0])))):
                            continue
                        if (w < (0.15 * self.display_size[0]) and ((x1 < (0.01 * self.display_size[0])) or (x2 > (0.99 * self.display_size[0])))):
                            continue
                        # 绘制矩形框和类别标签
                        pl.osd_img.draw_rectangle(x1, y1, int(w), int(h), color=(255, 0, 255, 0), thickness=2)
                        pl.osd_img.draw_string_advanced(x1, y1-50,32, " " + self.labels[det_box[0]] + " " + str(round(det_box[1], 2)), color=(255, 0, 255, 0))
                else:
                    pl.osd_img.clear()  # 如果没有检测结果，清空屏幕
    
        # 计算padding参数，确保输入图像尺寸与模型输入尺寸匹配
        def get_padding_param(self):
            # 根据目标宽度和高度计算比例因子
            dst_w = self.model_input_size[0]
            dst_h = self.model_input_size[1]
            input_width = self.rgb888p_size[0]
            input_high = self.rgb888p_size[1]
            ratio_w = dst_w / input_width
            ratio_h = dst_h / input_high
            # 选择较小的比例因子，以确保图像内容完整
            if ratio_w < ratio_h:
                ratio = ratio_w
            else:
                ratio = ratio_h
            # 计算新的宽度和高度
            new_w = int(ratio * input_width)
            new_h = int(ratio * input_high)
            # 计算宽度和高度的差值，并确定padding的位置
            dw = (dst_w - new_w) / 2
            dh = (dst_h - new_h) / 2
            top = int(round(dh - 0.1))
            bottom = int(round(dh + 0.1))
            left = int(round(dw - 0.1))
            right = int(round(dw + 0.1))
            return top, bottom, left, right
    
    if __name__=="__main__":
        # 显示模式，默认"hdmi",可以选择"hdmi"和"lcd"
        display_mode="lcd"
        # k230保持不变，k230d可调整为[640,360]
        rgb888p_size = [1920, 1080]
    
        if display_mode=="hdmi":
            display_size=[1920,1080]
        else:
            display_size=[800,480]
        # 模型路径
        kmodel_path="/sdcard/examples/kmodel/hand_det.kmodel"
        # 其它参数设置
        confidence_threshold = 0.2
        nms_threshold = 0.5
        labels = ["hand"]
        anchors = [26,27, 53,52, 75,71, 80,99, 106,82, 99,134, 140,113, 161,172, 245,276]   #anchor设置
    
        # 初始化PipeLine
        pl=PipeLine(rgb888p_size=rgb888p_size,display_size=display_size,display_mode=display_mode)
        pl.create()
        # 初始化自定义手掌检测实例
        hand_det=HandDetectionApp(kmodel_path,model_input_size=[512,512],labels=labels,anchors=anchors,confidence_threshold=confidence_threshold,nms_threshold=nms_threshold,nms_option=False,strides=[8,16,32],rgb888p_size=rgb888p_size,display_size=display_size,debug_mode=0)
        hand_det.config_preprocess()
        try:
            while True:
                os.exitpoint()                              # 检查是否有退出信号
                with ScopedTiming("total",1):
                    img=pl.get_frame()                      # 获取当前帧数据
                    res=hand_det.run(img)                   # 推理当前帧
                    hand_det.draw_result(pl,res)            # 绘制结果到PipeLine的osd图像
                    pl.show_image()                         # 显示当前的绘制结果
                    gc.collect()                            # 垃圾回收
        except Exception as e:
            sys.print_exception(e)
        finally:
            hand_det.deinit()                               # 反初始化
            pl.destroy()                                    # 销毁PipeLine实例

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


以上代码的主要流程如下，还是和之前的AI相关代码一样：

  1. **采集图像** ：从摄像头（默认摄像头是CSI2接口上的）获得原始图像。
  2. **预处理** ：对图像进行`resize`、`pad`、`normalize`等操作，让输入满足模型的格式要求（[512, 512]）。
  3. **模型推理** ：将图像输入到训练好的人体关键点检测模型（我们这个例子里面是“hand_det”，已经存储在TF卡里面了）得到模型输出。
  4. **后处理** ：根据模型输出获取关键点坐标、得分等信息。
  5. **可视化** ：将检测到的关键点和人体骨架连线画到屏幕或图像上，方便调试或演示。



在主循环中进行循环推理，通过`while True`循环来持续获取帧、推理、绘制，直到用户通过IDE手动停止或出现异常。

套路和之前的例程一样，这里就不再赘述了。

# 3 实际运行 ​

还是先准备一个图片: ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/hand-related/hand-detect/hand-detect_20241226_182145.png)  
在IDE中运行效果如下图所示： ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/hand-related/hand-detect/hand-detect_20241226_182500.png)

# 4 获取手的坐标 ​

上面代码中的`res=hand_det.run(img) # 推理当前帧`，res就是推理出来的结果,res又在`draw_result`中进行计算后知道手的坐标。 我们改造一下`draw_result`如下：

python
    
    
    def draw_result(self, pl, dets):
        with ScopedTiming("display_draw", self.debug_mode > 0):
            if dets:
                pl.osd_img.clear()
                for i, det_box in enumerate(dets):
                    # 提取检测框在模型分辨率上的坐标
                    x1, y1, x2, y2 = det_box[2], det_box[3], det_box[4], det_box[5]
    
                    # 转换为实际屏幕分辨率的坐标
                    screen_x1 = int(x1 * self.display_size[0] / self.rgb888p_size[0])
                    screen_y1 = int(y1 * self.display_size[1] / self.rgb888p_size[1])
                    screen_x2 = int(x2 * self.display_size[0] / self.rgb888p_size[0])
                    screen_y2 = int(y2 * self.display_size[1] / self.rgb888p_size[1])
    
                    # 计算屏幕上的宽高
                    screen_w = screen_x2 - screen_x1
                    screen_h = screen_y2 - screen_y1
    
                    # 输出实际屏幕上的坐标
                    print(f"手{i+1}的屏幕坐标: 左上角({screen_x1}, {screen_y1}), 右下角({screen_x2}, {screen_y2})")
    
                    # 过滤掉不符合条件的小框
                    if (screen_h < (0.1 * self.display_size[1])):
                        continue
                    if (screen_w < (0.25 * self.display_size[0]) and ((screen_x1 < (0.03 * self.display_size[0])) or (screen_x2 > (0.97 * self.display_size[0])))):
                        continue
                    if (screen_w < (0.15 * self.display_size[0]) and ((screen_x1 < (0.01 * self.display_size[0])) or (screen_x2 > (0.99 * self.display_size[0])))):
                        continue
    
                    # 绘制矩形框到屏幕上
                    pl.osd_img.draw_rectangle(screen_x1, screen_y1, screen_w, screen_h, color=(255, 0, 255, 0), thickness=2)
                    pl.osd_img.draw_string_advanced(screen_x1, screen_y1 - 50, 32, f"手{i+1}: " + self.labels[det_box[0]] + f" {round(det_box[1], 2)}", color=(255, 0, 255, 0))
            else:
                print("未检测到手")
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


在IDE中运行效果如下： ![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/hand-related/hand-detect/hand-detect_20241226_183434.png)

