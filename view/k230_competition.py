from libs.PipeLine import ScopedTiming
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
import os, sys, gc, time
from media.media import *
import nncase_runtime as nn
import ulab.numpy as np
import image
from media.sensor import *
from media.display import *
from machine import FPIOA, UART
from libs.Utils import *

# ======================== 硬件配置 ========================
fpioa = FPIOA()
fpioa.set_function(50, FPIOA.UART3_TXD)
fpioa.set_function(51, FPIOA.UART3_RXD)

uart = UART(UART.UART3, baudrate=115200, bits=UART.EIGHTBITS,
            parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE)

DISPLAY_WIDTH = 800
DISPLAY_HEIGHT = 480
OUT_RGB888P_WIDTH = ALIGN_UP(640, 16)
OUT_RGB888P_HEIGHT = 360
picture_width = 800
picture_height = 480

# ======================== 模型路径 ========================
KMODEL_PATH = "/sdcard/bestm.kmodel"
MODEL_INPUT_SIZE = [320, 320]

# ======================== 类别映射 ========================
# bestm.kmodel: 8 classes, 数字在 0-4, 豆子在 5-7
CLASS_ID = ["1", "2", "3", "4", "5", "g", "w", "y"]
CLASS_MAP = {
    "1": 0x01, "2": 0x02, "3": 0x03, "4": 0x04, "5": 0x05,
    "g": 0x06,  # 绿豆
    "w": 0x07,  # 芸豆
    "y": 0x08,  # 黄豆
}

# ======================== 通信常量 ========================
FRAME_LEN = 8
CMD_START_BEAN = 0x02
CMD_START_FRONT = 0x03
CMD_START_SIDE = 0x01
CMD_ACK_CLOSE = 0x06
K230_ACK = 0x0A


# ======================== 通信辅助函数 ========================
def calc_xor(data):
    cs = 0
    for b in data:
        cs ^= b
    return cs


def verify_xor(frame):
    return calc_xor(frame[:7]) == frame[7]


def send_frame(frame_type, payload):
    frame = bytearray(FRAME_LEN)
    frame[0] = frame_type
    frame[1] = 0x00
    for i in range(min(len(payload), 5)):
        frame[2 + i] = payload[i]
    frame[7] = calc_xor(frame[:7])
    uart.write(frame)


def send_ack():
    frame = bytearray(FRAME_LEN)
    frame[0] = K230_ACK
    frame[7] = calc_xor(frame[:7])
    uart.write(frame)


def wait_for_cmd(expected_cmd, sensor=None, timeout_ms=30000):
    start = time.ticks_ms()
    while True:
        if sensor is not None:
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
            Display.show_image(img, x=int((DISPLAY_WIDTH - picture_width) / 2),
                               y=int((DISPLAY_HEIGHT - picture_height) / 2))
            del img
        if uart.any() >= FRAME_LEN:
            data = uart.read(FRAME_LEN)
            if data and len(data) == FRAME_LEN and verify_xor(data):
                if data[0] == expected_cmd:
                    return True
        if timeout_ms > 0 and time.ticks_ms() - start > timeout_ms:
            return False
        time.sleep_ms(10)


# ======================== YOLOv11 检测类 ========================
class YOLOv11App(AIBase):
    def __init__(self, kmodel_path, model_input_size,
                 confidence_threshold=0.5, nms_threshold=0.45,
                 rgb888p_size=[640, 360], display_size=[800, 480], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.class_id = CLASS_ID
        self.kmodel_path = kmodel_path
        self.model_input_size = model_input_size
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT,
                                 np.uint8, np.uint8)
        self.pad_top = 0
        self.pad_left = 0
        self.scale_x = 1.0
        self.scale_y = 1.0
        self.osd_img = None

    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right = self.get_padding_param()
            self.ai2d.pad([top, bottom, left, right], 0, [104, 117, 123])
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                            [1, 3, self.model_input_size[1], self.model_input_size[0]])

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
                        cls_idx = list(result[4:]).index(max_score)
                        det_res.append([x, y, w, h, cls_idx, max_score])
                except Exception as e:
                    print(f"postprocess i={i} err: {e}")
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

    def draw_result(self, dets, img_display, status_text=""):
        if self.osd_img is None:
            self.osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
        osd_img = self.osd_img
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
                osd_img.draw_rectangle(x - w // 2, y - h // 2, w, h,
                                       color=(255, 0, 255, 0), thickness=2)
                label = self.class_id[int(det[-2])] if int(det[-2]) < len(self.class_id) else "?"
                osd_img.draw_string_advanced(x - w // 2, y - h // 2, 32,
                                             "{} {}".format(label, round(det[-1], 2)),
                                             color=(255, 0, 255, 0))
        if status_text:
            osd_img.draw_string_advanced(500, 430, 28, status_text, color=(255, 255, 255, 255))
        Display.show_image(img_display,
                           x=int((DISPLAY_WIDTH - picture_width) / 2),
                           y=int((DISPLAY_HEIGHT - picture_height) / 2))
        Display.show_image(osd_img, 0, 0, Display.LAYER_OSD1)

    def get_frame(self, img):
        with ScopedTiming("get a frame", self.debug_mode > 0):
            return img.to_numpy_ref()

    def run(self, input_img):
        try:
            input_np = self.get_frame(input_img)
        except Exception as e:
            print(f"get_frame err: {e}")
            return []
        try:
            self.tensors = self.preprocess(input_np)
        except Exception as e:
            print(f"preprocess err: {e}")
            return []
        try:
            self.results = self.inference(self.tensors)
        except Exception as e:
            print(f"inference err: {e}")
            return []
        try:
            res = self.postprocess(self.results)
        except Exception as e:
            print(f"postprocess err: {e}")
            return []
        return res

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


