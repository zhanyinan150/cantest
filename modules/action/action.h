#ifndef __ACTION_H
#define __ACTION_H

/**
 * @brief 初始化动作序列: 创建 action_1 任务跑抓豆子->放箱子完整动作
 * @note  在 App_Init 里调用(电机使能后)。与 MotorAutoTask 互斥(都调 Motor_XYZ),
 *        使能 Action_Init 时应停用 MotorAutoTask, 不可同时运行。
 */
void Action_Init(void);

#endif /* __ACTION_H */
