/**
  ******************************************************************************
  * @file    chassis.h
  * @brief   底盘差速控制模块 - 双 Emm_V5 步进轮 (CAN2)
  ******************************************************************************
  * @attention
  * ===== 底盘电机设置 (硬件配置, 勿随意改动) =====
  * 电机型号   : Emm42_V5.0 步进闭环驱动器 (张大头 ZDT), Emm5.0固件, 速度模式
  * CAN 总线   : CAN2, 扩展帧 (use_ext_id=1)
  * 驱动器地址 : 左=1, 右=2 (ID_Addr菜单, 与 CHASSIS_LEFT_ADDR/RIGHT_ADDR 一致)
  * 发送 ExtId : 左(addr=1) 0x00000100, 右(addr=2) 0x00000200  (ExtId = addr<<8)
  * 反馈 ID    : 同发送 ID (Emm_V5 收发同 ID, 靠数据字节区分命令/响应)
  * 方向 dir   : 0=CW(顺时针), 1=CCW(逆时针)
  *   前进     : 左 CW(dir=0) + 右 CCW(dir=1)
  *   后退     : 左 CCW(dir=1) + 右 CW(dir=0)
  * 使能       : Chassis_Init 调 Emm_V5_CAN_En_Control(addr, true) 使能双轮
  * CAN 波特率 : 1 Mbps (CAN2: APB1=42MHz, Prescaler=3, BS1=10TQ, BS2=3TQ, SJW=1TQ;
  *              波特率 = 42M / (3*(1+10+3)) = 1Mbps, 见 can.c MX_CAN2_Init)
  *
  * ---- 驱动器菜单设置 (首次上电在小屏幕配置, 详见 Emm_V5 说明书第4章) ----
  * P_Pul      : PUL_FOC   (FOC矢量闭环; 脉冲端口不用也需设此项)
  * P_Serial   : CAN1_MAP  (通讯端口复用为CAN)
  * En         : Hold      (一直使能, 由软件F3命令控制使能/失能)
  * MStep      : 16        (细分; 决定位置模式每转脉冲 = 200×16 = 3200)
  * Checksum   : 0x6B      (固定校验字节, 命令末尾均为0x6B)
  * Response   : Receive   (只返回确认收到命令; 到位返回需设Reached/Both)
  * CAN_Baud   : 1000000   (1Mbps, 须与主控CAN2波特率一致)
  * Cal        : 首次上电空载校准 (Cal菜单, 否则位置闭环误差 ±0.75°~1.5°)
  *
  * ---- Emm_V5 驱动参数 (来自代码与 mech_params.h, 详见说明书) ----
  * 速度范围   : 0~3000 RPM (Emm固件速度模式F6范围 0x0BB8, 说明书6.3.1)
  *              固件限幅 CHASSIS_MAX_RPM=3000, >3000 驱动器返回E2参数错误
  * 加速度 acc : 0~255档位, 0=直接启动(无固件加减速)。
  *              速度模式传acc=0, S形加减速由本模块软件余弦ramp控制;
  *              位置模式传CHASSIS_POS_ACC=30, 固件做线性梯形加减速(非S形)。
  *              加速度公式: t2-t1=(256-acc)*50us, 每隔(256-acc)*50us 增减1RPM
  * 位置模式   : 电机轴直驱轮子(无减速), FD命令clk单位=细分脉冲
  *              每转脉冲 = 200步/圈 × 16细分 = 3200 (mech_params.h CHASSIS_PULSE_PER_REV)
  *              脉冲数 clk = (距离cm / 轮周长cm) × 3200
  * 运动模式   : raF=false(00)=相对上次目标位置, raF=true(01)=绝对坐标
  *              ⚠️ Emm42_V5.0 只有00/01 (无02相对当前), 距离移动用00(相对上次目标),
  *              故两次位置命令之间不可插入速度模式运动, 否则目标基准漂移!
  * 轮径/周长  : 轮径 18cm, 周长 = π*18 ≈ 56.55cm (mech_params.h)
  * 多机同步   : snF=true 预存位置命令 + 广播 Emm_V5_CAN_Synchronous_motion(0) 同步启动
  * 状态标志   : S_FLAG(0x3A)  &0x01=使能 &0x02=到位 &0x04=堵转 &0x08=堵转保护
  * 到位窗口   : 默认0.3°(PRWindow菜单), |目标-实时|<窗口 时置位到位标志
  *
  * 两个步进轮挂 CAN2 (Emm_V5 驱动), 非大疆电机, 不经过 DJIMotorTask。
  * 命令直接经 Emm_V5_CAN_Vel_Control() 下发。
  *
  * 缓启动/缓停止 + S形加减速:
  *   ZDT 驱动器速度模式的 acc 参数是线性/梯形加减速, 非S形。
  *   本模块用余弦平滑曲线 0.5*(1-cos(πt/T)) 逐步刷新下发转速,
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

/* 默认 ramp 时间(ms): 0->目标转速的余弦S形加速时间 */
#define CHASSIS_DEFAULT_RAMP_MS   1000

