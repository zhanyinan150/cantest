/**
  ******************************************************************************
  * @file    action.c
  * @brief   动作序列(抓豆子->放箱子) + K230 三阶段通讯(主机)
  ******************************************************************************
  * action_1 任务: 上电后自动跑完整比赛流程, 不依赖串口命令。
  *
  * K230 端(view/k230_competition.py)是严格的三阶段状态机, 顺序不可打乱:
  *   Phase1 LOOK_BEAN(0x02)   -> ACK -> 豆子颜色数据帧 -> 等主机 CLOSE(0x06)
  *   Phase2 LOOK_NUMBER(0x03) -> ACK -> (识别完再发一个 ACK, 无数据帧) -> 等 CLOSE
  *   Phase3 LOOK_SIDE(0x01)   -> ACK -> 五数字数据帧 -> 等主机 CLOSE(0x06)
  * 因此每一步都必须"发命令->等ACK->等数据->回ACK", 不能只 k230_write 后盲等
  * osDelay: 对端没跟上时主机会拿着上一轮的过期数据继续跑。
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
#include "servo.h"       /* runActionGroup — 联调阶段调用已注释, 恢复时需要 */
#include "K230.h"        /* k230_write, K230 命令码, bean_color, Data_Handle1 */
#include "bsp_log.h"     /* printf 经日志队列 -> LogTask DMA 发 USART1 */
#include <stdio.h>

/* ---- 任务参数 ---- */
#define ACTION_TASK_STACK_SIZE   1024               /* 堆栈(word) */
#define ACTION_TASK_PRIORITY     osPriorityNormal   /* 与 MotorAutoTask 同级 */
#define ACTION_STARTUP_DELAY     1000               /* 等电机使能+M2006反馈稳定(ms) */

/* ---- K230 通讯超时/重试 ---- */
#define K230_ACK_TIMEOUT_MS      3000    /* 等对端 ACK */
#define K230_DATA_TIMEOUT_MS     15000   /* 等识别结果(含推理耗时) */
#define K230_RETRY_MAX           3       /* 重试次数, 只在一层生效, 勿嵌套 */

/* ---- 内部函数 ---- */
static void action_1(void *argument);
static void action_douzi_first(void);
static void action_xiangzi_first(void);

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

/* ==================== K230 通讯协议层 ==================== */

/* 调试输出: 走 printf -> fputc 行缓冲 -> 日志队列 -> LogTask DMA 发 USART1。
 * 不可直接 HAL_UART_Transmit(&huart1,...): 会与 LogTask 的 DMA 发送抢同一个
 * USART1, 正是 bsp_log.c 文件头列为 HardFault 根因的用法。 */
static void dbg(const char *s)
{
    printf("%s", s);
}

/**
  * @brief  等待事件标志(计数器语义), 成功后消费一个事件
  * @note   k230_ack_flag 是计数器(K230 可能连发多个 ACK), bean_flag /
  *         full_number_flag 是 0/1, 两者都适用"非零即有事件, 取走减一"。
  */
static int wait_flag(volatile uint8_t *flag, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (!(*flag))
    {
        osDelay(10);
        elapsed += 10;
        if (elapsed >= timeout_ms)
            return -1;
    }
    (*flag)--;          /* 消费一个事件, 不要粗暴清零以免丢掉已到达的下一个 */
    return 0;
}

/**
  * @brief  发一次命令 + 等一次 ACK, **不重试**
  * @note   重试策略一律由调用方决定。若这里也重试, 而调用方又套一层
  *         K230_RETRY_MAX 循环, 嵌套后实际重试 3x3=9 次(约 27s 才放弃),
  *         与宏名和文档说的"最多 3 次"对不上。
  */
