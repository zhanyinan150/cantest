/**
  ******************************************************************************
  * @file    action.c
  * @brief   K230 比赛通讯 + 动作序列 (STM32端)
  ******************************************************************************
  * action_1 任务: 与 K230 完成三阶段握手协议, 获取豆子颜色和数字位置后
  *                 调用 Data_Handle1() 执行放豆子动作。
  *
  *   P1: 等 K230 豆子数据 -> ACK -> 电机移动到正面识别位
  *   P2: 发 LOOK_NUMBER -> 等正面数字 -> ACK -> 电机移动到侧面识别位
  *   P3: 发 LOOK_SIDE -> 等完整5数字 -> ACK -> Data_Handle1 放豆子
  *
  * 数据由 k230_read() (DMA 中断回调) 解析填充, 本任务轮询 flag 标志。
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

/* ---- 任务参数 ---- */
#define ACTION_TASK_STACK_SIZE   1024
#define ACTION_TASK_PRIORITY     osPriorityNormal
#define ACTION_STARTUP_DELAY     1000
#define K230_WAIT_TIMEOUT_MS     10000  /* K230 单阶段等待超时 10s */

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

static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY);

    char buf[80];

    dbg("\r\n=== K230 Competition ===\r\n");

    /* ---- P1: 等 K230 豆子颜色帧 (K230 上电自动识别) ---- */
    dbg("[P1] Wait bean...\r\n");
    if (wait_flag(&bean_flag, K230_WAIT_TIMEOUT_MS) != 0)
    {
        dbg("[P1] TIMEOUT!\r\n");
        goto idle;
    }

    snprintf(buf, sizeof(buf), "[P1] Bean: %02X %02X %02X\r\n",
             bean_color[0], bean_color[1], bean_color[2]);
    dbg(buf);
    k230_write(K230_CMD_CLOSE);
    dbg("[P1] ACK sent\r\n");

    /* TODO: 电机移动到正面数字识别位置 */
    osDelay(1000);

    /* ---- P2: 触发正面数字识别 ---- */
    dbg("[P2] Sent LOOK_NUMBER\r\n");
    k230_write(K230_CMD_LOOK_NUMBER);

    if (wait_flag(&front_number_flag, K230_WAIT_TIMEOUT_MS) != 0)
    {
        dbg("[P2] TIMEOUT!\r\n");
        goto idle;
    }

    snprintf(buf, sizeof(buf), "[P2] Front: %02X %02X %02X\r\n",
             front_number[0], front_number[1], front_number[2]);
    dbg(buf);
    k230_write(K230_CMD_CLOSE);
    dbg("[P2] ACK sent\r\n");

    /* TODO: 电机移动到侧面数字识别位置 */
    osDelay(1000);

    /* ---- P3: 触发侧面数字识别 + 推理第5个 ---- */
    dbg("[P3] Sent LOOK_SIDE\r\n");
    k230_write(K230_CMD_LOOK_SIDE);

    if (wait_flag(&full_number_flag, K230_WAIT_TIMEOUT_MS) != 0)
    {
        dbg("[P3] TIMEOUT!\r\n");
        goto idle;
    }

    snprintf(buf, sizeof(buf), "[P3] Numbers: %02X %02X %02X %02X %02X\r\n",
             number_position[0], number_position[1], number_position[2],
             number_position[3], number_position[4]);
    dbg(buf);
    k230_write(K230_CMD_CLOSE);
    dbg("[P3] ACK sent\r\n");

    bean_locked = 0;

    /* ---- 放豆子: 根据豆子颜色查数字位置, 调对应动作组 ---- */
    dbg("[ACT] Placing beans...\r\n");
    Data_Handle1(bean_color[0]);
    Data_Handle1(bean_color[1]);
    Data_Handle1(bean_color[2]);
    dbg("[ACT] Done\r\n");

idle:
    dbg("=== All phases complete ===\r\n");
    for (;;)
        osDelay(1000);
}
