/**
  ******************************************************************************
  * @file    cmd_register.h
  * @brief   VOFA/串口命令注册表 (BSP层)
  ******************************************************************************
  * 各业务模块(Modules/App)在初始化时调用 CMD_Register() 注册自己的命令,
  * 命令分发时由 CMD_Dispatch() 按注册名查表调用对应 handler。
  *
  * 设计目的:
  *   - 解耦: 命令分发器(BSP/App)无需知道有哪些模块/命令, 不再写 if-else 链。
  *   - 分层: 模块自注册, 避免分发器反向依赖各模块头文件。
  *
  * 数据流:
  *   USART1 中断拼行 → osMessageQueue(命令行字符串) → CommandTask 取出
  *   → CMD_Dispatch(line) → 拆出命令名+参数 → 查表 → handler(arg)
  *
  * handler 约定:
  *   arg 为命令行中命令名之后的剩余字符串(已跳过空格), 无参数时为空串 ""。
  *   handler 自行 sscanf 解析所需参数类型。在 CommandTask 上下文执行(非中断)。
  ******************************************************************************
  */

#ifndef __CMD_REGISTER_H
#define __CMD_REGISTER_H

#include <stdint.h>
#include <stdbool.h>

/** @brief 命令处理函数原型。arg: 命令名后的参数字符串(可为空串), 不可为 NULL */
typedef void (*CMD_Handler_t)(const char *arg);

/**
  * @brief  注册一条命令
  * @param  name    命令名(ASCII, 无空格, 长度<CMD_NAME_MAX_LEN), 例 "fwd"
  * @param  handler 处理函数
  * @retval 0 成功, -1 表满或参数非法
  * @note   同名注册覆盖旧 handler。由模块在 Init 阶段调用。
  */
int CMD_Register(const char *name, CMD_Handler_t handler);

/**
  * @brief  分发一行命令
  * @param  line 完整命令行(如 "fwd 1500" 或 "stop"), 不可为 NULL
  * @retval 0 已分发给某 handler, -1 未知命令或格式错误
  * @note   拆出第一个 token 作为命令名, 其后(跳过空格)作为 arg 传给 handler。
  *         在任务上下文调用, 非中断。
  */
int CMD_Dispatch(const char *line);

/** @brief 已注册命令数(调试/诊断用) */
uint32_t CMD_Count(void);

#endif /* __CMD_REGISTER_H */
