"""用官方 libs.YOLO.YOLO11 类跑模型 —— 决定性实验

背景:
  k230_competition.py / k230_cam2_test.py 都是手写的 YOLOv11App(AIBase),
  自己做预处理配置 + 后处理解码。其中后处理是这么写的:

      for i in range(2100):
          result = results[0][0][:, i]      # 假设输出是 (1, 4+类别数, 2100)
          max_score = max(result[4:])

  这段有两个硬假设:
    1. 模型**只有一个**输出张量
    2. 该张量形状是 (1, 12, 2100), 即 [4 框 + 8 类] × 2100 anchor

  YOLO11/v8 的 kmodel 经不同导出方式转换后, 很可能是**三个**输出张量(对应
  8/16/32 三个下采样尺度的原始特征图), 而不是一个拼好的 (1,12,2100)。
  这时 results[0][0][:, i] 取到的完全是另一份数据, 解出来必然是空 ——
  **表现就是一个框都画不出来, 但推理本身不报错**。

本脚本改用官方 libs.YOLO 的 YOLO11 类。它由厂商维护, 解码逻辑与他们自己的
kmodel 转换工具链配套, 不做任何假设。

怎么读结果:
  - 官方类能出框, 手写的出不了  -> 手写 postprocess 的解码方式不对(最可能)
  - 官方类也出不了框            -> 模型/转换本身有问题, 或摄像头/光照/类别不对
                                   (此时先看开机打印的模型文件自检)

用法: 拷到 SD 卡, CanMV IDE 运行。conf 阈值故意压到 0.1, 有任何东西都会画出来。
"""

from libs.PipeLine import PipeLine, ScopedTiming
from libs.YOLO import YOLO11
import os, sys, gc
import nncase_runtime as nn
import ulab.numpy as np
import image
from media.sensor import *

# ======================== 可调参数 ========================
KMODEL_PATH = "/sdcard/bestm.kmodel"   # 想对比就改成 /sdcard/best.kmodel 再跑一次
CAMERA_ID = 2                          # 2=豆子(Phase1) 1=正面数字 0=侧面数字

# 与比赛脚本保持一致的类别表, 顺序**必须**和训练时一致
LABELS = ["1", "2", "3", "4", "5", "g", "w", "y"]

MODEL_INPUT_SIZE = [320, 320]          # 训练时的输入分辨率
RGB888P_SIZE = [640, 360]              # 与比赛脚本一致
DISPLAY_SIZE = [800, 480]
DISPLAY_MODE = "lcd"

CONF_THRESH = 0.1                      # 压到很低, 有任何检测都让它出来
NMS_THRESH = 0.45
MAX_BOXES = 50

PRINT_EVERY = 10                       # 每几帧打印一次, 免得刷屏


def check_model_file():
    """开机自检: 模型在不在、多大, 并列出 SD 卡上实际存在的所有 kmodel。

    列目录这步是有来由的 —— 仓库里 best / bestm / best1 / best2 几个名字混用过,
    代码和文档一度对不上。这里直接把实际有什么打出来, 省得再猜。
    """
    print("==== 模型文件自检 ====")
    ok = False
    try:
        st = os.stat(KMODEL_PATH)
        print("  %s 存在, %d 字节" % (KMODEL_PATH, st[6]))
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
    if not check_model_file():
        raise Exception("模型文件不存在, 先确认 KMODEL_PATH")

    print("==== 官方 YOLO11 类测试 ====")
    print("  model = %s" % KMODEL_PATH)
    print("  cam   = %d, conf = %.2f, labels = %d 类"
          % (CAMERA_ID, CONF_THRESH, len(LABELS)))

    # 指定摄像头: PipeLine 默认用板子的默认 sensor, 这里显式传入才能选 cam2
    sensor = Sensor(id=CAMERA_ID)
    pl = PipeLine(rgb888p_size=RGB888P_SIZE, display_size=DISPLAY_SIZE,
                  display_mode=DISPLAY_MODE)
    os.exitpoint(os.EXITPOINT_ENABLE)
    nn.shrink_memory_pool()
    pl.create(sensor=sensor)

    yolo = YOLO11(task_type="detect",
                  mode="video",
                  kmodel_path=KMODEL_PATH,
                  labels=LABELS,
                  rgb888p_size=RGB888P_SIZE,
                  model_input_size=MODEL_INPUT_SIZE,
                  display_size=DISPLAY_SIZE,
                  conf_thresh=CONF_THRESH,
                  nms_thresh=NMS_THRESH,
                  max_boxes_num=MAX_BOXES,
                  debug_mode=0)
    yolo.config_preprocess()

    frame = 0
    hit_frames = 0          # 出过框的帧数, 用来判断是"偶尔"还是"从来没有"
    try:
        while True:
            os.exitpoint()          # 不要用 try/except 包住它 —— 吞掉 IDE 停止
                                    # 信号会表现为 "推理失败: IDE interrupt" 刷屏
                                    # 且脚本停不下来 (k230_cam2_test.py 的老毛病)
            frame += 1
            img = pl.get_frame()
            res = yolo.run(img)
            yolo.draw_result(res, pl.osd_img)
            pl.show_image()

            # res 检测任务返回 [框位置, 分数, 类别索引] 的列表; 空列表 = 没检测到
            n = 0
            try:
                n = len(res[0]) if (res and len(res) > 0 and res[0] is not None) else 0
            except Exception:
                n = 0
            if n:
                hit_frames += 1

            if frame % PRINT_EVERY == 0:
                print("[F%4d] 检测到 %d 个 | 累计出框帧数 %d/%d"
                      % (frame, n, hit_frames, frame))
                if n:
                    # 打印每个框的类别与分数, 用来核对类别索引顺序对不对
                    try:
                        boxes, scores, idxs = res[0], res[1], res[2]
                        for k in range(min(n, 6)):
                            ci = int(idxs[k])
                            name = LABELS[ci] if ci < len(LABELS) else "?"
                            print("        %s(idx%d) %.2f  box=%s"
                                  % (name, ci, scores[k], boxes[k]))
                    except Exception as e:
                        print("        (解析 res 失败: %s) res=%s" % (e, res))
                elif hit_frames == 0 and frame >= 60:
                    print("        >>> 连续 %d 帧一个框都没有。" % frame)
                    print("        >>> 官方类都出不来, 说明不是手写解码的问题:")
                    print("        >>> 先查 模型文件对不对 / 类别数对不对 /")
                    print("        >>> 摄像头 cam%d 是否真的对着目标 / 光照" % CAMERA_ID)

            gc.collect()
    except KeyboardInterrupt:
        print("用户终止")
    except BaseException as e:
        sys.print_exception(e)
    finally:
        yolo.deinit()
        pl.destroy()
        print("==== 结束: 共 %d 帧, 出框 %d 帧 ====" % (frame, hit_frames))