# ======================== 摄像头辅助函数 ========================
def config_camera(camera_id):
    supported = [0, 1, 2]
    if camera_id not in supported:
        print(f"Unsupported camera ID: {camera_id}")
        return None
    sensor = Sensor(id=camera_id)
    sensor.reset()
    sensor.set_framesize(width=800, height=480, chn=CAM_CHN_ID_0)
    sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGHT, chn=CAM_CHN_ID_2)
    sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
    sensor.set_hmirror(False)
    sensor.set_vflip(False)
    print(f"Camera {camera_id} configured")
    return sensor


def close_sensor(sensor):
    if sensor is not None:
        sensor.stop()
        sensor.reset()
        del sensor
        gc.collect()


# ======================== 识别辅助函数 ========================
def recognize_beans(yolo_det, sensor, target_count=3, stable_frames=3):
    # 豆子: class indices 5-7 (g, w, y)
    last_result = None
    stable_count = 0
    while True:
        os.exitpoint()
        img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
        img_ai = sensor.snapshot(chn=CAM_CHN_ID_2)
        dets = yolo_det.run(img_ai)
        filtered = [d for d in dets if len(d) >= 6 and 5 <= int(d[4]) <= 7]
        filtered.sort(key=lambda x: x[0])
        current = []
        for det in filtered[:target_count]:
            cls_name = yolo_det.class_id[int(det[4])]
            current.append(CLASS_MAP.get(cls_name, 0x00))
        current += [0x00] * (target_count - len(current))
        yolo_det.draw_result(dets, img_display, "Bean scanning")
        del img_display, img_ai
        gc.collect()
        if 0x00 not in current:
            if current == last_result:
                stable_count += 1
            else:
                stable_count = 0
                last_result = current
            if stable_count >= stable_frames:
                print(f"Beans stable: {['0x%02X' % b for b in current]}")
                return current
        else:
            stable_count = 0
        time.sleep_ms(10)


def recognize_front_numbers(yolo_det, sensor, target_count=3, stable_frames=3):
    # 数字: class indices 0-4 (1, 2, 3, 4, 5)
    last_result = None
    stable_count = 0
    while True:
        os.exitpoint()
        img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
        img_ai = sensor.snapshot(chn=CAM_CHN_ID_2)
        dets = yolo_det.run(img_ai)
        filtered = [d for d in dets if len(d) >= 6 and 0 <= int(d[4]) <= 4]
        filtered.sort(key=lambda x: x[0])
        current = []
        for det in filtered[:target_count]:
            cls_name = yolo_det.class_id[int(det[4])]
            current.append(CLASS_MAP.get(cls_name, 0x00))
        current += [0x00] * (target_count - len(current))
        yolo_det.draw_result(dets, img_display, "Front numbers")
        del img_display, img_ai
        gc.collect()
        if 0x00 not in current:
            if current == last_result:
                stable_count += 1
            else:
                stable_count = 0
                last_result = current
            if stable_count >= stable_frames:
                print(f"Front numbers stable: {['0x%02X' % b for b in current]}")
                return current
        else:
            stable_count = 0
        time.sleep_ms(10)


def recognize_side_number(yolo_det, sensor, stable_frames=3):
    # 数字: class indices 0-4 (1, 2, 3, 4, 5)
    last_result = None
    stable_count = 0
    while True:
        os.exitpoint()
        img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
        img_ai = sensor.snapshot(chn=CAM_CHN_ID_2)
        dets = yolo_det.run(img_ai)
        filtered = [d for d in dets if len(d) >= 6 and 0 <= int(d[4]) <= 4]
        if filtered:
            best = max(filtered, key=lambda x: x[5])
            cls_name = yolo_det.class_id[int(best[4])]
            current = CLASS_MAP.get(cls_name, 0x00)
        else:
            current = 0x00
        yolo_det.draw_result(dets, img_display, "Side number")
        del img_display, img_ai
        gc.collect()
        if current != 0x00:
            if current == last_result:
                stable_count += 1
            else:
                stable_count = 0
                last_result = current
            if stable_count >= stable_frames:
                print(f"Side number stable: 0x{current:02X}")
                return current
        else:
            stable_count = 0
        time.sleep_ms(10)


