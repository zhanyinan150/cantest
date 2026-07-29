#ifndef __ACTION_H
#define __ACTION_H

#include <stdint.h>

/**
 * @brief 初始化动作序列: 创建 action_test 任务跑抓豆子->放箱子完整动作
 * @note  在 App_Init 里调用(电机使能后)。与 MotorAutoTask 互斥(都调 Motor_XYZ),
 *        使能 Action_Init 时应停用 MotorAutoTask, 不可同时运行。
 */
void Action_Init(void);

/**
 * @brief  从当前箱子直线移动X轴到目标箱子 (Y/Z不动)
 *         用坐标差算位移: delta = 目标坐标 - 当前坐标, 一步到位。
 * @param  target_box  目标箱子编号 1~5
 */
void goto_box(uint8_t target_box);

#endif /* __ACTION_H */
