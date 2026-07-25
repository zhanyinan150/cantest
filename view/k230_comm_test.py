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

# 配置串口：50/51 号脚是 K230 板上 UART3 的引脚
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
    "1": 0x01,
    "2": 0x02,
    "3": 0x03,
    "4": 0x04,
    "5": 0x05,
    "g": 0x06,
    "w": 0x07,
    "y": 0x08
}

bean_name_map = {0x06: "g", 0x07: "w", 0x08: "y"}


def frame_checksum(frame):
    """计算 8 字节帧 XOR 校验码 (byte[0]^...^byte[6])"""
    cs = 0
    for b in frame[:7]:
        cs ^= b
    return cs


def make_frame(frame_type, data_bytes):
    """构造 8 字节帧: [type, 0x00, data0..data4, checksum]"""
    frame = bytearray(8)
    frame[0] = frame_type
    for i in range(min(5, len(data_bytes))):
        frame[2 + i] = data_bytes[i]
    frame[7] = frame_checksum(frame)
    return frame


def check_command(data, cmd_byte):
    """检查 UART 数据是否为指定命令(8字节, 带 XOR 校验)"""
    if data and len(data) == 8:
        cs = 0
        for b in data[:7]:
            cs ^= b
        if cs == data[7] and data[0] == cmd_byte:
            return True
    return False


