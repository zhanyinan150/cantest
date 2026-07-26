# 人脸分割（解析）

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/ai-demo/face-related/face-segmentation.html>
> **最后更新**: 2024-12-19

---

# 1 本节介绍 ​

📝本节您将学习如何通过庐山派来分割人脸眼睛，鼻子嘴巴等，如无特殊说明，以后所有例程的显示设备均为通过外接[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)，在3.1寸小屏幕上显示。若用户无3.1寸屏幕扩展板也可以正常在IDE的缓冲区，只是受限于USB带宽，可能会帧率较低或卡顿。

🏆学习目标

1️⃣如何用庐山派开发板去识别人脸并将眼睛、鼻子、嘴巴、头发等分割出来。

庐山派开发板的固件是存储在TF中的，模型文件已经提前写入到固件中了，所以大家只需要复制下面的代码到IDE，传递到开发板上就可以正常运行了。**无需再额外拷贝** ，至于后面需要拷贝自己训练的模型那就是后话了。

人脸分割（解析）通常指的是将一张人脸图像中的各个组成部分（比如：眼睛、鼻子、嘴巴、头发、背景等）精确地从像素级别进行区分和标注的过程。和普通的人脸检测或人脸关键点定位不同，人脸分割希望得到的是更加精确的信息，就是说不仅要知道脸的位置，还要知道脸部内部不同区域的类别归属。通过人脸分割，可以实现人脸的更高层次理解。获取到这些信息后我们就可以进行人脸属性的分析。

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
            # 实例化Ai2d，用于实现模型预处理
            self.ai2d=Ai2d(debug_mode)
            # 设置Ai2d的输入输出格式和类型
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT,nn.ai2d_format.NCHW_FMT,np.uint8, np.uint8)
    
        # 配置预处理操作，这里使用了pad和resize，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
        def config_preprocess(self,input_image_size=None):
            with ScopedTiming("set preprocess config",self.debug_mode > 0):
                # 初始化ai2d预处理配置，默认为sensor给到AI的尺寸，可以通过设置input_image_size自行修改输入尺寸
                ai2d_input_size=input_image_size if input_image_size else self.rgb888p_size
                # 计算padding参数，并设置padding预处理
                self.ai2d.pad(self.get_pad_param(), 0, [104,117,123])
                # 设置resize预处理
                self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
                # 构建预处理流程,参数为预处理输入tensor的shape和预处理输出的tensor的shape
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])
    
        # 自定义后处理，results是模型输出的array列表，这里调用了aidemo库的face_det_post_process接口
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
    
    # 自定义人脸解析任务类
    class FaceParseApp(AIBase):
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
            # debug模式
            self.debug_mode=debug_mode
            # 实例化Ai2d，用于实现模型预处理
            self.ai2d=Ai2d(debug_mode)
            # 设置Ai2d的输入输出格式和类型
            self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT,nn.ai2d_format.NCHW_FMT,np.uint8, np.uint8)
    
        # 配置预处理操作，这里使用了affine，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
        def config_preprocess(self,det,input_image_size=None):
            with ScopedTiming("set preprocess config",self.debug_mode > 0):
                # 初始化ai2d预处理配置，默认为sensor给到AI的尺寸，可以通过设置input_image_size自行修改输入尺寸
                ai2d_input_size=input_image_size if input_image_size else self.rgb888p_size
                # 计算仿射变换矩阵并设置affine预处理
                matrix_dst = self.get_affine_matrix(det)
                self.ai2d.affine(nn.interp_method.cv2_bilinear,0, 0, 127, 1,matrix_dst)
                # 构建预处理流程,参数为预处理输入tensor的shape和预处理输出的tensor的shape
                self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])
    
        # 自定义后处理，results是模型输出的array列表，这里将第一个输出返回
        def postprocess(self,results):
            with ScopedTiming("postprocess",self.debug_mode > 0):
                return results[0]
    
        def get_affine_matrix(self,bbox):
            # 获取仿射矩阵，用于将边界框映射到模型输入空间
            with ScopedTiming("get_affine_matrix", self.debug_mode > 1):
                # 设置缩放因子
                factor = 2.7
                # 从边界框提取坐标和尺寸
                x1, y1, w, h = map(lambda x: int(round(x, 0)), bbox[:4])
                # 模型输入大小
                edge_size = self.model_input_size[1]
                # 平移距离，使得模型输入空间的中心对准原点
                trans_distance = edge_size / 2.0
                # 计算边界框中心点的坐标
                center_x = x1 + w / 2.0
                center_y = y1 + h / 2.0
                # 计算最大边长
                maximum_edge = factor * (h if h > w else w)
                # 计算缩放比例
                scale = edge_size * 2.0 / maximum_edge
                # 计算平移参数
                cx = trans_distance - scale * center_x
                cy = trans_distance - scale * center_y
                # 创建仿射矩阵
                affine_matrix = [scale, 0, cx, 0, scale, cy]
                return affine_matrix
    
    # 人脸解析任务
    class FaceParse:
        def __init__(self,face_det_kmodel,face_parse_kmodel,det_input_size,parse_input_size,anchors,confidence_threshold=0.25,nms_threshold=0.3,rgb888p_size=[1920,1080],display_size=[1920,1080],debug_mode=0):
            # 人脸检测模型路径
            self.face_det_kmodel=face_det_kmodel
            # 人脸解析模型路径
            self.face_pose_kmodel=face_parse_kmodel
            # 人脸检测模型输入分辨率
            self.det_input_size=det_input_size
            # 人脸解析模型输入分辨率
            self.parse_input_size=parse_input_size
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
            # 人脸检测任务类实例
            self.face_det=FaceDetApp(self.face_det_kmodel,model_input_size=self.det_input_size,anchors=self.anchors,confidence_threshold=self.confidence_threshold,nms_threshold=self.nms_threshold,rgb888p_size=self.rgb888p_size,display_size=self.display_size,debug_mode=0)
            # 人脸解析实例
            self.face_parse=FaceParseApp(self.face_pose_kmodel,model_input_size=self.parse_input_size,rgb888p_size=self.rgb888p_size,display_size=self.display_size)
            # 人脸检测预处理配置
            self.face_det.config_preprocess()
    
        # run函数
        def run(self,input_np):
            # 执行人脸检测
            det_boxes=self.face_det.run(input_np)
            parse_res=[]
            for det_box in det_boxes:
                # 对检测到每一个人脸进行人脸解析
                self.face_parse.config_preprocess(det_box)
                res=self.face_parse.run(input_np)
                parse_res.append(res)
            return det_boxes,parse_res
    
    
        # 绘制人脸解析效果
        def draw_result(self,pl,dets,parse_res):
            pl.osd_img.clear()
            if dets:
                draw_img_np = np.zeros((self.display_size[1],self.display_size[0],4),dtype=np.uint8)
                draw_img=image.Image(self.display_size[0], self.display_size[1], image.ARGB8888,alloc=image.ALLOC_REF,data=draw_img_np)
                for i,det in enumerate(dets):
                    # （1）将人脸检测框画到draw_img
                    x, y, w, h = map(lambda x: int(round(x, 0)), det[:4])
                    x = x * self.display_size[0] // self.rgb888p_size[0]
                    y = y * self.display_size[1] // self.rgb888p_size[1]
                    w = w * self.display_size[0] // self.rgb888p_size[0]
                    h = h * self.display_size[1] // self.rgb888p_size[1]
                    aidemo.face_parse_post_process(draw_img_np,self.rgb888p_size,self.display_size,self.parse_input_size[0],det.tolist(),parse_res[i])
                pl.osd_img.copy_from(draw_img)
    
    
    if __name__=="__main__":
        # 显示模式，默认"hdmi",可以选择"hdmi"和"lcd"，k230d受限于内存不支持
        display_mode="lcd"
        if display_mode=="hdmi":
            display_size=[1920,1080]
        else:
            display_size=[800,480]
        # 人脸检测模型路径
        face_det_kmodel_path="/sdcard/examples/kmodel/face_detection_320.kmodel"
        # 人脸解析模型路径
        face_parse_kmodel_path="/sdcard/examples/kmodel/face_parse.kmodel"
        # 其他参数
        anchors_path="/sdcard/examples/utils/prior_data_320.bin"
        rgb888p_size=[1920,1080]
        face_det_input_size=[320,320]
        face_parse_input_size=[320,320]
        confidence_threshold=0.5
        nms_threshold=0.2
        anchor_len=4200
        det_dim=4
        anchors = np.fromfile(anchors_path, dtype=np.float)
        anchors = anchors.reshape((anchor_len,det_dim))
    
        # 初始化PipeLine，只关注传给AI的图像分辨率，显示的分辨率
        pl=PipeLine(rgb888p_size=rgb888p_size,display_size=display_size,display_mode=display_mode)
        pl.create()
        fp=FaceParse(face_det_kmodel_path,face_parse_kmodel_path,det_input_size=face_det_input_size,parse_input_size=face_parse_input_size,anchors=anchors,confidence_threshold=confidence_threshold,nms_threshold=nms_threshold,rgb888p_size=rgb888p_size,display_size=display_size)
        try:
            while True:
                os.exitpoint()
                with ScopedTiming("total",1):
                    img=pl.get_frame()                      # 获取当前帧
                    det_boxes,parse_res=fp.run(img)         # 推理当前帧
                    fp.draw_result(pl,det_boxes,parse_res)  # 绘制当前帧推理结果
                    pl.show_image()                         # 展示推理效果
                    gc.collect()
        except Exception as e:
            sys.print_exception(e)
        finally:
            fp.face_det.deinit()
            fp.face_parse.deinit()
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


  * **FaceDetApp** ：负责人脸检测，比如模型的加载、前处理、后处理等。

  * **FaceParseApp** ：负责对检测出的人脸区域进行进一步解析。

  * **FaceParse** ：综合调用以上两个模块完成整个人脸检测和解析流程。

  * **Main** ：控制主流程，比如模型加载、循环检测、结果显示等。




# 3 运行效果 ​

首先我们准备一张人脸的图片： ![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-segmentation/face-segmentation_20241218_164448.png)

将上面代码复制进IDE里面并运行，此时在IDE的帧缓冲区和3.1寸屏幕上就都能看到结果了。

![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/ai-demo/face-related/face-segmentation/face-segmentation_20241218_170818.png)

