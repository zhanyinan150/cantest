/**
  ******************************************************************************
  * @file    app_init.c
  * @brief   应用层: 系统初始化编排 (App层)
  ******************************************************************************
  * 当前为【日志测试模式】: 只验证 USART1 DMA 日志输出, 所有子系统禁用。
  *   - BSPLogInit 创建日志队列 + LogTask(DMA 发 USART1)
  *   - LogTestTask 每秒打印一行, 验证 printf/LOG 经队列输出到 PA9
  *   - 所有电机/CAN/横移/激光/主流程 Init 包在 #if 0, 测试通过后改 #if 1 恢复
  *
  * 依赖方向: App -> BSP(log) -> Core(usart)
  ******************************************************************************
  */

#include "app_init.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "main.h"

#include "can.h"
#include "usart.h"
#include "lift.h"
#include "Emm_V5_CAN.h"
#include "chassis.h"
#include "chassis_demo.h"
#include "lateral.h"
#include "mission.h"
#include "laser.h"
#include "uart_callback.h"
#include "bsp_log.h"   /* BSPLogInit/LOG 宏/Log_PrintFloat */

#include "stdio.h"
#include "math.h"
#include "stdbool.h"

/* Emm_V5.h 与 Emm_V5_CAN.h 各自独立定义了 S_VER 等状态枚举且无相互 include
 * 保护, 不可在同一编译单元同时包含。此处仅需注册步进电机的 UART 接收回调,
 * 故只包含 Emm_V5_CAN.h(拿 Emm_V5_CAN_Init), 对回调函数作前向声明。
 * (UART_HandleTypeDef 已由 can.h -> stm32f4xx_hal.h 引入) */
extern void Emm_V5_UART_RxCpltCallback(UART_HandleTypeDef *huart);
extern void Emm_V5_Init(void);  /* 创建 UART5 接收信号量 (定义在 Emm_V5.c, 与 Emm_V5_CAN.h 枚举冲突故不 include 其头) */

static void LogTestTask(void *argument);

/**
  * @brief  应用层初始化 (日志测试模式)
  */
void App_Init(void)
{
  osDelay(500); /* 开机延迟, 等外设就绪 */

  /* 日志系统初始化: 创建日志队列 + LogTask(DMA 发 USART1)。之后所有 printf/LOG
   * 经队列由 LogTask 集中输出, 避免多任务并发抢 USART1。需在首个 printf 之前调用。 */
  BSPLogInit();

  /* ===== 日志测试: 立即打印几行验证 printf/LOG 宏都走队列输出 ===== */
  printf("\r\n===== Log Test Start =====\r\n");
  printf("USART1 DMA log output test\r\n");
  LOGINFO("info log: count=%d", 1);
  LOGWARNING("warn log: count=%d", 2);
  LOGERROR("error log: code=0x%X", 0xAB);
  printf("===== if you see above on PA9, log system OK =====\r\n");

  /* 创建周期打印任务: 每秒一行, 持续验证日志输出 (含运行 tick) */
  const osThreadAttr_t logTestAttr = {
    .name = "LogTest",
    .stack_size = 256 * 4,
    .priority = osPriorityBelowNormal,
  };
  osThreadNew(LogTestTask, NULL, &logTestAttr);

  /* ===== 完整子系统初始化 (日志测试通过后改 #if 1 恢复) =====
   * 顺序: Lift/CAN1 -> Emm_V5/CAN2 -> Chassis -> ChassisDemo
   *       -> UART5回调 -> Emm_V5_Init/Lateral -> Laser -> Mission */
#if 0
  /* 升降系统初始化：注册 M2006 电机 (CANRegister 内部会配置 FIFO0 过滤器接收 0x201 反馈帧)。
   * 注意: bsp_can 的 CANServiceInit() 不会调用 HAL_CAN_Start(),
   * 因此需在 Lift_Init() 配置完过滤器之后手动启动 CAN1 并使能接收中断。 */
  Lift_Init();
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);

  /* 步进电机(Emm_V5)初始化: CAN2 扩展帧通信。 */
  uint8_t stepper_ids[2] = {1, 2};  /* 左/右 */
  Emm_V5_CAN_Init(stepper_ids, 2);
  HAL_CAN_Start(&hcan2);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);

  /* 底盘(双 Emm_V5 步进轮)初始化: 使能双轮 + 创建 ChassisTask。 */
  Chassis_Init();
  ChassisDemo_Init();

  /* 横移 UART5 回调注册 + Emm_V5_Init + Lateral_Init */
  UART_Callback_Register(UART5, Emm_V5_UART_RxCpltCallback);
  Emm_V5_Init();
  Lateral_Init();

  /* 激光 ToF 测距 */
  Laser_Init();

  /* 主流程状态机 (依赖 "go" 命令触发) */
  Mission_Init();
#endif
}

/**
  * @brief  日志测试任务: 每秒打印一行(含系统 tick), 持续验证日志输出
  */
static void LogTestTask(void *argument)
{
    (void)argument;
    osDelay(1000);  /* 等 BSPLogInit/LogTask 就绪 + 上面那批日志先发完 */
    uint32_t n = 0;
    for (;;) {
        printf("[logtest] #%lu tick=%lums\r\n",
               (unsigned long)(++n),
               (unsigned long)osKernelGetTickCount());
        osDelay(1000);
    }
}
