/**
  ******************************************************************************
  * @file    app_init.c
  * @brief   应用层: 系统初始化编排 + 任务体 (App层)
  ******************************************************************************
  * 从 freertos.c 抽离的应用逻辑:
  *   - App_Init(): 升降/步进/CAN/UART 子系统初始化编排 + 任务创建
  *   - VofaMonitorTask: VOFA+ JustFloat 波形监控 + 调参命令消费
  *   - MissionTask:     多子系统流程编排 (升降/底盘/横移协同, Event Group 并行)
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
#include "mission.h"
#include "laser.h"
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
#define VOFA_TASK_STACK_SIZE       384                /* 堆栈(word), sscanf较耗栈 */
#define VOFA_TASK_PRIORITY         osPriorityBelowNormal /* 16, 最低, 不干扰控制任务 */
#define VOFA_TASK_PERIOD           10                 /* 波形发送周期(ms), 100Hz */

/* ---- 内部任务函数 ---- */
static void CommandTask(void *argument);
static void TelemetryTask(void *argument);

/**
  * @brief  应用层初始化: 子系统初始化 + 任务创建
  */
void App_Init(void)
{
  osDelay(500); /* Wait for USB enumeration */

  /* ===== 子系统初始化顺序 (依赖链自上而下) =====
   * 1. Lift_Init      : M2006 电机注册 CAN1 + 创建 LiftTask/DJIMotorTask
   * 2. CAN1 启动       : Lift_Init 配好过滤器后才能 start + 使能接收中断
   * 3. Emm_V5_CAN_Init: 底盘双步进轮注册 CAN2
   * 4. CAN2 启动       : 同上
   * 5. Chassis_Init    : 使能双轮 + 创建 ChassisTask
   * 6. UART5 回调注册  : 横移 Emm_V5 的 DMA 接收回调入分发表
   * 7. Emm_V5_Init     : 创建 UART5 DMA 接收信号量 (首次 Read_Encoder 前必须)
   * 8. Lateral_Init    : 启动 UART5 DMA + 创建 LateralTask
   * 9. Laser_Init      : 启动 USART2/3 DMA + IDLE 帧同步 + 创建 LaserMonitorTask
   * 10. Mission_Init   : 创建 Event Group + 注册底盘/横移到位回调 + 创建主流程状态机任务
   * 11. CommandTask    : VOFA/CDC 命令消费任务 (含 "go" 触发主流程)
   * 顺序关键: 回调注册必须在各模块 Init 之后, 信号量/总线必须在首次使用前就绪,
   *           Laser_Init 必须在 Mission_Init 之前 (主流程首状态用激光定位)。 */

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

  /* 激光 ToF 测距(双传感器): USART2(前)+USART3(后), 230400bps, IDLE 帧同步 + DMA 接收。
   * Laser_Init 使能 IDLE 中断 + 启动 DMA, 创建阈值监测任务。
   * ⚠️ 调用后 servo.c 已迁至 UART4(PA0/PA1, 9600), USART3 完全归激光, 无冲突。
   * 解析后提供查询 API(Laser_GetNearestDistance 等) + 阈值触发回调, 供 mission 接入。
   * 需在 Mission_Init 之前 (mission 主流程首状态要用激光定位)。 */
  Laser_Init();

  /* 主流程状态机编排(开机自动跑): 激光定位→等CDC(go命令)→底盘+横移并行到点1
   * →舵机动作(固定延时)→底盘+横移并行到点2→循环。用 Event Group 做底盘+横移并行
   * 同步(各模块到位回调 SetBits, WaitBits(pdWAIT_ALL) 等两者到位), CDC 触发用任务通知
   * (xTaskNotifyGive/ulTaskNotifyTake, 零轮询)。需在各子系统 + Laser_Init 之后
   * (注册回调 + 激光定位依赖)。 */
  Mission_Init();

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

