/**
 ******************************************************************************
 * @file    PID.h
 * @author  Wang Hongxi
 * @version V1.1.3
 * @date    2021/7/3
 * @brief   PID controller interface
 ******************************************************************************
 */
#ifndef _PID_H
#define _PID_H

#include "main.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"
#include <math.h>

#ifndef abs
#define abs(x) ((x > 0) ? x : -x)
#endif

typedef enum
{
    PID_IMPROVE_NONE = 0x00,
    PID_Integral_Limit = 0x01,
    PID_Derivative_On_Measurement = 0x02,
    PID_Trapezoid_Intergral = 0x04,
    PID_Proportional_On_Measurement = 0x08,
    PID_OutputFilter = 0x10,
    PID_ChangingIntegrationRate = 0x20,
    PID_DerivativeFilter = 0x40,
    PID_SCurve_Acceleration = 0x100,
} PID_Improvement_e;

typedef enum errorType_e
{
    PID_ERROR_NONE = 0x00U,
    PID_MOTOR_BLOCKED_ERROR = 0x01U
} ErrorType_e;

typedef struct
{
    uint64_t ERRORCount;
    ErrorType_e ERRORType;
} PID_ErrorHandler_t;

typedef struct
{
    float Kp;
    float Ki;
    float Kd;
    float MaxOut;
    float DeadBand;
    float MinOut;

    PID_Improvement_e Improve;
    float IntegralLimit;
    float CoefA;
    float CoefB;
    float Output_LPF_RC;
    float Derivative_LPF_RC;

    float MaxAccel;
    float MaxJerk;
    float Target_Speed;
    float Current_Speed;
    float Current_Accel;
    float SmoothRef;
    uint8_t SmoothRefInitialized;

    float Measure;
    float Last_Measure;
    float Err;
    float Last_Err;
    float Last_ITerm;

    float Pout;
    float Iout;
    float Dout;
    float ITerm;

    float Output;
    float Last_Output;
    float Last_Dout;

    float Ref;

    uint32_t DWT_CNT;
    float dt;

    PID_ErrorHandler_t ERRORHandler;
} PIDInstance;

typedef struct
{
    float Kp;
    float Ki;
    float Kd;
    float MaxOut;
    float DeadBand;
    float MinOut;

    PID_Improvement_e Improve;
    float IntegralLimit;
    float CoefA;
    float CoefB;
    float Output_LPF_RC;
    float Derivative_LPF_RC;

    float MaxAccel;
    float MaxJerk;
    float SmoothRef;
} PID_Init_Config_s;

void PIDInit(PIDInstance *pid, PID_Init_Config_s *config);
float PIDCalculate(PIDInstance *pid, float measure, float ref);

#endif /* _PID_H */