class YOLOv11App(AIBase):
    def __init__(self, kmodel_path, model_input_size, anchors, confidence_threshold=0.8, nms_threshold=0.2, rgb888p_size=[640, 360], display_size=[800, 480], debug_mode=0):
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

        self.last_result = None
        self.stable_count = 0
        self.side_result = None
        self.full_result = None
        self.front_result_a = None
        self.front_last_result = None
        self.front_stable_count = 0

        self.send_status_text = ""
        self.send_status_color = (255, 255, 255, 255)

        self.bean_frame_sent = False
        self.front_frame_sent = False
        self.full_frame_sent = False

    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right = self.get_padding_param()
            print("padding: {} {} {} {}".format(top, bottom, left, right))
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
                        x = result[0] * max(self.rgb888p_size) / max(self.model_input_size)
                        y = result[1] * max(self.rgb888p_size) / max(self.model_input_size)
                        w = result[2] * max(self.rgb888p_size) / max(self.model_input_size)
                        h = result[3] * max(self.rgb888p_size) / max(self.model_input_size)
                        det_res.append([x, y, w, h, list(result[4:]).index(max_score), max_score])
                except Exception as e:
                    print(f"i={i} 出错: {e}")
                    continue
            det_res.sort(key=lambda x: x[-1], reverse=True)
            det_res_single = []
            added_class = []
            for result in det_res:
                if not result[-2] in added_class:
                    added_class.append(result[-2])
                    det_res_single.append(result)
        return det_res_single

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
                label_text = self.class_id[cid] if cid < len(self.class_id) else "?"
                img_display.draw_string_advanced(label_x, y - h // 2, 40, "{} {}".format(label_text, round(det[-1], 2)), color=(255, 0, 0))

        if self.send_status_text:
            img_display.draw_string_advanced(580, 430, 30, self.send_status_text, color=(255, 255, 255))
        Display.show_image(img_display, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))

    def save_side_result(self, dets):
        """保存摄像头0(侧面)识别的数字, 取置信度最高的1个"""
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
        """根据4个不重复的1-5数字, 推测缺失的第5个"""
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

    def prevent_shaking(self, full_result):
        if full_result == self.last_result:
            self.stable_count += 1
        else:
            self.stable_count = 0
            self.last_result = full_result
        print(f"[防抖] count={self.stable_count}")
        if self.stable_count < 3:
            return False
        else:
            self.last_result = None
            self.stable_count = 0
            return True

    def draw_number_sort(self, dets, sensor_number, thing_num: int):
        filtered_dets = [det for det in dets if len(det) >= 6 and det[5] >= 0.80]
        res_sorted = sorted(filtered_dets, key=lambda x: x[0])

        # 数字识别场景
        if thing_num == 5:
            # 摄像头2(正面): 防抖 + 发送正面3数字帧
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
                        if not self.front_frame_sent:
                            frame = make_frame(0x03, self.front_result_a)
                            self.send_status_text = "发送正面: " + " ".join(str(x) for x in self.front_result_a)
                            uart.write(frame)
                            print(f"发送正面数字帧: {frame.hex()}")
                            self.front_frame_sent = True
                        self.front_stable_count = 0
                    else:
                        hex_strings = ['0x{:02X}'.format(x) for x in current_front]
                        print(f"正面数字防抖中：第{self.front_stable_count}帧，当前：{hex_strings}")
                return

            # 摄像头0(侧面): 整合 + 推测 + 发送5数字帧
            elif sensor_number == 0x01:
                self.save_side_result(res_sorted)
                integrated = self.integrate_results()
                fifth_num = self.infer_fifth_number(integrated)
                self.full_result = integrated + [fifth_num]
                self.full_result = self.full_result[:5] + [0x00] * (5 - len(self.full_result))

                if self.prevent_shaking(self.full_result):
                    if self.is_valid_data(self.full_result):
                        frame = make_frame(0x01, self.full_result)
                        self.send_status_text = "发送数字: " + " ".join(str(x) for x in self.full_result)
                        uart.write(frame)
                        print(f"发送完整5位数字帧: {frame.hex()}")
                        self.full_frame_sent = True
                    else:
                        print("侧面数据无效，跳过发送")
                    self.side_result = None
                    self.full_result = None
                return self.full_result

        # 豆子识别场景
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
                if self.prevent_shaking(send_data):
                    if self.is_valid_data(send_data):
                        frame = make_frame(0x02, send_data)
                        self.send_status_text = "发送豆子: " + " ".join(bean_name_map.get(x, "?") for x in send_data)
                        uart.write(frame)
                        print(f"发送豆子帧: {frame.hex()}")
                        self.bean_frame_sent = True
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
        return top, bottom, left, right

    def config_camera_and_display(self, camera_id):
        supported_ids = [0, 1, 2]
        if camera_id not in supported_ids:
            print(f"错误：不支持的摄像头ID {camera_id}，仅支持{supported_ids}")
            return None
        print(f"配置摄像头 {camera_id}")
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
    display_size = [800, 480]
    rgb888p_size = [640, 360]

    kmodel_path = "/sdcard/best1.kmodel"
    confidence_threshold = 0.3
    nms_threshold = 0.2
    anchors = None

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
        # ===== Phase 1: 豆子识别 (K230 上电自主启动, cam2) =====
        sensor1 = yolo_det.config_camera_and_display(2)
        Display.init(Display.ST7701, width=800, height=480, to_ide=True)
        MediaManager.init()
        time.sleep_ms(200)
        sensor1.run()
        time.sleep_ms(200)

        while not yolo_det.bean_frame_sent:
            os.exitpoint()
            clock.tick()
            time.sleep_ms(10)
            img_ai = sensor1.snapshot(chn=CAM_CHN_ID_2)
            img_display = sensor1.snapshot(chn=CAM_CHN_ID_0)
            dets = yolo_det.run(img_ai)
            yolo_det.draw_number_sort(dets, 0x02, 3)
            yolo_det.draw_result(dets, img_display)
            del img_display
            del img_ai
            gc.collect()

        # 等待 STM32 ACK (0x06)
        while True:
            os.exitpoint()
            data = uart.read()
            if check_command(data, 0x06):
                print("收到豆子ACK")
                break
            time.sleep_ms(10)

        yolo_det.safe_stop_sensor(sensor1)
        sensor1 = None
        gc.collect()
        time.sleep_ms(50)

        # 等待 STM32 触发正面数字识别 (0x03)
        while True:
            os.exitpoint()
            data = uart.read()
            if check_command(data, 0x03):
                print("收到正面数字触发命令")
                break
            time.sleep_ms(10)

        # ===== Phase 2: 正面数字识别 (cam1) =====
        sensor2 = yolo_det.config_camera_and_display(1)
        time.sleep_ms(200)
        sensor2.run()
        time.sleep_ms(200)
        yolo_det.reset_front_state()

        while not yolo_det.front_frame_sent:
            os.exitpoint()
            clock.tick()
            time.sleep_ms(10)
            img_ai = sensor2.snapshot(chn=CAM_CHN_ID_2)
            img_display = sensor2.snapshot(chn=CAM_CHN_ID_0)
            dets = yolo_det.run(img_ai)
            yolo_det.draw_number_sort(dets, 0x03, 5)
            yolo_det.draw_result(dets, img_display)
            del img_display
            del img_ai
            gc.collect()

        # 等待 STM32 ACK (0x06) - 收到后自动转侧面
        while True:
            os.exitpoint()
            data = uart.read()
            if check_command(data, 0x06):
                print("收到正面数字ACK, 转侧面识别")
                break
            time.sleep_ms(10)

        yolo_det.safe_stop_sensor(sensor2)
        sensor2 = None
        gc.collect()
        time.sleep_ms(50)

        # ===== Phase 3: 侧面数字识别 (cam0, 自动启动) =====
        sensor0 = yolo_det.config_camera_and_display(0)
        time.sleep_ms(200)
        sensor0.run()
        time.sleep_ms(200)

        while not yolo_det.full_frame_sent:
            os.exitpoint()
            clock.tick()
            time.sleep_ms(10)
            img_ai = sensor0.snapshot(chn=CAM_CHN_ID_2)
            img_display = sensor0.snapshot(chn=CAM_CHN_ID_0)
            dets = yolo_det.run(img_ai)
            yolo_det.draw_number_sort(dets, 0x01, 5)
            yolo_det.draw_result(dets, img_display)
            del img_display
            del img_ai
            gc.collect()

        # 等待 STM32 最终 ACK (0x06)
        while True:
            os.exitpoint()
            data = uart.read()
            if check_command(data, 0x06):
                print("收到最终ACK, K230停止")
                break
            time.sleep_ms(10)

        yolo_det.safe_stop_sensor(sensor0)
        sensor0 = None

        # 显示完成
        osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
        osd_img.clear()
        osd_img.draw_string_advanced(300, 220, 50, "完成", color=(255, 0, 255, 0))
        Display.show_image(osd_img, 0, 0, Display.LAYER_OSD1)

    except KeyboardInterrupt as e:
        print("用户终止：", e)
    except BaseException as e:
        print(f"异常：{e}")
    finally:
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
