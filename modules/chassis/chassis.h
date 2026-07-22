/**
  ******************************************************************************
  * @file    chassis.h
  * @brief   底盘差速控制模块 - 双 Emm_V5 步进轮 (CAN2)
  ******************************************************************************
  * @attention
  * 两个步进轮挂 CAN2 (Emm_V5 驱动), 非大疆电机, 不经过 DJIMotorTask。
  * 命令直接经 Emm_V5_CAN_Vel_Control() 下发。
  *
  * 缓启动/缓停止 + S形加减速:
  *   ZDT 驱动器速度模式的 acc 参数是线性/梯形加减速, 非S形。
  *   本模块在固件侧用余弦平滑曲线 0.5*(1-cos(πt/T)) 逐步刷新下发转速,
  *   驱动器跟踪该命令即得到S形速度曲线(起步/停止加加速度平滑)。
  *   驱动器 acc 固定传0, 避免两套加减速叠加。
  *
  * 方向约定 (经实测确认): 左CW(dir=0) + 右CCW(dir=1) = 整车前进。
  ******************************************************************************
  */

#ifndef __CHASSIS_H
#define __CHASSIS_H

#include "main.h"
#include "cmsis_os2.h"
#include <stdbool.h>
#include "mech_params.h"   /* 底盘机械参数 (轮径/每转脉冲/周长派生) */

/* 电机地址: 1=左轮, 2=右轮 (与 app_init.c stepper_ids={1,2} 一致) */
#define CHASSIS_LEFT_ADDR    1
#define CHASSIS_RIGHT_ADDR   2

/* 任务参数 */
#define CHASSIS_TASK_PERIOD_MS    20    /* ramp 刷新周期 20ms (50Hz) */
#define CHASSIS_TASK_STACK_SIZE   256   /* 堆栈(word) */
#define CHASSIS_TASK_PRIORITY     osPriorityNormal   /* 24, 与 LiftTestTask 同级, 低于 LiftTask(32) */

/* 默认 ramp 时间(ms): 0→目标转速的余弦S形加速时间 */
#define CHASSIS_DEFAULT_RAMP_MS   1000

/* 转速上限(RPM), Emm_V5 速度模式范围 0~3000 */
#define CHASSIS_MAX_RPM           3000

/* 机械参数 (轮径/每转脉冲/周长) 统一在 mech_params.h 定义, 周长由 π×轮径 派生 */
/* 位置模式默认运行转速(RPM) 与 加速度档位(0~255, 0=直接启动) */
#define CHASSIS_POS_VEL_RPM       600
#define CHASSIS_POS_ACC           30
#define CHASSIS_POS_TIMEOUT_MS    5000  /* 位置模式到位超时(墙钟) */

/**
  * @brief  底盘初始化: 使能双轮电机 + 创建 ChassisTask
  * @note   需在 Emm_V5_CAN_Init() 与 HAL_CAN_Start(&hcan2) 之后调用
  * @retval 0: 成功, -1: 失败
  */
int Chassis_Init(void);

/**
  * @brief  设置底盘目标速度(带余弦S形缓加减速)
  * @param  target_rpm 目标转速(RPM, 带符号: +前进 -后退), 自动限幅到 ±CHASSIS_MAX_RPM
  * @param  ramp_ms    0→目标的加速时间(ms), 0 表示用默认 CHASSIS_DEFAULT_RAMP_MS。
  *                    换向/减速也用此时长从当前转速平滑过渡到新目标。
  */
void Chassis_SetVelocity(float target_rpm, uint32_t ramp_ms);

/**
  * @brief  底盘缓停止(按上次 ramp 时长 S 形减速到 0)
  */
void Chassis_Stop(void);

/**
  * @brief  底盘立即停止(越过S形, 直接发停转命令, 用于急停)
  */
void Chassis_StopNow(void);

/**
  * @brief  获取当前指令转速(RPM, 带符号)
  */
float Chassis_GetCurrentVelocity(void);

/**
  * @brief  获取目标转速(RPM, 带符号)
  */
float Chassis_GetTargetVelocity(void);

/**
  * @brief 注册底盘相关 VOFA 命令到 bsp/cmd 注册表
  * @note  在 Chassis_Init() 内自动调用。注册命令:
  *        速度模式: fwd <rpm> 前进, rev <rpm> 后退, cstop 缓停, estop 急停
  *        位置模式: mfwd <cm> 前进指定距离, mrev <cm> 后退指定距离,
  *                  mtest 自动前进30cm→后退30cm循环测试
 */
void Chassis_RegisterCommands(void);

/**
  * @brief  位置模式移动指定距离(阻塞直到到位或超时)
  * @param  distance_cm 距离(cm, 带符号: +前进 -后退)
  * @param  vel_rpm     运行转速(RPM), 0 用默认 CHASSIS_POS_VEL_RPM
  * @retval 0 到位, -1 超时/堵转/参数非法
  * @note   内部: 停速度模式 → 多机同步预存位置命令 → 广播同步启动
  *         → Chassis_WaitArrive 轮询 S_FLAG 到位/堵转标志。
  *         驱动器内置 acc 线性缓启(非S形); S形需固件分段位置ramp(未实现)。
  *         需在 ChassisTask 之外的任务上下文调用(阻塞)。
  */
int Chassis_MoveDistance(float distance_cm, uint16_t vel_rpm);

/**
  * @brief  位置模式到位等待: 轮询双轮 S_FLAG 直到均到位或超时/堵转
  * @retval 0 双轮均到位, -1 超时或堵转
  * @note   S_FLAG(0x3A): &0x02=到位 &0x04=堵转 &0x08=堵转保护。
  *         阻塞调用(任务上下文), 与 Read_Encoder 共享 rx_buff 不可并发。
  */
int Chassis_WaitArrive(void);

#endif /* __CHASSIS_H */
