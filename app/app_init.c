/**
  ******************************************************************************
  * @file    app_init.c
  * @brief   应用层: 系统初始化编排 + 任务体 (App层)
  ******************************************************************************
  * 从 freertos.c 抽离的应用逻辑:
  *   - App_Init(): 升降/步进/CAN/UART 子系统初始化编排 + 任务创建
  *   - VofaMonitorTask: VOFA+ JustFloat 波形监控 + 调参命令消费
  *   - LiftTestTask:    升降上升/下降闭环测试
  *
  * 依赖方向: App → Modules(lift/Emm_V5) → BSP(uart/vofa) → Core(can/usart)
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
#include "lateral.h"
#include "uart_callback.h"
#include "cmd_register.h"
#include "telemetry.h"
#include "bsp_log.h"   /* Log_PrintFloat1/2: MicroLIB 安全浮点打印 */

#include "stdio.h"
#include "math.h"
#include "stdbool.h"

/* Emm_V5.h 与 Emm_V5_CAN.h 各自独立定义了 S_VER 等状态枚举且无相互 include
 * 保护, 不可在同一编译单元同时包含。此处仅需注册步进电机的 UART 接收回调,
 * 故只包含 Emm_V5_CAN.h(拿 Emm_V5_CAN_Init), 对回调函数作前向声明。
 * (UART_HandleTypeDef 已由 can.h → stm32f4xx_hal.h 引入) */
extern void Emm_V5_UART_RxCpltCallback(UART_HandleTypeDef *huart);
extern void Emm_V5_Init(void);  /* 创建 UART5 接收信号量 (定义在 Emm_V5.c, 与 Emm_V5_CAN.h 枚举冲突故不 include 其头) */

/* ---- 任务参数 ---- */
#define LIFT_TEST_TASK_STACK_SIZE  1024               /* 堆栈(word), printf→fputc→HAL_UART_Transmit 调用链深, 加大防栈溢出 */
#define LIFT_TEST_TASK_PRIORITY    osPriorityNormal   /* 24, 低于LiftTask(32), 不干扰闭环 */
#define LIFT_TEST_DISTANCE         100.0f             /* 单次上升/下降位移(cm), 可调 */
#define LIFT_TEST_HOLD_MS          1000               /* 到位后停留时间(ms) */
#define LIFT_TEST_WAIT_TIMEOUT     8000               /* 到位等待超时(ms) */

#define VOFA_TASK_STACK_SIZE       384                /* 堆栈(word), sscanf较耗栈 */
#define VOFA_TASK_PRIORITY         osPriorityBelowNormal /* 16, 最低, 不干扰控制任务 */
#define VOFA_TASK_PERIOD           10                 /* 波形发送周期(ms), 100Hz */

/* ---- 内部任务函数 ---- */
static void CommandTask(void *argument);
static void TelemetryTask(void *argument);
static void LiftTestTask(void *argument);

/**
  * @brief  应用层初始化: 子系统初始化 + 任务创建
  */
