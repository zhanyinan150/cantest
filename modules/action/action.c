/**
  ******************************************************************************
  * @file    action.c
  * @brief   动作序列(抓豆子->放箱子) + K230 三阶段通讯调用(主机)
  ******************************************************************************
  * action_1 任务: 上电后自动跑完整比赛流程, 不依赖串口命令。
  *
  * K230 三阶段通讯协议层(发命令->等ACK->等数据->回ACK + 超时重试)已移至
  * modules/k230/K230.c, 由 k230_phase1_bean / k230_phase2_front / k230_phase3_side
  * 三个函数封装, 本文件直接调用。协议细节见 K230.h 文件头注释。
  *
  * 流程编排:
  *   Phase1(豆子识别) -> action_douzi_first(抓豆) -> action_xiangzi_first(放箱,
  *   内含 Phase2/Phase3) -> Data_Handle1 分拣决策
  *
  * Motor_XYZ 为非阻塞(发完命令即返回), 靠 osDelay 等电机走完。
  *
  * [联调阶段配置] 验证 K230 识别 + XYZ 电机行程:
  *   - 升降(Z): 只执行第 1 步上升 35cm, 其余升降置 0(原参数注释保留在原处)
  *   - 爪子舵机: runActionGroup 全部注释, 改 USART1 文字打印占位
  *   - 分拣投料: 只打印"豆子->几号箱"决策结果, 不真正投料
  *   恢复真实动作时取消对应注释即可, 参数一个没丢。
  *
  * 日志一律用英文: UTF-8 中文字符串在 ARMCC V5.06 下会乱码/报错。
  ******************************************************************************
  */

#include "action.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "motor.h"       /* Motor_XYZ */
#include "servo.h"       /* runActionGroup - 联调阶段调用已注释, 恢复时需要 */
#include "K230.h"        /* k230_write, K230 命令码, bean_color, Data_Handle1,
                          * k230_phase1_bean / k230_phase2_front / k230_phase3_side */
#include "bsp_log.h"     /* printf 经日志队列 -> LogTask DMA 发 USART1 */
#include <stdio.h>

/* ---- 任务参数 ---- */
#define ACTION_TASK_STACK_SIZE   1024               /* 堆栈(word) */
#define ACTION_TASK_PRIORITY     osPriorityNormal   /* 与 MotorAutoTask 同级 */
#define ACTION_STARTUP_DELAY     1000               /* 等电机使能+M2006反馈稳定(ms) */

/* ---- 内部函数 ---- */
static void action_1(void *argument);
static void action_douzi_first(void);
static void action_xiangzi_first(void);
static void goto_box(uint8_t target_box);
static void drop_bean_at_box(uint8_t target_box, uint8_t servo_action);

/* ===== 动作注释 ===== */
/**
 * @brief  去豆子y为1，箱子为0
 *         远离墙x为0，靠近墙为1
 *         上升z为0，下降z为1
 *
 */

/**
  * @brief  初始化动作序列: 创建 action_1 任务
  */
// void Action_Init(void)
// {
//     const osThreadAttr_t attr = {
//         .name = "ActionTask",
//         .stack_size = ACTION_TASK_STACK_SIZE * 4,
//         .priority = (osPriority_t)ACTION_TASK_PRIORITY,
//     };
//     osThreadNew(action_1, NULL, &attr);
// }

/* ==================== 调试输出 ==================== */

/* 调试输出: 走 printf -> fputc 行缓冲 -> 日志队列 -> LogTask DMA 发 USART1。
 * 不可直接 HAL_UART_Transmit(&huart1,...): 会与 LogTask 的 DMA 发送抢同一个
 * USART1, 正是 bsp_log.c 文件头列为 HardFault 根因的用法。 */
static void dbg(const char *s)
{
    printf("%s", s);
}

/**
  * @brief  分拣决策: 依 bean_color 查 number_position 得箱位, 调 Action_1..5
  * @note   Action_1..5 目前仍是 K230.c 里的弱定义空桩, 需按机械行程实现放箱
  *         动作后才会真正投料; 在此之前本函数只输出决策结果供现场核对。
  */
