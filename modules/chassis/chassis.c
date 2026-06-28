/**
  ******************************************************************************
  * @file    chassis.c
  * @brief   底盘差速控制模块 - 双 Emm_V5 步进轮 (CAN2)
  ******************************************************************************
  * 模块职责分区(自上而下):
  *   1. 速度模式: 余弦S形ramp状态机 + 速度命令下发
  *   2. 位置模式: 多机同步 + S_FLAG到位检测
  *   3. 控制任务: ChassisTask 周期推进速度ramp
  *   4. VOFA命令: 各命令handler + 自注册 (速度/位置/mtest)
  *   5. 遥测:     波形通道getter
  *   6. 初始化:   Chassis_Init 使能电机+建任务+注册命令/遥测
  ******************************************************************************
  */

#include "chassis.h"
#include "Emm_V5_CAN.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "math.h"
#include "cmd_register.h"
#include "telemetry.h"

/* ---- 常量 ---- */
#define PI_F                            3.14159265f   /* 余弦S形ramp曲线用, 与周长无关 */
#define CHASSIS_MTEST_DISTANCE_CM       30.0f
#define CHASSIS_MTEST_HOLD_MS           1000

/* ---- 速度ramp状态机 ---- */
typedef enum {
    CHASSIS_IDLE,      /* vel_current==0, 无目标, 总线静默 */
    CHASSIS_RAMPING,   /* 正在沿余弦曲线从 vel_start 过渡到 vel_target */
    CHASSIS_CRUISE,    /* 已达 vel_target 且非0, 匀速保持 */
} chassis_state_t;

static struct {
    float           vel_current;   /* 当前指令转速(带符号) */
    float           vel_target;    /* 目标转速(带符号) */
    float           vel_start;     /* 本次 ramp 起点转速 */
    uint32_t        ramp_ms;       /* 本次 ramp 总时长 */
    uint32_t        ramp_elapsed;  /* 本次 ramp 已用时长(ms) */
    uint32_t        last_ramp_ms;  /* 最近一次 ramp 时长, 供 Chassis_Stop 复用 */
    chassis_state_t state;
    bool            idle_need_zero;/* IDLE 时需补发一帧 0 转速 */
    bool            enabled;
} chassis;

static osThreadId_t s_mtest_task = NULL;  /* mtest 自动循环任务句柄, estop/mstop 共享 */

/* 到位回调 (由上层 mission 注册, 位置模式到位时调用, 反向解耦) */
static void (*s_arrived_cb)(void) = NULL;
static bool s_pos_active = false;  /* 位置模式进行中标志 (ChassisTask 据此轮询 S_FLAG) */

/* ---- 内部函数前向声明 ---- */
static void    ChassisTask(void *argument);
static void    chassis_send(float vel_rpm);
static void    chassis_step(void);
static void    ChassisMTestTask(void *argument);
static uint8_t chassis_telemetry_getter(float *out, uint8_t max);


/* ================================================================== */
/* ===== 1. 速度模式: 余弦S形ramp + 双轮速度命令下发 ================ */
/* ================================================================== */

/**
  * @brief  把带符号转速映射为双轮 (dir, |vel|) 并下发
  * @note   前进(>=0): 左CW(dir=0) 右CCW(dir=1)
  *         后退(<0) : 左CCW(dir=1) 右CW(dir=0)
  *         acc 固定0, 加减速由固件侧 ramp 控制
  */
static void chassis_send(float vel_rpm)
{
    float av = fabsf(vel_rpm);
    if (av > 5000.0f) av = 5000.0f;
    uint16_t vel = (uint16_t)av;

    uint8_t dirL, dirR;
    if (vel_rpm >= 0.0f) { dirL = 0; dirR = 1; }  /* 前进 */
    else                 { dirL = 1; dirR = 0; }  /* 后退 */

    Emm_V5_CAN_Vel_Control(CHASSIS_LEFT_ADDR,  dirL, vel, 0, false);
    Emm_V5_CAN_Vel_Control(CHASSIS_RIGHT_ADDR, dirR, vel, 0, false);
}

/**
  * @brief  推进 ramp 状态机并下发本周期转速 (由 ChassisTask 周期调用)
  */
