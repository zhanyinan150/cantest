/**
  ******************************************************************************
  * @file    mission.h
  * @brief   主流程状态机编排 (激光定位→等CDC→移动→舵机→移动)
  ******************************************************************************
  * @attention
  * 完整流程 (开机自动执行, 用 FreeRTOS Event Group + 任务通知):
  *   MS_LASER_LOCATE : 读前后激光 → 算当前纵向位置 → 定点1目标 → 转 WAIT_CDC
  *   MS_WAIT_CDC     : 阻塞等 CDC 触发命令(go) → 收到后转 MOVE_1
  *   MS_MOVE_1       : 底盘+横移并行移动到点1 → 都到位转 SERVO
  *   MS_SERVO        : 发舵机动作组命令 → 固定延时等动作完成 → 转 MOVE_2
  *   MS_MOVE_2       : 底盘+横移并行移动到点2 → 都到位转 LASER_LOCATE(循环)
  *
  * 并行同步: 底盘(MoveDistanceAsync)+横移(MoveTo)各自非阻塞下发, Event Group
  *   WaitBits(pdWAIT_ALL) 等两者都到位。各模块到位回调 SetBits (反向解耦)。
  * CDC 触发: CommandTask 收到 "go" 命令 → Mission_Trigger() → xTaskNotifyGive。
  *   MissionTask 在 WAIT_CDC 用 xTaskNotifyTake 阻塞等待 (零轮询)。
  * 舵机完成: Lobot 控制板对动作组命令不回发完成帧, 用固定延时 osDelay 估计。
  *
  * ⚠️ 占位项 (实测/确认后改 mission.c):
  *   - 预设点1/点2 的底盘纵向目标 + 横移目标
  *   - 激光定位算法 (当前占位: 后激光距离作为纵向位置)
  *   - 舵机动作组号 + 延时时长
  ******************************************************************************
  */
#ifndef __MISSION_H
#define __MISSION_H

#include "main.h"
#include "FreeRTOS.h"   /* event_groups.h 要求 FreeRTOS.h 在前 */
#include "event_groups.h"
#include "cmsis_os2.h"
#include "stdbool.h"

/* Event Group bits: 每个子系统到位对应一个 bit (并行同步用) */
#define EVT_LIFT_BIT     (1u << 0)
#define EVT_CHASSIS_BIT  (1u << 1)
#define EVT_LATERAL_BIT  (1u << 2)

/* 任务参数 */
#define MISSION_TASK_STACK_SIZE   512    /* word, printf 链深 */
#define MISSION_TASK_PRIORITY     osPriorityNormal  /* 24, 与测试任务同级, 低于控制任务 */

/* 流程状态
 * 线性主干: LASER_LOCATE → WAIT_CDC → MOVE_1 → SERVO → MOVE_2 → 循环
 * 分支1: LASER_LOCATE 检测到前方障碍 → AVOID → 回 LASER_LOCATE
 * 分支2: MOVE_1 移动超时 → 回 LASER_LOCATE 重来 (不进 SERVO) */
typedef enum {
    MS_LASER_LOCATE = 0,   /* 激光定位: 读激光算位置; 前方有障碍则转 AVOID */
    MS_AVOID,              /* 避让: 前方障碍, 横移退开后回定位 */
    MS_WAIT_CDC,           /* 等 CDC 触发命令 */
    MS_MOVE_1,             /* 底盘+横移并行到点1; 超时则回 LASER_LOCATE */
    MS_SERVO,              /* 舵机动作 + 固定延时 */
    MS_MOVE_2,             /* 底盘+横移并行到点2 */
} MissionState;

/* 预设点: 底盘纵向目标(cm, 绝对) + 横移目标(cm, 绝对)。
 * 底盘是相对移动, 编排时用 "目标 - 激光算出的当前位置" 转成相对量。 */
typedef struct {
    float chassis_target_cm;   /* 底盘纵向目标位置(cm) */
    float lateral_target_cm;   /* 横移目标位移(cm) */
} MissionPoint;

/* 全局 Event Group (各模块回调经 Mission_OnArrived 设置 bit) */
extern EventGroupHandle_t g_mission_events;

/**
 * @brief 编排初始化: 创建 Event Group + 注册各模块到位回调 + 创建 MissionTask
 * @note  需在 Lift/Chassis/Lateral/Laser_Init 之后调用
 * @retval 0 成功, -1 失败
 */
int Mission_Init(void);

/** @brief 编排任务 (开机自动跑主流程状态机) */
void MissionTask(void *argument);

/** @brief 各模块到位回调入口: 设置对应 event bit (供各模块注册) */
void Mission_OnArrived(uint32_t evt_bit);

/** @brief CDC 触发: CommandTask 收到 "go" 命令时调用, 唤醒 WAIT_CDC 状态 */
void Mission_Trigger(void);

#endif /* __MISSION_H */