static void place_beans(void)
{
    char buf[100];

    bean_locked = 0;    /* 解锁, 允许下一轮豆子数据写入 */

    /* 先把 K230 的识别结果原样打出来, 便于判断是识别错还是查表错 */
    snprintf(buf, sizeof(buf), "[ACT] beans = %02X %02X %02X\r\n",
             bean_color[0], bean_color[1], bean_color[2]);
    dbg(buf);
    snprintf(buf, sizeof(buf), "[ACT] boxes(1..5) = %02X %02X %02X %02X %02X\r\n",
             number_position[0], number_position[1], number_position[2],
             number_position[3], number_position[4]);
    dbg(buf);

    /* Data_Handle1 返回命中的箱子位置(1~5), 负值=失败必须报出来:
     *   -1 = 豆子颜色非法(K230 发来的不是 0x06/07/08)
     *   -2 = 该数字不在 number_position 里(识别错或第5位推理失败)
     * 忽略返回值的话, 这两种情况下豆子会被静默丢弃, 现场完全看不出发生了什么。
     * 注: Data_Handle1 内部调的 Action_1..5 仍是 K230.c 里的 __weak 空桩,
     * 所以只做决策不投料, 与本阶段"抓放用文字代替"一致。 */
    dbg("[ACT] >>> Place beans (TEXT ONLY, no real drop) <<<\r\n");
    for (int b = 0; b < 3; b++)
    {
        uint8_t c = bean_color[b];
        const char *cname = (c == BEAN_GREEN)  ? "GREEN"  :
                            (c == BEAN_YUN)    ? "YUN"    :
                            (c == BEAN_YELLOW) ? "YELLOW" : "INVALID";
        int pos = Data_Handle1(c);

        if (pos > 0)
            snprintf(buf, sizeof(buf),
                     "[SIM] bean%d %s(%02X) -> box %d : drop here\r\n",
                     b + 1, cname, c, pos);
        else if (pos == -1)
            snprintf(buf, sizeof(buf),
                     "[SIM] bean%d %s(%02X) -> SKIPPED: bad color\r\n",
                     b + 1, cname, c);
        else
            snprintf(buf, sizeof(buf),
                     "[SIM] bean%d %s(%02X) -> SKIPPED: not in boxes\r\n",
                     b + 1, cname, c);
        dbg(buf);
    }
    dbg("[ACT] Done\r\n");
}

/* ==================== 主流程 ==================== */

/**
 * @brief  上电自动动作任务: 视觉识别 + 抓豆子 + 放箱子, 完成后挂起
 * @note   视觉任一阶段失败时不中止动作序列 -- 电机行程本身不依赖识别结果,
 *         只有最后的 place_beans 分拣决策依赖, 失败会在日志里明确报出来。
 *         这样现场至少能看到机构走完, 而不是停在原地无从判断。
 */
static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY); /* 等电机使能 + M2006 反馈稳定 */

    dbg("\r\n=== K230 Competition (Master) ===\r\n");

    /* Phase1 必须在抓豆之前: 抓完再识别就来不及决定放哪个箱 */
    (void)k230_phase1_bean();

    action_douzi_first();   /* 抓豆子, 动作截止到抓完豆子已完成升降结构移到右边 */

    action_xiangzi_first(); /* 放箱子, 内含 Phase2(正面数字) + Phase3(侧面数字) */

    place_beans();          /* 分拣决策: 颜色 -> 箱位 */

    dbg("=== All phases complete ===\r\n");
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

/* ===== 分界线前: 抓豆子 ===== */
/**
  * @brief  抓豆子动作序列
  *         起始点 -> 抓左边豆子 -> 抓中间豆子 -> 准备去放箱子
  */
