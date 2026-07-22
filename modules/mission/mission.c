/**
  ******************************************************************************
  * @file    mission.c
  * @brief   主流程状态机编排 (激光定位→等CDC→移动→舵机→移动)
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
#include "laser.h"
#include "servo.h"
#include "cmd_register.h"
#include "stdio.h"

/* ===== VOFA/CDC 命令 =====
 * go: 触发主流程从 WAIT_CDC 进入移动 (CDC 下发 "go" 即可启动) */
static void cmd_go(const char *arg)
{
    (void)arg;
    Mission_Trigger();
}

/* 全局 Event Group */
EventGroupHandle_t g_mission_events = NULL;

/* MissionTask 句柄 (供 Mission_Trigger 发任务通知) */
static osThreadId_t s_mission_task = NULL;

/* ===== 预设点 (占位, 实测后改) =====
 * 点1: 流程第一次移动的目标 (激光定位后, 底盘走到 chassis_target, 横移走到 lateral_target)
 * 点2: 舵机动作完成后, 第二次移动的目标 */
static const MissionPoint POINT_1 = {
    .chassis_target_cm = 50.0f,    /* 底盘纵向目标位置(cm) 占位 */
    .lateral_target_cm = 5.0f,     /* 横移目标位移(cm) 占位 */
};
static const MissionPoint POINT_2 = {
    .chassis_target_cm = 0.0f,     /* 回起点附近 占位 */
    .lateral_target_cm = 0.0f,     /* 横移回零 占位 */
};

/* 舵机动作参数 (占位): 动作组号 + 固定延时(ms)估计完成 */
#define SERVO_ACTION_GROUP    1u
#define SERVO_ACTION_TIMES    1u
#define SERVO_ACTION_DELAY_MS 2000u

/* 避让参数 (占位): 前方障碍距离阈值 + 避让横移量 */
#define OBSTACLE_THRESH_MM    200u    /* 前激光 < 此值判定为前方有障碍 */
#define AVOID_LATERAL_CM      10.0f   /* 避让: 横移退开 10cm */

/* 激光定位参数 (占位, 实测后改):
 * - FIELD_LENGTH_MM: 场地总长(前后墙间距), 用于前后激光交叉验证
 * - LOCATE_TOL_MM:   前后激光之和与总长的容差, 超过则视为有干扰/无效
 * 布局: 前激光朝前墙, 后激光朝后墙, 理论上 d_front + d_back = FIELD_LENGTH_MM。
 * 当前位置(距后墙) = d_back, 也可 = FIELD_LENGTH_MM - d_front, 两者应一致。 */
#define FIELD_LENGTH_MM       2000u
#define LOCATE_TOL_MM         100u

/* 激光定位: 用前后两激光确定纵向位置。
 * 原理: 前激光测到前墙 d_front, 后激光测到后墙 d_back, 场地总长 L。
 *   - 理论 d_front + d_back = L, 据此交叉验证数据可信度。
 *   - 位置(距后墙, mm) = d_back; 也可 = L - d_front, 取两者平均更稳。
 *   - 若和与 L 偏差超容差 → 数据异常, 降级用单侧(优先后激光), 并告警。
 * 通过 front_mm 输出前激光距离(供障碍判断), 返回纵向位置(cm)。 */