def infer_fifth(known_nums):
    all_nums = {0x01, 0x02, 0x03, 0x04, 0x05}
    known_set = set(known_nums) - {0x00}
    if len(known_set) == 4:
        missing = all_nums - known_set
        return missing.pop()
    return 0x00


# ======================== 主函数 ========================
if __name__ == "__main__":
    clock = time.clock()
    sensor = None
    yolo_det = None
    front_numbers = None

    try:
        Display.init(Display.ST7701, width=800, height=480, to_ide=True)
        MediaManager.init()
        time.sleep_ms(200)

        # 加载唯一模型 bestm.kmodel (8 classes: 1-5, g, w, y)
        yolo_det = YOLOv11App(KMODEL_PATH, MODEL_INPUT_SIZE,
                              rgb888p_size=[640, 360], display_size=[800, 480])
        yolo_det.config_preprocess()

        # ============ Phase 1: 等待 STM32 命令 -> 豆子识别 (Camera 2) ============
        print("========== Phase 1: Waiting for LOOK_BEAN (0x02) ==========")
        if not wait_for_cmd(CMD_START_BEAN, timeout_ms=30000):
            print("Phase 1 command timeout!")
            raise Exception("P1 timeout")
        print("Command received, sending ACK")
        send_ack()

        sensor = config_camera(2)
        sensor.run()
        time.sleep_ms(200)

        bean_result = recognize_beans(yolo_det, sensor)

        # 发送豆子数据 [0x02, 0x00, b1, b2, b3, 0x00, 0x00, XOR]
        send_frame(CMD_START_BEAN, bean_result)
        print(f"Bean data sent: {['0x%02X' % b for b in bean_result]}")

        # 等待 STM32 应答
        if not wait_for_cmd(CMD_ACK_CLOSE, sensor=sensor, timeout_ms=10000):
            print("Phase 1 ACK timeout!")
        print("STM32 ACK received, closing camera 2")

        close_sensor(sensor)
        sensor = None

        # ============ Phase 2: 等待 STM32 命令 -> 正面数字识别 (Camera 1) ============
        print("========== Phase 2: Waiting for LOOK_NUMBER (0x03) ==========")
        if not wait_for_cmd(CMD_START_FRONT, timeout_ms=30000):
            print("Phase 2 command timeout!")
            raise Exception("P2 timeout")
        print("Command received, sending ACK")
        send_ack()

        sensor = config_camera(1)
        sensor.run()
        time.sleep_ms(200)

        front_numbers = recognize_front_numbers(yolo_det, sensor)
        print(f"Front numbers: {['0x%02X' % b for b in front_numbers]}")

        # 发送正面数字 [0x03, 0x00, n1, n2, n3, 0x00, 0x00, XOR]
        send_frame(CMD_START_FRONT, front_numbers)
        print("Front number data sent")

        # 等待 STM32 应答
        if not wait_for_cmd(CMD_ACK_CLOSE, sensor=sensor, timeout_ms=10000):
            print("Phase 2 ACK timeout!")
        print("STM32 ACK received, closing camera 1")

        close_sensor(sensor)
        sensor = None

        # ============ Phase 3: 等待 STM32 命令 -> 侧面数字 + 推理 (Camera 0) ============
        print("========== Phase 3: Waiting for LOOK_SIDE (0x01) ==========")
        if not wait_for_cmd(CMD_START_SIDE, timeout_ms=30000):
            print("Phase 3 command timeout!")
            raise Exception("P3 timeout")
        print("Command received, sending ACK")
        send_ack()

        sensor = config_camera(0)
        sensor.run()
        time.sleep_ms(200)

        side_number = recognize_side_number(yolo_det, sensor)
        print(f"Side number: 0x{side_number:02X}")

        # 推理第5个数字
        known = [side_number] + front_numbers
        fifth = infer_fifth(known)
        full_result = known + [fifth]
        print(f"Full 5 numbers: {['0x%02X' % b for b in full_result]}")

        # 发送完整5个数字 [0x01, 0x00, n1, n2, n3, n4, n5, XOR]
        send_frame(CMD_START_SIDE, full_result)
        print("Full number data sent")

        # 等待 STM32 应答
        if not wait_for_cmd(CMD_ACK_CLOSE, sensor=sensor, timeout_ms=10000):
            print("Phase 3 ACK timeout!")
        print("STM32 ACK received, closing camera 0")

        close_sensor(sensor)
        sensor = None

        print("========== All phases complete ==========")

    except KeyboardInterrupt as e:
        print("User interrupted:", e)
    except BaseException as e:
        print(f"Exception: {e}")
    finally:
        if sensor is not None:
            sensor.stop()
            sensor.reset()
            del sensor
        if yolo_det is not None:
            yolo_det.deinit()
            del yolo_det
        Display.deinit()
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        MediaManager.deinit()
        nn.shrink_memory_pool()
        gc.collect()