static void action_douzi_first(void)
{
    dbg("[ACT] === grab beans ===\r\n");

    /* 起始点到抓左边豆子: X+44 Y+185 Z+35(上升) */
    dbg("[ACT] 1/7 move to left bean (X+44 Y+185 Z+35)\r\n");
    (void)Motor_XYZ(1, 400, 50, 44.0f,  /* X: dir=1(右), 300rpm, acc=30, 44cm */
                    1, 100, 20, 185.0f, /* Y: dir=1(前), 100rpm, acc=20, 185cm */
                    0, 35.0f);          /* Z: dir=0(上), 35cm */

    runActionGroup(1, 1);               //舵机初始化
    osDelay(6000);

    //降爪子去抓左边豆子: Z 下降 30cm
    dbg("[ACT] 2/7 claw down (Z-30)\r\n");
    (void)Motor_XYZ(1, 400, 50, 0,      /* X: 不动 */
                    1, 100, 20, 0,      /* Y: 不动 */
                    1, 30.0f);          /* Z: dir=1(下), 30cm */
    osDelay(2500);

    runActionGroup(2, 1);               //白爪抓
    osDelay(3000);

    //升爪子复位: Z 上升 30cm
    dbg("[ACT] 3/7 claw up (Z+30)\r\n");
    (void)Motor_XYZ(1, 400, 50, 0,      /* X: 不动 */
                    1, 100, 20, 0,      /* Y: 不动 */
                    0, 30.0f);          /* Z: dir=0(上), 30cm */
    osDelay(2500);

    /* 抓中间豆子: X-20.5 */
    dbg("[ACT] 4/7 move to middle bean (X-20.5)\r\n");
    (void)Motor_XYZ(0, 400, 50, 20.5f,  /* X: dir=0(左), 300rpm, acc=20, 20.5cm */
                    1, 100, 20, 0,      /* Y: 不动 */
                    0, 0.0f);           /* Z: 不动 */
    osDelay(4000);

    runActionGroup(3, 1);               //转轴去抓中间豆子
    osDelay(2000);

    // 降爪子去抓中间豆子: Z 下降 20cm
    dbg("[ACT] 5/7 claw down (Z-20)\r\n");
    (void)Motor_XYZ(1, 400, 50, 0,      /* X: 不动 */
                    1, 100, 20, 0,      /* Y: 不动 */
                    1, 20.0f);          /* Z: dir=1(下), 20cm */
    osDelay(2500);

    runActionGroup(4, 1);               //黑爪抓
    osDelay(2000);

    // 升爪子复位: Z 上升 20cm
    dbg("[ACT] 6/7 claw up (Z+20)\r\n");
    (void)Motor_XYZ(1, 400, 50, 0,      /* X: 不动 */
                    1, 100, 20, 0,      /* Y: 不动 */
                    0, 20.0f);          /* Z: dir=0(上), 20cm */
    osDelay(2500);

    runActionGroup(5, 1);               //转轴，进去放箱子准备阶段
    osDelay(2000);

    /* 准备去放箱子: X-60 */
    dbg("[ACT] 7/7 move toward box (X-60)\r\n");
    (void)Motor_XYZ(0, 400, 50, 60.0f,  /* X: dir=0(左), 300rpm, acc=20, 60cm */
                    1, 100, 20, 0,      /* Y: 不动 */
                    0, 0.0f);           /* Z: 不动 */
    osDelay(9000);
    dbg("[ACT] === grab beans done ===\r\n");
}

/* ===== 分界线后: 放箱子(含障碍物避让) ===== */
/**
  * @brief  放箱子动作序列
  *         障碍物动作1 -> 障碍物动作2 -> Phase2(正面数字)
  *         -> 到箱子 -> Phase3(侧面数字)
  * @note   Phase2/Phase3 的位置对应 K230 状态机顺序, 不可对调。
  */