static void chassis_step(void)
{
    if (chassis.state == CHASSIS_RAMPING)
    {
        chassis.ramp_elapsed += CHASSIS_TASK_PERIOD_MS;
        if (chassis.ramp_elapsed >= chassis.ramp_ms)
        {
            /* ramp 完成 */
            chassis.vel_current = chassis.vel_target;
            if (chassis.vel_target == 0.0f) {
                chassis.state = CHASSIS_IDLE;
                chassis.idle_need_zero = true;
                printf("[chassis] 停止\r\n");
            } else {
                chassis.state = CHASSIS_CRUISE;
                printf("[chassis] 匀速 %+.0f rpm\r\n", (double)chassis.vel_target);
            }
        }
        else
        {
            /* 余弦S形: alpha = 0.5*(1 - cos(π * t/T)), 起止导数为0, 加加速度平滑 */
            float alpha = 0.5f * (1.0f - cosf(PI_F * (float)chassis.ramp_elapsed / (float)chassis.ramp_ms));
            chassis.vel_current = chassis.vel_start + (chassis.vel_target - chassis.vel_start) * alpha;
        }
        chassis_send(chassis.vel_current);
    }
    else if (chassis.state == CHASSIS_CRUISE)
    {
        /* 匀速保持: 周期刷新防丢帧 */
        chassis_send(chassis.vel_current);
    }
    else if (chassis.state == CHASSIS_IDLE && chassis.idle_need_zero)
    {
        /* 进入IDLE补发一帧0, 之后静默省总线 */
        chassis_send(0.0f);
        chassis.idle_need_zero = false;
    }
}

/**
  * @brief  设置底盘目标速度(触发余弦S形 ramp)
  */
void Chassis_SetVelocity(float target_rpm, uint32_t ramp_ms)
{
    if (target_rpm >  (float)CHASSIS_MAX_RPM) target_rpm =  (float)CHASSIS_MAX_RPM;
    if (target_rpm < -(float)CHASSIS_MAX_RPM) target_rpm = -(float)CHASSIS_MAX_RPM;
    if (ramp_ms == 0) ramp_ms = CHASSIS_DEFAULT_RAMP_MS;

    chassis.last_ramp_ms = ramp_ms;

    /* 新目标与当前几乎一致: 直接进入匀速/静止, 不再 ramp */
    if (fabsf(target_rpm - chassis.vel_current) < 1.0f) {
        chassis.vel_target = target_rpm;
        if (target_rpm == 0.0f) {
            chassis.state = CHASSIS_IDLE;
            chassis.idle_need_zero = true;
        } else {
            chassis.state = CHASSIS_CRUISE;
        }
        return;
    }

    printf("[chassis] ramp %+.0f → %+.0f rpm / %lums\r\n",
           (double)chassis.vel_current, (double)target_rpm, (unsigned long)ramp_ms);

    chassis.vel_start   = chassis.vel_current;  /* 从当前转速平滑过渡, 支持运行中改目标/换向 */
    chassis.vel_target  = target_rpm;
    chassis.ramp_ms     = ramp_ms;
    chassis.ramp_elapsed = 0;
    chassis.state       = CHASSIS_RAMPING;
}

/**
  * @brief  底盘缓停止(按上次 ramp 时长 S 形减速到 0)
  */
void Chassis_Stop(void)
{
    uint32_t r = chassis.last_ramp_ms ? chassis.last_ramp_ms : CHASSIS_DEFAULT_RAMP_MS;
    Chassis_SetVelocity(0.0f, r);
}

/**
  * @brief  底盘立即停止(急停, 越过S形直接发停转命令)
  */
void Chassis_StopNow(void)
{
    chassis.vel_current = 0.0f;
    chassis.vel_target  = 0.0f;
    chassis.state       = CHASSIS_IDLE;
    chassis.idle_need_zero = false;
    Emm_V5_CAN_Stop_Now(CHASSIS_LEFT_ADDR,  false);
    Emm_V5_CAN_Stop_Now(CHASSIS_RIGHT_ADDR, false);
    printf("[chassis] 急停\r\n");
}

float Chassis_GetCurrentVelocity(void) { return chassis.vel_current; }
float Chassis_GetTargetVelocity(void)  { return chassis.vel_target;  }


