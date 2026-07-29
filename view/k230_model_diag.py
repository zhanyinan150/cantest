"""K230 模型识别诊断脚本

用途: 比赛脚本 k230_competition.py 识别不出东西, 但单摄像头测试脚本能画出框时,
      用它定位卡在哪一环。推理路径(预处理/推理/后处理/padding换算)与比赛脚本
      **逐行一致**, 只把判定门槛拆开逐层打印, 所以结论可以直接搬回比赛脚本。

为什么"测试脚本能识别"不等于"比赛脚本能识别":

  k230_cam2_test.py 的 draw_result 画**所有**检测框, 不看类别 —— 只要模型吐出
  一个框, 屏幕上就有东西, 看起来"能识别"。
  而 k230_competition.py 的 recognize_* 要同时过三道门槛才算成功:

    门槛1  置信度 > 0.3          (测试脚本用的是 0.2, 低了 0.1)
    门槛2  类别索引落在指定区间   (豆子 5~7 / 数字 0~4)
    门槛3  同时识别到 3 个目标, 且连续 3 帧结果完全一致

  本脚本把这三道门槛拆开, 每帧打印各层剩几个, 一眼看出是哪层滤没的。

用法:
  1. 拷到 SD 卡, 用 CanMV IDE 运行
  2. 把要识别的东西放到摄像头前
  3. 看串口输出的 [F...] 行

怎么读输出:
  raw=5 | >=0.30: 3 | bean(5~7): 2 | need3: MISS | stable 0/3
   ^^^^^   ^^^^^^^^   ^^^^^^^^^^^^   ^^^^^^^^^^^   ^^^^^^^^^
   |       |          |              |             连续几帧一致
   |       |          |              够不够 3 个
   |       |          类别过滤后剩几个
   |       过阈值后剩几个
   模型原始输出(阈值压到 0.05)

  - raw=0            -> 模型压根没输出, 查模型文件/摄像头/光照
  - raw>0 但 >=0.30 是 0 -> **置信度不够, 降 CONF_MAIN 或重训模型**
  - >=0.30 有但 bean 是 0 -> **类别索引对不上**, 看下面的"各类别最高分"
  - bean 够但 need3 一直 MISS -> 只认出部分目标, 摆位/光照/遮挡问题
  - need3 OK 但 stable 攒不满 -> 结果在抖, 放宽 STABLE_FRAMES

注意: 本脚本不做 UART 通讯, 纯本地诊断。
"""

from libs.PipeLine import ScopedTiming
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
import os, gc, time
from media.media import *
import nncase_runtime as nn
import ulab.numpy as np
import image
from media.sensor import *
from media.display import *
from libs.Utils import *

# ======================== 可调参数 ========================
KMODEL_PATH = "/sdcard/bestm.kmodel"   # 与比赛脚本保持一致
MODEL_INPUT_SIZE = [320, 320]

CAMERA_ID = 2          # 2=看豆子(Phase1) 1=正面数字(Phase2) 0=侧面数字(Phase3)
MODE = "bean"          # "bean" 看类别 5~7, "number" 看类别 0~4

CONF_RAW = 0.05        # 后处理阈值压到很低, 好看到"差一点就过线"的检测
CONF_MAIN = 0.3        # 比赛脚本实际用的阈值 (k230_competition.py 的 0.3)
NMS_THRESHOLD = 0.45
TARGET_COUNT = 3       # 比赛脚本要求同时识别到几个
STABLE_FRAMES = 3      # 比赛脚本要求连续几帧一致

PRINT_EVERY = 5        # 每几帧打印一次, 免得刷屏

# ======================== 固定配置 (与比赛脚本一致) ========================
DISPLAY_WIDTH = 800
DISPLAY_HEIGHT = 480
OUT_RGB888P_WIDTH = ALIGN_UP(640, 16)
OUT_RGB888P_HEIGHT = 360