static void action_xiangzi_first(void)
{
    dbg("[ACT] === place to box ===\r\n");

    /* 豆子到箱子障碍物动作一 */
    dbg("[ACT] obstacle step1 (Y-100)\r\n");
    (void)Motor_XYZ(0, 400, 50, 0,       /* X: 不动 */
                    0, 100, 20, 100.0f,  /* Y: dir=0(后), 100rpm, acc=20, 100cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(6000);

    /* 豆子到箱子障碍物动作2 */
    dbg("[ACT] obstacle step2 (X+65 Y-160)\r\n");
    (void)Motor_XYZ(1, 400, 50, 65.0f,   /* X: dir=1(右), 300rpm, acc=20, 65cm */
                    0, 50, 20, 160.0f,   /* Y: dir=0(后), 50rpm, acc=20, 160cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(6000);

    (void)k230_phase2_front();   /* 看正面数字 */

    /* 箱子障碍物到箱子 */
    dbg("[ACT] move to box (Y-55)\r\n");
    (void)Motor_XYZ(1, 400, 50, 0,       /* X: 不动 */
                    0, 100, 20, 55.0f,   /* Y: dir=0(后), 100rpm, acc=20, 55cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(8000);//这里到达了第4个箱子位置

    (void)k230_phase3_side();    /* 看侧面数字 + 推理第5位 */

    dbg("[ACT] === place to box done ===\r\n");
}

/* ===== 放豆子: 箱子间X轴移动 (以第4个箱子为原点) ===== */
/* 各箱子相对第4个箱子的X轴距离(cm)与方向:
 *   dir=0(左/远离墙) -> 箱子1,2,3
 *   dir=1(右/靠近墙) -> 箱子5
 *   箱子4为原点, 距离=0
 * 箱子编号1~5从左到右, 与 K230 number_position 一致。 */
static const float   box_x_offset[6] = {0, 93.0f, 73.0f, 35.0f, 0.0f, 25.0f};
static const uint8_t box_x_dir[6]    = {0, 0,      0,      0,      0,     1};
/* 当前所在箱子编号, action_xiangzi_first 结束后应为4 */
static uint8_t s_current_box = 4;

/**
 * @brief  从当前箱子移动X轴到目标箱子 (Y/Z不动)
 *         内部经第4个箱子中转: 不在4号时先回4号, 再去目标。
 *         所有距离以第4个箱子为基准, 回4号=原方向取反。
 * @param  target_box  目标箱子编号 1~5
 */
static void goto_box(uint8_t target_box)
{
    if (target_box < 1 || target_box > 5)
        return;
    if (target_box == s_current_box)
        return;

    /* 不在第4个箱子时, 先回到第4个箱子(方向取反) */
    if (s_current_box != 4)
    {
        uint8_t ret_dir = box_x_dir[s_current_box] ? 0 : 1;
        (void)Motor_XYZ(ret_dir, 400, 50, box_x_offset[s_current_box],
                        1, 100, 20, 0,   /* Y: 不动 */
                        0, 0.0f);         /* Z: 不动 */
        osDelay(3000);
        s_current_box = 4;
    }

    /* 从第4个箱子移动到目标箱子 */
    if (target_box != 4)
    {
        (void)Motor_XYZ(box_x_dir[target_box], 400, 50, box_x_offset[target_box],
                        1, 100, 20, 0,   /* Y: 不动 */
                        0, 0.0f);         /* Z: 不动 */
        osDelay(3000);
    }
    s_current_box = target_box;
}

/**
 * @brief  前往目标箱子放豆子 (X轴移动 + 舵机放料)
 *         在 action_xiangzi_first 到达第4个箱子后调用。
 *         Y/Z不动, 只动X; 到达后执行舵机放料动作。
 * @param  target_box     目标箱子编号 1~5
 * @param  servo_action   放料舵机动作组编号 (根据爪子/放料动作选择)
 */
static void drop_bean_at_box(uint8_t target_box, uint8_t servo_action)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "[ACT] goto box %d, servo %d\r\n",
             target_box, servo_action);
    dbg(buf);

    goto_box(target_box);

    runActionGroup(servo_action, 1);  /* 舵机放料 */
    osDelay(2000);
}