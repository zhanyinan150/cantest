from libs.PipeLine import PipeLine, ScopedTiming
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
import os,sys,gc,time,random,utime,urandom, math
import ujson
from media.media import *
from time import *
import nncase_runtime as nn
import ulab.numpy as np
import image,aidemo,aicube
from media.sensor import *
from media.display import *
from machine import Pin,FPIOA,UART
#import time, os, sys
from libs.PipeLine import ScopedTiming
from libs.Utils import *

#配置串口
fpioa = FPIOA()
fpioa.set_function(50, FPIOA.UART3_TXD)
fpioa.set_function(51, FPIOA.UART3_RXD)
fpioa.set_function(53, FPIOA.GPIO53)

# ===== 用户按键 SW4 (K230 N4 / GPIO53) =====
# 硬件: 按键接 VDD_3V3, GPIO 下拉到 GND, 高电平有效 (按下=1, 释放=0)
# 交互: 短按(<长按阈值) 拍照10张; 长按(>=长按阈值) 切换下一个摄像头
BTN_PIN = 53
BTN_LONG_PRESS_MS = 1000   # 长按阈值: 持续按下 >=1s 判长按
BTN_SHORT_PRESS_MS = 30    # 短按消抖: 按下 >=30ms 才算有效(滤抖动)
BTN_DEBOUNCE_MS = 2        # 电平消抖: 两次采样间隔
BTN_POLL_MS = 10           # 主循环帧间隔(按键轮询间隔)

btn = Pin(BTN_PIN, Pin.IN, Pin.PULL_DOWN)  # 下拉, 按下时拉高到 VDD_3V3

PHOTO_DIR = "/sdcard/photo"              # 拍照保存目录(SD卡)
PHOTO_COUNT = 10                         # 短按连拍张数
PHOTO_INTERVAL_MS = 120                  # 连拍间隔(ms)


class Button:
    """GPIO 按键状态机 (轮询, 高电平有效)。
    每帧调 poll(), 返回 'short' / 'long' / None。
    - 上升沿开始计时; 持续按下达 LONG_PRESS_MS 立即返回 'long' (边沿触发, 不等释放)
    - 释放时若未触发长按且时长>=SHORT_PRESS_MS, 返回 'short'
    - 长按触发后到释放前不再重复返回 (long_triggered 标志)"""
    def __init__(self, pin):
        self.pin = pin
        self.pressing = False
        self.press_start = 0
        self.long_triggered = False

    def _read_debounced(self):
        """两次采样消抖, 返回稳定电平 (1=按下, 0=释放)"""
        l1 = self.pin.value()
        time.sleep_ms(BTN_DEBOUNCE_MS)
        l2 = self.pin.value()
        return l2 if l1 == l2 else 0  # 不一致视为释放, 避免误触发

    def poll(self):
        now = time.ticks_ms()
        level = self._read_debounced()
        if not self.pressing:
            if level == 1:  # 上升沿: 开始按下
                self.pressing = True
                self.press_start = now
                self.long_triggered = False
        else:
            if level == 1:  # 持续按下
                if not self.long_triggered and \
                   time.ticks_diff(now, self.press_start) >= BTN_LONG_PRESS_MS:
                    self.long_triggered = True
                    return 'long'
            else:  # 下降沿: 释放
                dur = time.ticks_diff(now, self.press_start)
                self.pressing = False
                if not self.long_triggered and dur >= BTN_SHORT_PRESS_MS:
                    return 'short'
        return None


def ensure_photo_dir():
    """确保拍照目录存在 (已存在则忽略)"""
    try:
        os.mkdir(PHOTO_DIR)
    except OSError:
        pass

#对分辨率进行一些配置
display_width = 800
display_height = 480

uart = UART(UART.UART3, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)

OUT_RGB888P_WIDTH = ALIGN_UP(640, 16)
OUT_RGB888P_HEIGHT = 360
# 显示模式选择
DISPLAY_MODE = "LCD"

picture_width = 800
picture_height = 480

if DISPLAY_MODE == "LCD":
    DISPLAY_WIDTH = 800
    DISPLAY_HEIGHT = 480