/* ================================================================== */
/* ===== 2. 位置模式: 多机同步启动 + S_FLAG到位检测 ================ */
/* ================================================================== */
/* 距离(cm) → 脉冲数: clk = (distance / 周长) × 每转脉冲数(65536) */

/**
  * @brief  轮询 S_FLAG 直到两轮均到位或超时/堵转
  * @retval 0 双轮均到位, -1 超时或堵转
  * @note   S_FLAG(0x3A): &0x02=到位 &0x04=堵转 &0x08=堵转保护。任一堵转立即返回。
  *         阻塞调用(任务上下文), 与 Read_Encoder 共享 rx_buff 不可并发。
  *         超时按墙钟计(osKernelGetTickCount), 含 Read_Flag 内部阻塞时间。
  */
int Chassis_WaitArrive(void)
{
    uint32_t start_tick = osKernelGetTickCount();
    const uint32_t step = 20;
    while ((osKernelGetTickCount() - start_tick) < CHASSIS_POS_TIMEOUT_MS) {
        int32_t fL = Emm_V5_CAN_Read_Flag(CHASSIS_LEFT_ADDR);
        int32_t fR = Emm_V5_CAN_Read_Flag(CHASSIS_RIGHT_ADDR);
        if (fL < 0 || fR < 0) {  /* 读取失败, 短暂等待重试 */
            osDelay(step); continue;
        }
        if ((fL & EMM_FLAG_STALL) || (fR & EMM_FLAG_STALL) ||
            (fL & EMM_FLAG_STALL_PROT) || (fR & EMM_FLAG_STALL_PROT)) {
            printf("[chassis] 堵转! L=0x%X R=0x%X\r\n", (unsigned)fL, (unsigned)fR);
            return -1;
        }
        if ((fL & EMM_FLAG_ARRIVED) && (fR & EMM_FLAG_ARRIVED))
            return 0;
        osDelay(step);
    }
    printf("[chassis] 到位超时\r\n");
    return -1;
}

/**
  * @brief  位置模式移动指定距离(阻塞直到到位或超时)
  * @note   停速度模式 → 多机同步预存位置命令 → 广播同步启动 → 轮询S_FLAG到位。
  *         驱动器内置 acc 线性缓启(非S形); S形需固件分段位置ramp(未实现)。
  */
int Chassis_MoveDistance(float distance_cm, uint16_t vel_rpm)
{
    if (Chassis_MoveDistanceAsync(distance_cm, vel_rpm) != 0)
        return -1;
    return Chassis_WaitArrive();
}

/**
  * @brief  位置模式移动指定距离(非阻塞: 仅下发命令, 不等待到位)
  * @note   到位检测由 ChassisTask 周期查 S_FLAG 完成, 到位时调 s_arrived_cb 通知上层。
  *         适合编排任务并行等待多模块到位 (配合 Event Group)。
  * @retval 0 已下发, -1 参数非法(距离0)
  */
int Chassis_MoveDistanceAsync(float distance_cm, uint16_t vel_rpm)
{
    if (distance_cm == 0.0f)
        return -1;
    if (vel_rpm == 0)
        vel_rpm = CHASSIS_POS_VEL_RPM;
    if (vel_rpm > 5000)
        vel_rpm = 5000;

    /* 1. 停速度模式, 避免与位置命令冲突 */
    Chassis_StopNow();

    /* 2. 距离 → 脉冲数(相对运动) */
    float rev = fabsf(distance_cm) / CHASSIS_WHEEL_CIRCUMFERENCE_CM;
    uint32_t clk = (uint32_t)(rev * CHASSIS_PULSE_PER_REV);
    if (clk == 0) clk = 1;

    /* 3. 方向映射: 前进(>=0) 左CW右CCW; 后退(<0) 左CCW右CW (与速度模式一致) */
    uint8_t dirL, dirR;
    if (distance_cm >= 0.0f) { dirL = 0; dirR = 1; }
    else                     { dirL = 1; dirR = 0; }

    printf("[chassis] 位置模式(异步) %+.1fcm = %lu脉冲, vel=%u rpm\r\n",
           (double)distance_cm, (unsigned long)clk, vel_rpm);

    /* 4. 多机同步: 两轮 snF=true 预存运动, 广播同步启动消除航向漂移 */
    Emm_V5_CAN_Pos_Control(CHASSIS_LEFT_ADDR,  dirL, vel_rpm, CHASSIS_POS_ACC, clk, false, true);
    Emm_V5_CAN_Pos_Control(CHASSIS_RIGHT_ADDR, dirR, vel_rpm, CHASSIS_POS_ACC, clk, false, true);
    Emm_V5_CAN_Synchronous_motion(0);

    s_pos_active = true;  /* 标记位置运动进行中, ChassisTask 据此轮询到位 */
    return 0;
}