/* 转速上限(RPM), Emm固件速度模式F6范围 0~3000 (0x0BB8), >3000驱动器返回E2 */
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
  * @param  ramp_ms    0->目标的加速时间(ms), 0 表示用默认 CHASSIS_DEFAULT_RAMP_MS。
  *                    换向/减速也用此时长从当前转速平滑过渡到新目标。
  */
void Chassis_SetVelocity(float target_rpm, uint32_t ramp_ms);

/**
  * @brief  设置底盘目标速度(立即, 无S形ramp)
  * @param  target_rpm 目标转速(RPM, 带符号: +前进 -后退), 自动限幅到 ±CHASSIS_MAX_RPM
  * @note   越过余弦S形ramp, 直接把当前转速设为目标并立即下发。停止须配合 Chassis_StopNow
  *         (立即停), 不要用 Chassis_Stop (会 S 形减速)。用于不需要平滑加速的场景。
  */
void Chassis_SetVelocityImmediate(float target_rpm);

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
  *                  mtest 自动前进30cm->后退30cm循环测试
  */
void Chassis_RegisterCommands(void);

/**
  * @brief  位置模式移动指定距离(阻塞直到到位或超时)
  * @param  distance_cm 距离(cm, 带符号: +前进 -后退)
  * @param  vel_rpm     运行转速(RPM), 0 用默认 CHASSIS_POS_VEL_RPM
  * @retval 0 到位, -1 超时/堵转/参数非法
  * @note   内部: 停速度模式 -> 多机同步预存位置命令 -> 广播同步启动
  *         -> Chassis_WaitArrive 轮询 S_FLAG 到位/堵转标志。
  *         驱动器内置 acc 线性缓启(非S形); S形需固件分段位置ramp(未实现)。
  *         需在 ChassisTask 之外的任务上下文调用(阻塞)。
  */
int Chassis_MoveDistance(float distance_cm, uint16_t vel_rpm);

/**
  * @brief  位置模式移动指定距离(非阻塞: 仅下发命令, 不等待到位)
  * @note   到位检测由 ChassisTask 周期查 S_FLAG 完成, 到位时调已注册的到位回调。
  *         适合编排任务并行等待多模块到位 (配合 Event Group)。需在 ChassisTask 外调用。
  *         ⚠️ 与 Chassis_MoveDistance(阻塞版) 不可并发: 两者都触发 ChassisTask 读 S_FLAG,
  *            且阻塞版内部 Chassis_WaitArrive 也读 S_FLAG, 并发会撕裂 CAN2 rx_buff。
  *            mission 用异步版后, 勿再并发发 mfwd/mrev 命令。
  *         ⚠️ Emm42_V5.0 位置模式raF=00(相对上次目标), 两次位置命令间不可插速度模式。
  * @retval 0 已下发, -1 参数非法(距离0)
  */
int Chassis_MoveDistanceAsync(float distance_cm, uint16_t vel_rpm);

/**
  * @brief  位置模式到位等待: 轮询双轮 S_FLAG 直到均到位或超时/堵转
  * @retval 0 双轮均到位, -1 超时或堵转
  * @note   S_FLAG(0x3A): &0x02=到位 &0x04=堵转 &0x08=堵转保护。
  *         阻塞调用(任务上下文), 与 Read_Encoder 共享 rx_buff 不可并发。
  */
int Chassis_WaitArrive(void);

/**
  * @brief  注册位置模式到位回调 (上层 mission 用于事件驱动编排)
  * @note   回调在 ChassisTask 上下文执行, 应简短(如 xEventGroupSetBits)。
  *         反向解耦: chassis 不 include mission, 由 mission 主动注册。
  */
void Chassis_SetArrivedCallback(void (*cb)(void));

#endif /* __CHASSIS_H */
