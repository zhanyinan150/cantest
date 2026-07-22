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
from libs.PipeLine import ScopedTiming
from libs.Utils import *

# 串口配置：50/51 号脚是 K230 板上 UART3 的引脚，必须用 UART3
# （改用 UART2 会报 set pin func failed，因为 UART2 绑不到这两个脚）
fpioa = FPIOA()
fpioa.set_function(50, FPIOA.UART3_TXD)
fpioa.set_function(51, FPIOA.UART3_RXD)
fpioa.set_function(53, FPIOA.GPIO53)

display_width = 800
display_height = 480

uart = UART(UART.UART3, baudrate=115200, bits=UART.EIGHTBITS, parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)

OUT_RGB888P_WIDTH = ALIGN_UP(640, 16)
OUT_RGB888P_HEIGHT = 360
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


def two_side_pad_param(input_size, output_size):
    ratio_w = output_size[0] / input_size[0]
    ratio_h = output_size[1] / input_size[1]
    ratio = min(ratio_w, ratio_h)
    new_w = int(ratio * input_size[0])
    new_h = int(ratio * input_size[1])
    dw = (output_size[0] - new_w) / 2
    dh = (output_size[1] - new_h) / 2
    top = int(round(dh - 0.1))
    bottom = int(round(dh + 0.1))
    left = int(round(dw - 0.1))
    right = int(round(dw + 0.1))
    return top, bottom, left, right, ratio


def split_coordinates(value):
    high_byte = (value >> 8) & 0xFF
    low_byte = value & 0xFF
    return high_byte, low_byte