static int send_cmd_once(uint8_t cmd)
{
    k230_ack_flag = 0;                       /* 丢弃上一轮残留 ACK */
    if (k230_write(cmd) != 0)                /* 发送失败(总线忙) */
    {
        dbg("[RTY] uart tx failed\r\n");
        osDelay(50);
        return -1;
    }
    return wait_flag(&k230_ack_flag, K230_ACK_TIMEOUT_MS);
}

/* 发命令 + 等 K230 ACK, 超时重试最多 K230_RETRY_MAX 次 */
static int send_cmd_and_wait_ack(uint8_t cmd)
{
    for (int retry = 0; retry < K230_RETRY_MAX; retry++)
    {
        if (send_cmd_once(cmd) == 0)
            return 0;
        dbg("[RTY] no ack, retry...\r\n");
    }
    return -1;
}

/* 发命令 + 等 ACK + 等数据, 任一步超时则重发命令, 最多 K230_RETRY_MAX 次。
 * 这里必须调 send_cmd_once 而不是 send_cmd_and_wait_ack —— 后者自带重试, 嵌在
 * 本函数的循环里就是 9 次。 */
static int send_cmd_wait_ack_wait_data(uint8_t cmd, volatile uint8_t *data_flag)
{
    for (int retry = 0; retry < K230_RETRY_MAX; retry++)
    {
        /* 发命令前清数据标志: 否则上一轮/上电前残留的 1 会让 wait_flag 立刻
         * 返回成功, 而 bean_color/number_position 里是过期数据。 */
        *data_flag = 0;

        if (send_cmd_once(cmd) != 0)
        {
            dbg("[RTY] no ack, retry...\r\n");
            continue;
        }
        if (wait_flag(data_flag, K230_DATA_TIMEOUT_MS) == 0)
            return 0;

        /* 数据超时: 此时 K230 多半卡在"等主机 ACK"这一步, 直接重发识别命令
         * 它是不认的(它只等 0x06)。先补发一个 CLOSE 让对端退回空闲态, 再重发。 */
        dbg("[RTY] data timeout, send CLOSE to resync then resend\r\n");
        k230_write(K230_CMD_CLOSE);
        osDelay(200);
    }
    return -1;
}

/* ---- 三阶段封装: 与 k230_competition.py 的 Phase1/2/3 一一对应 ---- */

/**
  * @brief  Phase1: 豆子颜色识别, 结果填入 bean_color[3]
  * @retval 0=成功, -1=重试耗尽
  */
static int k230_phase1_bean(void)
{
    char buf[80];

    dbg("[P1] Send LOOK_BEAN\r\n");
    if (send_cmd_wait_ack_wait_data(K230_CMD_LOOK_BEAN, &bean_flag) != 0)
    {
        dbg("[P1] FAILED after retries!\r\n");
        return -1;
    }
    snprintf(buf, sizeof(buf), "[P1] Bean: %02X %02X %02X\r\n",
             bean_color[0], bean_color[1], bean_color[2]);
    dbg(buf);
    k230_write(K230_CMD_CLOSE);      /* 回应答, K230 才会关摄像头进入下一阶段 */
    dbg("[P1] ACK sent\r\n");
    return 0;
}

/**
  * @brief  Phase2: 正面数字识别(K230 端自存, 不回传数据帧, 只发两个 ACK)
  * @retval 0=成功, -1=重试耗尽
  */
