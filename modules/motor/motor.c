/**
  ******************************************************************************
  * @file    motor.c
  * @brief   XYZ 三轴联动控制 (Modules层)
  ******************************************************************************
  * 详见 motor.h
  ******************************************************************************
  */

#include "motor.h"
#include "Emm_V5_CAN.h"   /* X/Y 步进 (CAN2) */
#include "lift.h"         /* Z 升降 (CAN1, M2006 闭环) */
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "math.h"
#include "bsp_log.h"
#include "cmd_register.h" /* CMD_Register / CMD_Handler_t */
#include "stdio.h"        /* sscanf */

/* 注意: 不可 #include "Emm_V5.h" (UART版), 其 S_VER 等枚举与 Emm_V5_CAN.h 冲突,
 *       app_init.c 已有同样约束。Y轴仍用 CAN 版驱动(Emm_V5_CAN.h)。
 * X轴改用 UART5 (Emm_V5.c) 通信, 故对所需函数作前向声明(不可 include Emm_V5.h)。
 * (bool 已由 motor.h -> <stdbool.h> 引入)
 * 机械参数(周长/减速比/每转脉冲)由 motor.h -> mech_params.h 提供, 不在本文件定义。 */
extern void Emm_V5_En_Control(uint8_t addr, bool state, bool snF);
extern void Emm_V5_Pos_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, uint32_t clk, bool raF, bool snF);

/* ==================== 内部辅助 ==================== */

/**
 * @brief  距离(cm)换算为步进电机脉冲数
 *         公式: clk = (距离 / 每转行程) * 每转脉冲 * 减速比
 * @param  distance_cm      目标移动距离(cm), <=0 时返回 0
 * @param  circumference_cm 每转直线行程(cm): 同步带=带轮周长, 丝杠=导程, 齿轮齿条=齿轮周长
 * @param  gear_ratio       减速比(电机轴:输出轴), 直驱=1.0
 * @return 脉冲数(>=1); 距离非正或行程非正时返回 0
 */
static uint32_t Motor_DistanceToClk(float distance_cm, float circumference_cm,
                                    float gear_ratio)
{
    if (distance_cm <= 0.0f || circumference_cm <= 0.0f)
        return 0;
    float clk = (distance_cm / circumference_cm) * MOTOR_XY_PULSE_PER_REV * gear_ratio;
    if (clk < 1.0f) clk = 1.0f;
    return (uint32_t)clk;
}

/**
 * @brief  X/Y 转速限幅到 [0, MOTOR_XY_VEL_MAX]
 * @param  vel 期望转速(RPM)
 * @return 限幅后的转速, 超上限返回 MOTOR_XY_VEL_MAX
 */
static uint16_t Motor_ClampVel(uint16_t vel)
{
    return (vel > MOTOR_XY_VEL_MAX) ? MOTOR_XY_VEL_MAX : vel;
}

/**
 * @brief  X/Y 加速度档位限幅到 [0, MOTOR_XY_ACC_MAX]
 * @param  acc 期望加速度档位(0~255, 0=直接启动)
 * @return 限幅后的加速度档位
 */
static uint8_t Motor_ClampAcc(uint8_t acc)
{
    return (acc > MOTOR_XY_ACC_MAX) ? MOTOR_XY_ACC_MAX : acc;
}

/* ==================== 公开接口 ==================== */

/**
 * @brief  使能 X/Y 轴步进电机 (Z 轴由 Lift_Init 内部 DJIMotorEnable 使能)
 * @note   X(3) + Y双电机(1,2) 共三只步进, 需在 Emm_V5_CAN_Init 注册后调用
 *         Y双电机使能固定 snF=true 同步预存, 末尾广播 Synchronous_motion 同时触发
 *         (mxyz 命令注册已随 cmd_mxyz 删除而移除, CommandTask 停用)
 */
void Motor_Init(void)
{
    Emm_V5_En_Control(MOTOR_X_ADDR,   true, false);  /* X单电机改用 UART5 (Emm_V5.c) */
    /* Y双电机使能固定 snF=true 同步: 预存后由 Synchronous_motion 同时触发,
     * 保证两电机同时上电, 避免单边先使能导致机构受力不均。 */
    Emm_V5_CAN_En_Control(MOTOR_Y_ADDR_1, true, true);
    Emm_V5_CAN_En_Control(MOTOR_Y_ADDR_2, true, true);
    Emm_V5_CAN_Synchronous_motion(0);  /* 广播触发 Y1/Y2 同步使能 */
}

