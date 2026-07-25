/**
  ******************************************************************************
  * @file    mech_params.h
  * @brief   集中机械参数配置 (升降 / 底盘 / X/Y轴)
  ******************************************************************************
  * 设计原则:
  *   1. 只放"原始机械量"(轮径、减速比、每转脉冲), 派生量(周长/半径)一律
  *      由原始量经表达式计算, 避免手算硬编码带来的数值漂移与维护负担。
  *   2. 各模块(.h) include 本文件后即可使用, .c 无需改动。
  *   3. 宏名保持与历史代码一致, 平滑迁移。
  *
  * 修改机械参数时只需改本文件, 周长等派生量自动同步。
  ******************************************************************************
  */

#ifndef __MECH_PARAMS_H
#define __MECH_PARAMS_H

/* 圆周率 (周长派生用) */
#define MECH_PI  3.14159265f


/* ================================================================== */
/* ===== 升降机构: 单 M2006 减速电机 + 主动轮提升 ================== */
/* ================================================================== */
/* M2006 为 "P36" 减速电机, 自带 36:1 减速器, 电机轴->主动轮非直驱。
 * 位移换算必须计入减速比 (见 lift.c Lift_AngleToDisplacement)。 */

#define LIFT_WHEEL_DIAMETER_M         0.032f     /* 主动轮直径 (m) */
/* 半径与周长均由直径派生, 勿手填 */
#define LIFT_WHEEL_RADIUS_M           (LIFT_WHEEL_DIAMETER_M * 0.5f)
#define LIFT_WHEEL_CIRCUMFERENCE_M    (MECH_PI * LIFT_WHEEL_DIAMETER_M)
#define LIFT_GEAR_RATIO               36.0f     /* 减速比 36:1 (电机轴:输出轴) */

/* ---- 兼容旧宏名 (lift.c 历史引用) ---- */
#define LIFT_WHEEL_DIAMETER           LIFT_WHEEL_DIAMETER_M
#define LIFT_WHEEL_RADIUS             LIFT_WHEEL_RADIUS_M
#define LIFT_WHEEL_CIRCUMFERENCE      LIFT_WHEEL_CIRCUMFERENCE_M


/* ================================================================== */
/* ===== 底盘: 双 Emm_V5 步进轮, 电机轴直驱轮子 (无减速) =========== */
/* ================================================================== */
/* Emm_V5 为步进闭环驱动, 电机轴直驱轮子, 减速比=1, 故无 GEAR_RATIO 宏。
 *
 * ⚠️ 位置模式 FD 命令的 clk 单位 = 细分脉冲, 不是编码器值!
 *   - 发送位置命令 FD: clk = 细分脉冲数, 16细分下 3200脉冲/圈
 *   - 读取编码器反馈 0x31/0x33: 返回值 0-65535 表示一圈 (编码器4倍频)
 *   两者单位不同, 勿混用。CHASSIS_PULSE_PER_REV 用于发送命令, 故=3200。 */

#define CHASSIS_WHEEL_DIAMETER_CM     18.0f     /* 轮径 (cm) */
/* 周长由直径派生, 勿手填 */
#define CHASSIS_WHEEL_CIRCUMFERENCE_CM  (MECH_PI * CHASSIS_WHEEL_DIAMETER_CM)
#define CHASSIS_STEPS_PER_REV         200       /* 1.8°电机每转步数 (360/1.8) */
#define CHASSIS_MICROSTEP             16        /* 驱动器细分 (MStep菜单), 须与电机设置一致 */
#define CHASSIS_PULSE_PER_REV         ((float)(CHASSIS_STEPS_PER_REV * CHASSIS_MICROSTEP))  /* 3200 脉冲/圈 */


/* ================================================================== */
/* ===== X/Y 轴: Emm_V5 步进电机 (CAN2, XYZ起重机机构) ============= */
/* ================================================================== */
/* X轴(单电机 ID3) + Y轴(双电机同步 ID1,2), 经减速箱驱动同步带/齿轮齿条。
 * 位移换算见 modules/motor/motor.c::Motor_DistanceToClk。
 * 电机轴->输出轴非直驱时必须填减速比, 否则距离换算错误。
 * 直径按实物量取填入, 周长由 π×直径 派生, 勿手填。 */

/* ---- X 轴 (单电机) ---- */
#define MOTOR_X_WHEEL_DIAMETER_CM      1.2f    /* 主动轮直径(cm), 量实物填 (占位5.73->周长≈18) */
#define MOTOR_X_WHEEL_CIRCUMFERENCE_CM (MECH_PI * MOTOR_X_WHEEL_DIAMETER_CM)  /* 派生: 每转行程 */
#define MOTOR_X_GEAR_RATIO             1.0f     /* 减速比(电机轴:输出轴), 直驱=1 */

/* ---- Y 轴 (双电机同步, 共用同一套参数) ---- */
#define MOTOR_Y_WHEEL_DIAMETER_CM      10.0f    /* 主动轮直径(cm), 量实物填 */
#define MOTOR_Y_WHEEL_CIRCUMFERENCE_CM (MECH_PI * MOTOR_Y_WHEEL_DIAMETER_CM)  /* 派生: 每转行程 */
#define MOTOR_Y_GEAR_RATIO             1.0f     /* 减速比, 直驱=1 */

/* Emm_V5 位置模式 FD 的 clk 单位 = 细分脉冲 (非编码器值65536!)
 * 16细分下 3200脉冲/圈 (200步×16细分), X/Y 共用。
 * 改细分时同步改 MOTOR_XY_MICROSTEP, 每转脉冲自动 = 200×细分。 */
#define MOTOR_XY_STEPS_PER_REV        200       /* 1.8°电机每转步数 (360/1.8) */
#define MOTOR_XY_MICROSTEP            16        /* 驱动器细分 (MStep菜单), 须与电机设置一致 */
#define MOTOR_XY_PULSE_PER_REV        ((float)(MOTOR_XY_STEPS_PER_REV * MOTOR_XY_MICROSTEP))  /* 3200 脉冲/圈 */

#endif /* __MECH_PARAMS_H */