static float mission_laser_locate(uint16_t *front_mm)
{
    uint16_t d_front = Laser_GetNearestDistance(LASER_SIDE_FRONT);
    uint16_t d_back  = Laser_GetNearestDistance(LASER_SIDE_BACK);
    bool f_valid = (d_front != 0xFFFFu);
    bool b_valid = (d_back  != 0xFFFFu);
    printf("[mission] 激光: 前=%s%umm 后=%s%umm\r\n",
           f_valid ? "" : "无效(", f_valid ? (unsigned)d_front : 0u,
           b_valid ? "" : "无效(", b_valid ? (unsigned)d_back  : 0u);
    if (front_mm) *front_mm = d_front;

    /* 两激光都无效: 无法定位, 返回0并告警 */
    if (!f_valid && !b_valid) {
        printf("[mission] ⚠ 前后激光均无效, 定位失败\r\n");
        return 0.0f;
    }

    /* 只有一侧有效: 降级单侧定位 */
    if (!f_valid) {
        printf("[mission] 前激光无效, 用后激光单侧定位\r\n");
        return (float)d_back / 10.0f;
    }
    if (!b_valid) {
        printf("[mission] 后激光无效, 用前激光单侧定位\r\n");
        return (float)(FIELD_LENGTH_MM - d_front) / 10.0f;
    }

    /* 两侧都有效: 交叉验证。和与总长偏差 = 数据可信度。 */
    int32_t sum = (int32_t)d_front + (int32_t)d_back;
    int32_t diff = sum - (int32_t)FIELD_LENGTH_MM;  /* 正: 比总长大, 负: 比总长小 */
    if (diff < 0) diff = -diff;

    if (diff > (int32_t)LOCATE_TOL_MM) {
        /* 偏差超容差: 数据矛盾(可能前方有障碍物遮挡, 或激光误读)。
         * 优先信任后激光定位置(后墙通常无障碍), 但告警。 */
        printf("[mission] ⚠ 前后激光矛盾(和=%ldmm vs 总长%u, 偏差%ld), 用后激光\r\n",
               (long)sum, (unsigned)FIELD_LENGTH_MM, (long)diff);
        return (float)d_back / 10.0f;
    }

    /* 数据一致: 取两侧算出的位置平均值, 抵消单侧测量误差。 */
    float pos_by_back  = (float)d_back / 10.0f;                       /* 距后墙 */
    float pos_by_front = (float)(FIELD_LENGTH_MM - d_front) / 10.0f;  /* = 总长-距前墙 */
    float pos = (pos_by_back + pos_by_front) * 0.5f;
    printf("[mission] 定位: 后墙法=%.1fcm 前墙法=%.1fcm → 平均=%.1fcm (偏差%ldmm)\r\n",
           (double)pos_by_back, (double)pos_by_front, (double)pos, (long)diff);
    return pos;
}

/* ===== 到位回调 (各模块注册, 反向解耦) ===== */

static void mission_chassis_arrived(void) { Mission_OnArrived(EVT_CHASSIS_BIT); }
static void mission_lateral_arrived(void) { Mission_OnArrived(EVT_LATERAL_BIT); }

void Mission_OnArrived(uint32_t evt_bit)
{
    if (g_mission_events)
        xEventGroupSetBits(g_mission_events, evt_bit);
}

/* ===== CDC 触发 (CommandTask 收到 "go" 时调) ===== */
void Mission_Trigger(void)
{
    if (s_mission_task)
        xTaskNotifyGive(s_mission_task);  /* 唤醒 WAIT_CDC 状态 */
}

/* ===== 并行移动到预设点: 底盘相对 + 横移绝对, WaitBits 等两者到位 =====
 * chassis_cur: 当前底盘纵向位置(由激光算), 用于把绝对目标转成相对移动量。
 * 返回 true=两者都到位, false=超时。 */
static bool mission_move_to_point(const MissionPoint *pt, float chassis_cur)
{
    /* 底盘: 绝对目标 → 相对移动量 */
    float chassis_delta = pt->chassis_target_cm - chassis_cur;
    printf("[mission] 移动 底盘%.1fcm(→%.1f) + 横移→%.1fcm\r\n",
           (double)chassis_delta, (double)pt->chassis_target_cm, (double)pt->lateral_target_cm);

    /* 清 bits + 非阻塞下发两个动作 */
    const uint32_t expect = EVT_CHASSIS_BIT | EVT_LATERAL_BIT;
    xEventGroupClearBits(g_mission_events, expect);
    Chassis_MoveDistanceAsync(chassis_delta, 0);
    Lateral_MoveTo(pt->lateral_target_cm);

    /* 阻塞等两者都到位 (并行同步点), 10s 超时保护 */
    EventBits_t bits = xEventGroupWaitBits(
        g_mission_events, expect,
        pdTRUE,   /* 退出清 bits */
        pdTRUE,   /* 全部到位才返回 */
        pdMS_TO_TICKS(10000));

    if ((bits & expect) == expect) {
        printf("[mission] 移动完成\r\n");
        return true;
    }
    printf("[mission] 移动超时! bits=0x%lX\r\n", (unsigned long)bits);
    return false;
}

/* ===== 舵机动作: 发动作组命令 + 固定延时等完成 =====
 * Lobot 控制板对动作组命令不回发完成帧, 用 osDelay 估计。 */
static void mission_servo_action(void)
{
    printf("[mission] 舵机动作组%d ×%d, 延时%ums\r\n",
           SERVO_ACTION_GROUP, SERVO_ACTION_TIMES, SERVO_ACTION_DELAY_MS);
    runActionGroup(SERVO_ACTION_GROUP, SERVO_ACTION_TIMES);
    osDelay(SERVO_ACTION_DELAY_MS);
    printf("[mission] 舵机动作完成(延时到)\r\n");
}

