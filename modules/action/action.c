/**
  ******************************************************************************
  * @file    action.c
  * @brief   K230 比赛通讯(主机) + 电机动作序列 (STM32端)
  ******************************************************************************
  * STM32 为通讯主机, 每个阶段: 发命令 -> 等K230 ACK -> 等数据 -> 发ACK
  *
  *   P1: 发 LOOK_BEAN -> 等 ACK -> 等豆子数据 -> 发 ACK -> 抓豆子
  *   P2: 发 LOOK_NUMBER -> 等 ACK -> 等正面数字 -> 发 ACK
  *   P3: 发 LOOK_SIDE -> 等 ACK -> 等完整5数字 -> 发 ACK -> 放豆子
  ******************************************************************************
  */

#include "action.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "motor.h"
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
static void action_douzi_first(void);

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

/* 发命令 + 等 K230 ACK, ACK 超时返回 -1 */
static int send_cmd_and_wait_ack(uint8_t cmd)
{
    k230_ack_flag = 0;
    k230_write(cmd);
    return wait_flag(&k230_ack_flag, K230_ACK_TIMEOUT_MS);
}

//备注一下后面的左边指有电池那边，右边指没电池那边

static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY);

    char buf[80];
    dbg("\r\n=== K230 Competition (Master) ===\r\n");

    /* ---- P1: 发命令看豆子 -> 等 ACK -> 等数据 -> 发 ACK ---- */
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

    /* ---- 抓豆子 ---- */
    dbg("[ACT] Grabbing beans...\r\n");
    action_douzi_first();

    /* ---- P2: 障碍物移动 -> 发命令看正面数字 ---- */
    dbg("[P2] Moving to front number position...\r\n");
    (void)Motor_XYZ(0, 300, 20, 0,
                    0, 100, 20, 100.0f,
                    0, 0.0f);
    osDelay(6000);
    (void)Motor_XYZ(1, 300, 20, 65.0f,
                    0, 50, 20, 160.0f,
                    0, 0.0f);
    osDelay(6000);

    dbg("[P2] Send LOOK_NUMBER\r\n");
    if (send_cmd_and_wait_ack(K230_CMD_LOOK_NUMBER) != 0)
    {
        dbg("[P2] ACK TIMEOUT!\r\n");
        goto idle;
    }
    dbg("[P2] ACK received, wait recognition done...\r\n");

    /* 等 K230 识别完成后的第二个 ACK (不发数据帧, 正面数字仅 K230 内部使用) */
    if (wait_flag(&k230_ack_flag, K230_DATA_TIMEOUT_MS) != 0)
    {
        dbg("[P2] RECOGNITION TIMEOUT!\r\n");
        goto idle;
    }
    dbg("[P2] Recognition done\r\n");
    k230_write(K230_CMD_CLOSE);
    dbg("[P2] ACK sent\r\n");

    /* ---- P3: 到箱子 -> 发命令看侧面数字 ---- */
    (void)Motor_XYZ(1, 300, 20, 0,
                    0, 100, 20, 55.0f,
                    0, 0.0f);
    osDelay(8000);

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

    /* ---- 放豆子 ---- */
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

static void action_douzi_first(void)
{
    (void)Motor_XYZ(1, 300, 30, 44.0f,
                    1, 100, 20, 185.0f,
                    0, 35.0f);
    osDelay(6000);

    (void)Motor_XYZ(0, 300, 20, 20.5f,
                    1, 100, 20, 0,
                    0, 0.0f);
    osDelay(4000);

    (void)Motor_XYZ(0, 300, 20, 60.0f,
                    1, 100, 20, 0,
                    0, 0.0f);
    osDelay(9000);
}
