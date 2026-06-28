/**
  ******************************************************************************
  * @file    mission.h
  * @brief   多子系统流程编排模块 (升降/底盘/横移 协同)
  ******************************************************************************
  * @attention
  * 用 FreeRTOS Event Group 做并行编排:
  *   - 每个子系统到位对应一个 event bit
  *   - 各模块在自身任务里判到位 → 调已注册回调 → Mission_OnArrived(bit)
  *     → xEventGroupSetBits (反向解耦: 各模块不 include mission)
  *   - MissionTask 每阶段下发动作(非阻塞) → xEventGroupWaitBits(pdWAIT_ALL)
  *     阻塞等待本阶段所有动作到位 → 推进下一阶段。零轮询, 纯事件驱动。
  *
  * 演示流程(5阶段, 含2个并行段), 占位数值待实测后改 MISSION_PHASES:
  *   阶段1: 升降→165cm            (顺序)
  *   阶段2: 底盘前进30cm + 横移5cm (并行)
  *   阶段3: 升降→110cm            (顺序)
  *   阶段4: 底盘后退30cm + 横移→0 (并行)
  *   阶段5: 升降→0cm              (顺序)
  *   → 循环
  ******************************************************************************
  */
#ifndef __MISSION_H
#define __MISSION_H

#include "main.h"
#include "FreeRTOS.h"   /* event_groups.h 要求 FreeRTOS.h 在前 */
#include "event_groups.h"
#include "cmsis_os2.h"
#include "stdbool.h"

/* Event Group bits: 每个子系统到位对应一个 bit */
#define EVT_LIFT_BIT     (1u << 0)
#define EVT_CHASSIS_BIT  (1u << 1)
#define EVT_LATERAL_BIT  (1u << 2)

/* 任务参数 */
#define MISSION_TASK_STACK_SIZE   512    /* word, printf 链深 */
#define MISSION_TASK_PRIORITY     osPriorityNormal  /* 24, 与测试任务同级, 低于控制任务 */
#define MISSION_PHASE_HOLD_MS     500    /* 阶段间停顿(ms) */
#define MISSION_PHASE_TIMEOUT_MS  10000  /* 单阶段等待到位超时(ms) */

/* 动作类型 */
typedef enum {
    ACT_NONE = 0,
    ACT_LIFT_TO,        /* 升降到绝对位移(cm) */
    ACT_CHASSIS_MOVE,   /* 底盘相对移动(cm) */
    ACT_LATERAL_TO,     /* 横移到绝对位移(cm) */
} MissionActType;

/* 单个动作 */
typedef struct {
    MissionActType type;
    float param;         /* cm (升降绝对/底盘相对/横移绝对) */
    uint32_t evt_bit;    /* 该动作到位对应的 event bit */
} MissionAction;

/* 一个阶段: 一组并行动作 (阶段内所有动作到位才推进) */
typedef struct {
    const MissionAction *actions;  /* 动作数组 */
    uint8_t action_count;          /* 动作数 (1=顺序段, >1=并行段) */
} MissionPhase;

/* 全局 Event Group (各模块回调经 Mission_OnArrived 设置 bit) */
extern EventGroupHandle_t g_mission_events;

/**
 * @brief 编排初始化: 创建 Event Group + 注册各模块到位回调 + 创建 MissionTask
 * @note  需在 Lift_Init/Chassis_Init/Lateral_Init 之后调用 (注册回调依赖各模块)
 * @retval 0 成功, -1 失败
 */
int Mission_Init(void);

/** @brief 编排任务 (开机自动循环演示流程) */
void MissionTask(void *argument);

/** @brief 各模块到位回调入口: 设置对应 event bit (供各模块注册) */
void Mission_OnArrived(uint32_t evt_bit);

#endif /* __MISSION_H */
