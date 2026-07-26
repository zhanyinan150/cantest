# 人眼注视方向

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/ai-demo/face-related/face-eye-gaze-tracking.html>
> **最后更新**: 2025-01-16

---

# 1 本节介绍 ​

📝本节您将学习如何通过庐山派来识别人眼的注视方向，如无特殊说明，以后所有例程的显示设备均为通过外接[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)，在3.1寸小屏幕上显示。若用户无3.1寸屏幕扩展板也可以正常在IDE的缓冲区，只是受限于USB带宽，可能会帧率较低或卡顿。

🏆学习目标

1️⃣如何用庐山派开发板去识别人眼的注视方向。

庐山派开发板的固件是存储在TF中的，模型文件已经提前写入到固件中了，所以大家只需要复制下面的代码到IDE，传递到开发板上就可以正常运行了。**无需再额外拷贝** ，至于后面需要拷贝自己训练的模型那就是后话了。

本节的主要内容就是通过人脸和眼睛的特征来确定人的注视方向。可用于人机交互，辅助设备与无障碍技术，驾驶员疲劳驾驶或分心检测等领域。

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
    import image
    import aidemo
    import random
    import gc
    import sys
    import math
    
    # 自定义人脸检测任务类
    class FaceDetApp(AIBase):
        def __init__(self,kmodel_path,model_input_size,anchors,confidence_threshold=0.25,nms_threshold=0.3,rgb888p_size=[1280,720],display_size=[1920,1080],debug_mode=0):
            super().__init__(kmodel_path,model_input_size,rgb888p_size,debug_mode)
            # kmodel路径
            self.kmodel_path=kmodel_path
            # 检测模型输入分辨率
            self.model_input_size=model_input_size
            # 置信度阈值
            self.confidence_threshold=confidence_threshold
            # nms阈值
            self.nms_threshold=nms_threshold
            self.anchors=anchors
            # sensor给到AI的图像分辨率，宽16字节对齐
            self.rgb888p_size=[ALIGN_UP(rgb888p_size[0],16),rgb888p_size[1]]
            # 视频输出VO分辨率，宽16字节对齐
            self.display_size=[ALIGN_UP(display_size[0],16),display_size[1]]
            # debug模式
            self.debug_mode=debug_mode
            # Ai2d实例，用于实现模型预处理
            self.ai2d=Ai2d(debug_mode)
            # 设置Ai2d的输入输出格式和类型
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT,nn.ai2d_format.NCHW_FMT,np.uint8, np.uint8)
    
        # 配置预处理操作，这里使用了padding和resize，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
        def config_preprocess(self,input_image_size=None):
            with ScopedTiming("set preprocess config",self.debug_mode > 0):
                # 初始化ai2d预处理配置，默认为sensor给到AI的尺寸，可以通过设置input_image_size自行修改输入尺寸
                ai2d_input_size=input_image_size if input_image_size else self.rgb888p_size
                # 设置padding预处理
                self.ai2d.pad(self.get_pad_param(), 0, [104,117,123])
                # 设置resize预处理
                self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
                # 构建预处理流程,参数为预处理输入tensor的shape和预处理输出的tensor的shape
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])
    
        # 自定义任务后处理,这里使用了aidemo库的face_det_post_process接口
        def postprocess(self,results):
            with ScopedTiming("postprocess",self.debug_mode > 0):
                res = aidemo.face_det_post_process(self.confidence_threshold,self.nms_threshold,self.model_input_size[0],self.anchors,self.rgb888p_size,results)
                if len(res)==0:
                    return res
                else:
                    return res[0]
    
        # 计算padding参数
        def get_pad_param(self):
            dst_w = self.model_input_size[0]
            dst_h = self.model_input_size[1]
            # 计算最小的缩放比例，等比例缩放
            ratio_w = dst_w / self.rgb888p_size[0]
            ratio_h = dst_h / self.rgb888p_size[1]
            if ratio_w < ratio_h:
                ratio = ratio_w
            else:
                ratio = ratio_h
            new_w = (int)(ratio * self.rgb888p_size[0])
            new_h = (int)(ratio * self.rgb888p_size[1])
            dw = (dst_w - new_w) / 2
            dh = (dst_h - new_h) / 2
            top = (int)(round(0))
            bottom = (int)(round(dh * 2 + 0.1))
            left = (int)(round(0))
            right = (int)(round(dw * 2 - 0.1))
            return [0,0,0,0,top, bottom, left, right]
    
    # 自定义注视估计任务类
    class EyeGazeApp(AIBase):
        def __init__(self,kmodel_path,model_input_size,rgb888p_size=[1920,1080],display_size=[1920,1080],debug_mode=0):
            super().__init__(kmodel_path,model_input_size,rgb888p_size,debug_mode)
            # kmodel路径
            self.kmodel_path=kmodel_path
            # 注视估计模型输入分辨率
            self.model_input_size=model_input_size
            # sensor给到AI的图像分辨率，宽16字节对齐
            self.rgb888p_size=[ALIGN_UP(rgb888p_size[0],16),rgb888p_size[1]]
            # 视频输出VO分辨率，宽16字节对齐
            self.display_size=[ALIGN_UP(display_size[0],16),display_size[1]]
            # debug模式
            self.debug_mode=debug_mode
            # Ai2d实例，用于实现模型预处理
            self.ai2d=Ai2d(debug_mode)
            # 设置Ai2d的输入输出格式和类型
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT,nn.ai2d_format.NCHW_FMT,np.uint8, np.uint8)
    
        # 配置预处理操作，这里使用了crop和resize，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
        def config_preprocess(self,det,input_image_size=None):
            with ScopedTiming("set preprocess config",self.debug_mode > 0):
                # 初始化ai2d预处理配置
                ai2d_input_size=input_image_size if input_image_size else self.rgb888p_size
                # 计算crop预处理参数
                x, y, w, h = map(lambda x: int(round(x, 0)), det[:4])
                # 设置crop预处理
                self.ai2d.crop(x,y,w,h)
                # 设置resize预处理
                self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
                # 构建预处理流程,参数为预处理输入tensor的shape和预处理输出的tensor的shape
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])
    
        # 自定义后处理，results是模型输出的array列表，这里调用了aidemo库的eye_gaze_post_process接口
        def postprocess(self,results):
            with ScopedTiming("postprocess",self.debug_mode > 0):
                post_ret = aidemo.eye_gaze_post_process(results)
                return post_ret[0],post_ret[1]
    
    # 自定义注视估计类
    class EyeGaze:
        def __init__(self,face_det_kmodel,eye_gaze_kmodel,det_input_size,eye_gaze_input_size,anchors,confidence_threshold=0.25,nms_threshold=0.3,rgb888p_size=[1920,1080],display_size=[1920,1080],debug_mode=0):
            # 人脸检测模型路径
            self.face_det_kmodel=face_det_kmodel
            # 人脸注视估计模型路径
            self.eye_gaze_kmodel=eye_gaze_kmodel
            # 人脸检测模型输入分辨率
            self.det_input_size=det_input_size
            # 人脸注视估计模型输入分辨率
            self.eye_gaze_input_size=eye_gaze_input_size
            # anchors
            self.anchors=anchors
            # 置信度阈值
            self.confidence_threshold=confidence_threshold
            # nms阈值
            self.nms_threshold=nms_threshold
            # sensor给到AI的图像分辨率，宽16字节对齐
            self.rgb888p_size=[ALIGN_UP(rgb888p_size[0],16),rgb888p_size[1]]
            # 视频输出VO分辨率，宽16字节对齐
            self.display_size=[ALIGN_UP(display_size[0],16),display_size[1]]
            # debug_mode模式
            self.debug_mode=debug_mode
            # 人脸检测实例
            self.face_det=FaceDetApp(self.face_det_kmodel,model_input_size=self.det_input_size,anchors=self.anchors,confidence_threshold=self.confidence_threshold,nms_threshold=self.nms_threshold,rgb888p_size=self.rgb888p_size,display_size=self.display_size,debug_mode=0)
            # 注视估计实例
            self.eye_gaze=EyeGazeApp(self.eye_gaze_kmodel,model_input_size=self.eye_gaze_input_size,rgb888p_size=self.rgb888p_size,display_size=self.display_size)
            # 人脸检测配置预处理
            self.face_det.config_preprocess()
    
        #run方法
        def run(self,input_np):
            # 先进行人脸检测
            det_boxes=self.face_det.run(input_np)
            eye_gaze_res=[]
            for det_box in det_boxes:
                # 对每一个检测到的人脸做注视估计
                self.eye_gaze.config_preprocess(det_box)
                pitch,yaw=self.eye_gaze.run(input_np)
                eye_gaze_res.append((pitch,yaw))
            return det_boxes,eye_gaze_res
    
        # 绘制注视估计效果
        def draw_result(self,pl,dets,eye_gaze_res):
            pl.osd_img.clear()
            if dets:
                for det,gaze_ret in zip(dets,eye_gaze_res):
                    pitch , yaw = gaze_ret
                    length = self.display_size[0]/ 2
                    x, y, w, h = map(lambda x: int(round(x, 0)), det[:4])
                    x = x * self.display_size[0] // self.rgb888p_size[0]
                    y = y * self.display_size[1] // self.rgb888p_size[1]
                    w = w * self.display_size[0] // self.rgb888p_size[0]
                    h = h * self.display_size[1] // self.rgb888p_size[1]
                    center_x = (x + w / 2.0)
                    center_y = (y + h / 2.0)
                    dx = -length * math.sin(pitch) * math.cos(yaw)
                    target_x = int(center_x + dx)
                    dy = -length * math.sin(yaw)
                    target_y = int(center_y + dy)
                    pl.osd_img.draw_arrow(int(center_x), int(center_y), target_x, target_y, color = (255,255,0,0), size = 30, thickness = 2)
    
    
    if __name__=="__main__":
        # 显示模式，默认"hdmi",可以选择"hdmi"和"lcd"，k230d受限于内存不支持
        display_mode="lcd"
        if display_mode=="hdmi":
            display_size=[1920,1080]
        else:
            display_size=[800,480]
        # 人脸检测模型路径
        face_det_kmodel_path="/sdcard/examples/kmodel/face_detection_320.kmodel"
        # 人脸注视估计模型路径
        eye_gaze_kmodel_path="/sdcard/examples/kmodel/eye_gaze.kmodel"
        # 其他参数
        anchors_path="/sdcard/examples/utils/prior_data_320.bin"
        rgb888p_size=[1920,1080]
        face_det_input_size=[320,320]
        eye_gaze_input_size=[448,448]
        confidence_threshold=0.5
        nms_threshold=0.2
        anchor_len=4200
        det_dim=4
        anchors = np.fromfile(anchors_path, dtype=np.float)
        anchors = anchors.reshape((anchor_len,det_dim))
    
        # 初始化PipeLine，只关注传给AI的图像分辨率，显示的分辨率
        pl=PipeLine(rgb888p_size=rgb888p_size,display_size=display_size,display_mode=display_mode)
        pl.create()
        eg=EyeGaze(face_det_kmodel_path,eye_gaze_kmodel_path,det_input_size=face_det_input_size,eye_gaze_input_size=eye_gaze_input_size,anchors=anchors,confidence_threshold=confidence_threshold,nms_threshold=nms_threshold,rgb888p_size=rgb888p_size,display_size=display_size)
        try:
            while True:
                os.exitpoint()
                with ScopedTiming("total",1):
                    img=pl.get_frame()                          # 获取当前帧
                    det_boxes,eye_gaze_res=eg.run(img)          # 推理当前帧
                    eg.draw_result(pl,det_boxes,eye_gaze_res)   # 绘制推理效果
                    pl.show_image()                             # 展示推理效果
                    gc.collect()
        except Exception as e:
            sys.print_exception(e)
        finally:
            eg.face_det.deinit()
            eg.eye_gaze.deinit()
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