/**
  * @brief  注册位置模式到位回调 (上层 mission 用于事件驱动编排)
  * @note   回调在 ChassisTask 上下文执行, 应简短(如 xEventGroupSetBits)。
  */
void Chassis_SetArrivedCallback(void (*cb)(void))
{
    s_arrived_cb = cb;
}


/* ================================================================== */
/* ===== 3. 控制任务: 周期推进速度ramp =============================== */
/* ================================================================== */

/**
  * @brief  底盘控制任务: 周期推进 ramp 状态机 + 位置模式到位检测
  */
static void ChassisTask(void *argument)
{
    (void)argument;
    osDelay(1000); /* 等电机使能稳定 */

    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        if (chassis.enabled)
            chassis_step();

        /* 位置模式到位检测 (非阻塞): 位置运动进行中时, 周期查 S_FLAG,
         * 到位/堵转则清 s_pos_active 并回调通知上层。 */
        if (s_pos_active) {
            int32_t fL = Emm_V5_CAN_Read_Flag(CHASSIS_LEFT_ADDR);
            int32_t fR = Emm_V5_CAN_Read_Flag(CHASSIS_RIGHT_ADDR);
            if (fL >= 0 && fR >= 0) {
                if ((fL & EMM_FLAG_STALL) || (fR & EMM_FLAG_STALL) ||
                    (fL & EMM_FLAG_STALL_PROT) || (fR & EMM_FLAG_STALL_PROT)) {
                    s_pos_active = false;
                    printf("[chassis] 位置堵转 L=0x%X R=0x%X\r\n", (unsigned)fL, (unsigned)fR);
                } else if ((fL & EMM_FLAG_ARRIVED) && (fR & EMM_FLAG_ARRIVED)) {
                    s_pos_active = false;
                    printf("[chassis] 位置到位\r\n");
                    if (s_arrived_cb) s_arrived_cb();
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(CHASSIS_TASK_PERIOD_MS));
    }
}


/* ================================================================== */
/* ===== 4. VOFA命令: 各handler + 自注册 (经 bsp/cmd 注册表) ========= */
/* ================================================================== */
/* handler 在 CommandTask 上下文执行。mfwd/mrev 阻塞调用 MoveDistance,
 * 期间无法处理新命令(测试场景可接受)。mtest 启动独立任务不阻塞 CommandTask。 */

/* --- 速度模式命令 --- */
static void cmd_fwd(const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) Chassis_SetVelocity( v, 0);
}
static void cmd_rev(const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) Chassis_SetVelocity(-v, 0);
}
static void cmd_cstop(const char *arg) { (void)arg; Chassis_Stop(); }
/* estop: 急停当前运动, 并终止 mtest 自动循环任务(否则下次循环重启运动)。 */
static void cmd_estop(const char *arg)
{
    (void)arg;
    if (s_mtest_task != NULL) {
        osThreadTerminate(s_mtest_task);
        s_mtest_task = NULL;
    }
    Chassis_StopNow();
}

/* --- 位置模式命令 --- */
static void cmd_mfwd(const char *arg)
{
    float d; if (sscanf(arg, "%f", &d) == 1) Chassis_MoveDistance( d, 0);
}
static void cmd_mrev(const char *arg)
{
    float d; if (sscanf(arg, "%f", &d) == 1) Chassis_MoveDistance(-d, 0);
}

