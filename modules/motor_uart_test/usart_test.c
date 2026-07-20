#include "Emm_V5.h"      /* Emm_V5_* 函数: 内部 UART5 DMA 发送给3号电机 */
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"
#include <stdint.h>       /* uint8_t/uint32_t (X_TEST_CLK 宏用) */
#include "usart.h"        /* huart5, HAL_UART_GetState (诊断用) */
#include "mech_params.h"   /* X轴机械参数 (周长/减速比/每转脉冲) */

/* ---- 任务参数 ---- */
#define UART_TEST_TASK_STACK    768              /* 堆栈(word) */
#define UART_TEST_TASK_PRIO     osPriorityNormal
#define UART_TEST_STARTUP_DELAY 1000             /* 上电稳定(ms) */

/* ---- X 轴往复运动参数 ---- */
#define X_AXIS_ADDR             3                /* 3号电机 = X轴 */
#define X_TEST_VEL_RPM          300              /* 转速(RPM), 30cm@300rpm≈1.36s */
#define X_TEST_ACC              180              /* 加速度档位 0~255, 0=直接启动 */
#define X_TEST_DISTANCE_CM      30.0f            /* 每次往复距离(cm), 前进/后退相同 */
#define X_MOVE_DELAY_MS         2000             /* 等 X 走完 30cm 的时间(ms), 留余量 */

/* 30cm -> 脉冲 (X轴: 周长 π×1.4cm, 减速比1, 每转65536脉冲) */
#define X_TEST_CLK  ((uint32_t)((X_TEST_DISTANCE_CM / MOTOR_X_WHEEL_CIRCUMFERENCE_CM) \
                                * MOTOR_XY_PULSE_PER_REV * MOTOR_X_GEAR_RATIO))

static void UartTestTask(void *argument);

/**
  * @brief  创建 UART5 测试任务 (仅 X轴往复)
  * @note   在 app_init.c 中调用以启用:
  *           extern void UartTest_Init(void);
  *           UartTest_Init();
  */
void UartTest_Init(void)
{
    const osThreadAttr_t attr = {
        .name = "UartTestTask",
        .stack_size = UART_TEST_TASK_STACK * 4,
        .priority = (osPriority_t)UART_TEST_TASK_PRIO,
    };
    osThreadNew(UartTestTask, NULL, &attr);
}

/**
  * @brief  UART5 测试任务 (经 Emm_V5.c 函数控制 3号 X轴电机)
  * @note   Emm_V5.c 各发送函数统一用 UART5 DMA (HAL_UART_Transmit_DMA) 发送。
  *         ⚠ 若 DMA 完成中断运行时未把 gState 复位回 READY, 会"只发第一帧"(老问题),
  *           届时改阻塞 HAL_UART_Transmit 或在每次发送前等 gState==READY+AbortTransmit。
  *         Pos_Control 内部含 HAL_Delay(50), 任务自身用 osDelay 让出 CPU。
  *         printf 经 USART1(PA9) 输出, 串口助手(115200)查看。
  */
static void UartTestTask(void *argument)
{
    // osDelay(UART_TEST_STARTUP_DELAY);
    // printf("[uart_test] task start\r\n");

    /* 解除堵转保护 (电机若曾堵转, 保护会锁住, 使能/位置命令不执行) */
//    Emm_V5_Reset_Clog_Pro(X_AXIS_ADDR);
//    osDelay(50);

    /* X轴使能 (单电机 snF=false 不同步) */
    Emm_V5_En_Control(X_AXIS_ADDR, true, false);
    osDelay(100);
    // printf("[uart_test] enabled, state=%ld\r\n", (long)HAL_UART_GetState(&huart5));

    /* X轴往复 30cm (前进/后退循环) */
    uint8_t dir = 0;  /* 0=前进, 1=后退, 每次发命令后翻转 */
    for (;;) {
        Emm_V5_Pos_Control(X_AXIS_ADDR, dir, X_TEST_VEL_RPM, X_TEST_ACC,
                           X_TEST_CLK, false, false);   /* 相对运动 raF=0, 不同步 snF=0; 函数内含 HAL_Delay(50) */
        // printf("[uart_test] pos dir=%d, state=%ld\r\n", dir, (long)HAL_UART_GetState(&huart5));
        // dir ^= 1;  /* 翻转方向: 前进->后退->前进... */
        osDelay(X_MOVE_DELAY_MS);
    }
}
