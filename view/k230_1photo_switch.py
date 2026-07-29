import os, gc, time
import nncase_runtime as nn
from media.media import *
from media.sensor import *
from media.display import *
from machine import Pin, FPIOA
import image

# ===== 按键 SW4 (K230 N4 / GPIO53) =====
# 硬件: 按键接 VDD_3V3, GPIO 下拉到 GND, 高电平有效 (按下=1, 释放=0)
# 交互: 短按(<长按阈值) 拍10张照片; 长按(>=长按阈值) 切换下一个摄像头
fpioa = FPIOA()
fpioa.set_function(53, FPIOA.GPIO53)

BTN_PIN = 53
BTN_LONG_PRESS_MS = 1000   # 长按阈值: 持续按下 >=1s 判长按
BTN_SHORT_PRESS_MS = 30    # 短按消抖: 按下 >=30ms 才算有效(滤抖动)
BTN_DEBOUNCE_MS = 2        # 电平消抖: 两次采样间隔
BTN_POLL_MS = 10           # 主循环帧间隔(按键轮询间隔)

btn = Pin(BTN_PIN, Pin.IN, Pin.PULL_DOWN)  # 下拉, 按下时拉高到 VDD_3V3

PHOTO_DIR = "/sdcard/photo"              # 拍照保存目录(SD卡)
PHOTO_COUNT = 10                         # 短按连拍张数: 按一下拍10张
PHOTO_INTERVAL_MS = 120                  # 连拍间隔(ms)

# 摄像头轮换顺序: 正面(2) -> 侧面(0) -> 豆子(1) -> 循环
CAMERA_IDS = [2, 0, 1]

DISPLAY_WIDTH = 800
DISPLAY_HEIGHT = 480

_photo_seq = [0]    # 全局递增序号 (K230 无 RTC, 用计数代替时间戳; list 容器, 函数内可直接改元素)


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


def config_camera(camera_id):
    """配置指定ID的摄像头, 返回配置好的 sensor 对象"""
    sensor = Sensor(id=camera_id)
    sensor.reset()
    # 显示通道: 800x480 + RGB565
    sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, chn=CAM_CHN_ID_0)
    sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    sensor.set_hmirror(False)
    sensor.set_vflip(False)
    print(f"摄像头 {camera_id} 配置完成")
    return sensor


def capture_photos(sensor, camera_id, count=PHOTO_COUNT, interval_ms=PHOTO_INTERVAL_MS):
    """从当前摄像头连拍 count 张, 保存到 PHOTO_DIR/cam{id}_{seq}.jpg"""
    print(f"[photo] start cam{camera_id} x{count}")
    for i in range(count):
        os.exitpoint()
        try:
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
            _photo_seq[0] += 1
            fname = f"{PHOTO_DIR}/cam{camera_id}_{_photo_seq[0]:05d}.jpg"
            img.save(fname)
            print(f"[photo] {i+1}/{count} {fname}")
            # LCD 右下角显示拍照进度
            osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
            osd_img.clear()
            osd_img.draw_string_advanced(580, 430, 30, f"PHOTO {i+1}/{count}", color=(0, 255, 0, 255))  # 绿色
            Display.show_image(img, x=0, y=0)
            Display.show_image(osd_img, 0, 0, Display.LAYER_OSD1)
            del img, osd_img
        except Exception as e:
            print(f"[photo] fail {i+1}: {e}")
        gc.collect()
        time.sleep_ms(interval_ms)
    print("[photo] done")


if __name__ == "__main__":
    ensure_photo_dir()
    button = Button(btn)

    os.exitpoint(os.EXITPOINT_ENABLE)
    nn.shrink_memory_pool()
    Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    MediaManager.init()
    time.sleep_ms(200)

    sensor = None
    cam_idx = 0
    try:
        while True:
            os.exitpoint()
            camera_id = CAMERA_IDS[cam_idx]

            # 切换传感器: 先停旧 -> 释放旧对象 -> 配新 -> run
            # 关键: 必须把旧 sensor 引用置空并 gc, 否则旧 Sensor 的 CSI/ISP 资源不会立即释放,
            # 新 sensor 拿不到资源, 画面/帧缓冲就会没。
            if sensor is not None:
                sensor.stop()
                sensor.reset()
                sensor = None
                gc.collect()
            time.sleep_ms(50)
            sensor = config_camera(camera_id)
            time.sleep_ms(200)
            sensor.run()
            time.sleep_ms(200)
            print(f"[cam] 切换到摄像头{camera_id}")

            while True:
                os.exitpoint()
                time.sleep_ms(BTN_POLL_MS)

                # === 按键检测 ===
                # 短按: 连拍 PHOTO_COUNT 张; 长按: break 切下一个摄像头
                evt = button.poll()
                if evt == 'long':
                    print("[btn] 长按 -> 切换摄像头")
                    # 切换前显示提示 (新 sensor 配置期间 LCD 停留此帧)
                    try:
                        img = sensor.snapshot(chn=CAM_CHN_ID_0)
                        osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
                        osd_img.clear()
                        osd_img.draw_string_advanced(280, 220, 50, "SWITCHING...", color=(255, 255, 0, 255))  # 黄色居中
                        Display.show_image(img, x=0, y=0)
                        Display.show_image(osd_img, 0, 0, Display.LAYER_OSD1)
                        del img, osd_img
                    except Exception as e:
                        print(f"[btn] switch disp fail: {e}")
                    break
                if evt == 'short':
                    print(f"[btn] 短按 -> 拍照{PHOTO_COUNT}张")
                    capture_photos(sensor, camera_id)
                    continue

                # 实时显示当前摄像头画面
                img_display = sensor.snapshot(chn=CAM_CHN_ID_0)
                # 右下角提示当前摄像头编号
                osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
                osd_img.clear()
                osd_img.draw_string_advanced(580, 430, 30, f"CAM{camera_id}", color=(255, 255, 255, 255))  # 白色
                Display.show_image(img_display, x=0, y=0)
                Display.show_image(osd_img, 0, 0, Display.LAYER_OSD1)
                del img_display, osd_img
                gc.collect()

            cam_idx = (cam_idx + 1) % len(CAMERA_IDS)

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
