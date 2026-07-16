/**
  ******************************************************************************
  * @file    app_init.c
  * @brief   应用层: 系统初始化编排 + 任务体 (App层)
  ******************************************************************************
  * 从 freertos.c 抽离的应用逻辑:
  *   - App_Init(): 升降/步进/CAN/UART 子系统初始化编排 + 任务创建
  *   - MotorAutoTask:  上电自动执行预设动作(不依赖串口命令)
  *
  * 依赖方向: App -> Modules(lift/motor/Emm_V5) -> BSP(uart/cmd) -> Core(can/usart)
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
#include "motor.h"       /* Motor_Init / Motor_XYZ / mxyz 命令 (XYZ起重机机构) */
#include "motor_test.h"  /* CAN2 步进独立测试 (逻辑分析仪调试用, 替代 Motor_Init) */
#include "uart_callback.h"
#include "cmd_register.h"
#include "telemetry.h"
#include "bsp_log.h"   /* Log_PrintFloat1/2: MicroLIB 安全浮点打印 */

#include "stdio.h"

/* Emm_V5.h 与 Emm_V5_CAN.h 各自独立定义了 S_VER 等状态枚举且无相互 include
 * 保护, 不可在同一编译单元同时包含。此处仅需注册步进电机的 UART 接收回调,
 * 故只包含 Emm_V5_CAN.h(拿 Emm_V5_CAN_Init), 对回调函数作前向声明。
 * (UART_HandleTypeDef 已由 can.h -> stm32f4xx_hal.h 引入) */
extern void Emm_V5_UART_RxCpltCallback(UART_HandleTypeDef *huart);

/* ---- 任务参数 ---- */
#define MOTOR_AUTO_TASK_STACK_SIZE 1024               /* 堆栈(word), Motor_XYZ + printf 链深 */
#define MOTOR_AUTO_TASK_PRIORITY   osPriorityNormal   /* 24, 低于 LiftTask(32)/DJIMotorTask(40), 不干扰闭环 */
#define MOTOR_AUTO_STARTUP_DELAY   2000               /* 上电后等待系统稳定(ms), 等电机使能+M2006反馈 */

#define VOFA_TASK_STACK_SIZE       768                /* 堆栈(word), mxyz命令 sscanf 12参数较耗栈, 原384不够 */
#define VOFA_TASK_PRIORITY         osPriorityBelowNormal /* 16, 最低, 不干扰控制任务 */
#define VOFA_TASK_PERIOD           10                 /* 波形发送周期(ms), 100Hz */

/* ---- 内部任务函数 ---- */
static void MotorAutoTask(void *argument);
//static void CommandTask(void *argument);
//static void TelemetryTask(void *argument);
/* LiftTestTask 已停用(Z 轴由 MotorAutoTask 控制), 声明注释掉 */
/* static void LiftTestTask(void *argument); */

/**
  * @brief  应用层初始化: 子系统初始化 + 任务创建
  */
void App_Init(void)
{
  osDelay(500); /* Wait for USB enumeration */


  /* 升降系统初始化：注册 M2006 电机 (CANRegister 内部会配置 FIFO0 过滤器接收 0x201 反馈帧)。
   * 注意: bsp_can 的 CANServiceInit() 不会调用 HAL_CAN_Start(),
   * 因此需在 Lift_Init() 配置完过滤器之后手动启动 CAN1 并使能接收中断,
   * dji_motor 才能收到 M2006 的反馈报文。
   * ↑ 初始化保留, Z 轴 M2006 升降靠它。 */
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
   * ===== 测试模式: 用 MotorTest_Init 替代原有步进初始化 =====
   * MotorTest_Init 内部完成: Emm_V5_CAN_Init{1,2,3} + HAL_CAN_Start + 使能 +
   *   创建 MotorTestTask(循环跑 4 个用例) + 打开帧发送日志(USART1 打印每帧)。
   * 用于逻辑分析仪对照 CAN2 帧内容。恢复正常业务时改回下方注释段即可。
   *
   * 注意: 不可再保留原 Emm_V5_CAN_Init/HAL_CAN_Start, 否则 CANRegister 重复
   *       注册同 ID 会 while(1) 卡死 (bsp_can.c::CANRegister 重复检测)。 */
  MotorTest_Init();
#if 0  /* 原有步进初始化(测试时停用) */
  uint8_t stepper_ids[3] = {1, 2, 3};  /* Y1, Y2, X */
  Emm_V5_CAN_Init(stepper_ids, 3);

  HAL_CAN_Start(&hcan2);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);

  /* XYZ起重机机构: Y双电机(1,2) + X(3) 步进, Z 由 lift(M2006) 升降。
   * Motor_Init: 使能三只步进 + 注册 mxyz 命令(串口发 mxyz 触发三轴联动)。
   * 底盘(Chassis_Init)与 Y 轴地址 1,2 冲突, 本机构不用底盘, 已停用。 */
  Motor_Init();
  /* Chassis_Init(); */  /* 停用: 底盘地址 1,2 与 Y 轴冲突 */
