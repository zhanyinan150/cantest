/**
 ******************************************************************************
 * @file    lift.c
 * @author  TKX Team (ported from 2025EPIQZJ, single-motor version)
 * @brief   升降系统控制模块 - 单 M2006 电机版本
 ******************************************************************************
 * @attention
 *
 * 升降系统，使用 1 个 M2006 电机：
 * - 控制单位：位移（厘米），负值表示向下运动
 * - 机械参数：主动轮直径 18cm，36:1 减速比
 * - PID 参数移植自 2025EPIQZJ 工程 lift.c
 *   外环(角度环) + 内环(速度环) 串级，不使用电流环(C610 电调自带)。
 *
 ******************************************************************************
 */

#include "lift.h"
#include "bsp_can.h"
#include "can.h"
#include "main.h"
#include "bsp_log.h"   /* Log_PrintFloat2: MicroLIB 安全浮点打印 */
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "math.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmd_register.h"
#include "telemetry.h"

/* 全局变量定义 */
DJIMotorInstance *lift_motor = NULL;
Lift_Status_t lift_status;
osThreadId_t liftTaskHandle = NULL;

/* 私有函数声明 */
static float Lift_AngleToDisplacement(float angle);
static float Lift_DisplacementToAngle(float displacement);
static void Lift_UpdateCurrentDisplacement(void);
static void Lift_UpdateCurrentSpeed(void);
static void Lift_ControlMotors(void);
static void Lift_SetTarget(float target_displacement);
bool Lift_WaitUntilAtTarget(uint32_t timeout_ms);
static uint8_t lift_telemetry_getter(float *out, uint8_t max);  /* 遥测波形通道 */

/**
 * @brief 统一的目标位移设置入口(带限幅)
 * @note 所有 Lift_Up/Down/MoveTo/To_* 都经此入口, 保证位移不越界。
 */
static void Lift_SetTarget(float target_displacement)
{
    if (target_displacement > LIFT_MAX_DISPLACEMENT)
        target_displacement = LIFT_MAX_DISPLACEMENT;
    else if (target_displacement < LIFT_MIN_DISPLACEMENT)
        target_displacement = LIFT_MIN_DISPLACEMENT;

    lift_status.target_displacement = target_displacement;
    lift_status.is_moving = true;
}

/**
 * @brief 升降系统初始化
 * @retval 0: 成功, -1: 失败
 */
int Lift_Init(void)
{
    // 状态初始化
    memset(&lift_status, 0, sizeof(Lift_Status_t));
    lift_status.enabled = true;
    lift_status.current_displacement = 0.0f;
    Lift_SetTarget(0.0f);
    lift_status.speed = 0.0f;
    lift_status.is_moving = false;

    // M2006 电机配置 (PID 参数移植自 2025EPIQZJ lift.c)
    Motor_Init_Config_s m2006_config = {
        .can_init_config = {
            .can_handle = &hcan1,
            .tx_id = LIFT_MOTOR_ID,
            .use_ext_id = 0,
        },
        .motor_type = M2006,
        .controller_param_init_config = {
            .angle_PID = {              /* 角度环（外环） */
                .Kp = 8.6f,
                .Ki = 0.0f,
                .Kd = 0.0f,
                .MaxOut = 10000.0f,
                .DeadBand = 1.0f,
                .Improve = PID_DerivativeFilter,
                .IntegralLimit = 10.0f,
                .Derivative_LPF_RC = 0.01f,
            },
            .speed_PID = {              /* 速度环（内环），输出即C610电流值 -10000~10000 */
                .Kp = 0.7f,
                .Ki = 0.03f,
                .Kd = 0.01f,
                .MaxOut = 10000.0f,
                .DeadBand = 0.5f,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = 3000.0f,
            },
        },
        .controller_setting_init_config = {
            .close_loop_type = ANGLE_AND_SPEED_LOOP,  /* 速度环 + 位置环串级 */
            .outer_loop_type = ANGLE_LOOP,               /* 外环为位置环 */
            .motor_reverse_flag = MOTOR_DIRECTION_REVERSE,
            .feedback_reverse_flag = FEEDBACK_DIRECTION_NORMAL,
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,
            .feedforward_flag = FEEDFORWARD_NONE,
        }
    };

    lift_motor = DJIMotorInit(&m2006_config);
    if (lift_motor == NULL) {
        printf("升降系统电机初始化失败!\r\n");
        return -1;
    }
    DJIMotorEnable(lift_motor);
    printf("升降系统初始化完成!\r\n");

    // 创建升降控制任务
    const osThreadAttr_t liftTask_attributes = {
        .name = "LiftTask",
        .stack_size = LIFT_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)LIFT_TASK_PRIORITY,
    };

    liftTaskHandle = osThreadNew(LiftTask, NULL, &liftTask_attributes);
    if (liftTaskHandle == NULL) {
        printf("升降任务创建失败!\r\n");
        return -1;
    }

    Lift_RegisterCommands();  /* 注册升降 VOFA 命令到 bsp/cmd */
    configASSERT(Telemetry_Register("lift", lift_telemetry_getter) == 0);
    return 0;
}

