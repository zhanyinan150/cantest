/**
  ******************************************************************************
  * @file    motor_test.c
  * @brief   CAN2 步进电机测试: 1、2号电机多机同步, 位置模式前后转, 每次 50cm
  ******************************************************************************
  * 电机 ID=1,2 (Emm_V5 步进闭环, CAN2 扩展帧), 两电机参数完全相同,
  * 多机同步驱动(snF=true 预存 + 广播 Synchronous_motion 同时触发)。
  *
  * 流程:
  *   1. 上电初始化: 清堵转保护 -> 设闭环模式 -> 同步使能
  *   2. 位置模式前后反转: 两电机发相同命令, 每次 50cm, 方向交替, 间隔 2 秒
  *
  * 50cm -> 脉冲换算 (Y轴机械参数, 见 mech_params.h):
  *   clk = (距离/周长) × 每转脉冲 × 减速比
  *       = (50 / (π×10)) × 65536 × 1 ≈ 104308 脉冲
  ******************************************************************************
  */

#include "motor_test.h"

#include "can.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "Emm_V5_CAN.h"
#include "mech_params.h"   /* Y轴机械参数 */
#include "bsp_log.h"
#include "stdio.h"

/* ==================== 任务参数 ==================== */
#define MOTOR_TEST_TASK_STACK_SIZE   512
#define MOTOR_TEST_TASK_PRIORITY     osPriorityNormal
#define MOTOR_TEST_STARTUP_DELAY     1000   /* 上电稳定(ms) */

/* ==================== 运动参数 ==================== */
#define TEST_DISTANCE_CM             50.0f  /* 每次移动距离(cm), 两电机相同 */
#define TEST_VEL_RPM                 300    /* 转速(RPM), Emm_V5 范围 0~3000, 两电机相同 */
#define TEST_ACC                     180    /* 加速度档位 0~255, 0=直接启动, 两电机相同 */
#define TEST_MOVE_DELAY_MS           2000   /* 两次位置命令间隔(ms)
                                             * 50cm@300rpm≈318ms, 留足余量 */

/* 50cm 换算为脉冲数 (Y轴: 周长π×10cm, 减速比1, 每转65536脉冲) */
#define TEST_CLK  ((uint32_t)((TEST_DISTANCE_CM / MOTOR_Y_WHEEL_CIRCUMFERENCE_CM) \
                              * MOTOR_XY_PULSE_PER_REV * MOTOR_Y_GEAR_RATIO))

/* ==================== 内部函数 ==================== */
//static void MotorTestTask(void *argument);

/* ==================== 公开接口 ==================== */

void MotorTest_Init(void)
{
#if 0  /* Y轴测试停用: 当前走 UART5 测 X轴(MotorUartTest_Init), 恢复 Y轴测试改 #if 1 */
    /* 1. 注册 1、2 号电机到 CAN2 (扩展帧) */
    uint8_t stepper_ids[2] = { MOTOR_TEST_ADDR_1, MOTOR_TEST_ADDR_2 };
    Emm_V5_CAN_Init(stepper_ids, 2);

    /* 2. 启动 CAN2 收发 + 接收中断 */
    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);

    printf("[motor_test] CAN2 state=%lu err=0x%lX\r\n",
           (unsigned long)HAL_CAN_GetState(&hcan2),
           (unsigned long)HAL_CAN_GetError(&hcan2));

    /* 3. 电机准备: 清堵转保护 -> 设闭环模式 (两电机逐个配置)
     *    电机收到命令不转, 多数是堵转保护未清或模式不对 */
    Emm_V5_CAN_Reset_Clog_Pro(MOTOR_TEST_ADDR_1);              osDelay(50);
    Emm_V5_CAN_Reset_Clog_Pro(MOTOR_TEST_ADDR_2);              osDelay(50);
    Emm_V5_CAN_Modify_Ctrl_Mode(MOTOR_TEST_ADDR_1, true, 1);   osDelay(50);  /* 闭环模式 (ZDT第二代: 0=开环 1=闭环, 手册5.6.7; 勿传老V5.0的2) */
    Emm_V5_CAN_Modify_Ctrl_Mode(MOTOR_TEST_ADDR_2, true, 1);   osDelay(50);

    /* 4. 两电机 snF=true 同步使能: 预存后广播 Synchronous_motion 同时触发,
     *    保证两电机同时上电, 避免单边先使能导致机构受力不均。 */
    Emm_V5_CAN_En_Control(MOTOR_TEST_ADDR_1, true, true);
    Emm_V5_CAN_En_Control(MOTOR_TEST_ADDR_2, true, true);
    Emm_V5_CAN_Synchronous_motion(0);  /* 广播地址0, 触发同步使能 */
    osDelay(100);

    /* 5. 打开帧日志: 每帧 CAN 都打印到 USART1, 供逻辑分析仪对照 */
    Emm_V5_CAN_SetFrameLog(true);
    printf("[motor_test] init done, addr=%d,%d, dist=%dcm, clk=%lu\r\n",
           MOTOR_TEST_ADDR_1, MOTOR_TEST_ADDR_2, (int)TEST_DISTANCE_CM, (unsigned long)TEST_CLK);

    /* 6. 创建测试任务 */
    const osThreadAttr_t task_attr = {
        .name = "MotorTestTask",
        .stack_size = MOTOR_TEST_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)MOTOR_TEST_TASK_PRIORITY,
    };
//    osThreadNew(MotorTestTask, NULL, &task_attr);
#endif  /* Y轴测试停用 */
}

/**
  * @brief  测试任务: 两电机多机同步, 位置模式前后反转, 每次 50cm, 方向交替
  * @note   流程: 两电机发完全相同的位置命令(snF=true 预存) ->
  *              广播 Synchronous_motion 同时启动 -> 等2s ->
  *              翻转方向重复。从电气上消除两电机起步时差, 走直线不跑偏。
  */
// static void MotorTestTask(void *argument)
// {
//     (void)argument;
//     osDelay(MOTOR_TEST_STARTUP_DELAY);

//     uint8_t dir = 0;  /* 0=CW(前), 1=CCW(后), 每次发命令后翻转 */
//     for (;;) {
//         printf("[motor_test] move addr=%d,%d dir=%d dist=%dcm clk=%lu (sync)\r\n",
//                MOTOR_TEST_ADDR_1, MOTOR_TEST_ADDR_2, dir,
//                (int)TEST_DISTANCE_CM, (unsigned long)TEST_CLK);

//         /* 两电机发完全相同的 dir/vel/acc/clk + snF=true 预存,
//          * 广播 Synchronous_motion 同时触发, 走直线不跑偏。 */
//         Emm_V5_CAN_Pos_Control(MOTOR_TEST_ADDR_1, dir, 20, 5, TEST_CLK, false, true);
//         Emm_V5_CAN_Pos_Control(MOTOR_TEST_ADDR_2, dir, 20, 5, TEST_CLK, false, true);
//         Emm_V5_CAN_Synchronous_motion(0);  /* 广播地址0, 触发所有预存电机同步启动 */

//         dir ^= 1;  /* 翻转方向: 前->后->前->后... */
//         osDelay(TEST_MOVE_DELAY_MS);
//     }
// }
