"""只测试 cam2 能不能打开！！"""
import os, time, gc, sys
from media.sensor import *
from media.display import *
from media.media import *

CAMERA_ID = 2

if __name__ == "__main__":
    try:
        os.exitpoint(os.EXITPOINT_ENABLE)
        nn.shrink_memory_pool()

        Display.init(Display.ST7701, width=800, height=480, to_ide=True)
        MediaManager.init()
        time.sleep_ms(200)

        print("=== 尝试打开 cam2 ===")
        sensor = Sensor(id=CAMERA_ID)
        print("  Sensor(id=2) 成功创建")
        sensor.reset()

        print("  set_framesize chn0 (800x480 RGB565)...")
        sensor.set_framesize(width=800, height=480, chn=CAM_CHN_ID_0)
        sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)

        print("  set_framesize chn2 (640x360 RGB888 Planar)...")
        from libs.Utils import ALIGN_UP
        OUT_RGB888P_WIDTH = ALIGN_UP(640,16)
        OUT_RGB888P_HEIGHT = 360
        sensor.set_framesize(width=OUT_RGB888P_WIDTH, height=OUT_RGB888P_HEIGHT, chn=CAM_CHN_ID_2)
        sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)

        sensor.set_hmirror(False)
        sensor.set_vflip(False)

        print("  run()...")
        sensor.run()
        time.sleep_ms(200)

        print("=== Camera 2 configured! 开始显示图像 ===")
        print("  如果屏幕有图像就成功了！")

        while True:
            os.exitpoint()
            img = sensor.snapshot(chn=CAM_CHN_ID_0)
            Display.show_image(img)
            del img
            gc.collect()

    except KeyboardInterrupt as e:
        print("用户终止:", e)
    except BaseException as e:
        sys.print_exception(e)
    finally:
        print()
        print("=== 清理 ===")
        try:
            sensor.stop()
        except:
            pass
        try:
            Display.deinit()
        except:
            pass
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        try:
            MediaManager.deinit()
        except:
            pass
        gc.collect()
        print("done")
