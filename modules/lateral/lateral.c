/**
  ******************************************************************************
  * @file    lateral.c
  * @brief   横移系统控制模块 - 单 Emm_V5 步进 + 同步带同步轮 (UART5)
  ******************************************************************************
  * @attention
  * 架构: LateralTask (50ms) 单任务独占 UART5 总线:
  *   - 每周期读编码器 → 更新当前位移
  *   - 消费意图标志 (stop/home/enable/pos_pending) 与模式状态机 (VELOCITY/POSITION)
  *   - VOFA 命令经 CommandTask 仅设置意图标志, 不直接访问总线 → 无并发冲突
  *
  * 方向处理: LATERAL_DIR_INVERT (mech_params.h) 统一翻转 CW/CCW 与编码器符号,
  *   使 "正位移=正向横移" 的约定与实际接线解耦, 接线反了改宏即可。
  ******************************************************************************
  */

#include "lateral.h"
#include "Emm_V5.h"
#include "usart.h"
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "cmd_register.h"
#include "telemetry.h"
#include "bsp_log.h"

/* 全局变量 */
Lateral_Status_t lateral_status;
osThreadId_t lateralTaskHandle = NULL;

/* 私有函数声明 */
static float Lateral_PulsesToCm(int32_t pulses);
static int32_t Lateral_CmToPulses(float cm);
static float Lateral_DirSign(void);
static uint8_t Lateral_DirForSigned(float signed_eff);  /* 由"有效空间有符号量"算物理 dir(0=CW,1=CCW) */
static uint8_t lateral_telemetry_getter(float *out, uint8_t max);

/* ===== 换算与方向辅助 ===== */

static float Lateral_DirSign(void)
{
    return LATERAL_DIR_INVERT ? -1.0f : 1.0f;
}

static float Lateral_PulsesToCm(int32_t pulses)
{
    return (float)pulses / LATERAL_PULSE_PER_REV * LATERAL_PULLEY_CIRCUMFERENCE_CM;
}

static int32_t Lateral_CmToPulses(float cm)
{
    return (int32_t)(cm / LATERAL_PULLEY_CIRCUMFERENCE_CM * LATERAL_PULSE_PER_REV);
}

/* 由"有效空间有符号量" (正=正向横移) 推物理方向 dir。
 * 推导: eff = s*accumulated (s=±1), CW(dir0)→accumulated+。
 *   欲使 eff 增加 (signed_eff>0): invert0 时 dir0, invert1 时 dir1。
 *   故 dir = (s*signed_eff >= 0) ? 0 : 1。 */
static uint8_t Lateral_DirForSigned(float signed_eff)
{
    return (Lateral_DirSign() * signed_eff >= 0.0f) ? 0 : 1;
}

/* ===== 初始化 ===== */

int Lateral_Init(void)
{
    memset(&lateral_status, 0, sizeof(Lateral_Status_t));
    lateral_status.enabled = false;
    lateral_status.mode = LATERAL_MODE_IDLE;
    lateral_status.enable_pending = -1;  /* 无待应用 */
    lateral_status.current_displacement = 0.0f;
    lateral_status.target_displacement = 0.0f;
    lateral_status.target_rpm = 0;
    lateral_status.arrived = false;

    /* 创建横移控制任务 (任务首帧使能电机, 所有总线操作在此任务内) */
    const osThreadAttr_t lateralTask_attributes = {
        .name = "LateralTask",
        .stack_size = LATERAL_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)LATERAL_TASK_PRIORITY,
    };
    lateralTaskHandle = osThreadNew(LateralTask, NULL, &lateralTask_attributes);
    if (lateralTaskHandle == NULL) {
        printf("横移任务创建失败!\r\n");
        return -1;
    }

    Lateral_RegisterCommands();
    configASSERT(Telemetry_Register("lateral", lateral_telemetry_getter) == 0);
    printf("横移系统初始化完成 (UART5 addr=%d, 周长=%.1fcm 占位)\r\n",
           LATERAL_MOTOR_ADDR, LATERAL_PULLEY_CIRCUMFERENCE_CM);
    return 0;
}

/* ===== 控制任务 (独占 UART5) ===== */