以上程序的主要流程如下：

  1. 通过Pipline从摄像头获取当前视频的帧数据（img）用于推理，另一路直接提供给显示设备（在这里是我们的3.1寸屏幕）。
  2. 先进行人脸检测，用提前已经部署在TF卡中的人脸检测模型（`face_detection_320.kmodel`）对获取的视频帧进行人脸区域检测。
  3. 再进行注视方向的估计，对检测出的人脸区域进行二次处理，将提取的脸部局部区域输入到眼部注视方向估计模型（`eye_gaze.kmodel`）中，得到眼睛注视方向的俯仰角（pitch）和偏航角（yaw）。



本案例中，人脸检测模型输入为320x320，眼部注视模型输入为448x448。通过`Ai2d`进行图像预处理时，我们先对原始图像进行适当的裁剪（crop）和缩放（resize），来确保输入模型的数据格式和维度符合预期。

人脸检测通过`confidence_threshold`过滤低置信度目标（在本例中为0.5），又通过`nms_threshold`来进行非极大值抑制（NMS）（在本例中为0.2），减少重复检测框的出现。调整这两个参数会显著影响检测效果和稳定性。大家可以自己调节调节试试。

注视模型通过对眼部区域的特征提取，输出两个角度（pitch和yaw），可视为眼球在俯仰和左右偏转方向上的旋转度数。

  * `pitch`：上下方向偏转角度（俯仰角）。

  * `yaw`：左右方向偏转角度（偏航角）。