/* ===== 主流程状态机任务 ===== */
void MissionTask(void *argument)
{
    (void)argument;
    osDelay(3000);  /* 等各子系统稳定 (电机使能/激光出帧/反馈就绪) */
    printf("[mission] 主流程任务启动\r\n");

    MissionState state = MS_LASER_LOCATE;
    float chassis_cur = 0.0f;   /* 激光算出的当前底盘纵向位置 */
    uint16_t front_mm = 0xFFFF; /* 前激光距离(mm), 障碍判断用 */

    for (;;) {
        switch (state) {

        case MS_LASER_LOCATE: {
            printf("[mission] === 激光定位 ===\r\n");
            chassis_cur = mission_laser_locate(&front_mm);
            printf("[mission] 当前纵向位置=%.1fcm, 前激光=%umm\r\n",
                   (double)chassis_cur,
                   (front_mm == 0xFFFFu) ? 0u : (unsigned)front_mm);
            /* 分支1: 前方有障碍(前激光有效且<阈值) → 避让; 否则正常等CDC */
            if (front_mm != 0xFFFFu && front_mm < OBSTACLE_THRESH_MM) {
                printf("[mission] 前方障碍(%umm<%u), 转避让\r\n",
                       (unsigned)front_mm, (unsigned)OBSTACLE_THRESH_MM);
                state = MS_AVOID;
            } else {
                state = MS_WAIT_CDC;
            }
            break;
        }

        case MS_AVOID: {
            /* 避让: 横移退开一段, 底盘不动。用 Lateral_MoveTo 相对当前退开。
             * 这里简化为直接横移到 current+AVOID, 实际可改方向/距离。 */
            printf("[mission] === 避让: 横移退开 %.1fcm ===\r\n", (double)AVOID_LATERAL_CM);
            Lateral_MoveDistance(AVOID_LATERAL_CM);
            Lateral_WaitArrival(5000);  /* 等横移避让完成, 5s超时 */
            state = MS_LASER_LOCATE;    /* 避让后重新定位, 再判断是否还有障碍 */
            break;
        }

        case MS_WAIT_CDC: {
            printf("[mission] === 等待CDC触发 (发 'go' 命令) ===\r\n");
            /* 阻塞等任务通知 (Mission_Trigger 的 xTaskNotifyGive 发), 清零计数。
             * ulTaskNotifyTake 返回通知值(此处不用), portMAX_DELAY 无限等。零轮询。 */
            (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            printf("[mission] 收到CDC触发, 开始移动\r\n");
            state = MS_MOVE_1;
            break;
        }

        case MS_MOVE_1: {
            printf("[mission] === 移动到点1 (底盘+横移并行) ===\r\n");
            bool ok = mission_move_to_point(&POINT_1, chassis_cur);
            /* 分支2: 移动超时 → 回定位重来; 成功 → 舵机动作 */
            state = ok ? MS_SERVO : MS_LASER_LOCATE;
            break;
        }

        case MS_SERVO: {
            printf("[mission] === 舵机动作 ===\r\n");
            mission_servo_action();
            state = MS_MOVE_2;
            break;
        }

        case MS_MOVE_2: {
            printf("[mission] === 移动到点2 (底盘+横移并行) ===\r\n");
            /* 第二次移动: 重新激光定位算当前位置, 再走到点2 */
            chassis_cur = mission_laser_locate(&front_mm);
            mission_move_to_point(&POINT_2, chassis_cur);
            printf("[mission] === 流程一轮完成, 循环 ===\r\n");
            state = MS_LASER_LOCATE;  /* 循环 */
            break;
        }

        default:
            state = MS_LASER_LOCATE;
            break;
        }
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

    /* 2. 注册各模块到位回调 (反向解耦: 各模块不依赖 mission)
     *    本流程只用底盘+横移并行, 升降未用(不注册 lift 回调) */
    Chassis_SetArrivedCallback(mission_chassis_arrived);
    Lateral_SetArrivedCallback(mission_lateral_arrived);

    /* 2.1 注册 CDC/VOFA 触发命令: "go" 唤醒 WAIT_CDC 状态 */
    configASSERT(CMD_Register("go", cmd_go) == 0);

    /* 3. 创建主流程任务 (开机自动跑状态机) */
    const osThreadAttr_t attr = {
        .name = "MissionTask",
        .stack_size = MISSION_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)MISSION_TASK_PRIORITY,
    };
    s_mission_task = osThreadNew(MissionTask, NULL, &attr);
    if (s_mission_task == NULL) {
        printf("[mission] 任务创建失败!\r\n");
        return -1;
    }

    printf("[mission] 编排初始化完成 (状态机 + Event Group + CDC触发)\r\n");
    return 0;
}
