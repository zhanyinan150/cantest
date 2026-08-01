/**
  ******************************************************************************
  * @file    action.c
  * @brief   动作序列(单爪抓豆子->放箱子) + K230 三阶段通讯调用(主机)
  ******************************************************************************
  * action_all 任务: 上电后自动跑完整比赛流程, 不依赖串口命令。
  *
  * 单爪结构: 只用黑爪, 分3次抓豆(左->中->右), 每次抓完去箱子放豆。
  *   - 只有第1次抓豆前做 Phase1(豆子识别)
  *   - 只有第1次放箱时做 Phase2/Phase3(箱子识别)
  *
  * 豆子X轴坐标(以初始点为原点, 靠墙dir=1为正方向):
  *   左豆: +35    中豆: +14.5    右豆: -57.5
  *   左豆->中豆: dir=0(远离墙) 20.5cm
  *   中豆->右豆: dir=0(远离墙) 24cm
  *
  * 每次抓完豆后都先走到右豆位置(X=-57.5)再去放箱子:
  *   左豆->右豆: dir=0 92.5cm  中豆->右豆: dir=0 24cm  右豆: 已在位
  *   右豆->4号箱: dir=1 82cm (action_gouto_xiangzi_first/secend)
  *
  * 舵机动作组:
  *   左/右豆: 组15(准备) -> Z降 -> 组16(抓) -> Z升
  *   中豆:    组3(准备)  -> Z降 -> 组4(抓)  -> Z升
  *   抓完过渡: 组12
  *
  * K230 三阶段通讯协议层已移至 modules/k230/K230.c。
  * Motor_XYZ 为非阻塞(发完命令即返回), 靠 osDelay 等电机走完。
  * 日志一律用英文: UTF-8 中文字符串在 ARMCC V5.06 下会乱码/报错。
  ******************************************************************************
  */

#include "action.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "motor.h"       /* Motor_XYZ */
#include "servo.h"       /* runActionGroup */
#include "K230.h"        /* k230_write, K230 命令码, bean_color, Data_yinshe,
                          * k230_wait_ready / k230_phase1_bean /
                          * k230_phase2_front / k230_phase3_side */
#include "bsp_log.h"     /* printf 经日志队列 -> LogTask DMA 发 USART1 */
#include <stdio.h>

/* ---- 任务参数 ---- */
#define ACTION_TASK_STACK_SIZE   1024               /* 堆栈(word) */
#define ACTION_TASK_PRIORITY     osPriorityNormal   /* 与 MotorAutoTask 同级 */
#define ACTION_STARTUP_DELAY     1000               /* 等电机使能+M2006反馈稳定(ms) */

/* ---- 内部函数 ---- */
static void action_all(void *argument);
static void action_grab_left(void);
static void action_grab_middle(void);
static void action_grab_right(void);
static void action_gouto_xiangzi_first(void);
static void action_gouto_xiangzi_secend(void);
void goto_box(uint8_t target_box);
void move_box(uint8_t target_box);
void goto_box_ready(uint8_t target_box);

/* ===== 动作注释 ===== */
/**
 * @brief  去豆子y为1，箱子为0
 *         远离墙x为0，靠近墙为1
 *         上升z为0，下降z为1
 *
 */

/**
  * @brief  初始化动作序列: 创建 action_all 任务
  */
void Action_Init(void)
{
    const osThreadAttr_t attr = {
        .name = "ActionTask",
        .stack_size = ACTION_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)ACTION_TASK_PRIORITY,
    };
    osThreadNew(action_all, NULL, &attr);
}

/* ==================== 调试输出 ==================== */

/* 调试输出: 走 printf -> fputc 行缓冲 -> 日志队列 -> LogTask DMA 发 USART1。
 * 不可直接 HAL_UART_Transmit(&huart1,...): 会与 LogTask 的 DMA 发送抢同一个
 * USART1, 正是 bsp_log.c 文件头列为 HardFault 根因的用法。 */
static void dbg(const char *s)
{
    printf("%s", s);
}

