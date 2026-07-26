/**
  ******************************************************************************
  * @file    action.c
  * @brief   K230 比赛通讯(主机) + 动作序列 (STM32端)
  ******************************************************************************
  * STM32 为通讯主机, 每个阶段: 发命令 -> 等K230 ACK -> 等数据 -> 发ACK
  * 电机动作用文字占位, 后续替换为实际 Motor_XYZ 调用
  ******************************************************************************
  */

#include "action.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "K230.h"
#include "usart.h"
#include "bsp_log.h"     /* printf 经日志队列 -> LogTask DMA 发 USART1 */
#include <string.h>
#include <stdio.h>

#define ACTION_TASK_STACK_SIZE   1024
#define ACTION_TASK_PRIORITY     osPriorityNormal
#define ACTION_STARTUP_DELAY     1000
#define K230_ACK_TIMEOUT_MS      3000
#define K230_DATA_TIMEOUT_MS     15000
#define K230_RETRY_MAX           3

static void action_1(void *argument);

void Action_Init(void)
{
    const osThreadAttr_t attr = {
        .name = "ActionTask",
        .stack_size = ACTION_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)ACTION_TASK_PRIORITY,
    };
    osThreadNew(action_1, NULL, &attr);
}

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

/* 发命令 + 等 K230 ACK, 超时重试最多 K230_RETRY_MAX 次 */
static int send_cmd_and_wait_ack(uint8_t cmd)
{
    for (int retry = 0; retry < K230_RETRY_MAX; retry++)
    {
        k230_ack_flag = 0;                       /* 丢弃上一轮残留 ACK */
        if (k230_write(cmd) != 0)                /* 发送失败(总线忙)直接重试 */
        {
            dbg("[RTY] uart tx failed\r\n");
            osDelay(50);
            continue;
        }
        if (wait_flag(&k230_ack_flag, K230_ACK_TIMEOUT_MS) == 0)
            return 0;
        dbg("[RTY] no ack, retry...\r\n");
    }
    return -1;
}

/* 发命令 + 等 ACK + 等数据, 任一步超时则重发命令, 最多 K230_RETRY_MAX 次 */
static int send_cmd_wait_ack_wait_data(uint8_t cmd, volatile uint8_t *data_flag)
{
    for (int retry = 0; retry < K230_RETRY_MAX; retry++)
    {
        /* 发命令前清数据标志: 否则上一轮/上电前残留的 1 会让 wait_flag 立刻
         * 返回成功, 而 bean_color/number_position 里是过期数据。 */
        *data_flag = 0;

        if (send_cmd_and_wait_ack(cmd) != 0)
            continue;
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

static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY);

    char buf[80];
    dbg("\r\n=== K230 Competition (Master) ===\r\n");

    /* ======== Phase 1: 豆子识别 ======== */
    dbg("[P1] Send LOOK_BEAN\r\n");
    if (send_cmd_wait_ack_wait_data(K230_CMD_LOOK_BEAN, &bean_flag) != 0)
    {
        dbg("[P1] FAILED after retries!\r\n");
        goto idle;
    }
    snprintf(buf, sizeof(buf), "[P1] Bean: %02X %02X %02X\r\n",
             bean_color[0], bean_color[1], bean_color[2]);
    dbg(buf);
    k230_write(K230_CMD_CLOSE);
    dbg("[P1] ACK sent\r\n");

    /* ---- Motor: grab beans ---- */
    dbg("[ACT] >>> Motor: grab beans (start->left->middle->ready) <<<\r\n");
    osDelay(1000);

    /* ======== Phase 2: front number ======== */
    /* ---- Motor: obstacle avoidance ---- */
    dbg("[ACT] >>> Motor: obstacle avoidance <<<\r\n");
    osDelay(1000);

    dbg("[P2] Send LOOK_NUMBER\r\n");
    if (send_cmd_and_wait_ack(K230_CMD_LOOK_NUMBER) != 0)
    {
        dbg("[P2] FAILED after retries!\r\n");
        goto idle;
    }
    dbg("[P2] ACK received, wait recognition done...\r\n");

    /* Phase 2: K230 识别完再发一个 ACK(同样是 0x0A), 不发数据帧。
     * 注意不要在这里清零 k230_ack_flag: 若 K230 识别很快, 第二个 ACK 可能在
     * send_cmd_and_wait_ack 返回前就已到达并被计数, 清零会把它抹掉导致白等。 */
    {
        int p2_ok = 0;
        for (int retry = 0; retry < K230_RETRY_MAX; retry++)
        {
            if (wait_flag(&k230_ack_flag, K230_DATA_TIMEOUT_MS) == 0)
            {
                p2_ok = 1;
                break;
            }
            dbg("[P2] recognition timeout, resync + resend LOOK_NUMBER\r\n");
            k230_write(K230_CMD_CLOSE);
            osDelay(200);
            send_cmd_and_wait_ack(K230_CMD_LOOK_NUMBER);
        }
        if (!p2_ok)
        {
            dbg("[P2] FAILED after retries!\r\n");
            goto idle;
        }
    }
    dbg("[P2] Recognition done\r\n");
    k230_write(K230_CMD_CLOSE);
    dbg("[P2] ACK sent\r\n");

    /* ======== Phase 3: side number + inference ======== */
    /* ---- Motor: move to box ---- */
    dbg("[ACT] >>> Motor: move to box <<<\r\n");
    osDelay(1000);

    dbg("[P3] Send LOOK_SIDE\r\n");
    if (send_cmd_wait_ack_wait_data(K230_CMD_LOOK_SIDE, &full_number_flag) != 0)
    {
        dbg("[P3] FAILED after retries!\r\n");
        goto idle;
    }
    snprintf(buf, sizeof(buf), "[P3] Numbers: %02X %02X %02X %02X %02X\r\n",
             number_position[0], number_position[1], number_position[2],
             number_position[3], number_position[4]);
    dbg(buf);
    k230_write(K230_CMD_CLOSE);
    dbg("[P3] ACK sent\r\n");

    bean_locked = 0;

    /* ---- place beans ----
     * Data_Handle1 返回命中的箱子位置(1~5), 负值=失败必须报出来:
     *   -1 = 豆子颜色非法(K230 发来的不是 0x06/07/08)
     *   -2 = 该数字不在 number_position 里(识别错或第5位推理失败)
     * 原来忽略返回值, 这两种情况下豆子会被静默丢弃, 现场完全看不出发生了什么。*/
    dbg("[ACT] >>> Place beans <<<\r\n");
    for (int b = 0; b < 3; b++)
    {
        int pos = Data_Handle1(bean_color[b]);
        if (pos > 0)
            snprintf(buf, sizeof(buf), "[ACT] bean%d color=%02X -> box %d\r\n",
                     b + 1, bean_color[b], pos);
        else
            snprintf(buf, sizeof(buf), "[ACT] bean%d color=%02X -> SKIPPED (err %d)\r\n",
                     b + 1, bean_color[b], pos);
        dbg(buf);
    }
    dbg("[ACT] Done\r\n");

idle:
    dbg("=== All phases complete ===\r\n");
    for (;;)
        osDelay(1000);
}