CLASS_ID = ["1", "2", "3", "4", "5", "g", "w", "y"]
CLASS_MAP = {
    "1": 0x01, "2": 0x02, "3": 0x03, "4": 0x04, "5": 0x05,
    "g": 0x06, "w": 0x07, "y": 0x08,
}


# ======================== 检测类 (与比赛脚本逐行一致) ========================
class YOLOv11App(AIBase):
    def __init__(self, kmodel_path, model_input_size,
                 confidence_threshold=0.3, nms_threshold=0.45,
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
        self.shape_reported = False   # 输出张量形状只打印一次

    def config_preprocess(self, input_image_size=None):
        ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
        top, bottom, left, right = self.get_padding_param()
        # 8 个值: NCHW 各维度前后两侧, 只在 H/W 填充。少传成 4 个会让 bottom
        # 落到 batch 维上, H/W 没补 -> 模型收到的不是 letterbox 图, 检不出框。
        self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [104, 117, 123])
        self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
        self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                        [1, 3, self.model_input_size[1], self.model_input_size[0]])

    def preprocess(self, input_np):
        return [self.ai2d.run(input_np)]

    def inference(self, tensors):
        self.results.clear()
        for i in range(self.kpu.inputs_size()):
            self.kpu.set_input_tensor(i, tensors[i])
        self.kpu.run()
        for i in range(self.kpu.outputs_size()):
            output_data = self.kpu.get_output_tensor(i)
            result = output_data.to_numpy()
            self.results.append(result)
            del output_data

        # 只打印一次输出张量形状 —— 这是判断"模型对不对"最直接的证据。
        # 期望 (1, 4+类别数, anchor数) = (1, 12, 2100)  [4 框 + 8 类, 320x320]
        if not self.shape_reported:
            self.shape_reported = True
            print("---- 模型输出张量 ----")
            print("  outputs_size = %d" % self.kpu.outputs_size())
            for i, r in enumerate(self.results):
                print("  output[%d].shape = %s" % (i, str(r.shape)))
            try:
                ch = self.results[0].shape[1]
                print("  -> 通道数 %d = 4(框) + %d(类别)" % (ch, ch - 4))
                if ch - 4 != len(CLASS_ID):
                    print("  !! 类别数 %d 与 CLASS_ID 的 %d 个不符 —— 模型和标签表对不上"
                          % (ch - 4, len(CLASS_ID)))
                    print("  !! 这就是识别不出东西的根因, 换模型或改 CLASS_ID")
            except Exception as e:
                print("  (形状解析失败: %s)" % e)
            print("----------------------")

    def postprocess(self, results):
        """与比赛脚本一致, 只是阈值由调用方压低到 CONF_RAW"""
        det_res = []
        err_count = 0
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
                err_count += 1
                if err_count <= 3:
                    print("postprocess i=%d err: %s" % (i, e))
                continue
        if err_count > 3:
            print("postprocess: %d errors total (suppressed)" % err_count)
        det_res.sort(key=lambda x: x[-1], reverse=True)
        return self._nms(det_res, self.nms_threshold)

    def _iou(self, box1, box2):
        x1, y1, w1, h1 = box1[0], box1[1], box1[2], box1[3]
        x2, y2, w2, h2 = box2[0], box2[1], box2[2], box2[3]
        ax1, ay1 = x1 - w1 / 2, y1 - h1 / 2
        ax2, ay2 = x1 + w1 / 2, y1 + h1 / 2
        bx1, by1 = x2 - w2 / 2, y2 - h2 / 2
        bx2, by2 = x2 + w2 / 2, y2 + h2 / 2
        ix1, iy1 = max(ax1, bx1), max(ay1, by1)
        ix2, iy2 = min(ax2, bx2), min(ay2, by2)
        iw, ih = max(0, ix2 - ix1), max(0, iy2 - iy1)
        inter = iw * ih
        union = w1 * h1 + w2 * h2 - inter
        return inter / union if union > 0 else 0

    def _nms(self, dets, thresh):
        keep = []
        while dets:
            best = dets.pop(0)
            keep.append(best)
            dets = [d for d in dets if self._iou(best, d) < thresh]
        return keep

    def run(self, input_img):
        """注意: 这里**不能**用 except Exception 包住整段 —— os.exitpoint() 抛的
        IDE 停止异常会被吞掉, 表现为 '推理失败: IDE interrupt' 无限刷屏, 而且
        脚本停不下来。k230_cam2_test.py 正是这个毛病。这里干脆不catch, 让异常
        原样冒泡到主循环的 except 去。"""
        input_np = input_img.to_numpy_ref()
        tensors = self.preprocess(input_np)
        self.inference(tensors)
        return self.postprocess(self.results)

    def get_padding_param(self):
        dst_w, dst_h = self.model_input_size[0], self.model_input_size[1]
        ratio = min(dst_w / self.rgb888p_size[0], dst_h / self.rgb888p_size[1])
        new_w = int(ratio * self.rgb888p_size[0])
        new_h = int(ratio * self.rgb888p_size[1])
        dw, dh = (dst_w - new_w) / 2, (dst_h - new_h) / 2
        top, left = 0, 0
        bottom = int(round(dh * 2 + 0.1))
        right = int(round(dw * 2 - 0.1))
        self.pad_top, self.pad_left = top, left
        self.scale_x = self.rgb888p_size[0] / new_w if new_w > 0 else 1.0
        self.scale_y = self.rgb888p_size[1] / new_h if new_h > 0 else 1.0
        return top, bottom, left, right


