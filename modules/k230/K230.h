/**
  ******************************************************************************
  * @file    K230.h
  * @brief   K230 视觉模块通信接口 (Modules层)
  ******************************************************************************
  * 移植自参考工程 crane_1 的 K230.h, 适配本工程 BSP 分层。
  *
  * 通信: USART3 (PB10 TX / PB11 RX), 115200 8N1, DMA 收发
  *   发送: k230_write(command) 发 8 字节
  *     [0]=command, [1]=1(关闭时=0), [2..7]=0
  *   接收: K230 发 8 字节, [0]=帧类型:
  *     0x02 = 豆子颜色帧, [2..4]=三颗豆子颜色(BEAN_GREEN/YUN/YELLOW)
  *     0x01 = 数字位置帧, [2..6]=五个箱子的数字编号(1=黄/2=绿/3=白)
  *
  * 典型数据流:
  *   k230_write(K230_CMD_LOOK_BEAN)   -> K230 看豆 -> bean_color[3] 填充, bean_flag=1
  *   k230_write(K230_CMD_LOOK_NUMBER) -> K230 看正面数字 (K230端保存, 不回传)
  *   k230_write(K230_CMD_LOOK_SIDE)   -> K230 看侧面数字 -> number_position[5] 填充
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

/* ---- K230 命令码 (k230_write 参数, byte[0], 与 K230 端匹配) ----
 * 摄像头对应关系以 view/K230比赛通讯协议.md 与 k230_competition.py 为准:
 *   2=看豆(cam2), 3=看正面数字(cam1), 1=看侧面数字(cam0), 6=关闭/应答 */
#define K230_CMD_LOOK_BEAN      2
#define K230_CMD_LOOK_NUMBER    3
#define K230_CMD_LOOK_SIDE      1
#define K230_CMD_CLOSE          6

/* ---- 豆子颜色编码 (K230 返回) ---- */
#define BEAN_GREEN   0x06   /* 绿豆 */
#define BEAN_YUN     0x07   /* 芸豆 */
#define BEAN_YELLOW  0x08   /* 黄豆 */

/* ---- Bean_Show LED 指示灯引脚 (诊断用, 按需修改) ---- */
#define K230_LED_PORT      GPIOB
#define K230_LED_PIN       GPIO_PIN_2

/* ---- K230 接收缓冲 (DMA 直接写入, 8 字节, byte[7]=XOR校验) ---- */
extern uint8_t K230_Rx[K230_RX_BUF_SIZE];

/* ---- 解析后的数据 ---- */
/* 豆子颜色(0x06绿/0x07芸/0x08黄), 下标0-2对应第1-3颗 */
extern __IO uint8_t bean_color[3];
/* 数字位置编号(1=黄/2=绿/3=云), 下标0-4对应箱子1-5(左到右) */
extern __IO uint8_t number_position[5];
/* 豆子数据就绪标志(1=有新数据), 消费后清零 */
extern __IO uint8_t bean_flag;
/* bean_color 锁: 收到一帧后置1, 防止覆盖; 需外部清零才能接收下一帧 */
extern __IO uint8_t bean_locked;
/* 正面3个数字(K230 cam2 识别), 下标0-2 */
extern __IO uint8_t front_number[3];
/* 正面数字就绪标志(1=有新数据), 消费后清零 */
extern __IO uint8_t front_number_flag;
/* 完整5数字就绪标志(1=有新数据), 消费后清零 */
extern __IO uint8_t full_number_flag;
/* 数字数据帧计数(每收到一帧非全零 number_position 自增) */
extern __IO uint8_t count;
/* K230 ACK (0x0A) 计数器: 每收到一帧 0x0A 自增, 消费后自减或清零。
 * 用计数器而非布尔标志, 避免 Phase2 连发两个 ACK 时丢掉第二个。 */
extern __IO uint8_t k230_ack_flag;
/* K230 就绪标志(1=模型加载完成): 上电握手用, 主机等到此标志才发 LOOK_BEAN */
extern __IO uint8_t k230_ready_flag;

/* ---- 接收统计(诊断用, 调试器观察是否丢帧) ---- */
extern __IO uint32_t k230_rx_frames;     /* 成功解析的整帧数 */
extern __IO uint32_t k230_rx_badxor;     /* XOR 校验失败帧数 */
extern __IO uint32_t k230_rx_badlen;     /* 长度非 8 的残帧数 */
extern __IO uint32_t k230_rx_rearm_err;  /* 重新武装 DMA 失败次数 */

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
  * @brief  初始化 K230 模块: 注册 USART3 接收回调 + 启动 DMA 接收
  * @note   在 App_Init 中调用 (UART_Callback_Init 之后)
  */
