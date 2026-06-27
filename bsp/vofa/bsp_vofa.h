/**
  ******************************************************************************
  * @file    bsp_vofa.h
  * @brief   VOFA+ JustFloat 协议波形发送接口
  ******************************************************************************
  * @attention
  * JustFloat 协议: N个4字节小端float + 4字节帧尾(0x00 0x00 0x80 0x7f)
  * 经 USART1 发送, VOFA+ 选择"JustFloat"协议即可解析绘制波形。
  * 注意: 使用VOFA时需禁用printf的USART1输出(见bsp_printf.c宏开关)。
  ******************************************************************************
  */

#ifndef __BSP_VOFA_H
#define __BSP_VOFA_H

#include "stdint.h"

/* JustFloat 帧尾 (0x00 0x00 0x80 0x7f) */
#define VOFA_JUSTFLOAT_TAIL0  0x00
#define VOFA_JUSTFLOAT_TAIL1  0x00
#define VOFA_JUSTFLOAT_TAIL2  0x80
#define VOFA_JUSTFLOAT_TAIL3  0x7f

/* 单帧最多通道数 (受发送缓冲区限制) */
#define VOFA_MAX_CHANNELS     16

/**
  * @brief  发送多个float通道(JustFloat协议), VOFA+自动分通道绘图
  * @param  data float数组指针
  * @param  n    通道数(1~16)
  * @note   阻塞发送, USART1 @115200bps 每帧约(n*4+4)*10/115200 秒
  *         4通道≈2.4ms, 建议在低优先级任务中调用, 周期≥10ms
  */
void VOFA_SendFloats(float *data, uint8_t n);

/**
  * @brief  发送单个float通道
  * @param  v 数值
  */
void VOFA_SendFloat(float v);

#endif /* __BSP_VOFA_H */
