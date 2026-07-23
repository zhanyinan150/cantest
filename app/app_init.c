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
#include "motor.h"       /* Motor_Init / Motor_XYZ / mxyz 命令 (XYZ起重机机构) */
#include "uart_callback.h"
#include "cmd_register.h"
#include "telemetry.h"
#include "bsp_log.h"   /* Log_PrintFloat1/2: MicroLIB 安全浮点打印 */

#include "stdio.h"

/* Emm_V5.h 与 Emm_V5_CAN.h 各自独立定义了 S_VER 等状态枚举且无相互 include
 * 保护, 不可在同一编译单元同时包含。此处只包含 Emm_V5_CAN.h, 不 include Emm_V5.h。 */

/* ---- 任务参数 ---- */
#define MOTOR_AUTO_TASK_STACK_SIZE 1024               /* 堆栈(word), Motor_XYZ + printf 链深 */
#define MOTOR_AUTO_TASK_PRIORITY   osPriorityNormal   /* 24, 低于 LiftTask(32)/DJIMotorTask(40), 不干扰闭环 */
#define MOTOR_AUTO_STARTUP_DELAY   2000               /* 上电后等待系统稳定(ms), 等电机使能+M2006反馈 */

/* ---- 内部任务函数 ---- */
static void MotorAutoTask(void *argument);

/**
  * @brief  应用层初始化: 子系统初始化 + 任务创建
  */
void App_Init(void)
{
  osDelay(500); /* Wait for USB enumeration */

  /* 日志系统初始化: 建日志队列 + LogTask(DMA 发 USART1)。upstream 队列版 fputc/LOG
   * 依赖此队列, 不调则 printf/LOG 全被 Log_EnqueueLine 丢弃(无打印)。
   * 须在任何 printf/LOG 之前(Lift_Init 里就有 printf)。 */
  BSPLogInit();

  /* ===== 升降系统(Z轴) 启用 =====
   * Lift_Init 注册 M2006 + 创建 LiftTask/DJIMotorTask, CAN1 启动接收反馈。 */
#if 1
  /* 升降系统初始化：注册 M2006 电机 (CANRegister 内部会配置 FIFO0 过滤器接收 0x201 反馈帧)。
   * 注意: bsp_can 的 CANServiceInit() 不会调用 HAL_CAN_Start(),
   * 因此需在 Lift_Init() 配置完过滤器之后手动启动 CAN1 并使能接收中断,
   * dji_motor 才能收到 M2006 的反馈报文。
   * ↑ 初始化保留, Z 轴 M2006 升降靠它。 */
  Lift_Init();

  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);
#endif

  /* 步进电机(Emm_V5)初始化: CAN2 扩展帧通信。
   * 注意: 不可再保留原 Emm_V5_CAN_Init/HAL_CAN_Start, 否则 CANRegister 重复
   *       注册同 ID 会 while(1) 卡死 (bsp_can.c::CANRegister 重复检测)。 */
#if 1  /* 步进初始化启用: Emm_V5_CAN_Init{1,2} + CAN2 启动 + Motor_Init 使能 X/Y */
  /* Y1/Y2 走 CAN2(扩展帧). X(3) 走 UART5(Emm_V5.c, huart5), 不在 CAN2 注册,
   * 故只向 bsp_can 注册 {1,2} 两个 CAN2 实例。X 的使能/位置命令由
   * Motor_Init / Motor_XYZ 经 Emm_V5_* (UART版) 下发, 不经过 bsp_can。 */
  uint8_t stepper_ids[2] = {1, 2};  /* Y1, Y2 (X=3 走 UART5, 不在此) */
  Emm_V5_CAN_Init(stepper_ids, 2);

  HAL_CAN_Start(&hcan2);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);

  /* XYZ起重机机构: Y双电机(1,2) CAN2 + X(3) UART5 + Z 由 lift(M2006) 升降。
   * Motor_Init: 使能三只步进(X 经 UART5, Y1/Y2 经 CAN2 同步)。
   * 底盘模块(chassis)已移除: 其地址 1,2 与 Y 轴冲突, 本机构不用底盘。 */
  Motor_Init();