/* --- mtest 自动循环: 前进30cm ↔ 后退30cm --- */
static void ChassisMTestTask(void *argument)
{
    (void)argument;
    osDelay(1000);
    printf("[chassis] mtest 启动: 前进%dcm ↔ 后退%dcm 循环\r\n",
           (int)CHASSIS_MTEST_DISTANCE_CM, (int)CHASSIS_MTEST_DISTANCE_CM);
    for (;;) {
        Chassis_MoveDistance( CHASSIS_MTEST_DISTANCE_CM, 0);
        osDelay(CHASSIS_MTEST_HOLD_MS);
        Chassis_MoveDistance(-CHASSIS_MTEST_DISTANCE_CM, 0);
        osDelay(CHASSIS_MTEST_HOLD_MS);
    }
}
static void cmd_mtest(const char *arg)
{
    (void)arg;
    if (s_mtest_task != NULL) {
        printf("[chassis] mtest 已在运行\r\n");
        return;
    }
    const osThreadAttr_t attr = {
        .name = "ChassisMTest",
        .stack_size = 384 * 4,
        .priority = (osPriority_t)CHASSIS_TASK_PRIORITY,
    };
    s_mtest_task = osThreadNew(ChassisMTestTask, NULL, &attr);
}
/* mstop: 终止 mtest 循环任务并急停。estop 已能停 mtest, mstop 提供独立语义。 */
static void cmd_mstop(const char *arg)
{
    (void)arg;
    if (s_mtest_task != NULL) {
        osThreadTerminate(s_mtest_task);
        s_mtest_task = NULL;
        printf("[chassis] mtest 已停止\r\n");
    } else {
        printf("[chassis] mtest 未运行\r\n");
    }
    Chassis_StopNow();  /* 终止可能正在执行的位置运动 */
}

void Chassis_RegisterCommands(void)
{
    int r = 0;
    r |= CMD_Register("fwd",  cmd_fwd);
    r |= CMD_Register("rev",  cmd_rev);
    r |= CMD_Register("cstop", cmd_cstop);
    r |= CMD_Register("estop", cmd_estop);
    r |= CMD_Register("mfwd", cmd_mfwd);
    r |= CMD_Register("mrev", cmd_mrev);
    r |= CMD_Register("mtest", cmd_mtest);
    r |= CMD_Register("mstop", cmd_mstop);
    configASSERT(r == 0);
}


/* ================================================================== */
/* ===== 5. 遥测: 波形通道getter (经 bsp/telemetry 注册表) ========= */
/* ================================================================== */
/* 通道: 目标转速(rpm) / 当前指令转速(rpm) */
static uint8_t chassis_telemetry_getter(float *out, uint8_t max)
{
    if (max < 2)
        return 0;
    out[0] = chassis.vel_target;
    out[1] = chassis.vel_current;
    return 2;
}


/* ================================================================== */
/* ===== 6. 初始化 =================================================== */
/* ================================================================== */

/**
  * @brief  底盘初始化: 使能双轮 + 建任务 + 注册命令/遥测
  * @note   需在 Emm_V5_CAN_Init() 与 HAL_CAN_Start(&hcan2) 之后调用
  */
int Chassis_Init(void)
{
    memset(&chassis, 0, sizeof(chassis));
    chassis.enabled = true;
    chassis.idle_need_zero = true;  /* 初始补发一帧0, 确保电机静止 */

    /* 使能双轮 (闭环保持) */
    Emm_V5_CAN_En_Control(CHASSIS_LEFT_ADDR,  true, false);
    Emm_V5_CAN_En_Control(CHASSIS_RIGHT_ADDR, true, false);

    const osThreadAttr_t attr = {
        .name = "ChassisTask",
        .stack_size = CHASSIS_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)CHASSIS_TASK_PRIORITY,
    };
    if (osThreadNew(ChassisTask, NULL, &attr) == NULL) {
        printf("[chassis] 任务创建失败!\r\n");
        return -1;
    }

    printf("[chassis] 初始化完成, 左=%d 右=%d, 默认ramp=%lums\r\n",
           CHASSIS_LEFT_ADDR, CHASSIS_RIGHT_ADDR, (unsigned long)CHASSIS_DEFAULT_RAMP_MS);

    Chassis_RegisterCommands();  /* 注册底盘 VOFA 命令到 bsp/cmd */
    configASSERT(Telemetry_Register("chassis", chassis_telemetry_getter) == 0);
    return 0;
}