void LateralTask(void *argument)
{
    (void)argument;
    printf("横移任务启动\r\n");

    /* 首帧: 使能电机 (总线操作集中在任务内, 避免与 Init 上下文竞争) */
    osDelay(100);  /* 等 Emm_V5 信号量就绪 + UART5 稳定 */
    Emm_V5_En_Control(LATERAL_MOTOR_ADDR, true, false);
    lateral_status.enabled = true;
    printf("[lateral] 电机已使能\r\n");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t dbg_cnt = 0;
    uint32_t pos_start = 0;

    for (;;) {
        /* 1. 读编码器 → 当前位移 (有效空间) */
        int32_t enc = Emm_V5_Read_Encoder(LATERAL_MOTOR_ADDR);
        lateral_status.current_displacement = Lateral_DirSign() * Lateral_PulsesToCm(enc);

        /* 2. 消费意图标志 (优先级: home > enable > stop > 模式状态机) */
        if (lateral_status.home_request) {
            Emm_V5_Reset_CurPos_To_Zero(LATERAL_MOTOR_ADDR);
            Emm_V5_Reset_Encoder_Accumulation(LATERAL_MOTOR_ADDR);
            lateral_status.home_request = false;
            lateral_status.mode = LATERAL_MODE_IDLE;
            lateral_status.current_displacement = 0.0f;
            lateral_status.target_displacement = 0.0f;
            printf("[lateral] 已回零 (位移清零)\r\n");
        }

        if (lateral_status.enable_pending >= 0) {
            bool en = (lateral_status.enable_pending != 0);
            Emm_V5_En_Control(LATERAL_MOTOR_ADDR, en, false);
            lateral_status.enabled = en;
            lateral_status.enable_pending = -1;
            printf("[lateral] 使能=%d\r\n", en);
        }

        if (lateral_status.stop_request) {
            Emm_V5_Stop_Now(LATERAL_MOTOR_ADDR, false);
            lateral_status.stop_request = false;
            lateral_status.mode = LATERAL_MODE_IDLE;
            printf("[lateral] 已停止\r\n");
        }

        /* 3. 模式状态机 */
        switch (lateral_status.mode) {
        case LATERAL_MODE_VELOCITY: {
            int16_t rpm = lateral_status.target_rpm;
            int16_t mag = (rpm < 0) ? (int16_t)(-rpm) : rpm;
            if (mag > LATERAL_MAX_RPM) mag = LATERAL_MAX_RPM;
            uint8_t dir = Lateral_DirForSigned((float)rpm);
            Emm_V5_Vel_Control(LATERAL_MOTOR_ADDR, dir, (uint16_t)mag, 0, false);
            break;
        }
        case LATERAL_MODE_POSITION: {
            /* 新目标/重定向: 发送相对位置命令 */
            if (lateral_status.pos_pending) {
                float delta = lateral_status.target_displacement - lateral_status.current_displacement;
                int32_t clk = Lateral_CmToPulses(delta);
                if (clk == 0) {
                    lateral_status.arrived = true;
                    lateral_status.mode = LATERAL_MODE_IDLE;
                    lateral_status.pos_pending = false;
                } else {
                    uint8_t dir = Lateral_DirForSigned(delta);
                    Emm_V5_Pos_Control(LATERAL_MOTOR_ADDR, dir,
                                       LATERAL_DEFAULT_VEL_RPM, LATERAL_DEFAULT_ACC,
                                       (uint32_t)(clk < 0 ? -clk : clk), false, false);
                    lateral_status.pos_pending = false;
                    pos_start = HAL_GetTick();
                }
            } else {
                /* 到位/超时判定 */
                float err = fabsf(lateral_status.current_displacement - lateral_status.target_displacement);
                if (err <= LATERAL_POS_TOLERANCE_CM) {
                    lateral_status.arrived = true;
                    lateral_status.mode = LATERAL_MODE_IDLE;
                    printf("[lateral] 到位 disp=%.2fcm\r\n", lateral_status.current_displacement);
                } else if ((HAL_GetTick() - pos_start) > LATERAL_POS_TIMEOUT_MS) {
                    lateral_status.arrived = false;
                    lateral_status.mode = LATERAL_MODE_IDLE;
                    Emm_V5_Stop_Now(LATERAL_MOTOR_ADDR, false);
                    printf("[lateral] 位置超时 disp=%.2f tgt=%.2f\r\n",
                           lateral_status.current_displacement, lateral_status.target_displacement);
                }
            }
            break;
        }
        default:
            break;
        }

        /* 4. 周期调试打印 (~1s 一次) */
        if (++dbg_cnt >= 20) {
            dbg_cnt = 0;
            printf("[lateral] mode=%d disp=%.2f tgt=%.2f rpm=%d\r\n",
                   lateral_status.mode, lateral_status.current_displacement,
                   lateral_status.target_displacement, lateral_status.target_rpm);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(LATERAL_TASK_PERIOD));
    }
}