class_1_map = {
    "one": 0x01,
    "two": 0x02,
    "three": 0x03,
    "four": 0x04,
    "five": 0x05,
    "greenbean": 0x06,
    "yellowbean": 0x07,
    "whitebean": 0x08
}

#分辨率比例转换，这里用于将lcd屏幕的800*480转换为适用于YOLO的分辨率640
def two_side_pad_param(input_size, output_size):
    ratio_w = output_size[0] / input_size[0]  # 宽度缩放比例
    ratio_h = output_size[1] / input_size[1]  # 高度缩放比例
    ratio = min(ratio_w, ratio_h)  # 取较小的缩放比例
    new_w = int(ratio * input_size[0])  # 新宽度
    new_h = int(ratio * input_size[1])  # 新高度
    dw = (output_size[0] - new_w) / 2  # 宽度差
    dh = (output_size[1] - new_h) / 2  # 高度差
    top = int(round(dh - 0.1))
    bottom = int(round(dh + 0.1))
    left = int(round(dw - 0.1))
    right = int(round(dw + 0.1))
    return top, bottom, left, right, ratio

def split_coordinates(value):
    """
    将一个16位数值拆分为高字节和低字节
    value: 要拆分的16位整数值
    return: (high_byte, low_byte) 元组
    """
    high_byte = (value >> 8) & 0xFF  # 右移8位获取高字节
    low_byte = value & 0xFF          # 与0xFF按位与获取低字节
    return high_byte, low_byte

