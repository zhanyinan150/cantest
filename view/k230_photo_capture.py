import time, os, sys
from media.sensor import *  # 导入 sensor 模块，使用摄像头相关接口
from media.display import *  # 导入 display 模块，使用 display 相关接口
from media.media import *  # 导入 media 模块，使用 media 相关接口
from machine import Pin
import uos  # uos 模块有 exists 和 mkdir 等文件操作

DISPLAY_WIDTH = 1920
DISPLAY_HEIGHT = 1080
DISPLAY_MODE = "LCD"
usr = Pin(53, Pin.IN, Pin.PULL_DOWN)  # 按键启动
photo_count = 0  # 已拍照片计数（起始编号1200）
max_photos = 900  # 总共拍照次数
photos_per_press = 10  # 每次按键拍 100 张
save_dir = '/data/photos_beans'  # 新建专用文件夹


# 确保目录存在
try:
    uos.listdir('/data')
except OSError:
    print("错误：未检测到 SD 卡")
    raise RuntimeError("SD 卡未插入")

try:
    uos.listdir(save_dir)
except OSError:
    uos.mkdir(save_dir)

try:

    Display.init(Display.ST7701, width=800, height=480, to_ide=True)


    sensor1 = Sensor(id=2)
    sensor1.reset()
    sensor1.set_framesize(width=800, height=480, chn=CAM_CHN_ID_0)
    sensor1.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)
    sensor1.set_hmirror(False)
    sensor1.set_vflip(False)  # 原 True 上下颠倒, 改 False 正常

    # 初始化媒体管理器
    MediaManager.init()

    # 启动传感器

    sensor1.run()

    # 创建一个 FPS 计时器，用于实时计算每秒帧数
    clock = time.clock()
    while True:
        clock.tick()
        os.exitpoint()

        img = sensor1.snapshot(chn=CAM_CHN_ID_0)
        Display.show_image(img, x=0, y=0)  # 全屏显示 1920x1080
        if usr.value() == 0:
            pass
        else:  # 每次按键拍 photos_per_press 张，总共拍 max_photos 张
            if photo_count < max_photos:
                for j in range(photos_per_press):
                    if photo_count >= max_photos:
                        break
                    img = sensor1.snapshot(chn=CAM_CHN_ID_0)
                    img.save(f'{save_dir}/photo_{photo_count}.jpg')
                    print(f"保存第{photo_count + 1}张照片到{save_dir}")
                    photo_count += 1
                    time.sleep(0.2)
                print(f"本次连拍完成：已拍{photo_count}/{max_photos}张")
                time.sleep(0.2)
            else:
                print(f"已完成{max_photos}张照片拍摄")
except KeyboardInterrupt as e:
    print("用户终止：", e)  # 捕获键盘中断异常
except BaseException as e:
    print(f"异常：{e}")  # 捕获其他异常
finally:
    # 清理资源
    Display.deinit()
    MediaManager.deinit()
    print("程序已退出")
