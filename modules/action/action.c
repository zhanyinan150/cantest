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
#include <string.h>
#include <stdio.h>

#define ACTION_TASK_STACK_SIZE   1024
#define ACTION_TASK_PRIORITY     osPriorityNormal
#define ACTION_STARTUP_DELAY     1000
#define K230_ACK_TIMEOUT_MS      3000
#define K230_DATA_TIMEOUT_MS     15000

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

static void dbg(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), 100);
}

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
    *flag = 0;
    return 0;
}

static int send_cmd_and_wait_ack(uint8_t cmd)
{
    k230_ack_flag = 0;
    k230_write(cmd);
    return wait_flag(&k230_ack_flag, K230_ACK_TIMEOUT_MS);
}

static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY);

    char buf[80];
    dbg("\r\n=== K230 Competition (Master) ===\r\n");

    /* ======== Phase 1: 豆子识别 ======== */
    dbg("[P1] Send LOOK_BEAN\r\n");
    if (send_cmd_and_wait_ack(K230_CMD_LOOK_BEAN) != 0)
    {
        dbg("[P1] ACK TIMEOUT!\r\n");
        goto idle;
    }
    dbg("[P1] ACK received, wait bean data...\r\n");

    if (wait_flag(&bean_flag, K230_DATA_TIMEOUT_MS) != 0)
    {
        dbg("[P1] DATA TIMEOUT!\r\n");
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
        dbg("[P2] ACK TIMEOUT!\r\n");
        goto idle;
    }
    dbg("[P2] ACK received, wait recognition done...\r\n");

    if (wait_flag(&k230_ack_flag, K230_DATA_TIMEOUT_MS) != 0)
    {
        dbg("[P2] RECOGNITION TIMEOUT!\r\n");
        goto idle;
    }
    dbg("[P2] Recognition done\r\n");
    k230_write(K230_CMD_CLOSE);
    dbg("[P2] ACK sent\r\n");

    /* ======== Phase 3: side number + inference ======== */
    /* ---- Motor: move to box ---- */
    dbg("[ACT] >>> Motor: move to box <<<\r\n");
    osDelay(1000);

    dbg("[P3] Send LOOK_SIDE\r\n");
    if (send_cmd_and_wait_ack(K230_CMD_LOOK_SIDE) != 0)
    {
        dbg("[P3] ACK TIMEOUT!\r\n");
        goto idle;
    }
    dbg("[P3] ACK received, wait full data...\r\n");

    if (wait_flag(&full_number_flag, K230_DATA_TIMEOUT_MS) != 0)
    {
        dbg("[P3] DATA TIMEOUT!\r\n");
        goto idle;
    }
    snprintf(buf, sizeof(buf), "[P3] Numbers: %02X %02X %02X %02X %02X\r\n",
             number_position[0], number_position[1], number_position[2],
             number_position[3], number_position[4]);
    dbg(buf);
    k230_write(K230_CMD_CLOSE);
    dbg("[P3] ACK sent\r\n");

    bean_locked = 0;

    /* ---- place beans ---- */
    dbg("[ACT] >>> Place beans: <<<\r\n");
    snprintf(buf, sizeof(buf), "[ACT] 豆子1: %02X -> 位置%d\r\n",
             bean_color[0], 0);
    dbg(buf);
    Data_Handle1(bean_color[0]);

    snprintf(buf, sizeof(buf), "[ACT] 豆子2: %02X -> 位置%d\r\n",
             bean_color[1], 0);
    dbg(buf);
    Data_Handle1(bean_color[1]);

    snprintf(buf, sizeof(buf), "[ACT] 豆子3: %02X -> 位置%d\r\n",
             bean_color[2], 0);
    dbg(buf);
    Data_Handle1(bean_color[2]);
    dbg("[ACT] Done\r\n");

idle:
    dbg("=== All phases complete ===\r\n");
    for (;;)
        osDelay(1000);
}