static int k230_phase2_front(void)
{
    dbg("[P2] Send LOOK_NUMBER\r\n");
    if (send_cmd_and_wait_ack(K230_CMD_LOOK_NUMBER) != 0)
    {
        dbg("[P2] FAILED after retries!\r\n");
        return -1;
    }
    dbg("[P2] ACK received, wait recognition done...\r\n");

    /* K230 识别完再发一个 ACK(同样是 0x0A), 不发数据帧。
     * 注意不要在这里清零 k230_ack_flag: 若 K230 识别很快, 第二个 ACK 可能在
     * send_cmd_and_wait_ack 返回前就已到达并被计数, 清零会把它抹掉导致白等。 */
    for (int retry = 0; retry < K230_RETRY_MAX; retry++)
    {
        if (wait_flag(&k230_ack_flag, K230_DATA_TIMEOUT_MS) == 0)
        {
            dbg("[P2] Recognition done\r\n");
            k230_write(K230_CMD_CLOSE);
            dbg("[P2] ACK sent\r\n");
            return 0;
        }
        dbg("[P2] recognition timeout, resync + resend LOOK_NUMBER\r\n");
        k230_write(K230_CMD_CLOSE);
        osDelay(200);
        /* 只重发一次, 不用 send_cmd_and_wait_ack, 否则又是 3x3 嵌套 */
        send_cmd_once(K230_CMD_LOOK_NUMBER);
    }

    dbg("[P2] FAILED after retries!\r\n");
    return -1;
}

/**
  * @brief  Phase3: 侧面数字 + 推理第5位, 结果填入 number_position[5]
  * @retval 0=成功, -1=重试耗尽
  */