/**
 * @brief  XYZ 三轴联动移动 (阻塞, 须在任务上下文调用, 非中断)
 * @param  x_dir       X轴方向, 透传 Emm_V5 (0=CW顺时针, 1=CCW逆时针), 实际左右按电机安装
 * @param  x_vel       X轴转速(RPM), 0~MOTOR_XY_VEL_MAX, 超出自动限幅
 * @param  x_acc       X轴加速度档位 0~255, 0=直接启动
 * @param  x_distance  X轴移动距离(cm), >0 有效, 0=该轴不动
 * @param  y_dir       Y轴方向, 透传 Emm_V5 (0=CW, 1=CCW), 实际前后按电机安装
 *                     (Y轴双电机同向, 若安装镜像需在 motor.c 内对其中一只翻转 dir)
 * @param  y_vel       Y轴转速(RPM), 0~MOTOR_XY_VEL_MAX, 超出自动限幅, 双电机共用
 * @param  y_acc       Y轴加速度档位 0~255, 0=直接启动, 双电机共用
 * @param  y_distance  Y轴移动距离(cm), >0 有效, 0=该轴不动, 双电机发相同脉冲
 * @param  z_dir       Z轴升降方向 (0=上, 1=下)
 * @param  z_distance  Z轴移动距离(cm), >0 有效, 0=不动
 * @return 0=全轴到位(X&&Y1&&Y2&&Z), -1=超时/堵转
 * @note   - Y轴双电机(ID 1,2)固定 snF=true 多机同步走直线: 两电机发完全相同的
 *           dir/vel/acc/clk + snF=true 预存, 广播 Synchronous_motion 同时触发。
 *           X与Y一起动时加入同步(三轴联动), X单独动时直接发。
 *         - Z 由 Lift_Up/Down 设目标后 lift 内部 LiftTask(20ms)闭环跟进。
 *         - Y到位判定需 Y1 与 Y2 均到位(双电机都 ARRIVED)。
 *         - 距离->脉冲换算用 mech_params.h 的 MOTOR_X/Y_WHEEL_CIRCUMFERENCE_CM。
 *         - 单位 cm(参考工程为 mm)。
 */