/* ===== API (仅设置意图标志) ===== */

int Lateral_SetVelocity(int16_t rpm)
{
    lateral_status.target_rpm = rpm;
    lateral_status.stop_request = false;
    lateral_status.mode = LATERAL_MODE_VELOCITY;
    return 0;
}

int Lateral_MoveTo(float cm)
{
    if (cm > LATERAL_MAX_DISPLACEMENT) cm = LATERAL_MAX_DISPLACEMENT;
    else if (cm < LATERAL_MIN_DISPLACEMENT) cm = LATERAL_MIN_DISPLACEMENT;
    lateral_status.target_displacement = cm;
    lateral_status.pos_pending = true;
    lateral_status.stop_request = false;
    lateral_status.mode = LATERAL_MODE_POSITION;
    return 0;
}

int Lateral_MoveDistance(float cm)
{
    return Lateral_MoveTo(lateral_status.current_displacement + cm);
}

int Lateral_Stop(void)
{
    lateral_status.stop_request = true;
    return 0;
}

int Lateral_Home(void)
{
    lateral_status.home_request = true;
    return 0;
}

int Lateral_Enable(bool en)
{
    lateral_status.enable_pending = en ? 1 : 0;
    return 0;
}

bool Lateral_WaitArrival(uint32_t timeout_ms)
{
    uint32_t waited = 0;
    const uint32_t step = 20;
    while (lateral_status.mode == LATERAL_MODE_POSITION && waited < timeout_ms) {
        osDelay(step);
        waited += step;
    }
    return lateral_status.arrived;
}

float Lateral_GetCurrentDisplacement(void)
{
    return lateral_status.current_displacement;
}

Lateral_Status_t *Lateral_GetStatus(void)
{
    return &lateral_status;
}

/* ===== VOFA 命令 ===== */

static void cmd_lvel(const char *arg)
{
    int v; if (sscanf(arg, "%d", &v) == 1) Lateral_SetVelocity((int16_t)v);
}
static void cmd_lpos(const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) Lateral_MoveDistance(v);
}
static void cmd_lto(const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) Lateral_MoveTo(v);
}
static void cmd_lstop(const char *arg) { (void)arg; Lateral_Stop(); }
static void cmd_lhome(const char *arg) { (void)arg; Lateral_Home(); }
static void cmd_len(const char *arg)
{
    int v; if (sscanf(arg, "%d", &v) == 1) Lateral_Enable(v != 0);
}
static void cmd_lstat(const char *arg)
{
    (void)arg;
    printf("[lateral] mode=%d en=%d disp=", lateral_status.mode, lateral_status.enabled);
    Log_PrintFloat2("", lateral_status.current_displacement);
    printf(" tgt=");
    Log_PrintFloat2("", lateral_status.target_displacement);
    printf(" rpm=%d\r\n", lateral_status.target_rpm);
}

void Lateral_RegisterCommands(void)
{
    int r = 0;
    r |= CMD_Register("lvel",  cmd_lvel);   /* lvel <rpm>  速度模式(带符号) */
    r |= CMD_Register("lpos",  cmd_lpos);   /* lpos <cm>   相对位置移动 */
    r |= CMD_Register("lto",   cmd_lto);    /* lto <cm>    绝对位置移动 */
    r |= CMD_Register("lstop", cmd_lstop);  /* 停止 */
    r |= CMD_Register("lhome", cmd_lhome);  /* 回零(清零) */
    r |= CMD_Register("len",   cmd_len);    /* len <0/1>   使能/失能 */
    r |= CMD_Register("lstat", cmd_lstat);  /* 查询状态 */
    configASSERT(r == 0);
}

/* ===== 遥测通道 =====
 * 通道: 目标位移(cm) / 当前位移(cm) / 目标RPM / 模式 */
static uint8_t lateral_telemetry_getter(float *out, uint8_t max)
{
    if (max < 4)
        return 0;
    out[0] = lateral_status.target_displacement;
    out[1] = lateral_status.current_displacement;
    out[2] = (float)lateral_status.target_rpm;
    out[3] = (float)lateral_status.mode;
    return 4;
}