void K230_Init(void);

/**
  * @brief  USART3 接收事件回调: IDLE 切帧后解析 K230 数据帧 + 重新武装接收
  * @param  huart  USART3 句柄
  * @param  size   本次收到的字节数, 非 8 视为残帧丢弃
  * @note   由 UART_Callback_RegisterEvent 注册, 中断上下文调用。
  */
void k230_read(UART_HandleTypeDef *huart, uint16_t size);

/**
  * @brief  向 K230 发送命令
  * @param  command  命令码: K230_CMD_LOOK_BEAN/NUMBER/SIDE/CLOSE
  * @retval 0=发送成功, -1=发送失败(总线忙/超时), 调用方应重试
  */
int k230_write(uint8_t command);

/**
  * @brief  根据 key 查找其在 number_position 中的位置(1~5), 执行 Action_1..5
  * @param  key  目标豆子颜色: BEAN_GREEN/BEAN_YELLOW/BEAN_YUN
  * @retval >0 命中的位置(1~5); -1=未知颜色; -2=该数字不在 number_position 中
  *         (K230 识别错或第5位推理失败, 调用方须处理, 不可当成功)
  */
int Data_Handle1(uint8_t key);

/* ==================== K230 通讯协议层 (三阶段主机通讯) ==================== */
/* 从 action.c 移植而来: 发命令->等ACK->等数据->回ACK 的三阶段封装,
 * 与 k230_competition.py 的 Phase1/2/3 一一对应, 由 action.c 调用。
 *
 * K230 端(k230_competition.py)是严格的三阶段状态机, 顺序不可打乱:
 *   Phase1 LOOK_BEAN(0x02)   -> ACK -> 豆子颜色数据帧 -> 等主机 CLOSE(0x06)
 *   Phase2 LOOK_NUMBER(0x03) -> ACK -> (识别完再发一个 ACK, 无数据帧) -> 等 CLOSE
 *   Phase3 LOOK_SIDE(0x01)   -> ACK -> 五数字数据帧 -> 等主机 CLOSE(0x06)
 * 因此每一步都必须"发命令->等ACK->等数据->回ACK", 不能只 k230_write 后盲等
 * osDelay: 对端没跟上时主机会拿着上一轮的过期数据继续跑。 */

/* ---- K230 通讯超时/重试 ---- */
#define K230_ACK_TIMEOUT_MS      3000    /* 等对端 ACK */
/* 必须大于 K230 端 RECOGNIZE_TIMEOUT_MS(20s) + 摄像头启动 200ms + 余量。
 * 小于它就会出现 5s 错位窗口: K230 还在识别循环里没出结果, 主机已判超时并发
 * CLOSE 重同步 + 重发命令; K230 识别完发出的是数据帧而不是 ACK, 主机不认继续
 * 等 ACK 超时, 而 K230 转去等 CLOSE 时正好读到主机重同步发的那个 0x06, 误判
 * 本阶段成功并进入下一阶段 —— 两端就此错开一个阶段。 */
#define K230_DATA_TIMEOUT_MS     25000   /* 等识别结果(含推理耗时) */
#define K230_RETRY_MAX           3       /* 重试次数, 只在一层生效, 勿嵌套 */
#define K230_READY_TIMEOUT_MS    30000   /* 等 K230 就绪(0x0B): 模型加载可能很慢 */

/**
  * @brief  上电握手: 阻塞等 K230 就绪(0x0B), 超时也放行
  * @note   必须在 k230_phase1_bean 之前调用: K230 加载模型慢, 主机先等就绪帧
  *         再发命令, 否则命令会在 K230 就绪前发出而被漏掉。
  * @retval 0=收到就绪帧, -1=超时(仍可继续)
  */
int k230_wait_ready(void);

/**
  * @brief  Phase1: 豆子颜色识别, 结果填入 bean_color[3]
  * @retval 0=成功, -1=重试耗尽
  */
int k230_phase1_bean(void);

/**
  * @brief  Phase2: 正面数字识别(K230 端自存, 不回传数据帧, 只发两个 ACK)
  * @retval 0=成功, -1=重试耗尽
  */
int k230_phase2_front(void);

/**
  * @brief  Phase3: 侧面数字 + 推理第5位, 结果填入 number_position[5]
  * @retval 0=成功, -1=重试耗尽
  */
int k230_phase3_side(void);

#endif /* __K230_H */