/**
 * @brief 升降控制任务函数
 * @param argument 任务参数
 */
void LiftTask(void *argument)
{
    printf("升降任务启动\r\n");

    /* vTaskDelayUntil 保证严格 20ms 周期, 避免 osDelay 累积抖动 */
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // 更新当前位移
        Lift_UpdateCurrentDisplacement();

        // 更新当前速度（用于监控）
        Lift_UpdateCurrentSpeed();

        // 控制电机
        Lift_ControlMotors();

        // 精确周期延时
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(LIFT_TASK_PERIOD));
    }
}

/**
 * @brief 更新当前位移
 */
static void Lift_UpdateCurrentDisplacement(void)
{
    if (!lift_status.enabled) return;
    if (lift_motor == NULL) return;

    float motor_angle = lift_motor->measure.total_angle;
    lift_status.current_displacement = Lift_AngleToDisplacement(motor_angle);
}

/**
 * @brief 控制电机
 */
static void Lift_ControlMotors(void)
{
    if (!lift_status.enabled) return;
    if (lift_motor == NULL) return;

    // 位移控制模式：将目标位移转换为电机目标角度
    float target_angle = Lift_DisplacementToAngle(lift_status.target_displacement);
    DJIMotorSetRef(lift_motor, target_angle);
}

/**
 * @brief 升降向上移动
 * @param displacement 相对当前位置的向上增量 (cm, 正值)
 * @retval 0: 成功, -1: 失败
 * @note 电机安装方向相反(motor_reverse_flag=REVERSE), 故向上=位移减小。
 */
int Lift_Up(float displacement)
{
    Lift_SetTarget(lift_status.target_displacement - displacement);
    return 0;
}

/**
 * @brief 升降向下移动
 * @param displacement 相对当前位置的向下增量 (cm, 正值)
 * @retval 0: 成功, -1: 失败
 * @note 电机安装方向相反(motor_reverse_flag=REVERSE), 故向下=位移增大。
 */
int Lift_Down(float displacement)
{
    Lift_SetTarget(lift_status.target_displacement + displacement);
    return 0;
}

/**
 * @brief 停止升降运动
 * @retval 0: 成功, -1: 失败
 */
int Lift_Stop(void)
{
    // 设置目标位移为当前位移，停止运动
    lift_status.target_displacement = lift_status.current_displacement;
    lift_status.is_moving = false;

    printf("升降运动已停止在位移: ");
    Log_PrintFloat2("", lift_status.current_displacement);
    printf("cm\r\n");
    return 0;
}

/**
 * @brief 获取升降状态
 * @return 升降状态结构体指针
 */
Lift_Status_t *Lift_GetStatus(void)
{
    return &lift_status;
}

/**
 * @brief 获取当前位移
 * @return 当前位移 (cm)，负值表示向下位移
 */
float Lift_GetCurrentDisplacement(void)
{
    return lift_status.current_displacement;
}

/**
 * @brief 电机角度转换为位移
 * @param angle 电机角度 (度)
 * @return 对应的位移 (cm)
 * @note 计算公式: 位移 = (角度 / 360度 / 减速比) * 主动轮周长 * 100
 */
