/**
 ******************************************************************************
 * @file    lift.h
 * @author  TKX Team (ported from 2025EPIQZJ, single-motor version)
 * @brief   升降系统控制模块头文件 - 单 M2006 电机版本
 ******************************************************************************
 * @attention
 *
 * 升降系统，使用 1 个 M2006 电机：
 * - 控制单位：位移（厘米），负值表示向下运动
 * - 机械参数：主动轮直径 18cm，36:1 减速比
 * - PID 参数移植自 2025EPIQZJ 工程 lift.c（位置环+速度环串级）
 *
 ******************************************************************************
 */

#ifndef __LIFT_H
#define __LIFT_H

#include "main.h"
#include "dji_motor.h"
#include "cmsis_os2.h"
#include "stdbool.h"
#include "mech_params.h"   /* 升降机械参数 (轮径/减速比/周长派生) */

/* 升降状态结构体 */
typedef struct {
    float current_displacement;  // 当前位移 (cm)
    float target_displacement;   // 目标位移 (cm)
    float speed;                // 当前速度 (cm/s) - 仅用于状态监控
    bool enabled;               // 系统使能状态
    bool is_moving;             // 是否正在运动
} Lift_Status_t;

/* 电机ID定义 */
#define LIFT_MOTOR_ID           1  // 单电机 ID = 1

/* 机械参数 (轮径/减速比/周长) 统一在 mech_params.h 定义, 周长由 π×轮径 派生 */

/* 运动参数定义 */
#define LIFT_DEFAULT_SPEED       10.0f   // 默认上升/下降速度 (cm/s)
#define LIFT_MAX_SPEED          20.0f   // 最大速度 (cm/s)
/* 位移上下限: 36:1 减速 + 多圈 total_angle 闭环, 实际行程远大于单圈。
 * 设 ±400cm, 匹配预设点位(最高360cm)。所有目标位置入口统一经此限幅。 */
#define LIFT_MAX_DISPLACEMENT   400.0f  // 最大向上位移 (cm)
#define LIFT_MIN_DISPLACEMENT  -400.0f  // 最大向下位移 (cm)
#define LIFT_TASK_PERIOD        20      // 任务周期 (ms)

/* 任务参数 */
#define LIFT_TASK_STACK_SIZE     512    // 任务堆栈大小 (word) - 加大防 printf/fputc 调用链栈溢出
/* 优先级: osPriorityAboveNormal(32)。高于 osPriorityNormal(24) 的测试/监控任务,
 * 低于 osPriorityHigh(40) 的 DJIMotorTask(PID控制核心)。
 * 注: CMSIS-RTOS V2 优先级数值越高=优先级越高: Low(8) < BelowNormal(16) < Normal(24) < AboveNormal(32) < High(40) */
#define LIFT_TASK_PRIORITY       osPriorityAboveNormal

/* 全局变量声明 */
extern DJIMotorInstance *lift_motor;
extern Lift_Status_t lift_status;
extern osThreadId_t liftTaskHandle;

/* 函数声明 */

/**
 * @brief 升降系统初始化
 * @retval 0: 成功, -1: 失败
 */
int Lift_Init(void);

/**
 * @brief 升降控制任务函数
 * @param argument 任务参数
 */
void LiftTask(void *argument);

/**
 * @brief 升降向上移动
 * @param displacement 目标位移 (cm)，相对于当前位置的增量
 * @retval 0: 成功, -1: 失败
 */
int Lift_Up(float displacement);

/**
 * @brief 升降向下移动
 * @param displacement 目标位移 (cm)，相对于当前位置的增量（正值）
 * @retval 0: 成功, -1: 失败
 */
int Lift_Down(float displacement);

/**
 * @brief 移动到指定位置
 * @param target_displacement 目标绝对位移 (cm)，负值表示向下位移
 * @retval 0: 成功, -1: 失败
 */
int Lift_MoveTo(float target_displacement);

/**
 * @brief 停止升降运动
 * @retval 0: 成功, -1: 失败
 */
int Lift_Stop(void);

/**
 * @brief 获取升降状态
 * @return 升降状态结构体指针
 */
Lift_Status_t* Lift_GetStatus(void);

/**
 * @brief 获取当前位移
 * @return 当前位移 (cm)，负值表示向下位移
 */
float Lift_GetCurrentDisplacement(void);

/**
 * @brief 阻塞等待升降到达目标位置(带超时)
 * @param timeout_ms 最大等待时间(ms), 0 表示使用默认超时
 * @retval true 已到位, false 超时未到位
 */
bool Lift_WaitUntilAtTarget(uint32_t timeout_ms);

/* ===== PID 调参 API =====
 * 封装对升降电机串级 PID 的读写, 避免外部直接访问 lift_motor->motor_controller
 * 内部结构, 实现"PID调参走模块API而非改内部变量"的解耦。 */

/** @brief PID 环选择 */
typedef enum {
    LIFT_LOOP_ANGLE = 0,  /* 角度环(外环) */
    LIFT_LOOP_SPEED = 1,  /* 速度环(内环) */
} Lift_Loop_t;

/**
 * @brief 设置指定环的 P/I/D 参数
 * @param loop  LIFT_LOOP_ANGLE / LIFT_LOOP_SPEED
 * @param Kp/Ki/Kd PID 参数 (传 NaN 或不需修改的项可用 Lift_GetPID 取原值)
 */
void Lift_SetPID(Lift_Loop_t loop, float Kp, float Ki, float Kd);

/**
 * @brief 读取指定环的 P/I/D 参数
 * @param loop  LIFT_LOOP_ANGLE / LIFT_LOOP_SPEED
 * @param Kp/Ki/Kd 输出指针, NULL 表示不取该项
 */
void Lift_GetPID(Lift_Loop_t loop, float *Kp, float *Ki, float *Kd);

/**
 * @brief 注册升降相关的 VOFA 命令到 bsp/cmd 注册表
 * @note  在 Lift_Init() 内自动调用。注册命令:
 *        up/dn/to <cm>  相对上升/下降/绝对移动
 *        stop           停止
 *        akp/aki/akd <v> 角度环 P/I/D
 *        skp/ski/skd <v> 速度环 P/I/D
 */
void Lift_RegisterCommands(void);

#endif /* __LIFT_H */