static int k230_phase3_side(void)
{
    char buf[80];

    dbg("[P3] Send LOOK_SIDE\r\n");
    if (send_cmd_wait_ack_wait_data(K230_CMD_LOOK_SIDE, &full_number_flag) != 0)
    {
        dbg("[P3] FAILED after retries!\r\n");
        return -1;
    }
    snprintf(buf, sizeof(buf), "[P3] Numbers: %02X %02X %02X %02X %02X\r\n",
             number_position[0], number_position[1], number_position[2],
             number_position[3], number_position[4]);
    dbg(buf);
    k230_write(K230_CMD_CLOSE);
    dbg("[P3] ACK sent\r\n");
    return 0;
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
 * @note   视觉任一阶段失败时不中止动作序列 —— 电机行程本身不依赖识别结果,
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
  * @note   [联调阶段] 升降(Z)只执行第 1 步上升 35cm, 其余 Z 升降置 0 不动;
  *         爪子舵机 runActionGroup 全部注释, 改为 USART1 文字打印占位。
  *         原参数一律以注释保留在原处, 恢复时取消注释即可。
  */
static void action_douzi_first(void)
{
    dbg("[ACT] === grab beans (Z: only step1 up, servo: text only) ===\r\n");

    /* 起始点到抓左边豆子: Z 上升 35cm —— 本阶段唯一执行的升降动作 */
    dbg("[ACT] 1/7 move to left bean (X+44 Y+185 Z+35)\r\n");
        (void)Motor_XYZ(1, 300, 30, 44.0f,  /* X: dir=1(右), 300rpm, acc=30, 44cm */
                    1, 100, 20, 185.0f,    /* Y: dir=1(前), 100rpm, acc=20, 185cm */
                    0, 35.0f);           /* Z: dir=0(上), 35cm */


        /* runActionGroup(1,1); */          //舵机初始化
        dbg("[SERVO-SIM] >>> servo init <<<\r\n");
        osDelay(6000);

          //降爪子去抓左边豆子 —— Z 暂不动(原: 1, 30)
        dbg("[ACT] 2/7 claw down [Z SKIPPED, orig: down 30]\r\n");
        (void)Motor_XYZ(1, 300, 30, 0,  /* X: dir=1(右), 300rpm, acc=30, 44cm */
                        1, 100, 20, 0, /* Y: dir=1(前), 100rpm, acc=20, 185cm */
                        0, 0.0f);        /* Z: 暂停用, 原为 1, 30 (下降30cm) */
        osDelay(2500);

        /* runActionGroup(2, 1); */         //白爪抓
        dbg("[SERVO-SIM] >>> WHITE claw GRAB left bean <<<\r\n");
        osDelay(3000);

        //升爪子复位 —— Z 暂不动(原: 0, 30)
        dbg("[ACT] 3/7 claw up [Z SKIPPED, orig: up 30]\r\n");
        (void)Motor_XYZ(1, 300, 30, 0, /* X: dir=1(右), 300rpm, acc=30, 44cm */
                        1, 100, 20, 0, /* Y: dir=1(前), 100rpm, acc=20, 185cm */
                        0, 0.0f);      /* Z: 暂停用, 原为 0, 30 (上升30cm) */
        osDelay(2500);

        /* 抓中间豆子 */
        dbg("[ACT] 4/7 move to middle bean (X-20.5)\r\n");
        (void)Motor_XYZ(0, 300, 20, 20.5f, /* X: dir=0(左), 300rpm, acc=20, 20.5cm */
                        1, 100, 20, 0,     /* Y: 不动 */
                        0, 0.0f);          /* Z: 不动 */
        osDelay(4000);

        /* runActionGroup(3, 1); */         //转轴去抓中间豆子
        dbg("[SERVO-SIM] >>> rotate axis to middle bean <<<\r\n");
        osDelay(2000);

        // 降爪子去抓中间豆子 —— Z 暂不动(原: 1, 20)
        dbg("[ACT] 5/7 claw down [Z SKIPPED, orig: down 20]\r\n");
        (void)Motor_XYZ(1, 300, 30, 0, /* X: dir=1(右), 300rpm, acc=30, 44cm */
                        1, 100, 20, 0, /* Y: dir=1(前), 100rpm, acc=20, 185cm */
                        0, 0.0f);      /* Z: 暂停用, 原为 1, 20 (下降20cm) */
        osDelay(2500);

        /* runActionGroup(4, 1); */         //黑爪抓
        dbg("[SERVO-SIM] >>> BLACK claw GRAB middle bean <<<\r\n");
        osDelay(2000);

        // 升爪子复位 —— Z 暂不动(原: 0, 20)
        dbg("[ACT] 6/7 claw up [Z SKIPPED, orig: up 20]\r\n");
        (void)Motor_XYZ(1, 300, 30, 0, /* X: dir=1(右), 300rpm, acc=30, 44cm */
                        1, 100, 20, 0, /* Y: dir=1(前), 100rpm, acc=20, 185cm */
                        0, 0.0f);      /* Z: 暂停用, 原为 0, 20 (上升20cm) */
        osDelay(2500);

        /* runActionGroup(5, 1); */         //转轴，进去放箱子准备阶段
        dbg("[SERVO-SIM] >>> rotate axis, ready for box <<<\r\n");
        osDelay(2000);

        /* 准备去放箱子 */
        dbg("[ACT] 7/7 move toward box (X-60)\r\n");
        (void) Motor_XYZ(0, 300, 20, 60.0f, /* X: dir=0(左), 300rpm, acc=20, 60cm */
                         1, 100, 20, 0,     /* Y: 不动 */
                         0, 0.0f);          /* Z: 不动 */
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
    (void)Motor_XYZ(0, 300, 20, 0,       /* X: 不动 */
                    0, 100, 20, 100.0f,  /* Y: dir=0(后), 100rpm, acc=20, 100cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(6000);

    /* 豆子到箱子障碍物动作2 */
    dbg("[ACT] obstacle step2 (X+65 Y-160)\r\n");
    (void)Motor_XYZ(1, 300, 20, 65.0f,   /* X: dir=1(右), 300rpm, acc=20, 65cm */
                    0, 50, 20, 160.0f,   /* Y: dir=0(后), 50rpm, acc=20, 160cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(6000);

    (void)k230_phase2_front();   /* 看正面数字 */

    /* 箱子障碍物到箱子 */
    dbg("[ACT] move to box (Y-55)\r\n");
    (void)Motor_XYZ(1, 300, 20, 0,       /* X: 不动 */
                    0, 100, 20, 55.0f,   /* Y: dir=0(后), 100rpm, acc=20, 55cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(8000);//这里到达了第4个箱子位置

    (void)k230_phase3_side();    /* 看侧面数字 + 推理第5位 */

    dbg("[ACT] === place to box done ===\r\n");
}
