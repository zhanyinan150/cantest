# 人体检测

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/ai-demo/body-related/body-detect.html>
> **最后更新**: 2024-12-25

---

# 1 本节介绍 ​

📝本节您将学习如何通过庐山派来人体，如无特殊说明，以后所有例程的显示设备均为通过外接[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)，在3.1寸小屏幕上显示。若用户无3.1寸屏幕扩展板也可以正常在IDE的缓冲区，只是受限于USB带宽，可能会帧率较低或卡顿。

🏆学习目标

1️⃣如何用庐山派开发板去进行人体检测。

庐山派开发板的固件是存储在TF中的，模型文件已经提前写入到固件中了，所以大家只需要复制下面的代码到IDE，传递到开发板上就可以正常运行了。**无需再额外拷贝** ，至于后面需要拷贝自己训练的模型那就是后话了。

本节的主要内容就是识别和定位图像或视频中存在的人体。可用于入侵检测，人数统计，行人检测或闯红灯检测等领域。

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
    
    # 自定义人体检测类
    class PersonDetectionApp(AIBase):
        def __init__(self,kmodel_path,model_input_size,labels,anchors,confidence_threshold=0.2,nms_threshold=0.5,nms_option=False,strides=[8,16,32],rgb888p_size=[224,224],display_size=[1920,1080],debug_mode=0):
            super().__init__(kmodel_path,model_input_size,rgb888p_size,debug_mode)
            self.kmodel_path=kmodel_path
            # 模型输入分辨率
            self.model_input_size=model_input_size
            # 标签
            self.labels=labels
            # 检测anchors设置
            self.anchors=anchors
            # 特征图降采样倍数
            self.strides=strides
            # 置信度阈值设置
            self.confidence_threshold=confidence_threshold
            # nms阈值设置
            self.nms_threshold=nms_threshold
            self.nms_option=nms_option
            # sensor给到AI的图像分辨率
            self.rgb888p_size=[ALIGN_UP(rgb888p_size[0],16),rgb888p_size[1]]
            # 显示分辨率
            self.display_size=[ALIGN_UP(display_size[0],16),display_size[1]]
            self.debug_mode=debug_mode
            # Ai2d实例，用于实现模型预处理
            self.ai2d=Ai2d(debug_mode)
            # 设置Ai2d的输入输出格式和类型
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT,nn.ai2d_format.NCHW_FMT,np.uint8, np.uint8)
    
        # 配置预处理操作，这里使用了pad和resize，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
        def config_preprocess(self,input_image_size=None):
            with ScopedTiming("set preprocess config",self.debug_mode > 0):
                # 初始化ai2d预处理配置，默认为sensor给到AI的尺寸，您可以通过设置input_image_size自行修改输入尺寸
                ai2d_input_size=input_image_size if input_image_size else self.rgb888p_size
                top,bottom,left,right=self.get_padding_param()
                self.ai2d.pad([0,0,0,0,top,bottom,left,right], 0, [0,0,0])
                self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])
    
        # 自定义当前任务的后处理
        def postprocess(self,results):
            with ScopedTiming("postprocess",self.debug_mode > 0):
                # 这里使用了aicube模型的后处理接口anchorbasedet_post_preocess
                dets = aicube.anchorbasedet_post_process(results[0], results[1], results[2], self.model_input_size, self.rgb888p_size, self.strides, len(self.labels), self.confidence_threshold, self.nms_threshold, self.anchors, self.nms_option)
                return dets
    
        # 绘制结果
        def draw_result(self,pl,dets):
            with ScopedTiming("display_draw",self.debug_mode >0):
                if dets:
                    pl.osd_img.clear()
                    for det_box in dets:
                        x1, y1, x2, y2 = det_box[2],det_box[3],det_box[4],det_box[5]
                        w = float(x2 - x1) * self.display_size[0] // self.rgb888p_size[0]
                        h = float(y2 - y1) * self.display_size[1] // self.rgb888p_size[1]
                        x1 = int(x1 * self.display_size[0] // self.rgb888p_size[0])
                        y1 = int(y1 * self.display_size[1] // self.rgb888p_size[1])
                        x2 = int(x2 * self.display_size[0] // self.rgb888p_size[0])
                        y2 = int(y2 * self.display_size[1] // self.rgb888p_size[1])
                        if (h<(0.1*self.display_size[0])):
                            continue
                        if (w<(0.25*self.display_size[0]) and ((x1<(0.03*self.display_size[0])) or (x2>(0.97*self.display_size[0])))):
                            continue
                        if (w<(0.15*self.display_size[0]) and ((x1<(0.01*self.display_size[0])) or (x2>(0.99*self.display_size[0])))):
                            continue
                        pl.osd_img.draw_rectangle(x1 , y1 , int(w) , int(h), color=(255, 0, 255, 0), thickness = 2)
                        pl.osd_img.draw_string_advanced( x1 , y1-50,32, " " + self.labels[det_box[0]] + " " + str(round(det_box[1],2)), color=(255,0, 255, 0))
                else:
                    pl.osd_img.clear()
    
        # 计算padding参数
        def get_padding_param(self):
            dst_w = self.model_input_size[0]
            dst_h = self.model_input_size[1]
            input_width = self.rgb888p_size[0]
            input_high = self.rgb888p_size[1]
            ratio_w = dst_w / input_width
            ratio_h = dst_h / input_high
            if ratio_w < ratio_h:
                ratio = ratio_w
            else:
                ratio = ratio_h
            new_w = (int)(ratio * input_width)
            new_h = (int)(ratio * input_high)
            dw = (dst_w - new_w) / 2
            dh = (dst_h - new_h) / 2
            top = int(round(dh - 0.1))
            bottom = int(round(dh + 0.1))
            left = int(round(dw - 0.1))
            right = int(round(dw - 0.1))
            return  top, bottom, left, right
    
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
        kmodel_path="/sdcard/examples/kmodel/person_detect_yolov5n.kmodel"
        # 其它参数设置
        confidence_threshold = 0.2
        nms_threshold = 0.6
        labels = ["person"]
        anchors = [10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326]
    
        # 初始化PipeLine
        pl=PipeLine(rgb888p_size=rgb888p_size,display_size=display_size,display_mode=display_mode)
        pl.create()
        # 初始化自定义人体检测实例
        person_det=PersonDetectionApp(kmodel_path,model_input_size=[640,640],labels=labels,anchors=anchors,confidence_threshold=confidence_threshold,nms_threshold=nms_threshold,nms_option=False,strides=[8,16,32],rgb888p_size=rgb888p_size,display_size=display_size,debug_mode=0)
        person_det.config_preprocess()
        try:
            while True:
                os.exitpoint()
                with ScopedTiming("total",1):
                    # 获取当前帧数据
                    img=pl.get_frame()
                    # 推理当前帧
                    res=person_det.run(img)
                    # 绘制结果到PipeLine的osd图像
                    person_det.draw_result(pl,res)
                    # 显示当前的绘制结果
                    pl.show_image()
                    gc.collect()
        except Exception as e:
            sys.print_exception(e)
        finally:
            person_det.deinit()
            pl.destroy()

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


以上代码实现了人体检测，使用的模型是基于`YOLOv5n` 的，他是YOLOv5系列中比较小巧的版本，占用内存小，推理速度更快，精度会有一定的损失，适合在嵌入式平台上部署。

【推理阶段】上面模型的推理由`person_det.run(img)`实现。`run`方法利用神经网络模型对输入的图像帧进行处理，输出检测的结果。还和之前介绍的一样，使用`Ai2d`类进行图像预处理。

【后处理阶段】检测结果的后处理通过`anchorbasedet_post_process`方法实现，主要功能是将模型的原始输出转换为我们可理解的边界框格式，想了解更深需要去看嘉楠的文档。通过锚点（anchors）、降采样倍率（strides）等参数生成每个检测目标的边界框和置信度。

如果对当前模型效果不满意，可以通过调整代码里的`confidence_threshold` 来控制检测的置信度阈值；值越大，漏检率可能提高，但误检率会降低。也可以调整 `nms_threshold` 来控制后处理 NMS（非极大值抑制）时的合并阈值；值越大，合并框更少；值越小，更容易产生多个重叠检测框。如果还不满意的话可以自行训练人体检测模型。

在这里返回的`dets`为一个列表，包含检测目标的信息，每个目标以一个数组表示，格式为：

python
    
    
    [label_index, confidence, x1, y1, x2, y2]

1  


`label_index`是标签索引（在本例中`0`表示“person”），`confidence`为置信度，`x1, y1, x2, y2`为边界框的坐标。

【人体坐标获取与显示】在`draw_result`方法中，将边界框转换为实际显示分辨率对应的坐标，并绘制到显示设备（我们的3.1寸屏幕）上。首先进行坐标转换：

根据目标框的模型坐标（基于`model_input_size`）和原图分辨率（`rgb888p_size`）之间的比例，计算在显示分辨率（`display_size`）中的实际坐标：

python
    
    
    x1, y1, x2, y2 = det_box[2],det_box[3],det_box[4],det_box[5]
    w = float(x2 - x1) * self.display_size[0] // self.rgb888p_size[0]
    h = float(y2 - y1) * self.display_size[1] // self.rgb888p_size[1]
    x1 = int(x1 * self.display_size[0] // self.rgb888p_size[0])
    y1 = int(y1 * self.display_size[1] // self.rgb888p_size[1])
    x2 = int(x2 * self.display_size[0] // self.rgb888p_size[0])
    y2 = int(y2 * self.display_size[1] // self.rgb888p_size[1])

1  
2  
3  
4  
5  
6  
7  


转换后，`x1, y1`为左上角坐标，`x2, y2`为右下角坐标，根据这个坐标我们就能在屏幕上框出人体了。

# 3 实际运行 ​

还是先准备一个图片: ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/body-related/body-detect/body-detect_20241225_142929.png) 在IDE中运行效果如下图所示： ![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/body-related/body-detect/body-detect_20241225_143521.png)

