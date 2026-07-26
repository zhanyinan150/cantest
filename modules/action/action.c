/**
  ******************************************************************************
  * @file    action.c
  * @brief   K230 通讯协议测试 (纯握手, 无电机动作)
  ******************************************************************************
  * action_1 任务: 与 K230 完成三阶段握手协议
  *   P1: 等豆子数据 -> ACK
  *   P2: 发 LOOK_NUMBER -> 等正面数字 -> ACK
  *   P3: 等完整5数字 -> ACK
  *
  * 数据由 k230_read() (DMA 中断回调) 解析填充, 本任务轮询 flag 标志。
  * DMA 中断不阻塞 CPU, LogTask 正常运行, printf 可用。
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

static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY);

    char buf[80];

    dbg("\r\n=== K230 Protocol ===\r\n");

    for (;;)
    {
        /* ---- P1: 等豆子颜色帧 ---- */
        dbg("[P1] Wait bean...\r\n");
        while (!bean_flag)
            osDelay(10);
        bean_flag = 0;

        snprintf(buf, sizeof(buf), "[P1] Bean: %02X %02X %02X\r\n",
                 bean_color[0], bean_color[1], bean_color[2]);
        dbg(buf);
        k230_write(K230_CMD_CLOSE);
        dbg("[P1] ACK\r\n");

        /* ---- P2: 触发正面数字识别 ---- */
        osDelay(500);
        k230_write(K230_CMD_LOOK_NUMBER);
        dbg("[P2] Sent LOOK_NUMBER\r\n");

        while (!front_number_flag)
            osDelay(10);
        front_number_flag = 0;

        snprintf(buf, sizeof(buf), "[P2] Front: %02X %02X %02X\r\n",
                 front_number[0], front_number[1], front_number[2]);
        dbg(buf);
        k230_write(K230_CMD_CLOSE);
        dbg("[P2] ACK\r\n");

        /* ---- P3: 等完整5数字帧 ---- */
        while (!full_number_flag)
            osDelay(10);
        full_number_flag = 0;

        snprintf(buf, sizeof(buf), "[P3] Numbers: %02X %02X %02X %02X %02X\r\n",
                 number_position[0], number_position[1], number_position[2],
                 number_position[3], number_position[4]);
        dbg(buf);
        k230_write(K230_CMD_CLOSE);
        dbg("[P3] ACK\r\n");

        bean_locked = 0;
        dbg("=== Done ===\r\n");
        for (;;)
            osDelay(1000);
    }
}
