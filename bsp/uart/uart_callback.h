/**
  ******************************************************************************
  * @file    uart_callback.h
  * @brief   UART接收回调统一分发 (BSP层)
  ******************************************************************************
  * HAL_UART_RxCpltCallback 是弱函数, 全工程只能有一个强定义。
  * 本模块作为全局回调路由:
  *   USART1 → 内建 VOFA+ 逐字节接收, 拼行后入命令消息队列
  *   其它串口 → 转发给上层模块通过 UART_Callback_Register() 注册的回调
  *
  * 分层约束: 不得 #include 任何 modules/ 头文件。业务模块(如步进电机)
  *           应自行注册回调, 而非由本文件直接调用其符号。
  *
  * 命令传递: USART1 中断拼完一行 → osMessageQueue(命令行字符串)。
  *           消费者(CommandTask)调用 UART_Callback_GetCmdQueue() 取队列句柄,
  *           阻塞 osMessageQueueGet 等待命令, 实现"有命令才唤醒"的事件驱动。
  ******************************************************************************
  */

#ifndef __UART_CALLBACK_H
#define __UART_CALLBACK_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

/** @brief VOFA 单条命令最大长度(含'\0'), 调用方可据此分配命令缓冲区 */
#define VOFA_RX_BUF_SIZE  64

/**
  * @brief  UART 接收完成回调函数类型
  * @param  huart  触发回调的 UART 句柄
  */
typedef void (*UART_RxCompleteHandler_t)(UART_HandleTypeDef *huart);

/**
  * @brief  注册某串口实例的接收完成回调
  * @note   由上层模块(Modules/App)在初始化时调用, 实现控制反转,
  *         避免本 BSP 文件反向依赖 modules 层。重复注册同一实例会覆盖旧回调。
  * @param  instance  UART 外设实例 (如 USART1/USART6)
  * @param  handler   回调函数指针, NULL 表示注销
  */
void UART_Callback_Register(USART_TypeDef *instance, UART_RxCompleteHandler_t handler);

/**
  * @brief  初始化UART回调分发模块
  * @note   启动 USART1 逐字节中断接收(VOFA命令), 并创建命令消息队列。
  *         需在 VOFA 命令消费者任务启动前调用。
  */
void UART_Callback_Init(void);

/**
  * @brief  获取VOFA命令消息队列句柄
  * @retval 队列句柄, 消费者用 osMessageQueueGet 阻塞等待命令行字符串。
  *         队列内每个元素为 char[VOFA_RX_BUF_SIZE] 的命令行(含'\0')。
  * @note   队列在 UART_Callback_Init 时创建, 深度 VOFA_CMD_QUEUE_DEPTH。
  */
osMessageQueueId_t UART_Callback_GetCmdQueue(void);

#endif /* __UART_CALLBACK_H */
