from libs.PipeLine import PipeLine, ScopedTiming
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
import os, sys, gc, time
import ujson
from media.media import *
from time import *
import nncase_runtime as nn
import ulab.numpy as np
import image, aidemo, aicube
from media.sensor import *
from media.display import *
from machine import Pin, FPIOA, UART
from libs.Utils import *

# 串口配置：50/51 号脚是 K230 板上 UART3 的引脚
fpioa = FPIOA()
fpioa.set_function(50, FPIOA.UART3_TXD)
fpioa.set_function(51, FPIOA.UART3_RXD)

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
    "1": 0x01,
    "2": 0x02,
    "3": 0x03,
    "4": 0x04,
    "5": 0x05,
    "g": 0x06,  # 绿豆
    "y": 0x07,  # 黄豆
    "w": 0x08,  # 云豆
}


def frame_checksum(frame):
    cs = 0
    for b in frame[:7]:
        cs ^= b
    return cs


def make_frame(frame_type, data_bytes):
    frame = bytearray(8)
    frame[0] = frame_type
    for i in range(min(5, len(data_bytes))):
        frame[2 + i] = data_bytes[i]
    frame[7] = frame_checksum(frame)
    return frame


class YOLOv11App(AIBase):
    def __init__(self, kmodel_path, model_input_size, anchors, confidence_threshold=0.5, nms_threshold=0.45, rgb888p_size=[640, 360], display_size=[800, 480], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.class_id = ["1", "2", "3", "4", "5", "g", "w", "y"]
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

        self.pad_top = 0
        self.pad_left = 0
        self.scale_x = 1.0
        self.scale_y = 1.0

        self.shake_state = {}

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
            # 8 个值: NCHW 各维度前后两侧, 只在 H/W 填充。少传成 4 个会让 bottom
            # 落到 batch 维上, H/W 没补 -> 模型收到的不是 letterbox 图, 检不出框。
            self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [104, 117, 123])
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]], [1, 3, self.model_input_size[1], self.model_input_size[0]])

    def postprocess(self, results):
        det_res = []
        with ScopedTiming("postprocess", self.debug_mode > 0):
            for i in range(2100):
                try:
                    result = results[0][0][:, i]
                    max_score = max(result[4:])
                    if max_score > self.confidence_threshold:
                        x = (result[0] - self.pad_left) * self.scale_x
                        y = (result[1] - self.pad_top) * self.scale_y
                        w = result[2] * self.scale_x
                        h = result[3] * self.scale_y
                        det_res.append([x, y, w, h, list(result[4:]).index(max_score), max_score])
                except Exception as e:
                    print(f"i={i} 出错: {e}")
                    continue
            det_res.sort(key=lambda x: x[-1], reverse=True)
            det_res = self._nms(det_res, self.nms_threshold)
        return det_res

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
        if dets:
            for det in dets:
                if len(det) < 6:
                    continue
                x, y, w, h = map(lambda v: int(round(v, 0)), det[:4])
                x = x * self.display_size[0] // self.rgb888p_size[0]
                y = y * self.display_size[1] // self.rgb888p_size[1]
                w = w * self.display_size[0] // self.rgb888p_size[0]
                h = h * self.display_size[1] // self.rgb888p_size[1]
                img_display.draw_rectangle(x - w // 2, y - h // 2, w, h, color=(255, 0, 0), thickness=2)
                label_x = x - w // 2
                if label_x < 0:
                    label_x = 0
                elif label_x > DISPLAY_WIDTH - 280:
                    label_x = DISPLAY_WIDTH - 280
                cid = int(det[-2])
                label_text = str(int(self.class_id[cid]) % 5 + 1) if cid <= 4 else self.class_id[cid]
                img_display.draw_string_advanced(label_x, y - h // 2, 40, "{} {}".format(label_text, round(det[-1], 2)), color=(255, 0, 0))
        if self.send_status_text:
            img_display.draw_string_advanced(580, 430, 30, self.send_status_text, color=(255, 255, 255))
        Display.show_image(img_display, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))

    def save_side_result(self, dets):
        if not dets:
            self.side_result = None
            return 0x00
        valid_dets = []
        for d in dets:
            if len(d) >= 6:
                class_idnum = int(d[4])
                if 0 <= class_idnum <= 4:
                    valid_dets.append(d)
                else:
                    cls_name = self.class_id[class_idnum] if class_idnum < len(self.class_id) else "unknown"
                    print(f"已拦截侧面干扰物: {cls_name}")
        if not valid_dets:
            self.side_result = None
            return 0x00
        best_det = max(valid_dets, key=lambda x: x[5])
        class_idnum = best_det[4]
        cls_name = self.class_id[int(class_idnum)] if int(class_idnum) < len(self.class_id) else ""
        self.side_result = class_1_map.get(cls_name, 0x00)
        print(f"侧面结果已保存: {cls_name} (0x{self.side_result:02X})")
        return self.side_result

    def integrate_results(self):
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
        if not data_list or all(item == 0x00 for item in data_list):
            self.send_status_text = "无效信息发送"
            self.send_status_color = (255, 255, 0, 0)
            return False
        self.send_status_text = "有效信息发送"
        self.send_status_color = (255, 0, 255, 0)
        return True

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
        res_sorted = sorted(dets, key=lambda x: x[0])

        if thing_num == 5:
            if sensor_number == 0x03:
                if self.front_last_result is None:
                    self.front_result_a = None
                    self.front_stable_count = 0
                current_front = []
                for det in res_sorted[:3]:
                    class_idnum = int(det[4])
                    if not (0 <= class_idnum <= 4):
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

            elif sensor_number == 0x01:
                self.save_side_result(res_sorted)
                integrated = self.integrate_results()
                fifth_num = self.infer_fifth_number(integrated)
                self.full_result = integrated + [fifth_num]
                self.full_result = self.full_result[:5] + [0x00] * (5 - len(self.full_result))

                MA = make_frame(sensor_number, self.full_result)
                if self.prevent_shaking(self.full_result, key='side'):
                    if self.is_valid_data(self.full_result):
                        uart.write(MA)
                        print(f"发送完整5位数字: {MA.hex()}")
                    else:
                        print("侧面数据无效，跳过发送")
                    self.side_result = None
                    self.full_result = None
                return self.full_result

        elif thing_num == 3:
            send_data = []
            for obj in res_sorted:
                if len(obj) >= 5:
                    class_idnum = int(obj[4])
                    if 5 <= class_idnum <= 7:
                        cls_name = self.class_id[class_idnum]
                        send_data.append(class_1_map.get(cls_name, 0x00))
            send_data = send_data[:3] + [0x00] * (3 - len(send_data))
            if 0x00 not in send_data:
                MA = make_frame(sensor_number, send_data)
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
        self.pad_top = top
        self.pad_left = left
        self.scale_x = self.rgb888p_size[0] / new_w if new_w > 0 else 1.0
        self.scale_y = self.rgb888p_size[1] / new_h if new_h > 0 else 1.0
        return top, bottom, left, right

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

    def config_camera_and_display(self, camera_id):
        supported_ids = [0, 1, 2]
        if camera_id not in supported_ids:
            print(f"错误：不支持的摄像头ID {camera_id}，仅支持{supported_ids}")
            return None
        sensor = Sensor(id=camera_id)
        sensor.reset()
        sensor.set_framesize(width=800, height=480, chn=CAM_CHN_ID_0)
        sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
        sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGHT, chn=CAM_CHN_ID_2)
        sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
        sensor.set_hmirror(False)
        sensor.set_vflip(False)
        print(f"摄像头 {camera_id} 配置完成")
        return sensor

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

    def reset_front_state(self):
        self.front_result_a = None
        self.front_last_result = None
        self.front_stable_count = 0
        if 'side' in self.shake_state:
            self.shake_state['side'] = {'last': None, 'count': 0}