# 自定义YOLO检测类，继承自AIBase基类
class YOLOv11App(AIBase):
    def __init__(self, kmodel_path, model_input_size, anchors, confidence_threshold=0.8, nms_threshold=0.2, rgb888p_size = [640, 360], display_size=[800,480], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)  # 调用基类的构造函数
        self.class_id = ["greenbean","yellowbean","whitebean","one", "two", "three", "four", "five"]
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
        self.last_result = None  #防抖收留此次数据的变量
        self.stable_count = 0 #防抖计数变量
        self.side_result = None  # 保存摄像头0（侧面）识别的数字a
        self.full_result = None  # 保存整合后的完整5个数字结果
        self.front_result_a = None
        self.front_last_result = None
        self.front_stable_count = 0
        # === 新增：用于 LCD 显示的数据状态 ===
        self.send_status_text = ""
        self.send_status_color = (255, 255, 255, 255) # 默认白色

    # 配置预处理操作，这里使用了pad和resize，Ai2d支持crop/shift/pad/resize/affine，具体代码请打开/sdcard/app/libs/AI2D.py查看
    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):  # 计时器，如果debug_mode大于0则开启
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size  # 初始化ai2d预处理配置，默认为sensor给到AI的尺寸，可以通过设置input_image_size自行修改输入尺寸
            top, bottom, left, right = self.get_padding_param()  # 获取padding参数
            print("padding: {} {} {} {}".format(top, bottom, left, right))
            self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [104, 117, 123])  # 填充边缘
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)  # 缩放图像
            self.ai2d.build([1,3,ai2d_input_size[1],ai2d_input_size[0]],[1,3,self.model_input_size[1],self.model_input_size[0]])  # 构建预处理流程

    # 自定义当前任务的后处理，results是模型输出array列表
    def postprocess(self, results):
        counter = 0
        det_res = []
        with ScopedTiming("postprocess", self.debug_mode > 0):
            # 输出形状为[1, 1, 14, 2100]
            # 意思是，输出了2100个框，每个框有14个数据，其中前4个数据是xywh，后面10个是每个类别对应的置信度，这里的xy指的是中心点坐标
            for i in range(2100):
                try:
                    result = results[0][0][:, i]
                    max_score = max(result[4:])
                    if max_score > self.confidence_threshold:
                        # 这里把位置信息恢复到1920 * 1080画布下的状态
                        x = result[0] * max(self.rgb888p_size) / max(self.model_input_size)
                        y = result[1] * max(self.rgb888p_size) / max(self.model_input_size)
                        w = result[2] * max(self.rgb888p_size) / max(self.model_input_size)
                        h = result[3] * max(self.rgb888p_size) / max(self.model_input_size)
                        det_res.append([x, y, w, h, list(result[4:]).index(max_score), max_score])
                except Exception as e:
                    # 捕获异常，跳过当前i，不卡死
                    print(f"i={i} 出错: {e}")
                    continue
            det_res.sort(key=lambda x:x[-1], reverse=True)
            det_res_single = []
            added_class = []
            for result in det_res:
                if not result[-2] in added_class:
                    added_class.append(result[-2])
                    det_res_single.append(result)
        return det_res_single

    # 绘制检测结果到画面上
    def draw_result(self,dets,img_display):
        # 直接画到 img_display (main layer), 绕过 OSD1 layer (本板 OSD1 不显示 -> 有画面无框)
        if dets:
            for det in dets:
                if len(det) < 6:
                    continue
                # 将检测框的坐标转换为显示分辨率下的坐标
                x, y, w, h = map(lambda x: int(round(x, 0)), det[:4])
                x = x * self.display_size[0] // self.rgb888p_size[0]
                y = y * self.display_size[1] // self.rgb888p_size[1]
                w = w * self.display_size[0] // self.rgb888p_size[0]
                h = h * self.display_size[1] // self.rgb888p_size[1]
                img_display.draw_rectangle(x - w//2, y - h // 2, w, h, color=(255, 0, 0), thickness=2)  # 红色框
                # 标签 x 限制在屏幕内 (右侧目标标签会超出画面右边界)
                label_x = x - w//2
                if label_x < 0:
                    label_x = 0
                elif label_x > DISPLAY_WIDTH - 280:
                    label_x = DISPLAY_WIDTH - 280
                img_display.draw_string_advanced(label_x, y - h // 2, 40, "{} {}".format(self.class_id[det[-2]], round(det[-1], 2)), color=(255, 0, 0))  # 红色标签

        # 状态提示也画到 img_display (右下角, 白色)
        if self.send_status_text:
            img_display.draw_string_advanced(580, 430, 30, self.send_status_text, color=(255, 255, 255))
        Display.show_image(img_display, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))

    ###往下看***********************************************************************#
    def save_side_result(self, dets):
        """保存摄像头0(侧面)识别的数字,取置信度最高的1个"""
        if not dets:
            self.side_result = None
            return 0x00

        # === 核心修复：强制拦截豆子 ===
        # 遍历所有识别到的目标，只保留 class_id 在 3 到 7 之间（即 one 到 five）的数字
        valid_dets = []
        for d in dets:
            if len(d) >= 6:
                class_idnum = int(d[4])
                if 3 <= class_idnum <= 7:
                    valid_dets.append(d)
                else:
                    # 如果检测到豆子 (0, 1, 2)，直接丢弃并打印提示
                    cls_name = self.class_id[class_idnum] if class_idnum < len(self.class_id) else "unknown"
                    print(f"已拦截侧面干扰物: {cls_name}")

        # 如果过滤完所有的豆子后，没有有效的数字了，直接返回 0x00
        if not valid_dets:
            self.side_result = None
            return 0x00
        # 取置信度最高的目标（避免多个误识别干扰）
        best_det = max(dets, key=lambda x: x[5])
        class_idnum = best_det[4]
        cls_name = self.class_id[int(class_idnum)] if int(class_idnum) < len(self.class_id) else ""
        self.side_result = class_1_map.get(cls_name, 0x00)
        print(f"侧面结果已保存: {cls_name} (0x{self.side_result:02X})")
        print(self.side_result)
        return self.side_result

    def integrate_results(self):
        """整合摄像头3保存的正面3个数字 + 摄像头0识别的侧面1个数字"""
        # 校验前置条件：必须先有正面3个数字和侧面1个数字
        if self.front_result_a is None or len(self.front_result_a) != 3:
            print("错误：未获取到有效正面3个数字结果")
            return [0x00] * 4
        if self.side_result is None:
            print("错误：未获取到侧面数字结果")
            return [0x00] * 4

        # 整合顺序：[侧面数字, 正面第1个, 正面第2个, 正面第3个]
        # 如需调整顺序，直接改这里即可
        integrated = [self.side_result] + self.front_result_a
        hex_list = []
        for x in integrated:
            hex_list.append('0x{:02X}'.format(x))
        print("4个已知数字：" + str(hex_list))
        return integrated

    def infer_fifth_number(self, known_nums):
        """根据4个不重复的1-5数字，推测缺失的第5个"""
        all_nums = {0x01, 0x02, 0x03, 0x04, 0x05}
        known_set = set(known_nums) - {0x00}
        if len(known_set) == 4:
            missing = all_nums - known_set
            fifth_num = missing.pop()
            print(f"推测第五个数字: 0x{fifth_num:02X}")
            return fifth_num
        else:
            print(f"无法推测，有效数字数量: {len(known_set)}")
            return 0x00
    #往上看**********************************************************************************************************************

    def is_valid_data(self, data_list):
        """
        检测准备发送或保存的数字列表是否有效，并更新LCD显示状态
        """
        # 1. 拦截条件：如果没数据或全是 0x00
        if not data_list or all(item == 0x00 for item in data_list):
            self.send_status_text = "无效信息发送"  # 触发拦截
            self.send_status_color = (255, 255, 0, 0) # 红色 (ARGB: 255透明度, 255红, 0绿, 0蓝)
            return False

        # 2. 放行条件：数据正常
        self.send_status_text = "有效信息发送"
        self.send_status_color = (255, 0, 255, 0) # 绿色 (ARGB: 255透明度, 0红, 255绿, 0蓝)
        return True

    #防抖函数
    def prevent_shaking(self,full_result):
        if full_result == self.last_result:
            self.stable_count += 1
        else:
            self.stable_count = 0
            self.last_result = full_result

        print(self.stable_count)
        print(self.last_result)
        if self.stable_count < 3:
            return False# 不稳定，不发送
        else:
            self.last_result = None
            self.stable_count = 0
            return True

    def draw_number_sort(self, dets, sensor_number, thing_num: int):
        # 1. 通用过滤：保留置信度≥0.8的目标
        filtered_dets = [det for det in dets if len(det)>=6 and det[5]>=0.80]
        res_sorted = sorted(filtered_dets, key=lambda x: x[0])  # 按x坐标从左到右排序

        # 2. 数字识别场景（thing_num=5）
        if thing_num == 5:
            # 摄像头2（正面）：加防抖+仅在此处清空旧数据
            if sensor_number == 0x03:
                # 满足需求1：只有开启摄像头2识别正面时，才清空上一轮的旧数据
                if self.front_last_result is None:
                    self.front_result_a = None
                    self.front_stable_count = 0
                # 获取当前帧的正面3个数字
                current_front = []
                for det in res_sorted[:3]:
                    class_idnum = det[4]
                    cls_name = self.class_id[int(class_idnum)] if int(class_idnum) < len(self.class_id) else ""
                    current_front.append(class_1_map.get(cls_name, 0x00))
                current_front += [0x00] * (3 - len(current_front))

                if 0x00 not in current_front:
                    #s正面数字加防抖（连续3帧相同才更新front_result_a）
                    if current_front == self.front_last_result:
                        self.front_stable_count += 1
                    else:
                        self.front_stable_count = 0
                        self.front_last_result = current_front

                    # 连续3帧稳定，才保存到front_result_a
                    if self.front_stable_count >= 3:
                        self.front_result_a = current_front
                        hex_strings = ['0x{:02X}'.format(x) for x in self.front_result_a]
                        print(f"正面3个数字已稳定保存：{hex_strings}")
                        # 防抖成功后重置计数，避免重复保存
                        self.front_stable_count = 0
                    else:
                        hex_strings = ['0x{:02X}'.format(x) for x in current_front]
                        print(f"正面数字防抖中：第{self.front_stable_count}帧，当前：{hex_strings}")
                return

            # 摄像头0（侧面）：整合+推测+发送（不再清空front_result_a）
            elif sensor_number == 0x01:
                # 保存侧面数字（每帧刷新）
                self.save_side_result(res_sorted)
                # 整合正面3个+侧面1个
                integrated = self.integrate_results()
                # 推测第五个数字
                fifth_num = self.infer_fifth_number(integrated)
                # 构造完整5个数字
                self.full_result = integrated + [fifth_num]
                self.full_result = self.full_result[:5] + [0x00] * (5 - len(self.full_result))

                # 防抖校验后发送
                MA = bytearray([
                    sensor_number, 0x00,
                    self.full_result[0], self.full_result[1], self.full_result[2],
                    self.full_result[3], self.full_result[4], 0x6B
                ])
                if self.prevent_shaking(self.full_result):
                    uart.write(MA)
                    print(f"发送完整5位数字: {MA.hex()}")
                    #  已删除此处的清空代码，front_result_a 仅在下次正面识别时清空
                    self.side_result = None
                    self.full_result = None
                return self.full_result

        # 3. 豆子识别场景（完全不变）
        elif thing_num == 3:
            send_data = []
            for obj in res_sorted:
                if len(obj)>=5:
                    cls_name = self.class_id[int(obj[4])] if int(obj[4])<len(self.class_id) else ""
                    if cls_name in class_1_map:
                        send_data.append(class_1_map[cls_name])
            send_data = send_data[:3] + [0x00]*(3 - len(send_data))
            if 0x00 not in send_data:
                MA = bytearray([sensor_number, 0x00, send_data[0], send_data[1], send_data[2], 0x00, 0x00, 0x6B])
                if self.prevent_shaking(send_data):
                    uart.write(MA)
                    return send_data
        return []

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

    #截取数据流（拍摄照片）
    def sensor_control(self,sensor):
        img_display = sensor.snapshot(chn=0)
        img_ai = sensor.snapshot(chn=2)
        return img_display, img_ai

    #对指定摄像头进行初始化
    def config_camera_and_display(self,camera_id):
        """
        配置指定ID的摄像头，返回配置好的sensor对象
        :param camera_id: 摄像头数字ID（目前支持0/2，可扩展）
        :return: 配置完成的Sensor对象，当None（ID不合法时）
        """
        # 校验摄像头ID合法性（可根据实际硬件扩展支持的ID）

        supported_ids = [0, 1, 2]
        if camera_id not in supported_ids:
            print(f"错误：不支持的摄像头ID {camera_id}，仅支持{supported_ids}")
            return None
        print(111)
        # 初始化摄像头对象
        sensor = Sensor(id=camera_id)
        print(222)
        sensor.reset()  # 重置摄像头
        print(222)
        # 配置【显示通道】（给LCD）：800x480 + RGB565
        sensor.set_framesize(width=800, height=480, chn=CAM_CHN_ID_0)
        sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
        # 配置【AI通道】（给YOLO）：对齐后的分辨率 + RGBP888
        sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGHT, chn=CAM_CHN_ID_2)
        sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
        print(333)
        # 镜像/翻转配置（和原有逻辑保持一致）
        sensor.set_hmirror(False)
        sensor.set_vflip(False)
        print(f"摄像头 {camera_id} 配置完成")
        return sensor

    # AIBase.py 里的 preprocess()
    def preprocess(self, input_np):
        with ScopedTiming("preprocess", self.debug_mode > 0):
            # 【步骤1.1】调用 ai2d.run() 做硬件预处理
            return [self.ai2d.run(input_np)]

    # AIBase.py 里的 inference()
    def inference(self, tensors):
        with ScopedTiming("set input", self.debug_mode > 0):
            self.results.clear()
            for i in range(self.kpu.inputs_size()):
                # 【步骤2.1】绑定输入张量
                self.kpu.set_input_tensor(i, tensors[i])
        with ScopedTiming("kpu run", self.debug_mode > 0):
            # 【步骤2.2】KPU 硬件推理（最容易卡死的地方）
            self.kpu.run()
        with ScopedTiming("get output", self.debug_mode > 0):
            # 【步骤2.3】获取输出张量
            for i in range(self.kpu.outputs_size()):
                output_data = self.kpu.get_output_tensor(i)
                result = output_data.to_numpy()
                self.results.append(result)
                del output_data
        return self.results

    def get_frame(self, img):
        with ScopedTiming("get a frame", self.debug_mode > 0):
            # 传入 img（Image对象），直接转 numpy 数组返回
            input_np = img.to_numpy_ref()
            #print(f"最终输入AI的 shape: {input_np.shape}")
            return input_np

    def run(self, input_img):
        #print("\n========== 进入 run() 函数 ==========")

        # ==========================
        # 关键：接收 image -> 自动转 numpy
        # ==========================
        try:
            # 调用你自己的 get_frame，把 image 转 numpy
            input_np = self.get_frame(input_img)
            #print("image 转 numpy 成功")
            #print(f"最终输入AI的 shape: {input_np.shape}")
        except Exception as e:
            #print(f"get_frame 转换失败: {e}")
            return []

        # ==========================
        # 预处理（AI2D，不会再卡死）
        # ==========================
        print("[1/3] 开始预处理...")
        try:
            self.tensors = self.preprocess(input_np)
            #print("[1/3]预处理完成")
        except Exception as e:
            #print(f"预处理失败: {e}")
            return []

        # ==========================
        # KPU 推理
        # ==========================
        print("[2/3] 开始推理...")
        try:
            self.results = self.inference(self.tensors)
            #print("[2/3]推理完成")
        except Exception as e:
            #print(f"推理失败: {e}")
            return []

        # ==========================
        # 后处理
        # ==========================
        print("[3/3] 开始后处理...")
        try:
            res = self.postprocess(self.results)
            print(f"[3/3]后处理完成，检测到 {len(res)} 个目标")
        except Exception as e:
            #print(f"后处理失败: {e}")
            return []

        print("========== run() 执行完毕 ==========\n")
        return res

    def reset_front_state(self):
        self.front_result_a = None
        self.front_last_result = None
        self.front_stable_count = 0


