# 录放视频

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/media/video.html>
> **最后更新**: 2025-01-22

---

# 1 本节介绍 ​

📝本节您将学习如何用庐山派去播放mp4视频,和之前一样，我们还是默认以显示设备为我们的[立创·3.1寸屏幕扩展板](https://item.szlcsc.com/44319751.html)为例。

🏆学习目标

1️⃣如何用庐山派去录制mp4视频。

2️⃣ 如何播放录制好的 mp4 视频。

TIP

本章不过多介绍底层协议，主要目的是让大家先用起来，如果有对编码格式想进一步了解的，请自行搜索相关知识。

想探寻 API 详细用法的请看下面这三个链接，所有 API 相关的请以嘉楠官方公布的为准。

[3.7 MP4 模块API手册 — K230 CanMV](https://developer.canaan-creative.com/k230_canmv/dev/zh/api/mpp/K230_CanMV_MP4%E6%A8%A1%E5%9D%97API%E6%89%8B%E5%86%8C.html#)

[3.5 VDEC 模块API手册 — K230 CanMV](https://developer.canaan-creative.com/k230_canmv/dev/zh/api/mpp/K230_CanMV_VDEC%E6%A8%A1%E5%9D%97API%E6%89%8B%E5%86%8C.html)

[3.6 VENC 模块API手册 — K230 CanMV](https://developer.canaan-creative.com/k230_canmv/dev/zh/api/mpp/K230_CanMV_VENC%E6%A8%A1%E5%9D%97API%E6%89%8B%E5%86%8C.html)

我们先录制，再播放视频。在录制视频时会同步录制音频（用板载的贴片麦克风），大家在播放视频时把耳机连接到3.5mm耳机接口就可以听到当前录制的声音了。

# 2 录制视频 ​

录制视频并未使用3.1寸屏幕来显示，只需要在默认摄像头`CSI2`位置接入我们附送的摄像头就可以进行录制了。

python
    
    
    # 立创·庐山派-K230-CanMV开发板资料与相关扩展板软硬件资料官网全部开源
    # 开发板官网：www.lckfb.com
    # 技术支持常驻论坛，任何技术问题欢迎随时交流学习
    # 立创论坛：www.jlc-bbs.com/lckfb
    # 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
    # 不靠卖板赚钱，以培养中国工程师为己任
    
    # Save MP4 file example
    #
    # Note: You will need an SD card to run this example.
    #
    # You can capture audio and video and save them as MP4.The current version only supports MP4 format, video supports 264/265, and audio supports g711a/g711u.
    
    from media.mp4format import *
    import os
    
    width = 800
    height = 480
    
    file_name = "/data/test.mp4"
    
    # 设置默认录制时间（单位：秒）
    MAX_RECORD_TIME = 30
    
    def mp4_muxer_test():
        print("mp4_muxer_test start")
    
        # 实例化mp4 container
        mp4_muxer = Mp4Container()
        mp4_cfg = Mp4CfgStr(mp4_muxer.MP4_CONFIG_TYPE_MUXER)
        if mp4_cfg.type == mp4_muxer.MP4_CONFIG_TYPE_MUXER:
            mp4_cfg.SetMuxerCfg(file_name, mp4_muxer.MP4_CODEC_ID_H265, width, height, mp4_muxer.MP4_CODEC_ID_G711U)
        # 创建mp4 muxer
        mp4_muxer.Create(mp4_cfg)
        # 启动mp4 muxer
        mp4_muxer.Start()
    
        start_time_ms = time.ticks_ms()  # 记录开始时间
        elapsed_time = 0
    
        frame_count = 0
    
        try:
            while True:
                os.exitpoint()
    
                # 处理音视频数据，按MP4格式写入文件
                mp4_muxer.Process()
    
                frame_count += 1
                print("frame_count = ", frame_count)
    
                # 检查当前时间与开始时间的差值是否超过最大录制时间
                elapsed_time = time.ticks_ms() - start_time_ms
                if elapsed_time >= MAX_RECORD_TIME*1000:
                    print("录制已超过最大时长，停止录制,请等待视频保存")
                    break
    
        except BaseException as e:
            print(e)
        # 停止mp4 muxer
        mp4_muxer.Stop()
        # 销毁mp4 muxer
        mp4_muxer.Destroy()
        print("mp4_muxer_test stop,video saved!")
    
    if __name__ == "__main__":
        os.exitpoint(os.EXITPOINT_ENABLE)
        mp4_muxer_test()

1  
2  
3  
4  
5  
6  
7  
8  
9  
10  
11  
12  
13  
14  
15  
16  
17  
18  
19  
20  
21  
22  
23  
24  
25  
26  
27  
28  
29  
30  
31  
32  
33  
34  
35  
36  
37  
38  
39  
40  
41  
42  
43  
44  
45  
46  
47  
48  
49  
50  
51  
52  
53  
54  
55  
56  
57  
58  
59  
60  
61  
62  
63  
64  
65  
66  
67  
68  
69  


修改`width`和`height`可以改变录制视频的宽度和高度，`file_name`可以改变存储路径（如果你要保存的文件夹没有被创建，记得要先创建，这里的data目录是和sdcard目录同级的），修改`MAX_RECORD_TIME`可以改变录制视频的长度，默认是录制30s的视频。

接下来，通过`mp4_muxer_test`这个自定义函数来进行视频的录制。函数中首先创建了一个`Mp4Container`对象，用来处理视频文件的生成和格式化。然后，配置视频的编码格式和参数，并启动视频录制。

录制过程通过一个`while`循环持续进行，每次循环都会处理音视频数据并将其写入MP4文件。在循环中，通过`time.ticks_ms()`获取当前时间，并与开始录制时的时间进行比较。当录制时间超过设定的最大时长（30秒）时，程序会输出提示信息并跳出循环，停止录制。

最后，录制停止后，调用`mp4_muxer.Stop()`和`mp4_muxer.Destroy()`来停止并销毁视频处理对象，表示录制已结束，视频文件已保存。这一段的时间可能会比较长，大家一定要看到串行终端打印出`mp4_muxer_test stop,video saved!`后再停止程序运行及断电。

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/media/video/video_20250121_154233.png)

程序运行成功后，可以把录制到的mp4文件复制到电脑上在电脑上播放，也可以继续看下面的播放视频章节，在IDE的帧缓冲区及3.1寸屏幕上直接播放。 ![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/media/video/video_20250121_154439.png)

复制到电脑上之后就可以随便使用一个支持播放mp4视频的播放器来播放了。

# 3 播放视频 ​

播放视频默认使用的是在上一节录制好的视频。

注意

当前的视频播放还有问题，目前对视频的播放会比正常的速度快，播放是会被加速。 请等待嘉楠固件的继续更新

python
    
    
    # play mp4 file example
    #
    # Note: You will need an SD card to run this example.
    #
    # You can load local files to play. The current version only supports MP4 format, video supports 264/265, and audio supports g711a/g711u.
    
    from media.player import * #导入播放器模块，用于播放mp4文件
    import os
    
    start_play = False #播放结束flag
    def player_event(event,data):
        global start_play
        if(event == K_PLAYER_EVENT_EOF): #播放结束标识
            start_play = False #设置播放结束标识
    
    def play_mp4_test(filename):
        global start_play
        player=Player() #创建播放器对象
        player.load(filename) #加载mp4文件
        player.set_event_callback(player_event) #设置播放器事件回调
        player.start() #开始播放
        start_play = True
    
        #等待播放结束
        try:
            while(start_play):
                time.sleep(0.1)
                os.exitpoint()
        except KeyboardInterrupt as e:
            print("user stop: ", e)
        except BaseException as e:
            sys.print_exception(e)
    
        player.stop() #停止播放
        print("play over")
    
    if __name__ == "__main__":
        os.exitpoint(os.EXITPOINT_ENABLE)
        play_mp4_test("/data/test.mp4")#播放mp4文件

1  
2  
3  
4  
5  
6  
7  
8  
9  
10  
11  
12  
13  
14  
15  
16  
17  
18  
19  
20  
21  
22  
23  
24  
25  
26  
27  
28  
29  
30  
31  
32  
33  
34  
35  
36  
37  
38  
39  


将上面的程序复制到IDE中，就能在帧缓冲区中看到正常播放的视频了。