#endif

  /* 升降控制任务已在 Lift_Init() 内创建 */

  /* 创建上电自动执行任务: 不依赖串口命令, 上电后自动跑预设动作序列。
   * 修改 MotorAutoTask 内的动作即可改行为。 */
#if 0  /* 测试模式停用: MotorTestTask 已在 MotorTest_Init 内创建, 避免抢 CAN2 总线 */
  const osThreadAttr_t motorAutoTask_attributes = {
    .name = "MotorAutoTask",
    .stack_size = MOTOR_AUTO_TASK_STACK_SIZE * 4,
    .priority = (osPriority_t)MOTOR_AUTO_TASK_PRIORITY,
  };
  osThreadNew(MotorAutoTask, NULL, &motorAutoTask_attributes);
#endif

  /* LiftTestTask 已停用: 持续升降会与 motor 模块争抢 Z 轴控制权。
   *   Z 轴现由 MotorAutoTask 控制。需要时取消 #if 1 恢复测试。 */
#if 0
  const osThreadAttr_t liftTestTask_attributes = {
    .name = "LiftTestTask",
    .stack_size = 1024 * 4,
    .priority = osPriorityNormal,
  };
  osThreadNew(LiftTestTask, NULL, &liftTestTask_attributes);
#endif

  /* 启动 USART1 接收中断, 接收 VOFA+/MATLAB 下发的调参命令
   * (配合 bsp_printf.c VOFA_UART1_EXCLUSIVE=1, USART1专供JustFloat波形+调参命令)
   * 回调分发逻辑位于 bsp/uart/uart_callback.c, 此处注册业务串口并启动接收。
   * UART5(步进电机DMA反馈)由 Emm_V5 注册, 实现 BSP↔Modules 解耦。 */
  UART_Callback_Register(UART5, Emm_V5_UART_RxCpltCallback);
  UART_Callback_Init();

  /* CommandTask 已停用(上电自动动作不依赖串口命令)。需要串口手动触发 mxyz 时
   * 取消下方注释恢复 CommandTask。 */
//  const osThreadAttr_t cmdTask_attributes = {
//    .name = "CmdTask",
//    .stack_size = VOFA_TASK_STACK_SIZE * 4,
//    .priority = (osPriority_t)VOFA_TASK_PRIORITY,
//  };
//  osThreadNew(CommandTask, NULL, &cmdTask_attributes);

  /* 创建波形监控任务: 调试期临时禁用(与 printf 抢 USART1 致 HardFault)。 */
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
 * 数据流: USART1中断拼行 -> osMessageQueue -> CommandTask 阻塞取出
 *         -> CMD_Dispatch 查 bsp/cmd 注册表 -> 各模块自注册的 handler
 * 各模块在 Init 时 CMD_Register 注册自己的命令, app 层无需知道命令清单。
 */
/**
  * @brief  命令消费任务: 阻塞等待命令队列, 取出后查注册表分发
  *         (上电自动动作不经过此任务, 这里仅供串口手动触发 mxyz 等命令)
  *         已停用, 需要时取消注释恢复。
  */
// static void CommandTask(void *argument)
// {
//     (void)argument;
//     osDelay(2000); /* 等待 M2006 反馈稳定 */

//     osMessageQueueId_t q = UART_Callback_GetCmdQueue();
//     char line[VOFA_RX_BUF_SIZE];

//     for (;;) {
//         /* 队列句柄为空(创建失败, 已在 Init 断言)时兜底休眠, 避免忙等锁死 CPU */
//         if (q == NULL) { osDelay(1000); continue; }
//         if (osMessageQueueGet(q, line, NULL, osWaitForever) == osOK) {
//             CMD_Dispatch(line);  /* 查注册表分发, 未知命令返回 -1 静默丢弃 */
//         }
//     }
// }

/**
  * @brief  波形遥测任务: 周期采集各模块注册的波形通道, 拼成JustFloat帧发送
  *         各模块经 Telemetry_Register 注册 getter, 本任务统一采集, 不硬编码通道。
  *         lift 通道: 目标位移/当前位移/电机角速度/电机电流
  *         chassis 通道: 目标转速/当前转速
  *         已停用, 需要时取消注释恢复。
  */
//static void TelemetryTask(void *argument)
//{
//    (void)argument;
//    osDelay(2000); /* 等待M2006反馈稳定 */