void App_Init(void)
{
  osDelay(500); /* Wait for USB enumeration */


  /* 升降系统初始化：注册 M2006 电机 (CANRegister 内部会配置 FIFO0 过滤器接收 0x201 反馈帧)。
   * 注意: bsp_can 的 CANServiceInit() 不会调用 HAL_CAN_Start(),
   * 因此需在 Lift_Init() 配置完过滤器之后手动启动 CAN1 并使能接收中断,
   * dji_motor 才能收到 M2006 的反馈报文。 */
  Lift_Init();

  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);

  /* CAN1 诊断: 打印启动状态、错误计数、电机CAN实例收发统计 */
  {
    extern DJIMotorInstance *lift_motor;
    CAN_HandleTypeDef *h = &hcan1;
    uint32_t canState = HAL_CAN_GetState(h);
    uint32_t canErr    = HAL_CAN_GetError(h);
    uint32_t txFree    = HAL_CAN_GetTxMailboxesFreeLevel(h);
    printf("[can1] state=%lu err=0x%lX txFree=%lu\r\n",
           (unsigned long)canState, (unsigned long)canErr, (unsigned long)txFree);
    if (lift_motor && lift_motor->motor_can_instance) {
      printf("[can1] motor rx_id=0x%lX rx_counter=%lu\r\n",
             (unsigned long)lift_motor->motor_can_instance->rx_id,
             (unsigned long)lift_motor->motor_can_instance->rx_counter);
    }
  }

  /* 步进电机(Emm_V5)初始化: CAN2 扩展帧通信。
   * CANRegister 内部会配置 CAN2 的扩展帧过滤器(bank 14+)接收各电机反馈。
   * 注册前需确保 MX_CAN2_Init() 已在 main() 中执行, 此处再启动 CAN2 收发。 */
  uint8_t stepper_ids[2] = {1, 2};  /* 左/右 */
  Emm_V5_CAN_Init(stepper_ids, 2);

  HAL_CAN_Start(&hcan2);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);

  /* 底盘(双 Emm_V5 步进轮)初始化: 使能双轮 + 创建 ChassisTask(余弦S形ramp)。
   * 需在 Emm_V5_CAN_Init 与 CAN2 启动之后调用。底盘命令经 VOFA 下发:
   *   fwd <rpm> = 前进, rev <rpm> = 后退, cstop = 缓停。 */
  Chassis_Init();

  /* 升降控制任务已在 Lift_Init() 内创建 */
  /* 创建升降上升/下降测试任务: 调用 Lift_Up/Down 验证 M2006 升降系统 */
  const osThreadAttr_t liftTestTask_attributes = {
    .name = "LiftTestTask",
    .stack_size = LIFT_TEST_TASK_STACK_SIZE * 4,
    .priority = (osPriority_t)LIFT_TEST_TASK_PRIORITY,
  };
  osThreadNew(LiftTestTask, NULL, &liftTestTask_attributes);

  /* 启动 USART1 接收中断, 接收 VOFA+/MATLAB 下发的调参命令
   * (配合 bsp_printf.c VOFA_UART1_EXCLUSIVE=1, USART1专供JustFloat波形+调参命令)
   * 回调分发逻辑位于 bsp/uart/uart_callback.c, 此处注册业务串口并启动接收。
   * UART5(步进电机DMA反馈)由 Emm_V5 注册, 实现 BSP↔Modules 解耦。 */
  UART_Callback_Register(UART5, Emm_V5_UART_RxCpltCallback);
  UART_Callback_Init();

  /* 横移系统(Emm_V5 步进, UART5)初始化: Emm_V5_Init 创建 UART5 DMA 接收信号量
   * (必须在首次 Read_Encoder 之前, 否则回调因 sem==NULL 失效); Lateral_Init 启动
   * LateralTask(任务首帧使能电机, 独占 UART5 总线, VOFA 命令仅设意图标志避免并发)。
   * 需在 UART_Callback_Register(UART5,...) 之后, 确保接收回调已入分发表。 */
  Emm_V5_Init();
  Lateral_Init();

  /* 创建命令消费任务: 阻塞等待 UART 命令队列, 分发到各模块。
   * 优先级 BelowNormal(16), 低频人工输入, 不干扰控制任务。 */
  const osThreadAttr_t cmdTask_attributes = {
    .name = "CmdTask",
    .stack_size = VOFA_TASK_STACK_SIZE * 4,   /* sscanf 较耗栈 */
    .priority = (osPriority_t)VOFA_TASK_PRIORITY,
  };
  osThreadNew(CommandTask, NULL, &cmdTask_attributes);

  /* 创建波形监控任务: 周期输出 JustFloat 波形 (第4步独立为可注册通道的 TelemetryTask)
   * 调试期临时禁用: TelemetryTask 用 HAL_UART_Transmit_DMA 发 USART1,
   * 与 printf(阻塞 HAL_UART_Transmit) 并发抢 USART1, 状态机错乱致 HardFault
   * (崩点 UART_WaitOnFlagUntilTimeout 读 USART1->SR)。先关掉让日志干净, 排查升降。 */
#if 0
  const osThreadAttr_t teleTask_attributes = {
    .name = "TeleTask",
    .stack_size = 256 * 4,
    .priority = (osPriority_t)VOFA_TASK_PRIORITY,
  };
  osThreadNew(TelemetryTask, NULL, &teleTask_attributes);