# ===== 10s 自动轮换识别（原版识别逻辑，不加修复；不依赖 STM32 触发） =====
# 顺序：正面 -> 侧面 -> 豆子 -> 循环（正面必须在侧面前，侧面才能整合5位数字）
SCENES = [
    (2, 0x03, 5),   # 正面数字：保存3位
    (0, 0x01, 5),   # 侧面数字：整合+推算+发送5位
    (1, 0x02, 3),   # 豆子：识别3个直接发送
]
SWITCH_INTERVAL_MS = 10000   # 每个摄像头单次识别时长（毫秒）


if __name__ == "__main__":
    clock = time.clock()
    display_mode = "lcd"
    if display_mode == "hdmi":
        display_size = [1920, 1080]
    else:
        display_size = [800, 480]
    rgb888p_size = [640, 360]
    display_size = [800, 480]

    kmodel_path = "/sdcard/best.kmodel"
    confidence_threshold = 0.3   # 原0.8过高, YOLO常用0.3~0.5; 过高会滤掉所有检测 -> 无框"什么都不显示"
    nms_threshold = 0.2
    anchors = None

    yolo_det = YOLOv11App(kmodel_path, model_input_size=[320, 320], anchors=anchors,
                          confidence_threshold=confidence_threshold, nms_threshold=nms_threshold,
                          rgb888p_size=rgb888p_size, display_size=display_size, debug_mode=0)
    yolo_det.config_preprocess()

    # 按键对象 + 拍照函数 (闭包访问 yolo_det 做 LCD 状态提示)
    button = Button(btn)
    _photo_seq = [0]  # 全局递增序号 (K230 无 RTC, 用计数代替时间戳; list 容器免 nonlocal)

    def capture_photos(sensor, camera_id, count=PHOTO_COUNT, interval_ms=PHOTO_INTERVAL_MS):
        """从当前摄像头连拍 count 张, 保存到 PHOTO_DIR/cam{id}_{seq}.jpg"""
        ensure_photo_dir()
        yolo_det.send_status_text = f"PHOTO x{count}"
        yolo_det.send_status_color = (0, 255, 0, 255)  # 绿色: 拍照中
        print(f"[photo] start cam{camera_id} x{count}")
        for i in range(count):
            os.exitpoint()
            try:
                img = sensor.snapshot(chn=CAM_CHN_ID_0)
                _photo_seq[0] += 1
                fname = f"{PHOTO_DIR}/cam{camera_id}_{_photo_seq[0]:05d}.jpg"
                img.save(fname)
                print(f"[photo] {i+1}/{count} {fname}")
                # LCD 实时显示拍照进度 (直接画到 img, 绕过 OSD1)
                img.draw_string_advanced(580, 430, 30, f"PHOTO {i+1}/{count}", color=(0, 255, 0))
                Display.show_image(img, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))
                del img
            except Exception as e:
                print(f"[photo] fail {i+1}: {e}")
            gc.collect()
            time.sleep_ms(interval_ms)
        yolo_det.send_status_text = ""
        print("[photo] done")

    sensor = None
    try:
        Display.init(Display.ST7701, width=800, height=480, to_ide=True)
        MediaManager.init()
        time.sleep_ms(200)

        scene_idx = 0
        while True:
            os.exitpoint()
            camera_id, sensor_number, thing_num = SCENES[scene_idx]

            # 进入正面场景前清掉上一轮的正面结果，重新采集
            if sensor_number == 0x03:
                yolo_det.reset_front_state()

            # 切换传感器：先停旧 -> 释放旧对象 -> 配新 -> run
            # 关键：必须把旧 sensor 引用置空并 gc，否则旧 Sensor 的 CSI/ISP 资源不会立即释放，
            # 新 sensor 拿不到资源，画面/帧缓冲就会没。
            if sensor is not None:
                sensor.stop()
                sensor.reset()
                sensor = None
                gc.collect()
            time.sleep_ms(50)
            sensor = yolo_det.config_camera_and_display(camera_id)
            time.sleep_ms(200)
            sensor.run()
            time.sleep_ms(200)
            print(f"[cam] 切换到摄像头{camera_id} sensor=0x{sensor_number:02X} thing_num={thing_num}")

            start_ms = time.ticks_ms()
            while True:
                os.exitpoint()
                clock.tick()
                time.sleep_ms(BTN_POLL_MS)

                # === 用户按键检测 (优先于检测帧) ===
                # 短按: 连拍 PHOTO_COUNT 张; 长按: break 切下一个摄像头 (外层 scene_idx+1)
                evt = button.poll()
                if evt == 'long':
                    print("[btn] 长按 -> 切换摄像头")
                    # 切换前显示提示 (直接画到 img, 绕过 OSD1; 新 sensor 配置期间 LCD 停留此帧)
                    try:
                        img = sensor.snapshot(chn=CAM_CHN_ID_0)
                        img.draw_string_advanced(280, 220, 50, "SWITCHING...", color=(255, 255, 0))  # 黄色居中
                        Display.show_image(img, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))
                        del img
                    except Exception as e:
                        print(f"[btn] switch disp fail: {e}")
                    break
                if evt == 'short':
                    print(f"[btn] 短按 -> 拍照{PHOTO_COUNT}张")
                    capture_photos(sensor, camera_id)
                    continue

                img_ai = sensor.snapshot(chn=CAM_CHN_ID_2)
                img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
                dets = yolo_det.run(img_ai)
                print(f"[main] dets={len(dets)} img_disp={'ok' if img_display else 'None'}", dets[0] if dets else "")
                yolo_det.draw_number_sort(dets, sensor_number, thing_num)
                yolo_det.draw_result(dets, img_display)
                print("[main] draw_result done")
                del img_display
                del img_ai
                gc.collect()
                # 10s 自动切换摄像头 (长按也可立即切换, 短按拍照)
                if time.ticks_diff(time.ticks_ms(), start_ms) >= SWITCH_INTERVAL_MS:
                    break

            scene_idx = (scene_idx + 1) % len(SCENES)

    except KeyboardInterrupt as e:
        print("用户终止：", e)
    except BaseException as e:
        print(f"异常：{e}")
    finally:
        if sensor is not None:
            try:
                sensor.stop()
                sensor.reset()
            except Exception as e:
                print(f"sensor 停止失败: {e}")
        try:
            yolo_det.deinit()
        except Exception as e:
            print(f"yolo_det.deinit: {e}")
        try:
            Display.deinit()
        except Exception as e:
            print(f"Display.deinit: {e}")
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        try:
            MediaManager.deinit()
        except Exception as e:
            print(f"MediaManager.deinit: {e}")
        try:
            nn.shrink_memory_pool()
        except Exception as e:
            print(f"shrink_memory_pool: {e}")
        gc.collect()
