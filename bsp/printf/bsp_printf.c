/**
  ******************************************************************************
  * @file    bsp_printf.c
  * @brief   printf 重定向: fputc 行缓冲 -> 日志队列 (经 LogTask DMA 发 USART1)
  ******************************************************************************
  * fputc 不再直接 HAL_UART_Transmit 阻塞发, 而是把字符累积到行缓冲, 遇 '\n'
  * 或缓冲满时整行入日志队列(osMessageQueue), 由 bsp_log.c 的 LogTask 统一
  * DMA 发送到 USART1。这样 printf 与 LOG 宏都走同一队列, USART1 仅 LogTask
  * 访问, 从根本上消除多任务并发抢 USART1 致 HardFault 的风险。
  *
  * 行缓冲竞态: static buf/idx 用 taskENTER_CRITICAL 保护(临界区极短, 仅存一
  * 字节+判换行+偶尔 memcpy), 多任务并发 printf 不会交错撕裂。
  * flush 时先拷贝到栈 tmp 再出队(critical 外), 避免 buf 被覆盖。
  *
  * BSPLogInit 之前(队列未建): Log_EnqueueLine 静默丢弃, fputc 不阻塞, 启动早期
  * 日志丢失可接受。
  ******************************************************************************
  */

#include "stdio.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "bsp_log.h"

extern UART_HandleTypeDef huart1;

int fputc(int ch, FILE *f)
{
    (void)f;
    static char buf[LOG_LINE_MAX_LEN];
    static int idx = 0;
    char tmp[LOG_LINE_MAX_LEN];
    bool flush = false;

    /* 临界区保护行缓冲, 防多任务并发 printf 交错 */
    taskENTER_CRITICAL();
    buf[idx++] = (char)ch;
    if (ch == '\n' || idx >= LOG_LINE_MAX_LEN - 2) {
        buf[idx] = '\0';
        memcpy(tmp, buf, (size_t)(idx + 1));  /* 拷贝到栈, critical 外入队 */
        flush = true;
        idx = 0;
    }
    taskEXIT_CRITICAL();

    if (flush) {
        Log_EnqueueLine(tmp);  /* 入队, LogTask DMA 发; 队列未建或满则丢 */
    }
    return ch;
}
