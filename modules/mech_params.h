/**
  ******************************************************************************
  * @file    mech_params.h
  * @brief   集中机械参数配置 (升降 / 底盘)
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
/* M2006 为 "P36" 减速电机, 自带 36:1 减速器, 电机轴→主动轮非直驱。
 * 位移换算必须计入减速比 (见 lift.c Lift_AngleToDisplacement)。 */

#define LIFT_WHEEL_DIAMETER_M         0.18f     /* 主动轮直径 (m) */
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
 * 位置命令 clk 单位 = 编码器 4 倍频后的每转脉冲数 (65536)。 */

#define CHASSIS_WHEEL_DIAMETER_CM     18.0f     /* 轮径 (cm) */
/* 周长由直径派生, 勿手填 */
#define CHASSIS_WHEEL_CIRCUMFERENCE_CM  (MECH_PI * CHASSIS_WHEEL_DIAMETER_CM)
#define CHASSIS_PULSE_PER_REV         65536.0f  /* Emm_V5 每转脉冲 (编码器4倍频, 一圈0-65535) */


/* ================================================================== */
/* ===== 横移机构: 单 Emm_V5 步进 + 同步带同步轮 =================== */
/* ================================================================== */
/* Emm_V5 步进闭环, 电机轴直驱同步轮(无减速), 同步带带动横向移动。
 * 位置命令 clk 单位 = 每转脉冲 65536 (编码器4倍频)。
 * ⚠️ LATERAL_PULLEY_CIRCUMFERENCE_CM 为占位值, 实测同步轮周长后改此处。
 * ⚠️ LATERAL_DIR_INVERT: 若实际方向与指令相反(上升命令却下降), 改 1 翻转。 */
#define LATERAL_PULLEY_CIRCUMFERENCE_CM  6.0f    /* 同步轮周长 (cm) 占位值, 待实测 */
#define LATERAL_PULSE_PER_REV            65536.0f
#define LATERAL_DIR_INVERT               0       /* 方向反转标志: 0=正常 1=翻转CW/CCW与编码器符号 */

#endif /* __MECH_PARAMS_H */
