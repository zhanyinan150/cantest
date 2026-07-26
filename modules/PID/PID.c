/**
 *******************************************************************************
 * @file    PID.c
 * @author  Wang Hongxi
 * @version V1.1.3
 * @date    2021/7/3
 * @brief   PID controller implementation
 *******************************************************************************
 */
#include "PID.h"
#include "bsp_dwt.h"

void PIDInit(PIDInstance *pid, PID_Init_Config_s *config)
{
    pid->Kp = config->Kp;
    pid->Ki = config->Ki;
    pid->Kd = config->Kd;
    pid->MaxOut = config->MaxOut;
    pid->DeadBand = config->DeadBand;
    pid->MinOut = config->MinOut;
    pid->SmoothRef = 0.0f;

    pid->Improve = config->Improve;
    pid->IntegralLimit = config->IntegralLimit;
    pid->CoefA = config->CoefA;
    pid->CoefB = config->CoefB;
    pid->Output_LPF_RC = config->Output_LPF_RC;
    pid->Derivative_LPF_RC = config->Derivative_LPF_RC;

    if (pid->Improve & PID_SCurve_Acceleration)
    {
        if (config->MaxAccel <= 0.0f && pid->MaxOut > 0.0f)
        {
            pid->MaxAccel = pid->MaxOut * 0.50f;
        }
        else
        {
            pid->MaxAccel = config->MaxAccel;
        }

        if (config->MaxJerk <= 0.0f && pid->MaxAccel > 0.0f)
        {
            pid->MaxJerk = pid->MaxAccel * 0.5f;
        }
        else
        {
            pid->MaxJerk = config->MaxJerk;
        }
    }
    else
    {
        pid->MaxAccel = config->MaxAccel;
        pid->MaxJerk = config->MaxJerk;
    }

    pid->Target_Speed = 0.0f;
    pid->Current_Speed = 0.0f;
    pid->Current_Accel = 0.0f;
    pid->SmoothRefInitialized = 0;

    pid->Measure = 0.0f;
    pid->Last_Measure = 0.0f;
    pid->Err = 0.0f;
    pid->Last_Err = 0.0f;
    pid->Last_ITerm = 0.0f;

    pid->Pout = 0.0f;
    pid->Iout = 0.0f;
    pid->Dout = 0.0f;
    pid->ITerm = 0.0f;

    pid->Output = 0.0f;
    pid->Last_Output = 0.0f;
    pid->Last_Dout = 0.0f;

    pid->Ref = 0.0f;

    pid->DWT_CNT = 0U;
    pid->dt = 0.0f;

    pid->ERRORHandler.ERRORCount = 0U;
    pid->ERRORHandler.ERRORType = PID_ERROR_NONE;
}

