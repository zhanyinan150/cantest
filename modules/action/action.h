#ifndef __ACTION_H
#define __ACTION_H

#include <stdint.h>

/**
 * @brief 初始化动作序列: 创建 action_all 任务跑单爪抓豆子->放箱子完整动作
 * @note  在 App_Init 里调用(电机使能后)。与 MotorAutoTask 互斥(都调 Motor_XYZ),
 *        使能 Action_Init 时应停用 MotorAutoTask, 不可同时运行。
 *
 * 单爪结构: 只用黑爪, 分3次抓豆(左->中->右), 每次抓完去箱子放豆。
 *   - 只有第1次抓豆前做 Phase1(豆子识别)
 *   - 只有第1次放箱时做 Phase2/Phase3(箱子识别)
 */
void Action_Init(void);

/**
 * @brief  从当前箱子移动X轴到目标箱子, 然后执行完整放料流程(单爪-黑爪)
 *         流程: X移动到3号箱子 -> 准备动作组(定位爪子朝向)
 *               -> X移动到目标箱子 -> Z下降15cm -> 开爪放豆子 -> Z上升15cm复位
 * @param  target_box  目标箱子编号 1~5
 *
 * 准备动作组 (按箱子位置, 仅黑爪):
 *   箱子1->组7  箱子2-4->组12  箱子5->组9
 * 开爪放豆子动作组:
 *   黑爪->组18
 */
void goto_box(uint8_t target_box);

/**
 * @brief  只移动X轴到目标箱子位置 (Y/Z不动, 不执行放料)
 * @param  target_box  目标箱子编号 1~5
 */
void move_box(uint8_t target_box);

/**
 * @brief  从当前箱子移动到目标箱子, 只做定位(不做放料)
 *         流程: X移动到3号箱子 -> 组15(准备) -> X移动到目标箱子
 * @param  target_box  目标箱子编号 1~5
 */
void goto_box_ready(uint8_t target_box);

#endif /* __ACTION_H */