//    TickType_t xLastWakeTime = xTaskGetTickCount();
//    for (;;) {
//        Telemetry_SampleAndSend();  /* 采集所有注册通道并发送 */
//        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(VOFA_TASK_PERIOD));
//    }
//}









/**
 * @brief  XYZ 三轴联动移动 (阻塞, 须在任务上下文调用, 非中断)
 * @param  x_dir       X轴方向, 透传 Emm_V5 (0=CW顺时针, 1=CCW逆时针), 实际左右按电机安装
 * @param  x_vel       X轴转速(RPM), 0~MOTOR_XY_VEL_MAX, 超出自动限幅
 * @param  x_acc       X轴加速度档位 0~255, 0=直接启动
 * @param  x_distance  X轴移动距离(cm), >0 有效, 0=该轴不动
 * @param  y_dir       Y轴方向, 透传 Emm_V5 (0=CW, 1=CCW), 实际前后按电机安装
 *                     (Y轴双电机同向, 若安装镜像需在 motor.c 内对其中一只翻转 dir)
 * @param  y_vel       Y轴转速(RPM), 0~MOTOR_XY_VEL_MAX, 超出自动限幅, 双电机共用
 * @param  y_acc       Y轴加速度档位 0~255, 0=直接启动, 双电机共用
 * @param  y_distance  Y轴移动距离(cm), >0 有效, 0=该轴不动, 双电机发相同脉冲
 * @param  z_dir       Z轴升降方向 (0=上, 1=下)
 * @param  z_vel       Z轴转速, 保留参数, lift 不支持(由其 PID 决定速度), 忽略
 * @param  z_acc       Z轴加速度, 保留参数, lift 不支持, 忽略
 * @param  z_distance  Z轴移动距离(cm), >0 有效, 0=不动
**/
/* ===== 上电自动执行任务 ===== */
/**
  * @brief  上电后自动执行预设动作序列 (不依赖串口命令)
  * @note   动作分两阶段:
  *           1. Z 轴先动 0.5 秒(Lift_Down 启动 -> osDelay(500) -> Lift_Stop 停住)
  *           2. 再让 X/Y 动(Motor_XYZ 只动 X/Y, Z 保持停住位置)
  *         修改各参数即可改动作。动作执行完后任务保活不退出。
  */
// static void MotorAutoTask(void *argument)
// {
//   (void)argument;
//   osDelay(MOTOR_AUTO_STARTUP_DELAY);  /* 等电机使能 + M2006 反馈稳定 */

//   printf("[motor_auto] 上电自动执行开始\r\n");

//   // /* ===== 1. Z 轴先动 0.5 秒 =====
//   //  * Lift_Down 非阻塞: 设目标后立即返回, Z 开始下移(LiftTask 持续 PID 跟进)。
//   //  * osDelay(500) 让 Z 动 0.5s (默认速度 ~5cm/s, 约下移 2.5cm)。
//   //  * Lift_Stop 把目标设为当前位移, Z 停在当前位置。 */
//   // Lift_Down(100.0f);   /* Z 下移(目标设大, 0.5s 走不完, 靠时间停) */
//   // osDelay(500);        /* Z 动 0.5 秒 */
//   // Lift_Stop();         /* Z 停在当前高度 */

//   /* ===== 2. 再让 X/Y 动 =====
//    * Motor_XYZ 只动 X/Y(Z distance=0), Z 保持上面停住的位置不动。
//    * Motor_XYZ 阻塞等待 X/Y 到位后返回。 */
//   int ret = Motor_XYZ(1,150, 180, 30.0f,    /* X: dir=1(右), 300rpm, acc=180, 10cm */
//                       1, 100, 20, 20.0f,    /* Y: dir=1(前), 300rpm, acc=180, 10cm (双电机同步) */
//                       1,   0,   0,  35.0f);   /* Z: 不动(保持上面停住的位置) */
//   if (ret == 0) printf("[motor_auto] 动作完成\r\n");
//   else          printf("[motor_auto] 动作失败(超时/堵转)\r\n");

//   for (;;) osDelay(1000);  /* 跑完保活, 不退出 */
// }




















/* ===== 升降上升/下降测试任务 (已停用) =====
 * Z 轴现由 MotorAutoTask 控制, 此测试任务不再使用。
 * 需要单独测升降时: 取消上方 LiftTestTask 创建的 #if 0, 并恢复本函数体。 */
#if 0
static void LiftTestTask(void *argument)
{
  (void)argument;
  osDelay(2000);

  printf("[lift_test] 升降测试任务启动, 单次位移 100cm\r\n");
  for (;;)
  {
    Lift_Up(100.0f);
    Lift_WaitUntilAtTarget(8000);
    osDelay(1000);
    Lift_Down(100.0f);
    Lift_WaitUntilAtTarget(8000);
    osDelay(1000);
  }
}
#endif