float PIDCalculate(PIDInstance *pid, float measure, float ref)
{
    pid->Measure = measure;
    pid->Ref = ref;

    /* 控制周期(秒), 固定 10ms —— 必须与实际调用本函数的任务周期一致。
     * 当前调用者是 DJIMotorTask (dji_motor.h: DJI_MOTOR_TASK_PERIOD = 10ms), 对得上。
     * ⚠ 改 DJI_MOTOR_TASK_PERIOD 时必须同步改这里, 否则 Ki/Kd 的实际增益会
     *   静默偏离标称值(周期翻倍则积分快一倍、微分弱一半), 表现为"PID 参数
     *   突然不好使了"却查不出原因。想彻底解耦可换 DWT_GetDeltaT(&pid->DWT_CNT)。*/
    pid->dt = 0.01f;

    if (pid->Improve & PID_SCurve_Acceleration)
    {
        if (!pid->SmoothRefInitialized)
        {
            pid->SmoothRef = ref;
            pid->Current_Speed = 0.0f;
            pid->Current_Accel = 0.0f;
            pid->SmoothRefInitialized = 1;
        }

        float diff = pid->Ref - pid->SmoothRef;
        float target_ratio = pid->Kp * diff / pid->MaxOut;

        if (target_ratio > 1.0f)
        {
            target_ratio = 1.0f;
        }
        else if (target_ratio < -1.0f)
        {
            target_ratio = -1.0f;
        }

        pid->Target_Speed = target_ratio * pid->MaxOut;

        float max_accel_change = pid->MaxJerk * pid->dt;
        float desired_accel = (pid->Target_Speed - pid->Current_Speed) / pid->dt;

        if (desired_accel > pid->MaxAccel)
        {
            desired_accel = pid->MaxAccel;
        }
        if (desired_accel < -pid->MaxAccel)
        {
            desired_accel = -pid->MaxAccel;
        }

        float accel_diff = desired_accel - pid->Current_Accel;
        if (accel_diff > max_accel_change)
        {
            accel_diff = max_accel_change;
        }
        else if (accel_diff < -max_accel_change)
        {
            accel_diff = -max_accel_change;
        }

        pid->Current_Accel += accel_diff;
        pid->Current_Speed += pid->Current_Accel * pid->dt;
        pid->SmoothRef += pid->Current_Speed * pid->dt;

        if ((diff > 0.0f && pid->SmoothRef > pid->Ref) ||
            (diff < 0.0f && pid->SmoothRef < pid->Ref))
        {
            pid->SmoothRef = pid->Ref;
            pid->Current_Speed = 0.0f;
            pid->Current_Accel = 0.0f;
        }
    }
    else
    {
        pid->SmoothRef = pid->Ref;
    }

    pid->Err = pid->SmoothRef - pid->Measure;
    if (fabsf(pid->Err) <= pid->DeadBand)
    {
        pid->Err = 0.0f;
    }

    /* 本周期的积分增量: 梯形法与矩形法二选一。
     * 原实现是"先按矩形法 ITerm += ..., 再用 ITerm = 梯形值"直接覆盖,
     * 等于把累积的积分量整个丢掉、退化成一个瞬时值 —— 积分作用完全失效,
     * 且抵消了前面的变积分逻辑。此处改为先算增量、再统一累加。 */
    float delta_i;
    if (pid->Improve & PID_Trapezoid_Intergral)
        delta_i = pid->Ki * (pid->Err + pid->Last_Err) * pid->dt / 2.0f;  /* 梯形法 */
    else
        delta_i = pid->Ki * pid->Err * pid->dt;                            /* 矩形法 */

    if (pid->Improve & PID_ChangingIntegrationRate)
    {
        /* 变积分: 误差小全速积分, 误差中等按比例衰减, 误差过大不积分(抗饱和) */
        float abs_err = fabsf(pid->Err);
        if (abs_err <= pid->CoefB)
        {
            pid->ITerm += delta_i;
        }
        else if (abs_err <= pid->CoefA + pid->CoefB)
        {
            /* CoefA 为 0 会除零产生 inf/NaN, 并顺着 Output 污染整条控制链,
             * 电机会收到一个荒唐的电流值。未配置 CoefA 时按"不积分"处理。 */
            if (pid->CoefA > 0.0f)
            {
                float rate = (pid->CoefA - abs_err + pid->CoefB) / pid->CoefA;
                pid->ITerm += delta_i * rate;
            }
        }
        /* abs_err > CoefA+CoefB: 误差过大, 本周期不积分 */
    }
    else
    {
        pid->ITerm += delta_i;
    }

    if (pid->Improve & PID_Integral_Limit)
    {
        if (pid->ITerm > pid->IntegralLimit)
        {
            pid->ITerm = pid->IntegralLimit;
        }
        else if (pid->ITerm < -pid->IntegralLimit)
        {
            pid->ITerm = -pid->IntegralLimit;
        }
    }

    pid->Pout = pid->Kp * pid->Err;

    if (pid->Improve & PID_Derivative_On_Measurement)
    {
        pid->Dout = pid->Kd * (pid->Last_Measure - pid->Measure) / pid->dt;
    }
    else
    {
        pid->Dout = pid->Kd * (pid->Err - pid->Last_Err) / pid->dt;
    }

    if (pid->Improve & PID_DerivativeFilter)
    {
        float alpha = pid->dt / (pid->Derivative_LPF_RC + pid->dt);
        pid->Dout = pid->Dout * alpha + pid->Last_Dout * (1.0f - alpha);
    }

    pid->Iout = pid->ITerm;
    pid->Output = pid->Pout + pid->Iout + pid->Dout;

    if (pid->Improve & PID_OutputFilter)
    {
        float alpha = pid->dt / (pid->Output_LPF_RC + pid->dt);
        pid->Output = pid->Output * alpha + pid->Last_Output * (1.0f - alpha);
    }

    if (pid->Output > pid->MaxOut)
    {
        pid->Output = pid->MaxOut;
    }
    else if (pid->Output < -pid->MaxOut)
    {
        pid->Output = -pid->MaxOut;
    }

    pid->Last_Measure = pid->Measure;
    pid->Last_Err = pid->Err;
    pid->Last_Output = pid->Output;
    pid->Last_Dout = pid->Dout;
    pid->Last_ITerm = pid->ITerm;

    return pid->Output;
}
