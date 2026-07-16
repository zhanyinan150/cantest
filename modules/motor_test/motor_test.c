/**
  ******************************************************************************
  * @file    motor_test.c
  * @brief   CAN2 步进电机测试: 1号电机位置模式前后转, 每次 20cm
  ******************************************************************************
  * 电机 ID=1 (Y1), Emm_V5 步进闭环, CAN2 扩展帧。
  *
  * 流程:
  *   1. 上电初始化: 清堵转保护 -> 设闭环模式 -> 使能
  *   2. 位置模式前后反转: 每次 20cm, 方向交替, 间隔 2 秒
  *
  * 20cm -> 脉冲换算 (Y轴机械参数, 见 mech_params.h):
  *   clk = (距离/周长) × 每转脉冲 × 减速比
  *       = (20 / (π×10)) × 65536 × 1 ≈ 41721 脉冲
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
#define TEST_DISTANCE_CM             50.0f  /* 每次移动距离(cm) */
#define TEST_VEL_RPM                 300    /* 转速(RPM), Emm_V5 范围 0~5000 */
#define TEST_ACC                     180    /* 加速度档位 0~255, 0=直接启动 */
#define TEST_MOVE_DELAY_MS           2000   /* 两次位置命令间隔(ms)
                                             * 20cm@300rpm≈130ms, 留足余量 */

/* 20cm 换算为脉冲数 (Y轴: 周长π×10cm, 减速比1, 每转65536脉冲) */
#define TEST_CLK  ((uint32_t)((TEST_DISTANCE_CM / MOTOR_Y_WHEEL_CIRCUMFERENCE_CM) \
                              * MOTOR_XY_PULSE_PER_REV * MOTOR_Y_GEAR_RATIO))

/* ==================== 内部函数 ==================== */
static void MotorTestTask(void *argument);

/* ==================== 公开接口 ==================== */

void MotorTest_Init(void)
{
    /* 1. 注册 1号电机到 CAN2 (扩展帧) */
    uint8_t stepper_ids[1] = { MOTOR_TEST_ADDR };
    Emm_V5_CAN_Init(stepper_ids, 1);

    /* 2. 启动 CAN2 收发 + 接收中断 */
    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);

    printf("[motor_test] CAN2 state=%lu err=0x%lX\r\n",
           (unsigned long)HAL_CAN_GetState(&hcan2),
           (unsigned long)HAL_CAN_GetError(&hcan2));

    /* 3. 电机准备: 清堵转保护 -> 设闭环模式 -> 使能
     *    电机收到命令不转, 多数是堵转保护未清或模式不对 */
    Emm_V5_CAN_Reset_Clog_Pro(MOTOR_TEST_ADDR);               /* 清除堵转保护 */
    osDelay(50);
    Emm_V5_CAN_Modify_Ctrl_Mode(MOTOR_TEST_ADDR, true, 2);    /* 存储为闭环模式 */
    osDelay(50);
    Emm_V5_CAN_En_Control(MOTOR_TEST_ADDR, true, false);      /* 使能电机 */
    osDelay(100);

    /* 4. 打开帧日志: 每帧 CAN 都打印到 USART1, 供逻辑分析仪对照 */
    Emm_V5_CAN_SetFrameLog(true);
    printf("[motor_test] init done, addr=%d, dist=%dcm, clk=%lu\r\n",
           MOTOR_TEST_ADDR, (int)TEST_DISTANCE_CM, (unsigned long)TEST_CLK);

    /* 5. 创建测试任务 */
    const osThreadAttr_t task_attr = {
        .name = "MotorTestTask",
        .stack_size = MOTOR_TEST_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)MOTOR_TEST_TASK_PRIORITY,
    };
    osThreadNew(MotorTestTask, NULL, &task_attr);
}

/**
  * @brief  测试任务: 位置模式前后反转, 每次 20cm, 方向交替
  * @note   流程: 发位置命令(CW, 20cm) -> 等2s ->
  *              发位置命令(CCW, 20cm) -> 等2s -> 循环
  */
static void MotorTestTask(void *argument)
{
    (void)argument;
    osDelay(MOTOR_TEST_STARTUP_DELAY);

    uint8_t dir = 0;  /* 0=CW(前), 1=CCW(后), 每次发命令后翻转 */
    for (;;) {
        printf("[motor_test] move addr=1 dir=%d dist=50cm clk=104308\r\n", dir);

        /* raF=false 相对运动(从当前位置走50cm), snF=false 立即执行 */
        Emm_V5_CAN_Pos_Control(1, dir, 50, 10,104308, false, false);

        dir ^= 1;  /* 翻转方向: 前->后->前->后... */
        osDelay(TEST_MOVE_DELAY_MS);
    }
}
