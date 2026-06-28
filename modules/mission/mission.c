/**
  ******************************************************************************
  * @file    mission.c
  * @brief   多子系统流程编排 (Event Group 并行编排器)
  ******************************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "cmsis_os.h"
#include "mission.h"
#include "lift.h"
#include "chassis.h"
#include "lateral.h"
#include "stdio.h"

/* 全局 Event Group */
EventGroupHandle_t g_mission_events = NULL;

/* ===== 演示流程阶段表 (占位数值, 实测后改) ===== */
/* 阶段1: 升降到工作高度 (顺序) */
static const MissionAction ph1[] = {
    { ACT_LIFT_TO, 165.0f, EVT_LIFT_BIT },
};
/* 阶段2: 底盘前进 + 横移 (并行) */
static const MissionAction ph2[] = {
    { ACT_CHASSIS_MOVE, 30.0f, EVT_CHASSIS_BIT },
    { ACT_LATERAL_TO,    5.0f, EVT_LATERAL_BIT },
};
/* 阶段3: 下降到取物高度 (顺序) */
static const MissionAction ph3[] = {
    { ACT_LIFT_TO, 110.0f, EVT_LIFT_BIT },
};
/* 阶段4: 底盘后退 + 横移回零 (并行) */
static const MissionAction ph4[] = {
    { ACT_CHASSIS_MOVE, -30.0f, EVT_CHASSIS_BIT },
    { ACT_LATERAL_TO,     0.0f, EVT_LATERAL_BIT },
};
/* 阶段5: 升降回零 (顺序) */
static const MissionAction ph5[] = {
    { ACT_LIFT_TO, 0.0f, EVT_LIFT_BIT },
};

static const MissionPhase MISSION_PHASES[] = {
    { ph1, 1 },
    { ph2, 2 },
    { ph3, 1 },
    { ph4, 2 },
    { ph5, 1 },
};
#define MISSION_PHASE_COUNT  (sizeof(MISSION_PHASES) / sizeof(MISSION_PHASES[0]))

/* ===== 到位回调 (各模块注册, 反向解耦) ===== */

static void mission_lift_arrived(void)    { Mission_OnArrived(EVT_LIFT_BIT); }
static void mission_chassis_arrived(void) { Mission_OnArrived(EVT_CHASSIS_BIT); }
static void mission_lateral_arrived(void) { Mission_OnArrived(EVT_LATERAL_BIT); }

void Mission_OnArrived(uint32_t evt_bit)
{
    /* 在各模块任务上下文调用, SetBits 可用于 ISR/任务, 这里是任务上下文 */
    if (g_mission_events)
        xEventGroupSetBits(g_mission_events, evt_bit);
}

/* ===== 下发单个动作 (非阻塞) ===== */
static void mission_dispatch(const MissionAction *a)
{
    switch (a->type) {
    case ACT_LIFT_TO:
        printf("[mission] 升降→%.1fcm\r\n", (double)a->param);
        Lift_MoveTo(a->param);
        break;
    case ACT_CHASSIS_MOVE:
        printf("[mission] 底盘移动%.1fcm\r\n", (double)a->param);
        Chassis_MoveDistanceAsync(a->param, 0);
        break;
    case ACT_LATERAL_TO:
        printf("[mission] 横移→%.1fcm\r\n", (double)a->param);
        Lateral_MoveTo(a->param);
        break;
    default:
        break;
    }
}

/* ===== 编排任务 ===== */
void MissionTask(void *argument)
{
    (void)argument;
    osDelay(2000);  /* 等各子系统稳定 (电机使能/反馈就绪) */
    printf("[mission] 编排任务启动, %u阶段\r\n", (unsigned)MISSION_PHASE_COUNT);

    for (;;) {
        for (uint8_t p = 0; p < MISSION_PHASE_COUNT; p++) {
            const MissionPhase *ph = &MISSION_PHASES[p];
            printf("[mission] === 阶段%u (%u动作%s) ===\r\n",
                   (unsigned)(p + 1), (unsigned)ph->action_count,
                   ph->action_count > 1 ? ", 并行" : "");

            /* 1. 收集本阶段期望的 event bits */
            uint32_t expect = 0;
            for (uint8_t i = 0; i < ph->action_count; i++)
                expect |= ph->actions[i].evt_bit;

            /* 2. 清除本阶段 bits (避免上一轮残留), 下发所有动作(非阻塞) */
            xEventGroupClearBits(g_mission_events, expect);
            for (uint8_t i = 0; i < ph->action_count; i++)
                mission_dispatch(&ph->actions[i]);

            /* 3. 阻塞等待本阶段所有动作到位 (waitAll=pdTRUE), 超时保护 */
            EventBits_t bits = xEventGroupWaitBits(
                g_mission_events, expect,
                pdTRUE,        /* 退出时清除已等到的 bits */
                pdTRUE,        /* pdTRUE=全部到位才返回 → 并行同步点 */
                pdMS_TO_TICKS(MISSION_PHASE_TIMEOUT_MS));

            if ((bits & expect) == expect) {
                printf("[mission] 阶段%u 完成\r\n", (unsigned)(p + 1));
            } else {
                printf("[mission] 阶段%u 超时! bits=0x%lX expect=0x%lX\r\n",
                       (unsigned)(p + 1), (unsigned long)bits, (unsigned long)expect);
            }

            osDelay(MISSION_PHASE_HOLD_MS);  /* 阶段间停顿 */
        }
        printf("[mission] === 流程一轮完成, 循环 ===\r\n");
    }
}

/* ===== 初始化 ===== */
int Mission_Init(void)
{
    /* 1. 创建 Event Group */
    g_mission_events = xEventGroupCreate();
    if (g_mission_events == NULL) {
        printf("[mission] Event Group 创建失败!\r\n");
        return -1;
    }

    /* 2. 注册各模块到位回调 (反向解耦: 各模块不依赖 mission) */
    Lift_SetArrivedCallback(mission_lift_arrived);
    Chassis_SetArrivedCallback(mission_chassis_arrived);
    Lateral_SetArrivedCallback(mission_lateral_arrived);

    /* 3. 创建编排任务 (开机自动跑演示流程) */
    const osThreadAttr_t attr = {
        .name = "MissionTask",
        .stack_size = MISSION_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)MISSION_TASK_PRIORITY,
    };
    if (osThreadNew(MissionTask, NULL, &attr) == NULL) {
        printf("[mission] 任务创建失败!\r\n");
        return -1;
    }

    printf("[mission] 编排初始化完成 (Event Group + 3模块到位回调)\r\n");
    return 0;
}