#endif
}

/* ===== VOFA+ 通信概览 =====
 * 上行: JustFloat 二进制波形, 各模块经 Telemetry_Register 注册通道,
 *       TelemetryTask 周期拼接发送(lift: 位移/速度/电流; chassis: 转速)。
 * 下行: 文本命令 "<cmd> <value>\n", 各模块经 CMD_Register 自注册,
 *       CommandTask 查表分发。命令清单见各模块 RegisterCommands。
 */

/* ===== 命令处理 (队列驱动 + 注册表分发) =====
 * 数据流: USART1中断拼行 → osMessageQueue → CommandTask 阻塞取出
 *         → CMD_Dispatch 查 bsp/cmd 注册表 → 各模块自注册的 handler
 * 各模块在 Init 时 CMD_Register 注册自己的命令, app 层无需知道命令清单。
 */
/**
  * @brief  命令消费任务: 阻塞等待命令队列, 取出后查注册表分发
  */
static void CommandTask(void *argument)
{
    (void)argument;
    osDelay(2000); /* 等待 M2006 反馈稳定 */

    osMessageQueueId_t q = UART_Callback_GetCmdQueue();
    char line[VOFA_RX_BUF_SIZE];

    for (;;) {
        /* 队列句柄为空(创建失败, 已在 Init 断言)时兜底休眠, 避免忙等锁死 CPU */
        if (q == NULL) { osDelay(1000); continue; }
        if (osMessageQueueGet(q, line, NULL, osWaitForever) == osOK) {
            CMD_Dispatch(line);  /* 查注册表分发, 未知命令返回 -1 静默丢弃 */
        }
    }
}

/**
  * @brief  波形遥测任务: 周期采集各模块注册的波形通道, 拼成JustFloat帧发送
  *         各模块经 Telemetry_Register 注册 getter, 本任务统一采集, 不硬编码通道。
  *         lift 通道: 目标位移/当前位移/电机角速度/电机电流
  *         chassis 通道: 目标转速/当前转速
  */
static void TelemetryTask(void *argument)
{
    (void)argument;
    osDelay(2000); /* 等待M2006反馈稳定 */

    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        Telemetry_SampleAndSend();  /* 采集所有注册通道并发送 */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(VOFA_TASK_PERIOD));
    }
}

/* ===== 升降上升/下降测试任务 ===== */
/**
  * @brief  升降上升/下降测试任务
  * @note   循环执行: 上升 LIFT_TEST_DISTANCE cm → 等待到位 → 停留
  *         → 下降 LIFT_TEST_DISTANCE cm → 等待到位 → 停留。
  *         底层闭环由 LiftTask(20ms) + DJIMotorTask(10ms) 完成。
  */
