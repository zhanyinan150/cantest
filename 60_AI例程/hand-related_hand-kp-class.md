# 手部关键点分类

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/ai-demo/hand-related/hand-kp-class.html>
> **最后更新**: 2024-12-27

---

# 1 本节介绍 ​

📝本节您将学习如何通过庐山派来进行手部关键点分类，如无特殊说明，以后所有例程的显示设备均为通过外接[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)，在3.1寸小屏幕上显示。若用户无3.1寸屏幕扩展板也可以正常在IDE的缓冲区，只是受限于USB带宽，可能会帧率较低或卡顿。

🏆学习目标

1️⃣如何用庐山派开发板去进行手部关键点分类。

庐山派开发板的固件是存储在TF中的，模型文件已经提前写入到固件中了，所以大家只需要复制下面的代码到IDE，传递到开发板上就可以正常运行了。**无需再额外拷贝** ，至于后面需要拷贝自己训练的模型那就是后话了。

本文主要介绍如何在庐山派开发板上进行手部关键点分类。可以用于手势控制，如VR，体感游戏，虚拟演奏，行为监控等场景。下面用到了两个模型**手部检测模型** 以及**手部关键点模型** 。

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
    import aicube
    import random
    import gc
    import sys
    
    # 自定义手掌检测任务类
    class HandDetApp(AIBase):
        def __init__(self,kmodel_path,labels,model_input_size,anchors,confidence_threshold=0.2,nms_threshold=0.5,nms_option=False, strides=[8,16,32],rgb888p_size=[1920,1080],display_size=[1920,1080],debug_mode=0):
            super().__init__(kmodel_path,model_input_size,rgb888p_size,debug_mode)
            # kmodel路径
            self.kmodel_path=kmodel_path
            self.labels=labels
            # 检测模型输入分辨率
            self.model_input_size=model_input_size
            # 置信度阈值
            self.confidence_threshold=confidence_threshold
            # nms阈值
            self.nms_threshold=nms_threshold
            # 锚框,目标检测任务使用
            self.anchors=anchors
            # 特征下采样倍数
            self.strides = strides
            # NMS选项，如果为True做类间NMS,如果为False做类内NMS
            self.nms_option = nms_option
            # sensor给到AI的图像分辨率，宽16字节对齐
            self.rgb888p_size=[ALIGN_UP(rgb888p_size[0],16),rgb888p_size[1]]
            # 视频输出VO分辨率，宽16字节对齐
            self.display_size=[ALIGN_UP(display_size[0],16),display_size[1]]
            # debug模式
            self.debug_mode=debug_mode
            # Ai2d实例用于实现预处理
            self.ai2d=Ai2d(debug_mode)
            # 设置ai2d的输入输出的格式和数据类型
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT,nn.ai2d_format.NCHW_FMT,np.uint8, np.uint8)
    
        # 配置预处理操作，这里使用了pad和resize，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
        def config_preprocess(self,input_image_size=None):
            with ScopedTiming("set preprocess config",self.debug_mode > 0):
                # 初始化ai2d预处理配置，默认为sensor给到AI的尺寸，可以通过设置input_image_size自行修改输入尺寸
                ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
                # 计算padding参数并应用pad操作，以确保输入图像尺寸与模型输入尺寸匹配
                top, bottom, left, right = self.get_padding_param()
                self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [114, 114, 114])
                # 使用双线性插值进行resize操作，调整图像尺寸以符合模型输入要求
                self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
                # 构建预处理流程,参数为预处理输入tensor的shape和预处理输出的tensor的shape
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])
    
        # 自定义当前任务的后处理，用于处理模型输出结果，这里使用了aicube库的anchorbasedet_post_process接口
        def postprocess(self,results):
            with ScopedTiming("postprocess",self.debug_mode > 0):
                dets = aicube.anchorbasedet_post_process(results[0], results[1], results[2], self.model_input_size, self.rgb888p_size, self.strides, len(self.labels), self.confidence_threshold, self.nms_threshold, self.anchors, self.nms_option)
                # 返回手掌检测结果
                return dets
    
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
    
    # 自定义手势关键点分类任务类
    class HandKPClassApp(AIBase):
        def __init__(self,kmodel_path,model_input_size,rgb888p_size=[1920,1080],display_size=[1920,1080],debug_mode=0):
            super().__init__(kmodel_path,model_input_size,rgb888p_size,debug_mode)
            # kmodel路径
            self.kmodel_path=kmodel_path
            # 检测模型输入分辨率
            self.model_input_size=model_input_size
            # sensor给到AI的图像分辨率，宽16字节对齐
            self.rgb888p_size=[ALIGN_UP(rgb888p_size[0],16),rgb888p_size[1]]
            # 视频输出VO分辨率，宽16字节对齐
            self.display_size=[ALIGN_UP(display_size[0],16),display_size[1]]
            self.crop_params=[]
            # debug模式
            self.debug_mode=debug_mode
            # Ai2d实例用于实现预处理
            self.ai2d=Ai2d(debug_mode)
            # 设置ai2d的输入输出的格式和数据类型
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT,nn.ai2d_format.NCHW_FMT,np.uint8, np.uint8)
    
        # 配置预处理操作，这里使用了crop和resize，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
        def config_preprocess(self,det,input_image_size=None):
            with ScopedTiming("set preprocess config",self.debug_mode > 0):
                ai2d_input_size=input_image_size if input_image_size else self.rgb888p_size
                self.crop_params = self.get_crop_param(det)
                self.ai2d.crop(self.crop_params[0],self.crop_params[1],self.crop_params[2],self.crop_params[3])
                self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])
    
        # 自定义后处理，得到手掌手势结果和手掌关键点数据
        def postprocess(self,results):
            with ScopedTiming("postprocess",self.debug_mode > 0):
                results=results[0].reshape(results[0].shape[0]*results[0].shape[1])
                results_show = np.zeros(results.shape,dtype=np.int16)
                results_show[0::2] = results[0::2] * self.crop_params[3] + self.crop_params[0]
                results_show[1::2] = results[1::2] * self.crop_params[2] + self.crop_params[1]
                gesture=self.hk_gesture(results_show)
                results_show[0::2] = results_show[0::2] * (self.display_size[0] / self.rgb888p_size[0])
                results_show[1::2] = results_show[1::2] * (self.display_size[1] / self.rgb888p_size[1])
                return results_show,gesture
    
        # 计算crop参数
        def get_crop_param(self,det_box):
            x1, y1, x2, y2 = det_box[2],det_box[3],det_box[4],det_box[5]
            w,h= int(x2 - x1),int(y2 - y1)
            w_det = int(float(x2 - x1) * self.display_size[0] // self.rgb888p_size[0])
            h_det = int(float(y2 - y1) * self.display_size[1] // self.rgb888p_size[1])
            x_det = int(x1*self.display_size[0] // self.rgb888p_size[0])
            y_det = int(y1*self.display_size[1] // self.rgb888p_size[1])
            length = max(w, h)/2
            cx = (x1+x2)/2
            cy = (y1+y2)/2
            ratio_num = 1.26*length
            x1_kp = int(max(0,cx-ratio_num))
            y1_kp = int(max(0,cy-ratio_num))
            x2_kp = int(min(self.rgb888p_size[0]-1, cx+ratio_num))
            y2_kp = int(min(self.rgb888p_size[1]-1, cy+ratio_num))
            w_kp = int(x2_kp - x1_kp + 1)
            h_kp = int(y2_kp - y1_kp + 1)
            return [x1_kp, y1_kp, w_kp, h_kp]
    
        # 求两个vector之间的夹角
        def hk_vector_2d_angle(self,v1,v2):
            with ScopedTiming("hk_vector_2d_angle",self.debug_mode > 0):
                v1_x,v1_y,v2_x,v2_y = v1[0],v1[1],v2[0],v2[1]
                v1_norm = np.sqrt(v1_x * v1_x+ v1_y * v1_y)
                v2_norm = np.sqrt(v2_x * v2_x + v2_y * v2_y)
                dot_product = v1_x * v2_x + v1_y * v2_y
                cos_angle = dot_product/(v1_norm*v2_norm)
                angle = np.acos(cos_angle)*180/np.pi
                return angle
    
        # 根据手掌关键点检测结果判断手势类别
        def hk_gesture(self,results):
            with ScopedTiming("hk_gesture",self.debug_mode > 0):
                angle_list = []
                for i in range(5):
                    angle = self.hk_vector_2d_angle([(results[0]-results[i*8+4]), (results[1]-results[i*8+5])],[(results[i*8+6]-results[i*8+8]),(results[i*8+7]-results[i*8+9])])
                    angle_list.append(angle)
                thr_angle,thr_angle_thumb,thr_angle_s,gesture_str = 65.,53.,49.,None
                if 65535. not in angle_list:
                    if (angle_list[0]>thr_angle_thumb)  and (angle_list[1]>thr_angle) and (angle_list[2]>thr_angle) and (angle_list[3]>thr_angle) and (angle_list[4]>thr_angle):
                        gesture_str = "fist"
                    elif (angle_list[0]<thr_angle_s)  and (angle_list[1]<thr_angle_s) and (angle_list[2]<thr_angle_s) and (angle_list[3]<thr_angle_s) and (angle_list[4]<thr_angle_s):
                        gesture_str = "five"
                    elif (angle_list[0]<thr_angle_s)  and (angle_list[1]<thr_angle_s) and (angle_list[2]>thr_angle) and (angle_list[3]>thr_angle) and (angle_list[4]>thr_angle):
                        gesture_str = "gun"
                    elif (angle_list[0]<thr_angle_s)  and (angle_list[1]<thr_angle_s) and (angle_list[2]>thr_angle) and (angle_list[3]>thr_angle) and (angle_list[4]<thr_angle_s):
                        gesture_str = "love"
                    elif (angle_list[0]>5)  and (angle_list[1]<thr_angle_s) and (angle_list[2]>thr_angle) and (angle_list[3]>thr_angle) and (angle_list[4]>thr_angle):
                        gesture_str = "one"
                    elif (angle_list[0]<thr_angle_s)  and (angle_list[1]>thr_angle) and (angle_list[2]>thr_angle) and (angle_list[3]>thr_angle) and (angle_list[4]<thr_angle_s):
                        gesture_str = "six"
                    elif (angle_list[0]>thr_angle_thumb)  and (angle_list[1]<thr_angle_s) and (angle_list[2]<thr_angle_s) and (angle_list[3]<thr_angle_s) and (angle_list[4]>thr_angle):
                        gesture_str = "three"
                    elif (angle_list[0]<thr_angle_s)  and (angle_list[1]>thr_angle) and (angle_list[2]>thr_angle) and (angle_list[3]>thr_angle) and (angle_list[4]>thr_angle):
                        gesture_str = "thumbUp"
                    elif (angle_list[0]>thr_angle_thumb)  and (angle_list[1]<thr_angle_s) and (angle_list[2]<thr_angle_s) and (angle_list[3]>thr_angle) and (angle_list[4]>thr_angle):
                        gesture_str = "yeah"
                return gesture_str
    
    # 手掌关键点分类任务
    class HandKeyPointClass:
        def __init__(self,hand_det_kmodel,hand_kp_kmodel,det_input_size,kp_input_size,labels,anchors,confidence_threshold=0.25,nms_threshold=0.3,nms_option=False,strides=[8,16,32],rgb888p_size=[1280,720],display_size=[1920,1080],debug_mode=0):
            # 手掌检测模型路径
            self.hand_det_kmodel=hand_det_kmodel
            # 手掌关键点模型路径
            self.hand_kp_kmodel=hand_kp_kmodel
            # 手掌检测模型输入分辨率
            self.det_input_size=det_input_size
            # 手掌关键点模型输入分辨率
            self.kp_input_size=kp_input_size
            self.labels=labels
            # anchors
            self.anchors=anchors
            # 置信度阈值
            self.confidence_threshold=confidence_threshold
            # nms阈值
            self.nms_threshold=nms_threshold
            self.nms_option=nms_option
            self.strides=strides
            # sensor给到AI的图像分辨率，宽16字节对齐
            self.rgb888p_size=[ALIGN_UP(rgb888p_size[0],16),rgb888p_size[1]]
            # 视频输出VO分辨率，宽16字节对齐
            self.display_size=[ALIGN_UP(display_size[0],16),display_size[1]]
            # debug_mode模式
            self.debug_mode=debug_mode
            self.hand_det=HandDetApp(self.hand_det_kmodel,self.labels,model_input_size=self.det_input_size,anchors=self.anchors,confidence_threshold=self.confidence_threshold,nms_threshold=self.nms_threshold,nms_option=self.nms_option,strides=self.strides,rgb888p_size=self.rgb888p_size,display_size=self.display_size,debug_mode=0)
            self.hand_kp=HandKPClassApp(self.hand_kp_kmodel,model_input_size=self.kp_input_size,rgb888p_size=self.rgb888p_size,display_size=self.display_size)
            self.hand_det.config_preprocess()
    
        # run函数
        def run(self,input_np):
            # 执行手掌检测
            det_boxes=self.hand_det.run(input_np)
            boxes=[]
            gesture_res=[]
            for det_box in det_boxes:
                # 对于检测到的每一个手掌执行关键点识别
                x1, y1, x2, y2 = det_box[2],det_box[3],det_box[4],det_box[5]
                w,h= int(x2 - x1),int(y2 - y1)
                if (h<(0.1*self.rgb888p_size[1])):
                    continue
                if (w<(0.25*self.rgb888p_size[0]) and ((x1<(0.03*self.rgb888p_size[0])) or (x2>(0.97*self.rgb888p_size[0])))):
                    continue
                if (w<(0.15*self.rgb888p_size[0]) and ((x1<(0.01*self.rgb888p_size[0])) or (x2>(0.99*self.rgb888p_size[0])))):
                    continue
                self.hand_kp.config_preprocess(det_box)
                results_show,gesture=self.hand_kp.run(input_np)
                gesture_res.append((results_show,gesture))
                boxes.append(det_box)
            return boxes,gesture_res
    
        # 绘制效果，绘制关键点、手掌检测框和识别结果
        def draw_result(self,pl,dets,gesture_res):
            pl.osd_img.clear()
            if len(dets)>0:
                for k in range(len(dets)):
                    det_box=dets[k]
                    x1, y1, x2, y2 = det_box[2],det_box[3],det_box[4],det_box[5]
                    w,h= int(x2 - x1),int(y2 - y1)
                    if (h<(0.1*self.rgb888p_size[1])):
                        continue
                    if (w<(0.25*self.rgb888p_size[0]) and ((x1<(0.03*self.rgb888p_size[0])) or (x2>(0.97*self.rgb888p_size[0])))):
                        continue
                    if (w<(0.15*self.rgb888p_size[0]) and ((x1<(0.01*self.rgb888p_size[0])) or (x2>(0.99*self.rgb888p_size[0])))):
                        continue
                    w_det = int(float(x2 - x1) * self.display_size[0] // self.rgb888p_size[0])
                    h_det = int(float(y2 - y1) * self.display_size[1] // self.rgb888p_size[1])
                    x_det = int(x1*self.display_size[0] // self.rgb888p_size[0])
                    y_det = int(y1*self.display_size[1] // self.rgb888p_size[1])
                    pl.osd_img.draw_rectangle(x_det, y_det, w_det, h_det, color=(255, 0, 255, 0), thickness = 2)
    
                    results_show=gesture_res[k][0]
                    for i in range(len(results_show)/2):
                        pl.osd_img.draw_circle(results_show[i*2], results_show[i*2+1], 1, color=(255, 0, 255, 0),fill=False)
                    for i in range(5):
                        j = i*8
                        if i==0:
                            R = 255; G = 0; B = 0
                        if i==1:
                            R = 255; G = 0; B = 255
                        if i==2:
                            R = 255; G = 255; B = 0
                        if i==3:
                            R = 0; G = 255; B = 0
                        if i==4:
                            R = 0; G = 0; B = 255
                        pl.osd_img.draw_line(results_show[0], results_show[1], results_show[j+2], results_show[j+3], color=(255,R,G,B), thickness = 3)
                        pl.osd_img.draw_line(results_show[j+2], results_show[j+3], results_show[j+4], results_show[j+5], color=(255,R,G,B), thickness = 3)
                        pl.osd_img.draw_line(results_show[j+4], results_show[j+5], results_show[j+6], results_show[j+7], color=(255,R,G,B), thickness = 3)
                        pl.osd_img.draw_line(results_show[j+6], results_show[j+7], results_show[j+8], results_show[j+9], color=(255,R,G,B), thickness = 3)
    
                    gesture_str=gesture_res[k][1]
                    pl.osd_img.draw_string_advanced( x_det , y_det-50,32, " " + str(gesture_str), color=(255,0, 255, 0))
    
    
    
    if __name__=="__main__":
        # 显示模式，默认"hdmi",可以选择"hdmi"和"lcd"
        display_mode="lcd"
        # k230保持不变，k230d可调整为[640,360]
        rgb888p_size = [1920, 1080]
    
        if display_mode=="hdmi":
            display_size=[1920,1080]
        else:
            display_size=[800,480]
        # 手掌检测模型路径
        hand_det_kmodel_path="/sdcard/examples/kmodel/hand_det.kmodel"
        # 手掌关键点模型路径
        hand_kp_kmodel_path="/sdcard/examples/kmodel/handkp_det.kmodel"
        # 其他参数
        anchors_path="/sdcard/examples/utils/prior_data_320.bin"
        hand_det_input_size=[512,512]
        hand_kp_input_size=[256,256]
        confidence_threshold=0.2
        nms_threshold=0.5
        labels=["hand"]
        anchors = [26,27, 53,52, 75,71, 80,99, 106,82, 99,134, 140,113, 161,172, 245,276]
    
        # 初始化PipeLine，只关注传给AI的图像分辨率，显示的分辨率
        pl=PipeLine(rgb888p_size=rgb888p_size,display_size=display_size,display_mode=display_mode)
        pl.create()
        hkc=HandKeyPointClass(hand_det_kmodel_path,hand_kp_kmodel_path,det_input_size=hand_det_input_size,kp_input_size=hand_kp_input_size,labels=labels,anchors=anchors,confidence_threshold=confidence_threshold,nms_threshold=nms_threshold,nms_option=False,strides=[8,16,32],rgb888p_size=rgb888p_size,display_size=display_size)
        try:
            while True:
                os.exitpoint()
                with ScopedTiming("total",1):
                    img=pl.get_frame()                          # 获取当前帧
                    det_boxes,gesture_res=hkc.run(img)          # 推理当前帧
                    hkc.draw_result(pl,det_boxes,gesture_res)   # 绘制当前帧推理结果
                    pl.show_image()                             # 展示推理结果
                    gc.collect()
        except Exception as e:
            sys.print_exception(e)
        finally:
            hkc.hand_det.deinit()
            hkc.hand_kp.deinit()
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
320  
321  
322  
323  
324  
325  
326  
327  
328  
329  
330  
331  
332  
333  
334  


以上代码的主要流程如下：

`HandDetApp`（手掌检测）

  * 利用预先训练好的手掌检测模型得到包含手掌的边界框。
  * 预处理时先 `pad + resize`；后处理中用 `anchorbasedet_post_process` 得到手掌框。



`HandKPClassApp`（关键点提取 + 手势分类）

  * 根据已检测到的手掌边界框，对图像做 `crop + resize`；
  * 推理后得到关键点坐标，并计算每根手指的关节角度来识别手势（如 fist、five、gun、love 等）。



`HandKeyPointClass`（整合主流程）

  * 先调用 `HandDetApp` 获得手部边界框；
  * 然后调用 `HandKPClassApp` 获得关键点和手势分类；
  * 在 `draw_result()`*方法中，将检测框、关键点和文字标注绘制出来。



# 3 实际运行 ​

我们准备一张有多个手部照片的图片： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/hand-related/hand-detect/hand-detect_20241226_182145.png)

在IDE中运行效果如下图所示： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/hand-related/hand-kp-class/hand-kp-class_20241227_145715.png)