def config_camera(camera_id):
    sensor = Sensor(id=camera_id)
    sensor.reset()
    sensor.set_framesize(width=800, height=480, chn=CAM_CHN_ID_0)
    sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGHT, chn=CAM_CHN_ID_2)
    sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
    sensor.set_hmirror(False)
    sensor.set_vflip(False)
    return sensor


def check_model_file():
    """开机自检: 模型文件在不在、多大, 顺便列出 SD 卡上所有 kmodel。

    列目录这步是有来由的 —— 仓库里 best.kmodel / bestm.kmodel / best1 / best2
    几个名字混用过, 文档和代码一度对不上。这里直接把实际有什么打出来。
    """
    print("==== 模型文件自检 ====")
    ok = False
    try:
        st = os.stat(KMODEL_PATH)
        print("  %s  存在, %d 字节" % (KMODEL_PATH, st[6]))
        ok = True
    except Exception:
        print("  !! %s 不存在" % KMODEL_PATH)
    try:
        names = [f for f in os.listdir("/sdcard") if f.endswith(".kmodel")]
        print("  /sdcard 下的 kmodel: %s" % (names if names else "(一个都没有)"))
    except Exception as e:
        print("  (列目录失败: %s)" % e)
    print("======================")
    return ok


if __name__ == "__main__":
    if MODE == "bean":
        idx_lo, idx_hi, tag = 5, 7, "bean(5~7)"
    else:
        idx_lo, idx_hi, tag = 0, 4, "num(0~4)"

    if not check_model_file():
        raise Exception("模型文件不存在, 先确认 KMODEL_PATH")

    sensor = None
    yolo = None
    try:
        os.exitpoint(os.EXITPOINT_ENABLE)
        nn.shrink_memory_pool()
        Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
        MediaManager.init()
        time.sleep_ms(200)

        # 阈值压到 CONF_RAW, 好看到"差一点就过线"的检测
        yolo = YOLOv11App(KMODEL_PATH, MODEL_INPUT_SIZE,
                          confidence_threshold=CONF_RAW, nms_threshold=NMS_THRESHOLD,
                          rgb888p_size=[640, 360], display_size=[800, 480])
        yolo.config_preprocess()

        sensor = config_camera(CAMERA_ID)
        time.sleep_ms(200)
        sensor.run()
        time.sleep_ms(200)

        print("==== 开始诊断 cam%d  mode=%s ====" % (CAMERA_ID, MODE))
        print("  原始阈值 %.2f / 比赛阈值 %.2f / 需 %d 个 / 连续 %d 帧"
              % (CONF_RAW, CONF_MAIN, TARGET_COUNT, STABLE_FRAMES))

        frame = 0
        stable_count = 0
        last_result = None

        while True:
            os.exitpoint()          # 不被 try 吞掉, IDE 停止能真正生效
            frame += 1

            img_ai = sensor.snapshot(chn=CAM_CHN_ID_2)
            img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
            dets = yolo.run(img_ai)

            # ---- 门槛1: 比赛阈值 ----
            over = [d for d in dets if d[5] > CONF_MAIN]
            # ---- 门槛2: 类别索引区间 ----
            hit = [d for d in over if idx_lo <= int(d[4]) <= idx_hi]
            hit.sort(key=lambda x: x[0])        # 按 x 排序, 与比赛脚本一致
            # ---- 门槛3: 数量 + 连续帧一致 ----
            cur = []
            for d in hit[:TARGET_COUNT]:
                cur.append(CLASS_MAP.get(CLASS_ID[int(d[4])], 0x00))
            cur += [0x00] * (TARGET_COUNT - len(cur))
            enough = (0x00 not in cur)
            if enough:
                if cur == last_result:
                    stable_count += 1
                else:
                    stable_count = 0
                    last_result = cur
            else:
                stable_count = 0

            # ---- 画所有框(含低分), 屏幕上直接看模型在看什么 ----
            for d in dets:
                x, y, w, h = [int(round(v, 0)) for v in d[:4]]
                x = x * yolo.display_size[0] // yolo.rgb888p_size[0]
                y = y * yolo.display_size[1] // yolo.rgb888p_size[1]
                w = w * yolo.display_size[0] // yolo.rgb888p_size[0]
                h = h * yolo.display_size[1] // yolo.rgb888p_size[1]
                strong = d[5] > CONF_MAIN
                col = (255, 0, 0) if strong else (128, 128, 0)   # 红=过线 暗黄=没过
                img_display.draw_rectangle(x - w // 2, y - h // 2, w, h, color=col, thickness=2)
                ci = int(d[4])
                name = CLASS_ID[ci] if ci < len(CLASS_ID) else "?"
                img_display.draw_string_advanced(
                    max(0, x - w // 2), max(0, y - h // 2 - 34), 30,
                    "%s(%d) %.2f" % (name, ci, d[5]), color=col)
            Display.show_image(img_display, x=0, y=0)

            if frame % PRINT_EVERY == 0:
                detail = " ".join(["%s(%d)%.2f" % (CLASS_ID[int(d[4])], int(d[4]), d[5])
                                   for d in dets[:6]])
                print("[F%4d] raw=%d | >=%.2f: %d | %s: %d | need%d: %s | stable %d/%d"
                      % (frame, len(dets), CONF_MAIN, len(over), tag, len(hit),
                         TARGET_COUNT, "OK" if enough else "MISS",
                         stable_count, STABLE_FRAMES))
                if detail:
                    print("        all: %s" % detail)
                else:
                    print("        all: (模型无任何输出)")
                if stable_count >= STABLE_FRAMES:
                    print("        >>> 稳定命中: %s  <<< 比赛脚本这时就会返回结果"
                          % (["0x%02X" % b for b in cur],))

            del img_display, img_ai
            gc.collect()

    except KeyboardInterrupt:
        print("用户终止")
    except BaseException as e:
        print("异常退出: %s" % e)
    finally:
        if sensor is not None:
            sensor.stop()
            sensor.reset()
            del sensor
        if yolo is not None:
            yolo.deinit()
            del yolo
        Display.deinit()
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        MediaManager.deinit()
        nn.shrink_memory_pool()
        gc.collect()
        print("诊断结束")
