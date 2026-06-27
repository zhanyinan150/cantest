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

    /* Control period. */
    pid->dt = 0.01f; /* Can be replaced by DWT_GetDeltaT(&pid->DWT_CNT). */

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

    if (pid->Improve & PID_ChangingIntegrationRate)
    {
        if (fabsf(pid->Err) <= pid->CoefB)
        {
            pid->ITerm += pid->Ki * pid->Err * pid->dt;
        }
        else if (fabsf(pid->Err) <= pid->CoefA + pid->CoefB)
        {
            float rate = (pid->CoefA - fabsf(pid->Err) + pid->CoefB) / pid->CoefA;
            pid->ITerm += pid->Ki * pid->Err * pid->dt * rate;
        }
    }
    else
    {
        pid->ITerm += pid->Ki * pid->Err * pid->dt;
    }

    if (pid->Improve & PID_Trapezoid_Intergral)
    {
        pid->ITerm = pid->Ki * (pid->Err + pid->Last_Err) * pid->dt / 2.0f;
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