static float Lift_AngleToDisplacement(float angle)
{
    float displacement_m = (angle / 360.0f / LIFT_GEAR_RATIO) * LIFT_WHEEL_CIRCUMFERENCE;
    return displacement_m * 100.0f; /* m转换为cm */
}

/**
 * @brief 位移转换为电机角度
 * @param displacement 位移 (cm)
 * @return 对应的电机角度 (度)
 * @note 计算公式: 角度 = (位移[m] / 轮周长[m]) * 360度 * 减速比
 */
static float Lift_DisplacementToAngle(float displacement)
{
    float displacement_m = displacement / 100.0f; /* cm转换为m */
    float angle = (displacement_m / LIFT_WHEEL_CIRCUMFERENCE) * 360.0f * LIFT_GEAR_RATIO;
    return angle;
}

/**
 * @brief 更新当前速度（用于状态监控）
 */
static void Lift_UpdateCurrentSpeed(void)
{
    if (!lift_status.enabled) return;

    static float last_displacement = 0.0f;
    static uint32_t last_time = 0;

    uint32_t current_time = HAL_GetTick();

    // 初始化或时间间隔太小时跳过计算
    if (last_time == 0 || (current_time - last_time) < LIFT_TASK_PERIOD)
    {
        last_displacement = lift_status.current_displacement;
        last_time = current_time;
        return;
    }

    // 计算速度 = 位移变化 / 时间间隔
    float displacement_change = lift_status.current_displacement - last_displacement;
    float time_interval = (current_time - last_time) / 1000.0f; // ms转换为s

    lift_status.speed = displacement_change / time_interval; // cm/s

    last_displacement = lift_status.current_displacement;
    last_time = current_time;
}

/**
 * @brief 阻塞等待升降到达目标位置(带超时)
 * @param timeout_ms 最大等待时间(ms), 0 表示不限时(不推荐)
 * @retval true 已到位, false 超时未到位(电机失联/PID发散/目标越界等)
 * @note 原实现无超时, 电机失联会永久阻塞调用任务, 现加 5s 默认上限。
 */
bool Lift_WaitUntilAtTarget(uint32_t timeout_ms)
{
    const float tolerance = 2.0f; // 位移容差
    const uint32_t default_timeout = 8000; // 默认 8s
    if (timeout_ms == 0)
        timeout_ms = default_timeout;

    uint32_t waited = 0;
    const uint32_t step = 10;
    float error;

    do {
        error = fabsf(lift_status.current_displacement - lift_status.target_displacement);
        if (error <= tolerance)
            return true;
        vTaskDelay(pdMS_TO_TICKS(step));  // 等待 10ms
        waited += step;
    } while (waited < timeout_ms);

    return false;  // 超时
}

/**
 * @brief 移动到指定位置
 * @param target_displacement 目标绝对位移 (cm)，负值表示向下位移
 * @retval 0: 成功, -1: 失败
 */
int Lift_MoveTo(float target_displacement)
{
    if (!lift_status.enabled)
    {
        printf("升降系统未使能!\r\n");
        return -1;
    }

    Lift_SetTarget(target_displacement);  /* 统一经此入口限幅 */
    return 0;
}

/* ===== PID 调参 API ===== */
void Lift_SetPID(Lift_Loop_t loop, float Kp, float Ki, float Kd)
{
    if (lift_motor == NULL)
        return;
    if (loop == LIFT_LOOP_ANGLE) {
        lift_motor->motor_controller.angle_PID.Kp = Kp;
        lift_motor->motor_controller.angle_PID.Ki = Ki;
        lift_motor->motor_controller.angle_PID.Kd = Kd;
    } else {
        lift_motor->motor_controller.speed_PID.Kp = Kp;
        lift_motor->motor_controller.speed_PID.Ki = Ki;
        lift_motor->motor_controller.speed_PID.Kd = Kd;
    }
}

