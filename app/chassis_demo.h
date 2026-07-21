/**
  ******************************************************************************
  * @file    chassis_demo.h
  * @brief   底盘前进后退演示 (App层, 开机自动运行)
  ******************************************************************************
  * 程序化调用 chassis 模块 API, 演示双 Emm_V5 步进轮前进/后退:
  *   速度模式定时换向 -- 前进 N 秒 -> 缓停 -> 后退 N 秒 -> 缓停, 循环 DEMO_LOOPS 次。
  *
  * 不依赖 VOFA/串口命令: ChassisDemo_Init() 直接创建演示任务, 上电即自动运行,
  * 跑完 DEMO_LOOPS 次后 Chassis_StopNow + 自删除 (断电/复位亦可中止)。
  *
  * 依赖方向: App(chassis_demo) -> Modules(chassis) -> Core(can)
  * 前置条件: Chassis_Init() 须先执行 (双轮使能 + ChassisTask 就绪)。
  ******************************************************************************
  */

#ifndef __CHASSIS_DEMO_H
#define __CHASSIS_DEMO_H

/**
  * @brief  底盘演示初始化: 创建演示任务 (开机自动跑前进后退)
  * @note   需在 Chassis_Init() 之后调用。不注册任何串口命令。
  * @retval 0 成功, -1 任务创建失败
  */
int ChassisDemo_Init(void);

#endif /* __CHASSIS_DEMO_H */