int Motor_XYZ(uint8_t x_dir, uint16_t x_vel, uint8_t x_acc, float x_distance,
              uint8_t y_dir, uint16_t y_vel, uint8_t y_acc, float y_distance,
              uint8_t z_dir, float z_distance)
{

    bool x_active = (x_distance > 0.0f);
    bool y_active = (y_distance > 0.0f);
    bool z_active = (z_distance > 0.0f);

    printf("[motor] Motor_XYZ start: x=%d y=%d z=%d (1=active)\r\n",
           (int)x_active, (int)y_active, (int)z_active);

    uint16_t vel_x = Motor_ClampVel(x_vel);
    uint8_t  acc_x = Motor_ClampAcc(x_acc);
    uint16_t vel_y = Motor_ClampVel(y_vel);
    uint8_t  acc_y = Motor_ClampAcc(y_acc);

    /* ===== 2. 启动 Z 升降 (CAN1, lift 闭环) =====
     * lift 接口为相对位移: Lift_Up=向上, Lift_Down=向下。lift 内部 LiftTask(20ms)
     * 持续 PID 跟进 target, 无需此处阻塞发脉冲。读取设好后的 target 供到位判断。 */
    float z_target_cm = 0.0f;
    if (z_active)
    {
        if (z_dir == 0)
            Lift_Up(z_distance); /* 0=上 */
        else
            Lift_Down(z_distance); /* 1=下 */
        z_target_cm = Lift_GetStatus()->target_displacement;
        printf("[motor] Z start dir=%d target=", (int)z_dir);
        Log_PrintFloat2("", z_target_cm);  /* MicroLIB 下 printf("%.2f") 会 HardFault, 用 %d 拼接 */
        printf("cm\r\n");
    }
    osDelay(20);
        /* ===== 1. 启动 X/Y 步进 (CAN2) =====
         * Y轴双电机(ID 1,2)固定 snF=true 多机同步: 两电机发完全相同的
         *   dir / vel / acc / clk + snF=true 预存, 广播 Synchronous_motion 同时触发,
         *   从电气上消除两电机起步时差, 走直线不跑偏。
         * sync 仅用于决定是否发广播同步信号: Y动则必发(触发Y1/Y2预存命令)。 */

    if (x_active)
    {
        uint32_t clk_x = Motor_DistanceToClk(x_distance, MOTOR_X_WHEEL_CIRCUMFERENCE_CM, MOTOR_X_GEAR_RATIO);
        Emm_V5_Pos_Control(MOTOR_X_ADDR, x_dir, vel_x, acc_x, clk_x, false, false);  /* X改用 UART5 (Emm_V5.c) */
        printf("[motor] X sent clk=%lu vel=%u acc=%u (UART5)\r\n",
               (unsigned long)clk_x, (unsigned)vel_x, (unsigned)acc_x);
        osDelay(20);
    }
    if (y_active)
    {
        uint32_t clk_y = Motor_DistanceToClk(y_distance, MOTOR_Y_WHEEL_CIRCUMFERENCE_CM, MOTOR_Y_GEAR_RATIO);
        /* Y1/Y2 固定 snF=true 同步预存, 配合 Synchronous_motion 同时启动。
         * 若两电机安装方向镜像, 将其中一行的 y_dir 改为 (!y_dir)。 */
        Emm_V5_CAN_Pos_Control(MOTOR_Y_ADDR_1, y_dir, vel_y, acc_y, clk_y, false, true);
        Emm_V5_CAN_Pos_Control(MOTOR_Y_ADDR_2, !y_dir, vel_y, acc_y, clk_y, false, true);
    
        Emm_V5_CAN_Synchronous_motion(0);  /* 广播地址0, 触发所有预存电机同步启动 */
        printf("[motor] Y sent clk=%lu vel=%u acc=%u Y1dir=%d Y2dir=%d (CAN2+sync)\r\n",
               (unsigned long)clk_y, (unsigned)vel_y, (unsigned)acc_y,
               (int)y_dir, (int)(!y_dir));
		}

    /* ===== 3. 轮询等待全轴到位 (osDelay, 不忙等) =====
     * Y到位 = Y1 && Y2 均到位(双电机都 ARRIVED) */
    bool x_done = true;  /* X改用 UART5, 发送后 osDelay 等待, 不轮询到位 */
    bool y1_done = !y_active, y2_done = !y_active;
    bool z_done = !z_active;
    uint32_t start = osKernelGetTickCount();
    uint32_t timeout = (MOTOR_XY_TIMEOUT_MS > MOTOR_Z_TIMEOUT_MS)
                       ? MOTOR_XY_TIMEOUT_MS : MOTOR_Z_TIMEOUT_MS;

    for (;;)
    {
        /* Y 到位检测(X轴改用 UART5, 无到位查询, 已 osDelay 等待, x_done 恒 true):
         * S_FLAG &0x02=到位 &0x04=堵转 &0x08=堵转保护。
         * Read_Flag 阻塞等待该电机响应(≤100ms), 失败返回-1下轮重试。 */
        if (y_active && !y1_done)
        {
            int32_t f = Emm_V5_CAN_Read_Flag(MOTOR_Y_ADDR_1);
            if (f >= 0)
            {
                if (f & (EMM_FLAG_STALL | EMM_FLAG_STALL_PROT))
                { LOGERROR("[motor] Y1堵转 flag=0x%X", (unsigned)f); return -1; }
                if (f & EMM_FLAG_ARRIVED) y1_done = true;
            }
        }
        if (y_active && !y2_done)
        {
            int32_t f = Emm_V5_CAN_Read_Flag(MOTOR_Y_ADDR_2);
            if (f >= 0)
            {
                if (f & (EMM_FLAG_STALL | EMM_FLAG_STALL_PROT))
                { LOGERROR("[motor] Y2堵转 flag=0x%X", (unsigned)f); return -1; }
                if (f & EMM_FLAG_ARRIVED) y2_done = true;
            }
        }
        /* Z 到位检测: 位移误差 <= 容差 (与 lift.c::Lift_WaitUntilAtTarget 一致) */
        if (z_active && !z_done)
        {
            float err = fabsf(Lift_GetCurrentDisplacement() - z_target_cm);
            if (err <= MOTOR_Z_TOLERANCE_CM)
                z_done = true;
        }

        if (x_done && y1_done && y2_done && z_done)
            return 0;

        if ((osKernelGetTickCount() - start) >= timeout)
        {
            LOGERROR("[motor] XYZ超时 x=%d y1=%d y2=%d z=%d",
                    (int)x_done, (int)y1_done, (int)y2_done, (int)z_done);
            return -1;
        }
        osDelay(MOTOR_POLL_PERIOD_MS);
    }
}

