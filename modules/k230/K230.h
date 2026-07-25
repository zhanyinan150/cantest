/**
  ******************************************************************************
  * @file    K230.h
  * @brief   K230 视觉模块通信接口 (Modules层)
  ******************************************************************************
  * 移植自参考工程 crane_1 的 K230.h, 适配本工程 BSP 分层。
  *
  * 通信: USART2 (PA2 TX / PA3 RX), 115200 8N1, DMA 收发
  *   发送: k230_write(command) 发 8 字节
  *     [0]=command, [1]=1(关闭时=0), [2..7]=0
  *   接收: K230 发 8 字节, [0]=帧类型:
  *     0x02 = 豆子颜色帧, [2..4]=三颗豆子颜色(BEAN_GREEN/YELLOW/WHITE)
  *     0x01 = 数字位置帧, [2..6]=五个箱子的数字编号(1=黄/2=绿/3=白)
  *
  * 典型数据流:
  *   k230_write(1)  -> K230 看豆 -> bean_color[3] 填充, bean_flag=1
  *   k230_write(2)  -> K230 看中间数字 -> number_position[5] 填充
  *   Data_Handle1(颜色) -> 查 number_position 得位置 1~5 -> 调 Action_1..5
  *   Data_Handle1_1(颜色) -> 同上 -> 调 Action_6..10
  ******************************************************************************
  */
#ifndef __K230_H
#define __K230_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ---- 通信参数 ---- */
#define K230_RX_BUF_SIZE    8       /* K230 每帧收发 8 字节 */

/* ---- K230 命令码 (k230_write 参数) ---- */
/* 1=开启看豆, 2=开启看中间数字, 3=看侧面数字, 6=关闭摄像头 */
#define K230_CMD_LOOK_BEAN      1
#define K230_CMD_LOOK_NUMBER    2
#define K230_CMD_LOOK_SIDE      3
#define K230_CMD_CLOSE          6

/* ---- 豆子颜色编码 (K230 返回) ---- */
#define BEAN_YELLOW  0x07   /* 黄豆 */
#define BEAN_GREEN   0x06   /* 绿豆 */
#define BEAN_YUN     0x08   /* 云豆 */

/* ---- Bean_Show LED 指示灯引脚 (诊断用, 按需修改) ---- */
#define K230_LED_PORT      GPIOB
#define K230_LED_PIN       GPIO_PIN_2

/* ---- K230 接收缓冲 (DMA 直接写入, 8 字节) ---- */
extern uint8_t K230_Rx[K230_RX_BUF_SIZE];

/* ---- 解析后的数据 ---- */
/* 豆子颜色(0x06绿/0x07黄/0x08云), 下标0-2对应第1-3颗 */
extern __IO uint8_t bean_color[3];
/* 数字位置编号(1=黄/2=绿/3=云), 下标0-4对应箱子1-5 */
extern __IO uint8_t number_position[5];
/* 豆子数据就绪标志(1=有新数据), Bean_Show 消费后清零 */
extern __IO uint8_t bean_flag;
/* bean_color 锁: 收到一帧后置1, 防止覆盖; 需外部清零才能接收下一帧 */
extern __IO uint8_t bean_locked;
/* 数字数据帧计数(每收到一帧非全零 number_position 自增) */
extern __IO uint8_t count;

/* ---- K230 触发的动作组 (在 action.c 中实现, 此处仅声明) ----
 * Data_Handle1 根据 key(豆子颜色) 查 number_position 得位置 1~5, 调 Action_1..5
 * Data_Handle1_1 同上, 调 Action_6..10
 * K230.c 提供弱定义空桩, 用户在 action.c 中以同名强定义覆写即可 */
extern void Action_1(void);
extern void Action_2(void);
extern void Action_3(void);
extern void Action_4(void);
extern void Action_5(void);
extern void Action_6(void);
extern void Action_7(void);
extern void Action_8(void);
extern void Action_9(void);
extern void Action_10(void);

/* ---- 公开接口 ---- */

/**
  * @brief  初始化 K230 模块: 注册 USART2 接收回调 + 启动 DMA 接收
  * @note   在 App_Init 中调用 (UART_Callback_Init 之后)
  */
void K230_Init(void);

/**
  * @brief  USART2 DMA 接收完成回调: 解析 K230 数据帧 + 重启 DMA 接收
  * @note   由 UART_Callback_Register 注册, DMA 收满 8 字节后中断上下文调用;
  *         也可在外部手动调用以重新解析当前缓冲 (需传入 &huart2)
  */
void k230_read(UART_HandleTypeDef *huart);

/**
  * @brief  向 K230 发送命令
  * @param  command  命令码: K230_CMD_LOOK_BEAN/NUMBER/SIDE/CLOSE
  */
void k230_write(uint8_t command);

/**
  * @brief  根据 key 查找其在 number_position 中的位置(1~5), 执行 Action_1..5
  * @param  key  目标豆子颜色: BEAN_GREEN/BEAN_YELLOW/BEAN_YUN
  */
void Data_Handle1(uint8_t key);

/**
  * @brief  Data_Handle1 的第二组动作 (Action_6..10)
  * @param  key  目标豆子颜色: BEAN_GREEN/BEAN_YELLOW/BEAN_YUN
  */
void Data_Handle1_1(uint8_t key);

/**
  * @brief  LED 闪烁显示 bean_color 序列 (诊断用, 须在任务上下文调用)
  *         黄豆闪1次, 绿豆闪2次, 云豆闪3次; 阻塞等待 bean_flag
  */
void Bean_Show(void);

#endif /* __K230_H */