# ===== 场景配置 =====
# 命令字节 -> (摄像头ID, 模型路径, sensor_number, thing_num, 场景名)
# thing_num: 3=豆子场景, 5=数字场景
# sensor_number: 0x01=侧面数字, 0x02=豆子, 0x03=正面数字
SCENES = {
    0x02: (2, "/sdcard/best1.kmodel", 0x02, 3, "豆子"),
    0x03: (1, "/sdcard/best2.kmodel", 0x03, 5, "正面数字"),
    0x01: (0, "/sdcard/best2.kmodel", 0x01, 5, "侧面数字"),
}
STOP_CMD = b'\x06\x00\x00\x00\x00\x00\x00\x00'
IDLE_CAMERA = 2
IDLE_KMODEL = "/sdcard/best1.kmodel"


if __name__ == "__main__":
    clock = time.clock()
    rgb888p_size = [640, 360]
    display_size = [800, 480]

    confidence_threshold = 0.5
    nms_threshold = 0.45
    anchors = None

    all_kmodels = set([IDLE_KMODEL] + [s[1] for s in SCENES.values()])
    for kmodel_path in all_kmodels:
        try:
            os.stat(kmodel_path)
            print(f"模型文件存在: {kmodel_path}")
        except Exception as e:
            print(f"错误：模型文件不存在 {kmodel_path}")
            raise

    os.exitpoint(os.EXITPOINT_ENABLE)
    nn.shrink_memory_pool()
    Display.init(Display.ST7701, width=800, height=480, to_ide=True)
    MediaManager.init()
    time.sleep_ms(200)

    sensor = None
    yolo_det = None
    current_kmodel = None

    def load_model(kmodel_path):
        nonlocal yolo_det, current_kmodel
        if kmodel_path == current_kmodel:
            return
        if yolo_det is not None:
            try:
                yolo_det.deinit()
            except Exception as e:
                print(f"旧模型释放失败: {e}")
            yolo_det = None
            gc.collect()
            time.sleep_ms(50)
        print(f"加载模型: {kmodel_path}")
        yolo_det = YOLOv11App(kmodel_path, model_input_size=[320, 320], anchors=anchors,
                              confidence_threshold=confidence_threshold, nms_threshold=nms_threshold,
                              rgb888p_size=rgb888p_size, display_size=display_size, debug_mode=0)
        yolo_det.config_preprocess()
        current_kmodel = kmodel_path

    def switch_sensor(camera_id):
        nonlocal sensor
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

    try:
        load_model(IDLE_KMODEL)
        switch_sensor(IDLE_CAMERA)
        print("===== 待机中 (摄像头2)，等待 STM32 命令 =====")

        while True:
            os.exitpoint()
            clock.tick()
            time.sleep_ms(10)
            data = uart.read()
            img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
            Display.show_image(img_display, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))
            del img_display
            gc.collect()

            if data and len(data) == 8:
                cmd = data[0]
                if cmd not in SCENES:
                    continue

                camera_id, kmodel_path, sensor_num, thing_num, scene_name = SCENES[cmd]
                print(f"\n===== 收到命令 0x{cmd:02X}：{scene_name} =====")

                if sensor_num == 0x03:
                    yolo_det.reset_front_state()

                load_model(kmodel_path)
                switch_sensor(camera_id)
                print(f"[{scene_name}] 摄像头{camera_id} 已启动，开始识别")

                while True:
                    os.exitpoint()
                    clock.tick()
                    time.sleep_ms(10)
                    stop_data = uart.read()
                    if stop_data == STOP_CMD:
                        print(f"[{scene_name}] 收到停止命令，回待机")
                        break

                    img_ai = sensor.snapshot(chn=CAM_CHN_ID_2)
                    img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
                    dets = yolo_det.run(img_ai)
                    yolo_det.draw_number_sort(dets, sensor_num, thing_num)
                    yolo_det.draw_result(dets, img_display)
                    del img_display
                    del img_ai
                    gc.collect()

                load_model(IDLE_KMODEL)
                switch_sensor(IDLE_CAMERA)
                print("===== 已回待机 =====")

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
        if yolo_det is not None:
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
