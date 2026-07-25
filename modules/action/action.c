/**
  ******************************************************************************
  * @file    action.c
  * @brief   K230 通讯测试 (纯收发握手, 无电机动作)
  ******************************************************************************
  * action_1 任务: 上电后测试 STM32-K230 通讯握手, 通过 UART1(printf)
  * 上报每一步收到的数据到串口助手。
  *   Phase 1: 等 K230 豆子数据 -> ACK
  *   Phase 2: 发 LOOK_NUMBER -> 等正面数字 -> ACK
  *   Phase 3: 等 5 数字 -> ACK
  ******************************************************************************
  */

#include "action.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "K230.h"
#include "stdio.h"

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

static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY);

    printf("=== K230 Comm Test ===\n");

    /* Phase 1: 等 K230 豆子数据 (K230 上电自主识别) */
    printf("[P1] Waiting for bean data...\n");
    while (!bean_flag)
        osDelay(10);
    bean_flag = 0;
    printf("[P1] Bean: %02X %02X %02X\n", bean_color[0], bean_color[1], bean_color[2]);
    k230_write(K230_CMD_CLOSE);
    printf("[P1] ACK sent\n");

    /* Phase 2: 触发正面数字识别 */
    printf("[P2] Trigger front number...\n");
    k230_write(K230_CMD_LOOK_NUMBER);
    while (!front_number_flag)
        osDelay(10);
    front_number_flag = 0;
    printf("[P2] Front: %02X %02X %02X\n", front_number[0], front_number[1], front_number[2]);
    k230_write(K230_CMD_CLOSE);
    printf("[P2] ACK sent\n");

    /* Phase 3: 等 5 数字 (4 识别 + 1 推理) */
    printf("[P3] Waiting for 5 numbers...\n");
    while (!full_number_flag)
        osDelay(10);
    full_number_flag = 0;
    printf("[P3] Full: %02X %02X %02X %02X %02X\n",
           number_position[0], number_position[1], number_position[2],
           number_position[3], number_position[4]);
    k230_write(K230_CMD_CLOSE);
    printf("[P3] ACK sent\n");

    printf("=== Comm Test Done ===\n");

    for (;;)
        osDelay(1000);
}
