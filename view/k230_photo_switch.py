import time, os, sys
from media.sensor import *
from media.display import *
from media.media import *
from machine import Pin, FPIOA
import uos

fpioa = FPIOA()
fpioa.set_function(53, FPIOA.GPIO53)
usr = Pin(53, Pin.IN, Pin.PULL_DOWN)

camera_ids = [0, 1, 2]
cam_index = 0
current_cam = camera_ids[cam_index]

save_dirs = {
    0: '/data/photos_cam0',
    1: '/data/photos_cam1',
    2: '/data/photos_cam2',
}

photo_count = {0: 0, 1: 0, 2: 0}
photos_per_press = 10
max_photos = 900
long_press_ms = 1000

try:
    uos.listdir('/data')
except OSError:
    print("错误：未检测到 SD 卡")
    raise RuntimeError("SD 卡未插入")

for cam_id in camera_ids:
    try:
        uos.listdir(save_dirs[cam_id])
    except OSError:
        uos.mkdir(save_dirs[cam_id])


def config_sensor(cam_id):
    s = Sensor(id=cam_id)
    s.reset()
    s.set_framesize(width=800, height=480, chn=CAM_CHN_ID_0)
    s.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    s.set_hmirror(False)
    s.set_vflip(False)
    return s


def safe_stop_sensor(s):
    if s is not None:
        try:
            s.stop()
        except Exception as e:
            print(f"sensor.stop 失败: {e}")
        try:
            s.reset()
        except Exception as e:
            print(f"sensor.reset 失败: {e}")


def check_button():
    """检测按键: 'short'=短按拍照, 'long'=长按切换, None=无操作"""
    if usr.value() == 1:
        time.sleep_ms(20)  # 消抖
        if usr.value() != 1:
            return None
        press_start = time.ticks_ms()
        while usr.value() == 1:
            if time.ticks_ms() - press_start >= long_press_ms:
                while usr.value() == 1:
                    time.sleep_ms(10)
                return 'long'
            time.sleep_ms(10)
        return 'short'
    return None


sensor = None
try:
    Display.init(Display.ST7701, width=800, height=480, to_ide=True)
    MediaManager.init()

    sensor = config_sensor(current_cam)
    sensor.run()
    print(f"启动摄像头 {current_cam}, 保存到 {save_dirs[current_cam]}")

    clock = time.clock()
    while True:
        clock.tick()
        os.exitpoint()

        img = sensor.snapshot(chn=CAM_CHN_ID_0)
        img.draw_string(10, 10, "cam%d  %d/%d" % (current_cam, photo_count[current_cam], max_photos), color=(255, 0, 0), scale=2)
        Display.show_image(img, x=0, y=0)

        btn = check_button()
        if btn == 'short':
            if photo_count[current_cam] < max_photos:
                for j in range(photos_per_press):
                    if photo_count[current_cam] >= max_photos:
                        break
                    img = sensor.snapshot(chn=CAM_CHN_ID_0)
                    img.save(f'{save_dirs[current_cam]}/photo_{photo_count[current_cam]}.jpg')
                    print(f"[cam{current_cam}] 保存第{photo_count[current_cam] + 1}张")
                    photo_count[current_cam] += 1
                    time.sleep(0.2)
                print(f"[cam{current_cam}] 连拍完成: {photo_count[current_cam]}/{max_photos}")
            else:
                print(f"[cam{current_cam}] 已达上限 {max_photos} 张")

        elif btn == 'long':
            safe_stop_sensor(sensor)
            cam_index = (cam_index + 1) % len(camera_ids)
            current_cam = camera_ids[cam_index]
            time.sleep_ms(50)
            sensor = config_sensor(current_cam)
            sensor.run()
            print(f"切换到摄像头 {current_cam}, 保存到 {save_dirs[current_cam]}")
            time.sleep_ms(200)

except KeyboardInterrupt as e:
    print("用户终止：", e)
except BaseException as e:
    print(f"异常：{e}")
finally:
    safe_stop_sensor(sensor)
    try:
        Display.deinit()
    except Exception as e:
        print(f"Display.deinit: {e}")
    try:
        MediaManager.deinit()
    except Exception as e:
        print(f"MediaManager.deinit: {e}")
    print("程序已退出")
