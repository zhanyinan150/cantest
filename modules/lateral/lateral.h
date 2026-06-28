/**
  ******************************************************************************
  * @file    lateral.h
  * @brief   横移系统控制模块 - 单 Emm_V5 步进 + 同步带同步轮 (UART5)
  ******************************************************************************
  * @attention
  * 横移系统, 使用 1 个 Emm_V5 步进电机经 UART5 控制:
  * - 通信: UART5 (115200), Emm_V5 UART 协议 (与底盘 CAN2 步进独立总线, 不冲突)
  * - 控制单位: 位移 (cm), 正负表示横移方向
  * - 控制模式: 速度模式 (点动 RPM) + 位置模式 (移动到目标 cm, 电机内部闭环)
  * - 架构: LateralTask 单任务独占 UART5 总线 (读编码器+发指令),
  *          VOFA 命令只设置意图标志, 不直接访问总线, 避免并发冲突 (镜像 chassis)。
  * - 机械参数: 同步轮周长 LATERAL_PULLEY_CIRCUMFERENCE_CM (占位, 待实测)
  ******************************************************************************
  */
#ifndef __LATERAL_H
#define __LATERAL_H

#include "main.h"
#include "cmsis_os2.h"
#include "stdbool.h"
#include "mech_params.h"

/* 电机地址 (UART5 总线拨码设定, 与 CAN2 底盘电机的 1/2 独立, 不冲突) */
#define LATERAL_MOTOR_ADDR         1

/* 任务参数: 编码器读取(Q&A, 约35ms) + 指令发送, 50ms 周期足够 */
#define LATERAL_TASK_PERIOD        50
#define LATERAL_TASK_STACK_SIZE    512               /* word, printf 链深 */
#define LATERAL_TASK_PRIORITY      osPriorityBelowNormal  /* 16, 不干扰升降/底盘控制 */

/* 运动参数 */
#define LATERAL_DEFAULT_VEL_RPM    600               /* 位置模式移动速度 (RPM) */
#define LATERAL_DEFAULT_ACC        30                /* 加速度 0-255 (0=直接启动) */
#define LATERAL_MAX_RPM            3000
#define LATERAL_POS_TOLERANCE_CM   0.5f              /* 位置到位容差 (cm) */
#define LATERAL_POS_TIMEOUT_MS     8000              /* 位置模式超时 (ms) */

/* 位移限幅 (cm) */
#define LATERAL_MAX_DISPLACEMENT   200.0f
#define LATERAL_MIN_DISPLACEMENT  -200.0f

typedef enum {
    LATERAL_MODE_IDLE = 0,      /* 空闲: 电机保持(位置模式到位后)或已停止 */
    LATERAL_MODE_VELOCITY,      /* 速度模式: 持续按 RPM 旋转 */
    LATERAL_MODE_POSITION,      /* 位置模式: 移动到目标位移, 到位/超时后回 IDLE */
} Lateral_Mode_t;

typedef struct {
    float current_displacement;  /* 当前位移 (cm, 相对零点) */
    float target_displacement;   /* 目标位移 (cm) */
    int16_t target_rpm;          /* 速度模式目标 RPM (带符号) */
    Lateral_Mode_t mode;         /* 当前控制模式 */
    bool enabled;                /* 电机使能状态 */
    bool arrived;                /* 上次位置模式结果: true=到位 false=超时 */
    /* 任务内部意图标志 (由 API 设置, LateralTask 消费) */
    bool stop_request;           /* 请求停止 (中断位置模式) */
    bool home_request;           /* 请求回零 (当前位置清零) */
    bool pos_pending;            /* 位置命令待发送 (新目标/重定向) */
    int8_t enable_pending;       /* 使能待应用: -1=无 0=失能 1=使能 */
} Lateral_Status_t;

/* 全局变量声明 */
extern Lateral_Status_t lateral_status;
extern osThreadId_t lateralTaskHandle;

/* ===== API =====
 * 所有 API 仅设置意图标志, 实际总线操作由 LateralTask 执行 (线程安全)。
 * Lateral_WaitArrival 供测试任务阻塞等待到位 (不访问总线)。 */

/**
 * @brief 横移系统初始化: 创建 LateralTask (任务首帧使能电机)
 * @note  需在 Emm_V5_Init() (创建 UART5 接收信号量) 之后调用
 */
int Lateral_Init(void);

/** @brief 横移控制任务 (独占 UART5 总线) */
void LateralTask(void *argument);

/** @brief 速度模式: 设置目标 RPM (带符号, 正负=方向), 非阻塞 */
int Lateral_SetVelocity(int16_t rpm);

/** @brief 位置模式: 移动到绝对位移 (cm), 非阻塞 (到位由 LateralTask 判定) */
int Lateral_MoveTo(float cm);

/** @brief 位置模式: 相对当前位置移动 (cm), 非阻塞 */
int Lateral_MoveDistance(float cm);

/** @brief 停止 (中断位置模式, 下一任务周期生效) */
int Lateral_Stop(void);

/** @brief 回零: 当前位置清零 (电机内部位置 + 软件累计都清零) */
int Lateral_Home(void);

/** @brief 电机使能/失能 (下一任务周期生效) */
int Lateral_Enable(bool en);

/** @brief 阻塞等待位置模式到位/超时 (供测试任务用, 不访问总线) */
bool Lateral_WaitArrival(uint32_t timeout_ms);

/** @brief 注册位置模式到位回调 (上层 mission 用于事件驱动编排, 反向解耦) */
void Lateral_SetArrivedCallback(void (*cb)(void));

/** @brief 获取当前位移 (cm) */
float Lateral_GetCurrentDisplacement(void);

/** @brief 获取横移状态指针 */
Lateral_Status_t *Lateral_GetStatus(void);

/** @brief 注册横移 VOFA 命令 (lvel/lpos/lto/lstop/lhome/len) */
void Lateral_RegisterCommands(void);

#endif /* __LATERAL_H */