#endif

  /* 创建上电自动执行任务: 不依赖串口命令, 上电后自动跑预设动作序列。
   * 修改 MotorAutoTask 内的动作即可改行为。 */
#if 1  /* 启用 MotorAutoTask: 上电自动调 Motor_XYZ 让 X/Y/Z 错峰运动 */
  const osThreadAttr_t motorAutoTask_attributes = {
    .name = "MotorAutoTask",
    .stack_size = MOTOR_AUTO_TASK_STACK_SIZE * 4,
    .priority = (osPriority_t)MOTOR_AUTO_TASK_PRIORITY,
  };
  osThreadNew(MotorAutoTask, NULL, &motorAutoTask_attributes);
#endif

  /* 启动 USART1 接收中断, 接收 VOFA+/MATLAB 下发的调参命令
   * (配合 bsp_printf.c VOFA_UART1_EXCLUSIVE=1, USART1专供JustFloat波形+调参命令)
   * 回调分发逻辑位于 bsp/uart/uart_callback.c。
   * UART5 接收回调(Emm_V5_UART_RxCpltCallback)已随 Emm_V5.c 重构移除,
   * 当前 UART5 测试只发送不接收, 不再注册 UART5 回调。 */
  UART_Callback_Init();
}

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
  * @param  z_distance  Z轴移动距离(cm), >0 有效, 0=不动
  * 方向，速度，加速度，距离，z轴仅为方向和距离
**/
/* ===== 上电自动执行任务 ===== */
/**
  * @brief  上电后执行一次预设动作 (不依赖串口命令), 执行完任务挂起不退出
  * @note   启动延时等电机使能 + M2006 反馈稳定后, 调用一次 Motor_XYZ。
  *         Motor_XYZ 阻塞(轮询到位/超时), 返回后任务进入空循环 osDelay 挂起,
  *         不再调用 Motor_XYZ, 也不让任务函数直接 return(FreeRTOS 任务不可直接 return)。
  *         修改各参数即可改动作。
  */
static void MotorAutoTask(void *argument)
{
  (void)argument;
  osDelay(MOTOR_AUTO_STARTUP_DELAY);  /* 等电机使能 + M2006 反馈稳定 */

  (void)Motor_XYZ(1,50, 20, 0,    /* X: dir=1(右), 50rpm, acc=20, 10cm */
                  1, 100, 20, 135.0f,    /* Y: dir=1(前), 100rpm, acc=20, 50cm (双电机同步) */
                  0, 35.0f);             /* Z: dir=0(上), 25cm */

 (void)Motor_XYZ(1,50, 20, 0,    /* X: dir=1(右), 50rpm, acc=20, 10cm */
                  1, 100, 20, 30.0f,    /* Y: dir=1(前), 100rpm, acc=20, 50cm (双电机同步) */
                  0, 35.0f);             /* Z: dir=0(上), 25cm */

  osDelay(1500);
  (void)Motor_XYZ(1, 50, 20, 0,      /* X: dir=1(右), 50rpm, acc=20, 10cm */
                  1, 100, 20, 40.0f, /* Y: dir=1(前), 100rpm, acc=20, 50cm (双电机同步) */
                  0, 0);             /* Z: dir=0(上), 25cm */

  osDelay(1500);
  (void)Motor_XYZ(1, 50, 30, 0,      /* X: dir=1(右), 50rpm, acc=20, 10cm */
                  1, 100, 20, 120.0f, /* Y: dir=1(前), 100rpm, acc=20, 50cm (双电机同步) */
                  0, 0);             /* Z: dir=0(上), 25cm */
osDelay(2000);  /* 任务挂起不退出: 空循环 osDelay 让出 CPU, 不再调用 Motor_XYZ。 */
  for (;;) {
    osDelay(1000);
  }
}