/**
  ******************************************************************************
  * @file    motor.h
  * @brief   XYZ 三轴联动控制 (Modules层)
  ******************************************************************************
  * 机构配置 (与参考工程 crane_1 一致的 XYZ 起重机/分拣机构):
  *   X轴(左右): 单 Emm_V5 步进,       ID=3,   UART5 (Emm_V5.c, 不走 CAN)
  *   Y轴(前后): 双 Emm_V5 步进同步驱动, ID=1,2, CAN2 扩展帧
  *   Z轴(升降): M2006 大疆电机, CAN1 标准帧, 见 modules/lift/lift.c
  *
  * 移植自参考工程 motor.c::Motor_XYZ: Y 轴通信由 UART 改 CAN2, X 轴仍走 UART5,
  * 升降改用 lift 模块。X 与 Y 走不同总线, 故本文件对 X 所需的 Emm_V5_*(UART版)
  * 函数作前向声明而不 include Emm_V5.h —— 其枚举与 Emm_V5_CAN.h 冲突, 详见
  * motor.c 文件头。
  *
  * 机械参数(轮径/周长/减速比/每转脉冲)统一在 mech_params.h 集中配置,
  * 本文件只保留电气地址与运动参数(速度/加速度/超时等)。
  *
  * 依赖前提(须在 App_Init 中先完成):
  *   1. Emm_V5_CAN_Init 已注册 {1,2} 两个 CAN2 步进地址(Y1/Y2)。
  *      X(3) 走 UART5 不经 bsp_can, 不在此注册 —— 多注册会让 CANRegister
  *      重复检测 while(1) 卡死, 见 app_init.c 的同名说明。
  *   2. 三个步进已使能 (调用本模块 Motor_Init)
  *   3. Lift_Init 已完成 (Z轴 M2006 + LiftTask 运行)
  *   4. CAN1/CAN2 已 HAL_CAN_Start + 使能接收中断
  *
  * 注意: Y轴地址 1,2 与 chassis.c 底盘左右轮地址冲突, 二者不可同时启用。
  *       本机构为 XYZ 起重机, 应在 App_Init 中停用 Chassis_Init。
  ******************************************************************************
  */
#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"
#include "mech_params.h"   /* X/Y 机械参数: MOTOR_X/Y_WHEEL_DIAMETER_CM, CIRCUMFERENCE_CM, GEAR_RATIO, MOTOR_XY_PULSE_PER_REV */
#include <stdint.h>
#include <stdbool.h>

/* ---- 步进电机地址 (Emm_V5) ---- */
/* X轴: 单电机, 走 UART5 (Emm_V5.c), 不在 CAN2 上 */
#define MOTOR_X_ADDR            3
/* Y轴: 双电机同步(CAN2 扩展帧), 两电机发相同位置命令 + 同步启动, 避免扭轴 */
#define MOTOR_Y_ADDR_1          1
#define MOTOR_Y_ADDR_2          2

/* ---- 运动参数 (非机械量, 保留在本文件) ---- */
#define MOTOR_XY_VEL_MAX        3000   /* X/Y 转速上限(RPM), Emm_V5 范围 0~3000 */
#define MOTOR_XY_ACC_MAX        255    /* X/Y 加速度档位 0~255, 0=直接启动 */
#define MOTOR_Z_TOLERANCE_CM    2.0f   /* Z轴到位容差(cm), 与 lift 一致 */
#define MOTOR_XY_TIMEOUT_MS     10000  /* 到位超时(ms), X/Y 与 Z 取较大者 */
#define MOTOR_Z_TIMEOUT_MS      10000
#define MOTOR_POLL_PERIOD_MS    20     /* 到位轮询周期(ms) */

/**
  * @brief  使能 X/Y 轴步进电机 (Z 轴由 Lift_Init 内部使能)
  * @note   共三只步进: X(3) 经 UART5 直接发命令, Y1/Y2(1,2) 经 CAN2 同步使能。
  *         须在 Emm_V5_CAN_Init 注册 {1,2} 且 MX_UART5_Init 完成之后调用。
  *         内部会注册 VOFA 命令, 串口发 mxyz 即可触发 Motor_XYZ
  */
void Motor_Init(void);

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
  * @return 0=全轴到位(X&&Y1&&Y2&&Z), -1=超时/堵转
  * @note   - Y轴双电机(ID 1,2)固定 snF=true 多机同步走直线: 两电机发完全相同的
  *           dir/vel/acc/clk + snF=true 预存, 广播 Synchronous_motion 同时触发。
  *           X与Y一起动时加入同步(三轴联动), X单独动时直接发。
  *         - Z 由 Lift_Up/Down 设目标后 lift 内部 LiftTask(20ms)闭环跟进。
  *         - Y到位判定需 Y1 与 Y2 均到位(双电机都 ARRIVED)。
  *         - 距离->脉冲换算用 mech_params.h 的 MOTOR_X/Y_WHEEL_CIRCUMFERENCE_CM。
  *         - 单位 cm(参考工程为 mm)。
  */
void Motor_XYZ(uint8_t x_dir, uint16_t x_vel, uint8_t x_acc, float x_distance,
              uint8_t y_dir, uint16_t y_vel, uint8_t y_acc, float y_distance,
              uint8_t z_dir, float z_distance);

/**
  * @brief  注册 Motor VOFA 命令到 bsp/cmd 注册表
  * @note   在 Motor_Init() 内自动调用。注册命令:
  *         mxyz xdir xvel xacc xdist ydir yvel yacc ydist zdir zdist
  *         10个参数依次对应 Motor_XYZ, 某轴不动作传 0。例:
  *         mxyz 1 300 180 10 1 300 180 10 0 5
  *         (X右10cm + Y前10cm + Z升5cm, 三轴联动)
  *         执行期间 CommandTask 阻塞(与 chassis mfwd/mrev 一致)。
  */

#endif /* __MOTOR_H */