static void LiftTestTask(void *argument)
{
  (void)argument;
  osDelay(2000); /* 等待 M2006 反馈稳定 */

  printf("[lift_test] 升降测试任务启动, 单次位移 ");
  Log_PrintFloat1("", LIFT_TEST_DISTANCE);
  printf("cm\r\n");
  /* 打印电机反馈, 确认CAN通信正常 (ecd应在0-8191, C610不报温度故temp=0正常) */
  if (lift_motor != NULL) {
    printf("[lift_test] M2006反馈: ecd=%u speed=", (unsigned)lift_motor->measure.ecd);
    Log_PrintFloat1("", lift_motor->measure.speed_aps);
    printf("deg/s cur=%d temp=%dC(C610不报温度)\r\n",
           lift_motor->measure.real_current, lift_motor->measure.temperature);
    if (lift_motor->motor_can_instance) {
      printf("[lift_test] CAN1 rx_counter=%lu (0=从未收到反馈)\r\n",
             (unsigned long)lift_motor->motor_can_instance->rx_counter);
    }
  } else {
    printf("[lift_test] 警告: lift_motor未初始化!\r\n");
  }

  for (;;)
  {
    /* 上升 */
    printf("[lift_test] 上升 ");
    Log_PrintFloat1("", LIFT_TEST_DISTANCE);
    printf("cm\r\n");
    Lift_Up(LIFT_TEST_DISTANCE);

    /* 等待到位, 期间每100ms输出调试数据 */
    {
      uint32_t waited = 0;
      bool arrived = false;
      while (waited < LIFT_TEST_WAIT_TIMEOUT) {
        float err = fabsf(lift_status.current_displacement - lift_status.target_displacement);
        if (err <= 2.0f) { arrived = true; break; }
        /* 每100ms打印: 目标位移 / 当前位移 / 误差 / 角速度 / 电流 / PID输出 */
        printf("[lift] tgt=");
        Log_PrintFloat2("", lift_status.target_displacement);
        printf(" cur=");
        Log_PrintFloat2("", lift_status.current_displacement);
        printf(" err=");
        Log_PrintFloat2("", err);
        if (lift_motor) {
          printf(" spd=");
          Log_PrintFloat1("", lift_motor->measure.speed_aps);
          printf(" cur=%d pid_ref=", lift_motor->measure.real_current);
          Log_PrintFloat1("", lift_motor->motor_controller.pid_ref);
        }
        printf("\r\n");
        osDelay(100);
        waited += 100;
      }
      if (arrived) {
        printf("[lift_test] 上升到位 位移=");
        Log_PrintFloat2("", Lift_GetCurrentDisplacement());
        printf("cm 速度=");
        Log_PrintFloat1("", lift_motor ? lift_motor->measure.speed_aps : 0.0f);
        printf("deg/s 电流=%d\r\n", lift_motor ? lift_motor->measure.real_current : 0);
      } else {
        printf("[lift_test] 上升超时! 位移=");
        Log_PrintFloat2("", Lift_GetCurrentDisplacement());
        printf("cm 速度=");
        Log_PrintFloat1("", lift_motor ? lift_motor->measure.speed_aps : 0.0f);
        printf("deg/s 电流=%d\r\n", lift_motor ? lift_motor->measure.real_current : 0);
      }
    }
    osDelay(LIFT_TEST_HOLD_MS);

    /* 下降 */
    printf("[lift_test] 下降 ");
    Log_PrintFloat1("", LIFT_TEST_DISTANCE);
    printf("cm\r\n");
    Lift_Down(LIFT_TEST_DISTANCE);

    /* 等待到位, 期间每100ms输出调试数据 */
    {
      uint32_t waited = 0;
      bool arrived = false;
      while (waited < LIFT_TEST_WAIT_TIMEOUT) {
        float err = fabsf(lift_status.current_displacement - lift_status.target_displacement);
        if (err <= 2.0f) { arrived = true; break; }
        printf("[lift] tgt=");
        Log_PrintFloat2("", lift_status.target_displacement);
        printf(" cur=");
        Log_PrintFloat2("", lift_status.current_displacement);
        printf(" err=");
        Log_PrintFloat2("", err);
        if (lift_motor) {
          printf(" spd=");
          Log_PrintFloat1("", lift_motor->measure.speed_aps);
          printf(" cur=%d pid_ref=", lift_motor->measure.real_current);
          Log_PrintFloat1("", lift_motor->motor_controller.pid_ref);
        }
        printf("\r\n");
        osDelay(100);
        waited += 100;
      }
      if (arrived) {
        printf("[lift_test] 下降到位 位移=");
        Log_PrintFloat2("", Lift_GetCurrentDisplacement());
        printf("cm 速度=");
        Log_PrintFloat1("", lift_motor ? lift_motor->measure.speed_aps : 0.0f);
        printf("deg/s 电流=%d\r\n", lift_motor ? lift_motor->measure.real_current : 0);
      } else {
        printf("[lift_test] 下降超时! 位移=");
        Log_PrintFloat2("", Lift_GetCurrentDisplacement());
        printf("cm 速度=");
        Log_PrintFloat1("", lift_motor ? lift_motor->measure.speed_aps : 0.0f);
        printf("deg/s 电流=%d\r\n", lift_motor ? lift_motor->measure.real_current : 0);
      }
    }
    osDelay(LIFT_TEST_HOLD_MS);
  }
}