知道上面这两个信息之后就利用基本的三角函数关系，通过一个简单的向量计算，将注视角度转换为在图像坐标系上的一个红色箭头指示。

# 3 实际运行 ​

还是老三样，先准备图片： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-orientation-detection/face-orientation-detection_20241219_150812.png)  
在IDE中运行效果如下所示： ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-eye-gaze-tracking/face-eye-gaze-tracking_20241220_161053.png)  
我这里是检测到了两个人脸，各有一个箭头表示这两个人的注视方向。可以看到左边的人看向右边，而右边的人则看向左边。

写到这里我就很想试试如果被检测的人是斗鸡眼会发生什么情况：

那就再准备一个人在做斗鸡眼（没事两个眼睛就是同时向内看，双眼彼此靠近的搞怪眼神）的图片：

![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-eye-gaze-tracking/face-eye-gaze-tracking_20241220_162334.png) 在IDE中运行效果如下所示： ![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-eye-gaze-tracking/face-eye-gaze-tracking_20241220_162517.gif)  
可以看到这个箭头在来回变化，就像是一个没有方向感的人在之路，一会说朝左一会朝右。像是到了边界调节。模型无法轻松判断注视方向了。希望各位大佬自己来训练个模型能解决这个问题，其实说实话我也不知道人在做斗鸡眼的时候到底是看向哪里？难道是看向鼻子内部嘛哈哈，箭头好像也不好表示。