void Lift_GetPID(Lift_Loop_t loop, float *Kp, float *Ki, float *Kd)
{
    if (lift_motor == NULL)
        return;
    float p = 0, i = 0, d = 0;
    if (loop == LIFT_LOOP_ANGLE) {
        p = lift_motor->motor_controller.angle_PID.Kp;
        i = lift_motor->motor_controller.angle_PID.Ki;
        d = lift_motor->motor_controller.angle_PID.Kd;
    } else {
        p = lift_motor->motor_controller.speed_PID.Kp;
        i = lift_motor->motor_controller.speed_PID.Ki;
        d = lift_motor->motor_controller.speed_PID.Kd;
    }
    if (Kp) *Kp = p;
    if (Ki) *Ki = i;
    if (Kd) *Kd = d;
}

/* ===== 命令 handler (arg 为命令名后的参数字符串, 可为空串) ===== */
static void cmd_lift_up(const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) Lift_Up(v);
}
static void cmd_lift_dn(const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) Lift_Down(v);
}
static void cmd_lift_to(const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) Lift_MoveTo(v);
}
static void cmd_lift_stop(const char *arg)
{
    (void)arg;
    Lift_Stop();
}
/* 通用 PID 设置 handler: loop 固定, arg 解析 1 个 float, 其余两项经 Lift_GetPID 保留原值 */
static void cmd_set_pid_kp(Lift_Loop_t loop, const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) {
        float kp, ki, kd; Lift_GetPID(loop, &kp, &ki, &kd);
        Lift_SetPID(loop, v, ki, kd);
    }
}
static void cmd_set_pid_ki(Lift_Loop_t loop, const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) {
        float kp, ki, kd; Lift_GetPID(loop, &kp, &ki, &kd);
        Lift_SetPID(loop, kp, v, kd);
    }
}
static void cmd_set_pid_kd(Lift_Loop_t loop, const char *arg)
{
    float v; if (sscanf(arg, "%f", &v) == 1) {
        float kp, ki, kd; Lift_GetPID(loop, &kp, &ki, &kd);
        Lift_SetPID(loop, kp, ki, v);
    }
}
/* 各命令的薄包装 (CMD_Handler 签名固定, 用闭包式包装传 loop) */
static void cmd_akp(const char *arg) { cmd_set_pid_kp(LIFT_LOOP_ANGLE, arg); }
static void cmd_aki(const char *arg) { cmd_set_pid_ki(LIFT_LOOP_ANGLE, arg); }
static void cmd_akd(const char *arg) { cmd_set_pid_kd(LIFT_LOOP_ANGLE, arg); }
static void cmd_skp(const char *arg) { cmd_set_pid_kp(LIFT_LOOP_SPEED, arg); }
static void cmd_ski(const char *arg) { cmd_set_pid_ki(LIFT_LOOP_SPEED, arg); }
static void cmd_skd(const char *arg) { cmd_set_pid_kd(LIFT_LOOP_SPEED, arg); }

/* ===== 遥测波形通道 =====
 * 通道: 目标位移(cm) / 当前位移(cm) / 电机角速度(度/秒) / 电机电流
 */
static uint8_t lift_telemetry_getter(float *out, uint8_t max)
{
    if (max < 4 || lift_motor == NULL)
        return 0;
    out[0] = lift_status.target_displacement;
    out[1] = lift_status.current_displacement;
    out[2] = lift_motor->measure.speed_aps;
    out[3] = (float)lift_motor->measure.real_current;
    return 4;
}

void Lift_RegisterCommands(void)
{
    /* 注册失败(表满)断言暴露, 避免命令静默丢失 */
    int r = 0;
    r |= CMD_Register("up",   cmd_lift_up);
    r |= CMD_Register("dn",   cmd_lift_dn);
    r |= CMD_Register("to",   cmd_lift_to);
    r |= CMD_Register("stop", cmd_lift_stop);
    r |= CMD_Register("akp",  cmd_akp);
    r |= CMD_Register("aki",  cmd_aki);
    r |= CMD_Register("akd",  cmd_akd);
    r |= CMD_Register("skp",  cmd_skp);
    r |= CMD_Register("ski",  cmd_ski);
    r |= CMD_Register("skd",  cmd_skd);
    configASSERT(r == 0);
}
