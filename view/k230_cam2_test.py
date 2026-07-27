from libs.PipeLine import ScopedTiming
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
import os, sys, gc, time
import nncase_runtime as nn
import ulab.numpy as np
import image
from media.media import *
from media.sensor import *
from media.display import *
from libs.Utils import *

CAMERA_ID = 2
KMODEL_PATH = "/sdcard/best.kmodel"

OUT_RGB888P_WIDTH = ALIGN_UP(640, 16)
OUT_RGB888P_HEIGHT = 360
DISPLAY_WIDTH = 800
DISPLAY_HEIGHT = 480
picture_width = 800
picture_height = 480

class_1_map = {
    "1": 0x01,
    "2": 0x02,
    "3": 0x03,
    "4": 0x04,
    "5": 0x05,
    "g": 0x06,
    "w": 0x07,
    "y": 0x08,
}


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

    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right = self.get_padding_param()
            self.ai2d.pad([top, bottom, left, right], 0, [104, 117, 123])
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
                label_text = self.class_id[cid] if cid < len(self.class_id) else "?"
                img_display.draw_string_advanced(label_x, y - h // 2, 40, "{} {}".format(label_text, round(det[-1], 2)), color=(255, 0, 0))
        Display.show_image(img_display, x=int((DISPLAY_WIDTH - picture_width) / 2), y=int((DISPLAY_HEIGHT - picture_height) / 2))

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

    def config_camera(self, camera_id):
        sensor = Sensor(id=camera_id)
        sensor.reset()
        sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, chn=CAM_CHN_ID_0)
        sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
        sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGHT, chn=CAM_CHN_ID_2)
        sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
        sensor.set_hmirror(False)
        sensor.set_vflip(False)
        print(f"摄像头 {camera_id} 配置完成")
        return sensor


if __name__ == "__main__":
    clock = time.clock()
    rgb888p_size = [640, 360]
    display_size = [800, 480]

    confidence_threshold = 0.5
    nms_threshold = 0.45
    anchors = None

    try:
        os.stat(KMODEL_PATH)
        print(f"模型文件存在: {KMODEL_PATH}")
    except Exception as e:
        print(f"错误: 模型文件不存在 {KMODEL_PATH}, 请先放入SD卡根目录")
        raise

    yolo_det = YOLOv11App(KMODEL_PATH, model_input_size=[320, 320], anchors=anchors,
                          confidence_threshold=confidence_threshold, nms_threshold=nms_threshold,
                          rgb888p_size=rgb888p_size, display_size=display_size, debug_mode=0)
    yolo_det.config_preprocess()

    sensor = None
    try:
        Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        MediaManager.init()
        time.sleep_ms(200)

        sensor = yolo_det.config_camera(CAMERA_ID)
        time.sleep_ms(200)
        sensor.run()
        time.sleep_ms(200)
        print(f"===== cam{CAMERA_ID} + {KMODEL_PATH} 推理测试 =====")

        while True:
            os.exitpoint()
            clock.tick()
            time.sleep_ms(10)
            img_ai = sensor.snapshot(chn=CAM_CHN_ID_2)
            img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
            dets = yolo_det.run(img_ai)
            yolo_det.draw_result(dets, img_display)
            del img_display
            del img_ai
            gc.collect()

    except KeyboardInterrupt as e:
        print("用户终止:", e)
    except BaseException as e:
        print(f"异常: {e}")
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