class YOLOv11App(AIBase):
    def __init__(self, kmodel_path, model_input_size, anchors, confidence_threshold=0.8, nms_threshold=0.2, rgb888p_size=[640, 360], display_size=[800, 480], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.class_id = ["greenbean", "yellowbean", "whitebean", "one", "two", "three", "four", "five"]
        self.kmodel_path = kmodel_path
        self.model_input_size = model_input_size
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.anchors = anchors
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)

        # [FIX] 坐标还原参数，在 get_padding_param() 中计算
        self.pad_top = 0
        self.pad_left = 0
        self.scale_x = 1.0
        self.scale_y = 1.0

        # [FIX] 防抖状态按场景隔离，避免数字/豆子场景互相干扰
        self.shake_state = {}  # key -> {'last': None, 'count': 0}

        self.side_result = None
        self.full_result = None
        self.front_result_a = None
        self.front_last_result = None
        self.front_stable_count = 0

        self.send_status_text = ""
        self.send_status_color = (255, 255, 255, 255)

    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right = self.get_padding_param()
            print("padding: {} {} {} {}".format(top, bottom, left, right))
            # [FIX] AI2D.pad 标准签名是 4 个值 [top, bottom, left, right]
            self.ai2d.pad([top, bottom, left, right], 0, [104, 117, 123])
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]], [1, 3, self.model_input_size[1], self.model_input_size[0]])

    def postprocess(self, results):
        det_res = []
        with ScopedTiming("postprocess", self.debug_mode > 0):
            # [FIX] 8 类模型：输出形状 [1, 1, 12, 2100] (4 xywh + 8 classes)
            # 2100 = 40*40 + 20*20 + 10*10 (stride 8/16/32, 输入 320x320)
            for i in range(2100):
                try:
                    result = results[0][0][:, i]
                    max_score = max(result[4:])
                    if max_score > self.confidence_threshold:
                        # [FIX] 按实际缩放比和 padding 偏移还原坐标
                        x = (result[0] - self.pad_left) * self.scale_x
                        y = (result[1] - self.pad_top) * self.scale_y
                        w = result[2] * self.scale_x
                        h = result[3] * self.scale_y
                        det_res.append([x, y, w, h, list(result[4:]).index(max_score), max_score])
                except Exception as e:
                    print(f"i={i} 出错: {e}")
                    continue
            det_res.sort(key=lambda x: x[-1], reverse=True)
            # [FIX] 用 IoU NMS 替代"每类只留一个"，支持同类多目标（豆子场景）
            det_res = self._nms(det_res, self.nms_threshold)
        return det_res

    # [FIX] 新增：IoU 计算
    def _iou(self, box1, box2):
        x1, y1, w1, h1 = box1[0], box1[1], box1[2], box1[3]
        x2, y2, w2, h2 = box2[0], box2[1], box2[2], box2[3]
        ax1, ay1 = x1 - w1 / 2, y1 - h1 / 2
        ax2, ay2 = x1 + w1 / 2, y1 + h1 / 2
        bx1, by1 = x2 - w2 / 2, y2 - h2 / 2
        bx2, by2 = x2 + w2 / 2, y2 + h2 / 2
        ix1 = max(ax1, bx1)
        iy1 = max(ay1, by1)
        ix2 = min(ax2, bx2)
        iy2 = min(ay2, by2)
        iw = max(0, ix2 - ix1)
        ih = max(0, iy2 - iy1)
        inter = iw * ih
        union = w1 * h1 + w2 * h2 - inter
        if union <= 0:
            return 0.0
        return inter / union

    # [FIX] 新增：基于 IoU 的 NMS
    def _nms(self, det_res, iou_threshold):
        if not det_res:
            return []
        keep = []
        remaining = list(det_res)
        while remaining:
            best = remaining[0]
            keep.append(best)
            remaining = [d for d in remaining[1:] if self._iou(best, d) < iou_threshold]
        return keep

    def draw_result(self, dets, img_display):
        osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
        osd_img.clear()
        if dets:
            for det in dets:
                if len(det) < 6:
                    continue
                x, y, w, h = map(lambda v: int(round(v, 0)), det[:4])
                x = x * self.display_size[0] // self.rgb888p_size[0]
                y = y * self.display_size[1] // self.rgb888p_size[1]
                w = w * self.display_size[0] // self.rgb888p_size[0]
                h = h * self.display_size[1] // self.rgb888p_size[1]
                osd_img.draw_rectangle(x - w // 2, y - h // 2, w, h, color=(255, 0, 255, 0), thickness=2)
                osd_img.draw_string_advanced(x - w // 2, y - h // 2, 40, "{} {}".format(self.class_id[det[-2]], round(det[-1], 2)), color=(255, 0, 255, 0))

        if self.send_status_text:
            osd_img.draw_string_advanced(580, 430, 30, self.send_status_text, color=self.send_status_color)
        Display.show_image(img_display, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))
        Display.show_image(osd_img, 0, 0, Display.LAYER_OSD1)

    def save_side_result(self, dets):
        """保存摄像头0(侧面)识别的数字，取置信度最高的1个"""
        if not dets:
            self.side_result = None
            return 0x00

        # 过滤：只保留数字 (class_id 3~7: one~five)，拦截豆子
        valid_dets = []
        for d in dets:
            if len(d) >= 6:
                class_idnum = int(d[4])
                if 3 <= class_idnum <= 7:
                    valid_dets.append(d)
                else:
                    cls_name = self.class_id[class_idnum] if class_idnum < len(self.class_id) else "unknown"
                    print(f"已拦截侧面干扰物: {cls_name}")

        if not valid_dets:
            self.side_result = None
            return 0x00
        # [FIX] 从 valid_dets 取最高置信度（原来误用 dets，会把豆子选进去）
        best_det = max(valid_dets, key=lambda x: x[5])
        class_idnum = best_det[4]
        cls_name = self.class_id[int(class_idnum)] if int(class_idnum) < len(self.class_id) else ""
        self.side_result = class_1_map.get(cls_name, 0x00)
        print(f"侧面结果已保存: {cls_name} (0x{self.side_result:02X})")
        return self.side_result

    def integrate_results(self):
        """整合正面3个数字 + 侧面1个数字"""
        if self.front_result_a is None or len(self.front_result_a) != 3:
            print("错误：未获取到有效正面3个数字结果")
            return [0x00] * 4
        if self.side_result is None:
            print("错误：未获取到侧面数字结果")
            return [0x00] * 4
        integrated = [self.side_result] + self.front_result_a
        hex_list = ['0x{:02X}'.format(x) for x in integrated]
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

    def is_valid_data(self, data_list):
        """检测数据是否有效，并更新LCD显示状态"""
        if not data_list or all(item == 0x00 for item in data_list):
            self.send_status_text = "无效信息发送"
            self.send_status_color = (255, 255, 0, 0)
            return False
        self.send_status_text = "有效信息发送"
        self.send_status_color = (255, 0, 255, 0)
        return True

    # [FIX] 防抖函数：按 key 隔离状态，避免数字/豆子场景互相干扰
    def prevent_shaking(self, full_result, key='default'):
        if key not in self.shake_state:
            self.shake_state[key] = {'last': None, 'count': 0}
        state = self.shake_state[key]
        if full_result == state['last']:
            state['count'] += 1
        else:
            state['count'] = 0
            state['last'] = full_result
        print(f"[防抖-{key}] count={state['count']}")
        if state['count'] < 3:
            return False
        else:
            state['last'] = None
            state['count'] = 0
            return True

    def draw_number_sort(self, dets, sensor_number, thing_num: int):
        # 1. 通用过滤：保留置信度>=0.8的目标
        filtered_dets = [det for det in dets if len(det) >= 6 and det[5] >= 0.80]
        res_sorted = sorted(filtered_dets, key=lambda x: x[0])

        # 2. 数字识别场景（thing_num=5）
        if thing_num == 5:
            # 摄像头2（正面）：加防抖+仅在此处清空旧数据
            if sensor_number == 0x03:
                if self.front_last_result is None:
                    self.front_result_a = None
                    self.front_stable_count = 0
                current_front = []
                for det in res_sorted[:3]:
                    class_idnum = int(det[4])
                    # [FIX] 正面也必须过滤豆子，只接受 one~five (class 3~7)
                    if not (3 <= class_idnum <= 7):
                        cls_name = self.class_id[class_idnum] if class_idnum < len(self.class_id) else "unknown"
                        print(f"已拦截正面干扰物: {cls_name}")
                        continue
                    cls_name = self.class_id[class_idnum]
                    current_front.append(class_1_map.get(cls_name, 0x00))
                current_front += [0x00] * (3 - len(current_front))

                if 0x00 not in current_front:
                    if current_front == self.front_last_result:
                        self.front_stable_count += 1
                    else:
                        self.front_stable_count = 0
                        self.front_last_result = current_front

                    if self.front_stable_count >= 3:
                        self.front_result_a = current_front
                        hex_strings = ['0x{:02X}'.format(x) for x in self.front_result_a]
                        print(f"正面3个数字已稳定保存：{hex_strings}")
                        self.front_stable_count = 0
                    else:
                        hex_strings = ['0x{:02X}'.format(x) for x in current_front]
                        print(f"正面数字防抖中：第{self.front_stable_count}帧，当前：{hex_strings}")
                return

            # 摄像头0（侧面）：整合+推测+发送
            elif sensor_number == 0x01:
                self.save_side_result(res_sorted)
                integrated = self.integrate_results()
                fifth_num = self.infer_fifth_number(integrated)
                self.full_result = integrated + [fifth_num]
                self.full_result = self.full_result[:5] + [0x00] * (5 - len(self.full_result))

                MA = bytearray([
                    sensor_number, 0x00,
                    self.full_result[0], self.full_result[1], self.full_result[2],
                    self.full_result[3], self.full_result[4], 0x6B
                ])
                # [FIX] 防抖按 'side' 隔离；发送前校验有效性
                if self.prevent_shaking(self.full_result, key='side'):
                    if self.is_valid_data(self.full_result):
                        uart.write(MA)
                        print(f"发送完整5位数字: {MA.hex()}")
                    else:
                        print("侧面数据无效，跳过发送")
                    self.side_result = None
                    self.full_result = None
                return self.full_result

        # 3. 豆子识别场景
        elif thing_num == 3:
            send_data = []
            for obj in res_sorted:
                if len(obj) >= 5:
                    class_idnum = int(obj[4])
                    if class_idnum < len(self.class_id):
                        cls_name = self.class_id[class_idnum]
                        if cls_name in class_1_map:
                            send_data.append(class_1_map[cls_name])
            send_data = send_data[:3] + [0x00] * (3 - len(send_data))
            if 0x00 not in send_data:
                MA = bytearray([sensor_number, 0x00, send_data[0], send_data[1], send_data[2], 0x00, 0x00, 0x6B])
                # [FIX] 防抖按 'bean' 隔离；发送前校验
                if self.prevent_shaking(send_data, key='bean'):
                    if self.is_valid_data(send_data):
                        uart.write(MA)
                        print(f"发送豆子数据: {MA.hex()}")
                    else:
                        print("豆子数据无效，跳过发送")
                    return send_data
        return []

    def get_padding_param(self):
        dst_w = self.model_input_size[0]
        dst_h = self.model_input_size[1]
        ratio_w = dst_w / self.rgb888p_size[0]
        ratio_h = dst_h / self.rgb888p_size[1]
        ratio = min(ratio_w, ratio_h)
        new_w = int(ratio * self.rgb888p_size[0])
        new_h = int(ratio * self.rgb888p_size[1])
        dw = (dst_w - new_w) / 2
        dh = (dst_h - new_h) / 2
        top = int(round(0))
        bottom = int(round(dh * 2 + 0.1))
        left = int(round(0))
        right = int(round(dw * 2 - 0.1))
        # [FIX] 保存坐标还原参数
        self.pad_top = top
        self.pad_left = left
        self.scale_x = self.rgb888p_size[0] / new_w if new_w > 0 else 1.0
        self.scale_y = self.rgb888p_size[1] / new_h if new_h > 0 else 1.0
        return top, bottom, left, right

    def sensor_control(self, sensor):
        img_display = sensor.snapshot(chn=0)
        img_ai = sensor.snapshot(chn=2)
        return img_display, img_ai

    def config_camera_and_display(self, camera_id):
        supported_ids = [0, 1, 2]
        if camera_id not in supported_ids:
            print(f"错误：不支持的摄像头ID {camera_id}，仅支持{supported_ids}")
            return None
        print(f"配置摄像头 {camera_id}")
        sensor = Sensor(id=camera_id)
        sensor.reset()
        # 显示通道：800x480 RGB565
        sensor.set_framesize(width=800, height=480, chn=CAM_CHN_ID_0)
        sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
        # AI通道：对齐分辨率 + RGB888 PLANAR
        sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGHT, chn=CAM_CHN_ID_2)
        sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
        sensor.set_hmirror(False)
        sensor.set_vflip(False)
        print(f"摄像头 {camera_id} 配置完成")
        return sensor

    def preprocess(self, input_np):
        with ScopedTiming("preprocess", self.debug_mode > 0):
            return [self.ai2d.run(input_np)]

    def inference(self, tensors):
        with ScopedTiming("set input", self.debug_mode > 0):
            self.results.clear()
            for i in range(self.kpu.inputs_size()):
                self.kpu.set_input_tensor(i, tensors[i])
        with ScopedTiming("kpu run", self.debug_mode > 0):
            self.kpu.run()
        with ScopedTiming("get output", self.debug_mode > 0):
            for i in range(self.kpu.outputs_size()):
                output_data = self.kpu.get_output_tensor(i)
                result = output_data.to_numpy()
                self.results.append(result)
                del output_data
        return self.results

    def get_frame(self, img):
        with ScopedTiming("get a frame", self.debug_mode > 0):
            input_np = img.to_numpy_ref()
            return input_np

    def run(self, input_img):
        try:
            input_np = self.get_frame(input_img)
        except Exception as e:
            print(f"get_frame 失败: {e}")
            return []
        try:
            self.tensors = self.preprocess(input_np)
        except Exception as e:
            print(f"预处理失败: {e}")
            return []
        try:
            self.results = self.inference(self.tensors)
        except Exception as e:
            print(f"推理失败: {e}")
            return []
        try:
            res = self.postprocess(self.results)
        except Exception as e:
            print(f"后处理失败: {e}")
            return []
        return res

    def reset_front_state(self):
        self.front_result_a = None
        self.front_last_result = None
        self.front_stable_count = 0
        # [FIX] 同时清理侧面防抖状态
        if 'side' in self.shake_state:
            self.shake_state['side'] = {'last': None, 'count': 0}

    # [FIX] 新增：安全停止/释放传感器
    def safe_stop_sensor(self, sensor):
        if sensor is not None:
            try:
                sensor.stop()
            except Exception as e:
                print(f"sensor.stop 失败: {e}")
            try:
                sensor.reset()
            except Exception as e:
                print(f"sensor.reset 失败: {e}")


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
    confidence_threshold = 0.8
    # [FIX] NMS 阈值：0.2 偏激进，豆子靠得近可能被合并，建议 0.4~0.5
    nms_threshold = 0.45
    anchors = None

    # [FIX] 模型自检：确认文件存在再加载
    try:
        os.stat(kmodel_path)
        print(f"模型文件存在: {kmodel_path}")
    except Exception as e:
        print(f"错误：模型文件不存在 {kmodel_path}，请先放入SD卡根目录")
        raise

    yolo_det = YOLOv11App(kmodel_path, model_input_size=[320, 320], anchors=anchors,
                          confidence_threshold=confidence_threshold, nms_threshold=nms_threshold,
                          rgb888p_size=rgb888p_size, display_size=display_size, debug_mode=0)
    yolo_det.config_preprocess()

    sensor0 = None
    sensor1 = None
    sensor2 = None
    try:
        # 初始化顺序符合 skill 规范：Sensor配置 -> Display.init -> MediaManager.init -> sensor.run
        sensor1 = yolo_det.config_camera_and_display(1)
        Display.init(Display.ST7701, width=800, height=480, to_ide=True)
        MediaManager.init()
        time.sleep_ms(200)
        sensor1.run()

        while True:
            os.exitpoint()
            clock.tick()
            time.sleep_ms(10)
            data = uart.read()
            img1 = sensor1.snapshot(chn=CAM_CHN_ID_0)
            img_ai = sensor1.snapshot(chn=CAM_CHN_ID_2)
            Display.show_image(img1, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))
            del img1
            del img_ai
            gc.collect()

            if data and len(data) == 8:
                uart_flag = data
                # 摄像头1（豆子）
                if uart_flag == b'\x02\x01\x00\x00\x00\x00\x00\x00':
                    yolo_det.safe_stop_sensor(sensor1)
                    time.sleep_ms(50)
                    sensor1 = yolo_det.config_camera_and_display(1)
                    time.sleep_ms(200)
                    sensor1.run()
                    time.sleep_ms(200)
                    while True:
                        img_ai = sensor1.snapshot(chn=CAM_CHN_ID_2)
                        img_display = sensor1.snapshot(chn=CAM_CHN_ID_0)
                        dets = yolo_det.run(img_ai)
                        yolo_det.draw_number_sort(dets, 0x02, 3)
                        yolo_det.draw_result(dets, img_display)
                        del img_display
                        del img_ai
                        gc.collect()
                        list_flag_1 = uart.read()
                        if list_flag_1 == b'\x06\x00\x00\x00\x00\x00\x00\x00':
                            yolo_det.safe_stop_sensor(sensor1)
                            gc.collect()
                            time.sleep_ms(50)
                            sensor1 = yolo_det.config_camera_and_display(1)
                            sensor1.run()
                            break

                # 摄像头2（正面数字）
                elif uart_flag == b'\x03\x01\x00\x00\x00\x00\x00\x00':
                    yolo_det.safe_stop_sensor(sensor1)
                    time.sleep_ms(50)
                    yolo_det.reset_front_state()
                    sensor2 = yolo_det.config_camera_and_display(2)
                    time.sleep_ms(200)
                    sensor2.run()
                    time.sleep_ms(200)
                    while True:
                        img_ai = sensor2.snapshot(chn=CAM_CHN_ID_2)
                        img_display = sensor2.snapshot(chn=CAM_CHN_ID_0)
                        dets = yolo_det.run(img_ai)
                        yolo_det.draw_number_sort(dets, 0x03, 5)
                        yolo_det.draw_result(dets, img_display)
                        del img_display
                        del img_ai
                        gc.collect()
                        list_flag_1 = uart.read()
                        if list_flag_1 == b'\x06\x00\x00\x00\x00\x00\x00\x00':
                            yolo_det.safe_stop_sensor(sensor2)
                            gc.collect()
                            time.sleep_ms(50)
                            sensor1 = yolo_det.config_camera_and_display(1)
                            sensor1.run()
                            break

                # 摄像头0（侧面数字）
                elif uart_flag == b'\x01\x01\x00\x00\x00\x00\x00\x00':
                    yolo_det.safe_stop_sensor(sensor1)
                    time.sleep_ms(50)
                    sensor0 = yolo_det.config_camera_and_display(0)
                    time.sleep_ms(200)
                    sensor0.run()
                    time.sleep_ms(200)
                    while True:
                        img_ai = sensor0.snapshot(chn=CAM_CHN_ID_2)
                        img_display = sensor0.snapshot(chn=CAM_CHN_ID_0)
                        dets = yolo_det.run(img_ai)
                        yolo_det.draw_number_sort(dets, 0x01, 5)
                        yolo_det.draw_result(dets, img_display)
                        del img_display
                        del img_ai
                        gc.collect()
                        list_flag_1 = uart.read()
                        if list_flag_1 == b'\x06\x00\x00\x00\x00\x00\x00\x00':
                            # [FIX] 恢复完整 stop+reset，彻底释放
                            yolo_det.safe_stop_sensor(sensor0)
                            sensor0 = None
                            gc.collect()
                            time.sleep_ms(50)
                            sensor1 = yolo_det.config_camera_and_display(1)
                            sensor1.run()
                            break

    except KeyboardInterrupt as e:
        print("用户终止：", e)
    except BaseException as e:
        print(f"异常：{e}")
    finally:
        # [FIX] 清理所有传感器，符合 skill: stop() 必须在 MediaManager.deinit() 之前
        yolo_det.safe_stop_sensor(sensor0)
        yolo_det.safe_stop_sensor(sensor1)
        yolo_det.safe_stop_sensor(sensor2)
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
