/**
  ******************************************************************************
  * @file    chassis_demo.c
  * @brief   底盘前进后退演示 (App层, 开机自动运行)
  ******************************************************************************
  * 演示流程 (速度模式定时换向, 立即设速无S形, 跑 DEMO_LOOPS 次后自动停止):
  *   前进(立即设速 -> 匀速 -> 立即停 -> 停顿) ->
  *   后退(立即设速 -> 匀速 -> 立即停 -> 停顿) -> 循环
  *
  * Chassis_SetVelocityImmediate 为非阻塞调用: 直接把当前转速设为目标并下发,
  * 越过余弦S形ramp状态机。换向前 Chassis_StopNow 立即停, 不经S形减速。
  * 注意: 无ramp意味着启停有机械冲击, 适合小转速测试场景。
  *
  * 不依赖 VOFA/串口命令: ChassisDemo_Init 直接创建任务, 上电即跑。
  ******************************************************************************
  */

#include "chassis_demo.h"
#include "chassis.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "stdio.h"

/* ---- 演示参数 ---- */
#define DEMO_VEL_RPM        100.0f  /* 演示转速(RPM), +前进/-后退, 自动限幅 ±CHASSIS_MAX_RPM */
#define DEMO_RUN_MS         1000     /* 每次前进/后退匀速段持续时长(ms) */
#define DEMO_PAUSE_MS       2000     /* 换向间停顿(ms), 给机械结构/观测留余量 */
#define DEMO_LOOPS          3        /* 前进后退循环次数, 0=无限循环(断电/复位中止) */

/* 任务参数: 优先级与 ChassisTask 同级(Normal), 栈 384word 防 printf 链路溢出 */
#define DEMO_TASK_STACK_SIZE   384
#define DEMO_TASK_PRIORITY     osPriorityNormal

/**
  * @brief  底盘前进后退演示任务 (自动运行 DEMO_LOOPS 次后自删除)
  * @note   Chassis_SetVelocityImmediate 非阻塞立即设速, 用 osDelay 控制时序;
  *         换向前 Chassis_StopNow 立即停。无S形ramp, 启停有冲击。
  */
static void ChassisDemoTask(void *argument)
{
    (void)argument;
    /* 等电机使能稳定: Chassis_Init 内 ChassisTask 已 osDelay(1000), 此处再等 500ms 保险 */
    osDelay(1500);

    printf("[cdemo] 启动: 前进%lums <-> 后退%lums, 转速%drpm, 立即换向(无S形), 循环%s\r\n",
           (unsigned long)DEMO_RUN_MS, (unsigned long)DEMO_RUN_MS,
           (int)DEMO_VEL_RPM,
           (DEMO_LOOPS == 0) ? "无限" : "有限");

    uint32_t loop = 0;
    for (;;) {
        /* === 前进 === */
        printf("[cdemo] 第%lu次 前进\r\n", (unsigned long)(loop + 1));
        Chassis_SetVelocityImmediate(DEMO_VEL_RPM);  /* 立即设速, 无S形ramp */
        osDelay(DEMO_RUN_MS);
        Chassis_StopNow();                            /* 立即停(越过S形) */
        osDelay(DEMO_PAUSE_MS);

        /* === 后退 === */
        printf("[cdemo] 第%lu次 后退\r\n", (unsigned long)(loop + 1));
        Chassis_SetVelocityImmediate(-DEMO_VEL_RPM);
        osDelay(DEMO_RUN_MS);
        Chassis_StopNow();
        osDelay(DEMO_PAUSE_MS);

        loop++;
        if (DEMO_LOOPS > 0 && loop >= DEMO_LOOPS) {
            printf("[cdemo] 完成 %lu 次前进后退循环, 停止\r\n", (unsigned long)loop);
            Chassis_StopNow();  /* 保险: 确保电机停转 */
            vTaskDelete(NULL);  /* 自删除 */
        }
    }
}

/**
  * @brief  底盘演示初始化: 创建演示任务 (开机自动跑, 不注册命令)
  */
int ChassisDemo_Init(void)
{
    const osThreadAttr_t attr = {
        .name = "ChassisDemo",
        .stack_size = DEMO_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)DEMO_TASK_PRIORITY,
    };
    if (osThreadNew(ChassisDemoTask, NULL, &attr) == NULL) {
        printf("[cdemo] 任务创建失败!\r\n");
        return -1;
    }
    printf("[cdemo] 演示任务已创建 (开机自动运行 %s)\r\n",
           (DEMO_LOOPS == 0) ? "无限循环" : "有限循环");
    return 0;
}