/* ==================== 主流程 ==================== */
/*
 * 单爪结构: 分3次抓豆(左->中->右), 每次抓完去箱子放豆。
 * 只有第1次抓豆前识别豆子(Phase1), 第1次放箱时识别箱子(Phase2/Phase3)。
 *
 * 豆子X轴坐标(以初始点为原点, 靠墙dir=1为正):
 *   左豆 +35, 中豆 +14.5, 右豆 -57.5
 */

/* ===== 箱子间X轴移动 (以第4个箱子为原点) ===== */
/* 以第4个箱子为坐标原点(0), 左为负, 右为正, 单位cm。
 *   箱子1: -93  箱子2: -73  箱子3: -35  箱子4: 0  箱子5: +25
 * 箱子编号1~5从左到右, 与 K230 number_position 一致。
 * 方向: dir=0(左/远离墙), dir=1(右/靠近墙)。 */
static const float box_x_coord[6] = {0, -93.0f, -73.0f, -35.0f, 0.0f, 25.0f};
/* 当前所在箱子编号, action_gouto_xiangzi 结束后应为4 */
static uint8_t s_current_box = 4;

static void action_all(void *argument)
{
    (void)argument;

    /* 等 K230 就绪(模型加载完成, 0x0B 心跳): 必须在 phase1 之前,
     * 否则命令会在 K230 还没启动接收时就被发出, K230 漏掉命令后
     * 全阶段 no ack。超时(30s)也放行--K230 可能已就绪只是漏了就绪帧。 */
    osDelay(1000);
    (void)k230_wait_ready();

    /* Phase1 必须在第1次抓豆之前: 抓完再识别就来不及决定放哪个箱 */
    (void)k230_phase1_bean();

    /* ===== 第一次: 抓左边豆子 -> 放箱(含识别箱子) ===== */
    action_grab_left();                        /* 抓左边豆子, 结束后走到右边豆子位置 */
    action_gouto_xiangzi_first();              /* 放箱子, 内含 Phase2(正面) + Phase3(侧面) */
    goto_box(Data_yinshe(bean_color[0]));      /* 走到左边豆子映射箱子位置, 黑爪放 */
    goto_box_ready(4);                         /* 回到第4个箱子(中转) */

    /* ===== 第二次: 抓中间豆子 -> 放箱(无识别) ===== */
    action_grab_middle();                      /* 抓中间豆子, 结束后走到右边豆子位置 */
    action_gouto_xiangzi_secend();             /* 放箱子(无视觉) */
    goto_box(Data_yinshe(bean_color[1]));      /* 走到中间豆子映射箱子位置, 黑爪放 */
    goto_box_ready(4);                         /* 回到第4个箱子(中转) */

    /* ===== 第三次: 抓右边豆子 -> 放箱(无识别) ===== */
    action_grab_right();                       /* 抓右边豆子, 结束后动作组12过渡 */
    action_gouto_xiangzi_secend();             /* 放箱子(无视觉) */
    goto_box(Data_yinshe(bean_color[2]));      /* 走到右边豆子映射箱子位置, 黑爪放 */

    for (;;)
    {
        osDelay(1000);
    }
}

    /* ===== 动作注释 ===== */
    /**
     * @brief  去豆子y为1，箱子为0
     *         远离墙x为0，靠近墙为1
     *         上升z为0，下降z为1
     *
     */

    /* ===== 抓豆子 ===== */

    /**
     * @brief  第一次抓豆: 抓左边豆子 (X=+35, 靠墙方向)
     *         起始点(X=0) -> X+35 Y+188 Z+35 -> 组15(准备) -> Z降 -> 组16(抓) -> Z升
     *         -> X-92.5(走到右豆位置: +35到-57.5=92.5cm) -> 组12(过渡去放箱子)
     */
    static void action_grab_left(void)
    {
        dbg("[ACT] === grab left bean ===\r\n");

        /* 起始点到抓左边豆子: X+35(靠墙) Y+188 Z+35(上升) */
        dbg("[ACT] 1/6 move to left bean (X+35 Y+188 Z+35)\r\n");
        (void)Motor_XYZ(1, 800, 50,35.0f,  /* X: dir=1(靠墙/右), 35cm -> 左豆位置 */
                        1, 50, 20, 190.0f,  /* Y: dir=1(前), 190cm */
                        0, 35.0f);          /* Z: dir=0(上), 35cm */

        runActionGroup(15, 1);              /* 黑爪准备抓左边豆子 */
        osDelay(6000);

        /* 降爪子去抓左边豆子: Z 下降 30cm */
        dbg("[ACT] 2/6 claw down (Z-30)\r\n");
        (void)Motor_XYZ(1, 800, 50,0, /* X: 不动 */
                        1, 50, 20, 0, /* Y: 不动 */
                        1, 25.0f);     /* Z: dir=1(下), 25cm */
        osDelay(3000);

        runActionGroup(16, 1); /* 黑爪抓 */
        osDelay(3000);

        /* 升爪子复位: Z 上升 30cm */
        dbg("[ACT] 3/6 claw up (Z+30)\r\n");
        (void)Motor_XYZ(1, 800, 50,0, /* X: 不动 */
                        1, 50, 20, 0, /* Y: 不动 */
                        0, 25.0f);     /* Z: dir=0(上), 25cm */
        osDelay(2500);


        /* 走到右边豆子位置: X-92.5(远离墙)
         * 左豆X=+35, 右豆X=-57.5, 距离=92.5cm */
        dbg("[ACT] 5/6 move to right bean (X-92.5)\r\n");
        (void)Motor_XYZ(0, 800, 50,92.5f, /* X: dir=0(远离墙/左), 92.5cm -> 右豆位置 */
                        1, 50, 20, 0,     /* Y: 不动 */
                        0, 0.0f);          /* Z: 不动 */
        osDelay(3000);

        /* 动作组12: 过渡, 去放箱子 */
        dbg("[ACT] 4/6 action group 12 (toward right bean)\r\n");
        runActionGroup(12, 1);

        osDelay(6000);

        dbg("[ACT] === grab left bean done ===\r\n");
    }

    /**
     * @brief  第二次抓豆: 抓中间豆子 (X=+14.5)
     *         流程与 action_grab_right 一致: 先从箱子去右豆位置, 再根据X坐标去抓中豆。
     *         从4号箱 -> Y+120 -> X-82 Y+195(到右豆) -> X+24(到中豆) -> 组3(准备)
     *         -> Z降 -> 组4(抓) -> Z升 -> 组12(过渡) -> X-24(回右豆位置)
     *
     * 右豆X=-57.5, 中豆X=+14.5, 距离=24cm
     * (从4号箱回豆区: Y+120, 然后X-82 Y+195 到右豆位置, 再X+24 到中豆位置)
     */
    static void action_grab_middle(void)
    {
        dbg("[ACT] === grab middle bean ===\r\n");

        /* 从4号箱子回到豆子区域: Y+120 (与 action_grab_right 相同) */
        dbg("[ACT] 1/8 return to bean area (Y+120)\r\n");
        (void)Motor_XYZ(0, 800, 50, 0.0f,    /* X: 不动 */
                        1, 50, 20, 110.0f,    /* Y: dir=1(前), 110cm */
                        0, 0.0f);             /* Z: 不动 */
        osDelay(6000);

        /* 到右边豆子位置: X-82 Y+195 (与 action_grab_right 相同)
         * 右豆X=-57.5, 4号箱X≈+24.5, 需远离墙走82cm */
        dbg("[ACT] 2/8 move to right bean (X-82 Y+195)\r\n");
        (void)Motor_XYZ(0, 800, 50, 82.0f,   /* X: dir=0(远离墙/左), 82cm -> 右豆位置 */
                        1, 50, 20, 207.0f,    /* Y: dir=1(前), 207cm */
                        0, 0.0f);             /* Z: 不动 */
        osDelay(8000);

        /* 从右豆去中间豆子位置: X+24(靠近墙)
         * 右豆X=-57.5, 中豆X=+14.5, 距离=24cm */
        dbg("[ACT] 3/8 move to middle bean (X+24)\r\n");
        (void)Motor_XYZ(1, 800, 50,24.0f,   /* X: dir=1(靠墙/右), 24cm -> 中豆位置 */
                        1, 50, 20, 0,         /* Y: 不动 */
                        0, 0.0f);             /* Z: 不动 */

        osDelay(3000);
        /* 准备抓中间豆子 */

        runActionGroup(3, 1); /* 转轴去抓中间豆子 */
        osDelay(2000);

        /* 降爪子去抓中间豆子: Z 下降 20cm */
        dbg("[ACT] 4/8 claw down (Z-20)\r\n");
        (void)Motor_XYZ(1, 800, 50,0, /* X: 不动 */
                        1, 50, 20, 0, /* Y: 不动 */
                        1, 15.0f);     /* Z: dir=1(下), 15cm */
        osDelay(2500);

        runActionGroup(4, 1); /* 黑爪抓中间豆子 */
        osDelay(3000);

        /* 升爪子复位: Z 上升 20cm */
        dbg("[ACT] 5/8 claw up (Z+20)\r\n");
        (void)Motor_XYZ(1, 800, 50,0, /* X: 不动 */
                        1, 50, 20, 0, /* Y: 不动 */
                        0, 15.0f);     /* Z: dir=0(上), 15cm */
        osDelay(2500);

        /* 动作组12: 过渡, 去右边豆子方向 */
        dbg("[ACT] 6/8 action group 12 (toward right bean)\r\n");
        runActionGroup(12, 1);
        osDelay(3000);

        /* 走到右边豆子位置: X-24(远离墙)
         * 中豆X=+14.5, 右豆X=-57.5, 距离=24cm */
        dbg("[ACT] 7/8 move to right bean (X-24)\r\n");
        (void)Motor_XYZ(0, 800, 50,24.0f, /* X: dir=0(远离墙/左), 24cm -> 右豆位置 */
                        1, 50, 20, 0,     /* Y: 不动 */
                        0, 0.0f);          /* Z: 不动 */
        osDelay(3000);

        dbg("[ACT] === grab middle bean done ===\r\n");
    }

    /**
     * @brief  第三次抓豆: 抓右边豆子 (X=-57.5)
     *         从4号箱子回去 -> 到右边豆子位置 -> 组15(准备) -> Z降 -> 组16(抓) -> Z升
     *         -> 组12(过渡去放箱子)
     *         抓完后已在右豆位置, 无需额外X移动, 直接去放箱子。
     *
     * 右豆X=-57.5, 4号箱X≈+24.5, 差=-82 -> dir=0(远离墙) 82cm
     */
    static void action_grab_right(void)
    {
        dbg("[ACT] === grab right bean ===\r\n");

        /* 从4号箱子回到豆子区域: Y+120 */
        dbg("[ACT] 1/7 return to bean area (Y+120)\r\n");
        (void)Motor_XYZ(0, 800, 50, 0.0f,    /* X: 不动 */
                        1, 50, 20, 110.0f,    /* Y: dir=1(前), 110cm */
                        0, 0.0f);             /* Z: 不动 */
        osDelay(6000);

        /* 到右边豆子位置: X-82 Y+195
         * 右豆X=-57.5, 4号箱X≈+24.5, 需远离墙走82cm */
        dbg("[ACT] 2/7 move to right bean (X-82 Y+195)\r\n");
        (void)Motor_XYZ(0, 800, 50, 82.0f,   /* X: dir=0(远离墙/左), 82cm -> 右豆位置 */
                        1, 50, 20, 207.0f,    /* Y: dir=1(前), 207cm */
                        0, 0.0f);             /* Z: 不动 */

        /* 准备黑爪抓右边豆子 */
        runActionGroup(15, 1); /* 黑爪准备抓右边豆子 */
        osDelay(9000);



        /* 降爪子去抓右边豆子: Z 下降 25cm */
        dbg("[ACT] 3/7 claw down (Z-25)\r\n");
        (void)Motor_XYZ(0, 800, 50, 0,  /* X: 不动 */
                        1, 50, 20, 0,  /* Y: 不动 */
                        1, 20.0f);      /* Z: dir=1(下), 20cm */
        osDelay(4000);

        runActionGroup(16, 1); /* 黑爪抓 */
        osDelay(8000);

        /* 升爪子复位: Z 上升 25cm */
        dbg("[ACT] 4/7 claw up (Z+25)\r\n");
        (void)Motor_XYZ(1, 800, 50, 0, /* X: 不动 */
                        1, 50, 20, 0, /* Y: 不动 */
                        0, 20.0f);     /* Z: dir=0(上), 20cm */
        osDelay(4000);


        dbg("[ACT] === grab right bean done ===\r\n");
    }


    /* ===== 分界线后: 放箱子(含障碍物避让) ===== */
    /**
     * @brief  第1次放箱子动作序列(含 Phase2/Phase3 视觉识别)
     *         障碍物动作1 -> 障碍物动作2 -> Phase2(正面数字)
     *         -> 到箱子 -> Phase3(侧面数字)
     * @note   Phase2/Phase3 的位置对应 K230 状态机顺序, 不可对调。
     *         仅第1次放箱调用此函数, 后续放箱用 action_gouto_xiangzi_secend。
     */
    static void action_gouto_xiangzi_first(void)
    {
        dbg("[ACT] === place to box (with vision) ===\r\n");

        (void)Motor_XYZ(0, 800, 50, 0,     /* X: 不动 */
                        0, 50, 20, 102.0f, /* Y: dir=0(后), 50rpm, acc=20, 102cm */
                        0, 0.0f);          /* Z: 不动 */
        osDelay(5000);

        /* 豆子到箱子障碍物动作2 */
        (void)Motor_XYZ(1, 800, 100, 82.0f, /* X: dir=1(右), 600rpm, acc=20, 82cm */
                        0, 50, 20, 158.0f,  /* Y: dir=0(后), 50rpm, acc=20, 158cm */
                        0, 0.0f);           /* Z: 不动 */

        osDelay(8000);
        (void)k230_phase2_front(); /* 看正面数字 */

        /* 箱子障碍物到箱子 */
        (void)Motor_XYZ(1, 800, 50, 0,    /* X: 不动 */
                        0, 50, 20, 52.0f, /* Y: dir=0(后), 50rpm, acc=20, 52cm */
                        0, 0.0f);         /* Z: 不动 */
        osDelay(3000);                    // 这里到达了第4个箱子位置

        (void)k230_phase3_side(); /* 看侧面数字 + 推理第5位 */
        osDelay(1000);

        (void)Motor_XYZ(1, 800, 50, 0,   /* X: 不动 */
                        0, 50, 20, 8.5f, /* Y: dir=0(后), 50rpm, acc=20, 8.5cm */
                        0, 0.0f);        /* Z: 不动 */
        osDelay(1500);                   // 这里到达了第4个箱子位置
        dbg("[ACT] === place to box (with vision) done ===\r\n");
    }

    /* ===== 放箱子(不含视觉识别) ===== */
    /**
      * @brief  放箱子动作序列(无 Phase2/Phase3)
      *         障碍物动作1 -> 障碍物动作2 -> 到箱子
      *         与 action_gouto_xiangzi_first 相同的电机动作, 但不调 K230 视觉。
      *         用于已经识别完数字、只需机械移动到箱子的场景。
      *         动作组12(准备黑爪放箱子)已移至各抓豆函数末尾, 此函数不含舵机动作。
      */

      //从豆子去放箱子(无视觉)
    static void action_gouto_xiangzi_secend(void)
    {
        dbg("[ACT] === place to box (no vision) ===\r\n");

        /* 豆子到箱子障碍物动作一 */
        dbg("[ACT] obstacle step1 (Y-100)\r\n");
        (void)Motor_XYZ(0, 800, 50, 0,      /* X: 不动 */
                        0, 50, 20, 100.0f, /* Y: dir=0(后), 50rpm, acc=20, 100cm */
                        0, 0.0f);           /* Z: 不动 */
        osDelay(6000);

        /* 豆子到箱子障碍物动作2 */
        dbg("[ACT] obstacle step2 (X+82 Y-215)\r\n");
        (void)Motor_XYZ(1, 800, 50, 82.0f, /* X: dir=1(右), 300rpm, acc=20, 82cm */
                        0, 50, 20, 217.0f, /* Y: dir=0(后), 50rpm, acc=20, 217cm */
                        0, 0.0f);          /* Z: 不动 */
                                    

        osDelay(2000);
                        /* 动作组12: 过渡, 去放箱子 */
        dbg("[ACT] 5/7 action group 12 (toward box)\r\n");
        runActionGroup(12, 1);

        osDelay(7500);

        dbg("[ACT] === place to box (no vision) done ===\r\n");
    }

    /**
     * @brief  放料流程(单爪-黑爪): 先到3号箱子做定位, 再去目标箱子放料
     *         流程: 移动到3号箱子 -> 准备动作组(定位爪子朝向)
     *               -> 移动到目标箱子 -> Z下降 -> 开爪放豆子 -> Z上升复位
     * @param  target_box  目标箱子位置 1~5
     *
     * 准备动作组 (按箱子位置, 仅黑爪):
     *   箱子1->组7  箱子2-4->组12  箱子5->组9
     * 开爪放豆子动作组:
     *   黑爪->组18
     */
    void goto_box(uint8_t target_box)
    {
        if (target_box < 1 || target_box > 5)
            return;

        /* 1. 先到第3号箱子(中转), 在那里做定位动作组 */
        move_box(3);

        /* 2. 准备放箱子: 根据箱子位置选择定位动作组(仅黑爪) */
        uint8_t prep_group;
        if      (target_box == 1) prep_group = 7;   /* 黑爪准备放箱子1 */
        else if (target_box == 5) prep_group = 9;   /* 黑爪准备放箱子5 */
        else                      prep_group = 12;  /* 黑爪准备放中间箱子(2-4) */
        runActionGroup(prep_group, 1);
        osDelay(3000);

        /* 3. 再去目标箱子 */
        move_box(target_box);

        /* 4. Z轴下降25cm (X/Y不动) */
        (void)Motor_XYZ(1, 800, 50,0, /* X: 不动 */
                        1, 50, 20, 0,  /* Y: 不动 */
                        1, 22.0f);     /* Z: dir=1(下), 25cm */
        osDelay(4000);

        /* 5. 开爪放豆子 (黑爪->组18) */
        runActionGroup(18, 1);
        osDelay(5000);

        /* 6. Z轴上升25cm复位 */
        (void)Motor_XYZ(1, 800, 50,0, /* X: 不动 */
                        1, 50, 20, 0, /* Y: 不动 */
                        0, 22.0f);     /* Z: dir=0(上), 25cm */
        osDelay(4000);
    }

    /**
     * @brief  从当前箱子移动到目标箱子, 只做定位(不做放料)
     *         流程: X移动到3号箱子(中转) -> 组15(准备) -> X移动到目标箱子
     * @param  target_box  目标箱子位置 1~5
     */
    void goto_box_ready(uint8_t target_box)
    {
        if (target_box < 1 || target_box > 5)
            return;

        /* 1. 先到第3号箱子(中转) */
        move_box(3);

        /* 2. 组15(准备) */
        runActionGroup(15, 1);
        osDelay(3000);

        /* 3. 再去目标箱子 */
        move_box(target_box);
    }

    /**
     * @brief  只移动X轴到目标箱子位置 (Y/Z不动, 不执行放料)
     *         与 goto_box 共用 box_x_coord / s_current_box 位置跟踪。
     * @param  target_box  目标箱子位置 1~5
     */
    void move_box(uint8_t target_box)
    {
        if (target_box < 1 || target_box > 5)
            return;
        if (target_box == s_current_box)
            return;

        float delta = box_x_coord[target_box] - box_x_coord[s_current_box];
        uint8_t dir = (delta > 0.0f) ? 1 : 0; /* >0 向右, <0 向左 */
        float dist = (delta > 0.0f) ? delta : -delta;

        (void)Motor_XYZ(dir, 800, 50,dist,
                        1, 50, 20, 0, /* Y: 不动 */
                        0, 0.0f);      /* Z: 不动 */

        osDelay(6000);
        s_current_box = target_box;
    }
