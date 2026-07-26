/**
  ******************************************************************************
  * @file    bsp_log.c
  * @brief   日志系统: 队列驱动 + LogTask DMA 发 USART1
  ******************************************************************************
  * 数据流:
  *   任意任务调 LOG(...)/printf -> Log_Enqueue/fputc 行缓冲 -> osMessageQueue
  *                                                                ↓
  *   LogTask (专用任务, BelowNormal) -> osMessageQueueGet 取一行
  *                                     -> HAL_UART_Transmit_DMA 发 USART1
  *                                     -> 等 TxCplt 信号量 -> 取下一行
  *
  * 设计要点:
  *   - 调用方非阻塞: LOG 宏 vsnprintf 到栈缓冲后入队(0 超时, 满则丢), 不等发送。
  *   - USART1 独占: 只有 LogTask 调 HAL_UART_Transmit_DMA, printf 也走队列(fputc
  *     行缓冲入队), 故无多任务并发抢 USART1 致 HardFault 风险。
  *   - DMA 发送: 比逐字节 HAL_UART_Transmit 阻塞高效, 发送期间 LogTask 阻塞等
  *     信号量, 串行发送避免 tx_buf 被覆盖。
  *   - ISR 安全: TxCplt 回调用 xSemaphoreGiveFromISR + portYIELD_FROM_ISR。
  *
  * 限制:
  *   - MicroLIB 下 vsnprintf 不支持 %f, 用 Log_PrintFloat1/2 或 %d 拼接。
  *   - BSPLogInit 之前的 printf 因队列未建而丢弃(启动早期)。
  *   - 队列满时丢弃新日志(调试场景可接受)。
  ******************************************************************************
  */

#include "bsp_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "usart.h"   /* huart1 */

/* 日志队列: 每元素一行 char[LOG_LINE_MAX_LEN], osMessageQueuePut/Get 值拷贝 */
static osMessageQueueId_t s_log_queue = NULL;
/* DMA 发送完成信号量(FreeRTOS 原生, 可在 ISR 释放) */
static SemaphoreHandle_t s_tx_done = NULL;

#define LOG_TASK_STACK_SIZE   256                /* word, vsnprintf/strlen 不深 */
#define LOG_TASK_PRIORITY     osPriorityBelowNormal  /* 16, 低优先级不干扰控制 */

static void LogTask(void *argument);

void BSPLogInit(void)
{
    if (s_log_queue == NULL) {
        s_log_queue = osMessageQueueNew(LOG_QUEUE_DEPTH, LOG_LINE_MAX_LEN, NULL);
        configASSERT(s_log_queue != NULL);
    }
    if (s_tx_done == NULL) {
        s_tx_done = xSemaphoreCreateBinary();
        configASSERT(s_tx_done != NULL);
    }
    const osThreadAttr_t attr = {
        .name = "LogTask",
        .stack_size = LOG_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)LOG_TASK_PRIORITY,
    };
    configASSERT(osThreadNew(LogTask, NULL, &attr) != NULL);
}

void Log_EnqueueLine(const char *line)
{
    if (s_log_queue == NULL || line == NULL) return;
    osMessageQueuePut(s_log_queue, line, 0, 0);  /* 非阻塞, 满则丢 */
}

void Log_Enqueue(const char *level, const char *fmt, ...)
{
    if (s_log_queue == NULL) return;
    char line[LOG_LINE_MAX_LEN];
    int off = 0;
    if (level && level[0]) {
        off += snprintf(line + off, LOG_LINE_MAX_LEN - off, "%s ", level);
    }
    va_list args;
    va_start(args, fmt);
    off += vsnprintf(line + off, (size_t)(LOG_LINE_MAX_LEN - off), fmt, args);
    va_end(args);
    /* 追加 \r\n, 防越界 */
    if (off > LOG_LINE_MAX_LEN - 3) off = LOG_LINE_MAX_LEN - 3;
    line[off++] = '\r';
    line[off++] = '\n';
    line[off] = '\0';
    osMessageQueuePut(s_log_queue, line, 0, 0);
}

/**
  * @brief 日志输出任务: 阻塞取队列一行, DMA 发 USART1, 等完成发下一行
  */
static void LogTask(void *argument)
{
    (void)argument;
    static char tx_buf[LOG_LINE_MAX_LEN];  /* DMA 发送缓冲, LogTask 独占, 串行发送不被覆盖 */

    for (;;) {
        if (osMessageQueueGet(s_log_queue, tx_buf, NULL, osWaitForever) != osOK)
            continue;
        uint16_t len = (uint16_t)strlen(tx_buf);
        if (len == 0) continue;

        /* DMA 发送 USART1, 等完成信号量(100ms 超时兜底防 DMA 卡死) */
        if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)tx_buf, len) == HAL_OK) {
            if (xSemaphoreTake(s_tx_done, pdMS_TO_TICKS(100)) != pdTRUE) {
                /* 超时: DMA 可能仍在读 tx_buf。若直接进下一轮, 循环顶部的
                 * osMessageQueueGet 会就地覆盖 tx_buf, 正在发送的内容变乱码。
                 * 必须先中止传输确保 DMA 松开缓冲, 再复用。
                 * 用 AbortTransmit 而非 Abort: 后者会清掉 RXNEIE, 连带打断
                 * USART1 上 VOFA 命令的逐字节接收(见 bsp_vofa.c 同款注释)。 */
                HAL_UART_AbortTransmit(&huart1);
                /* 清掉可能在 Abort 之后才到达的迟到信号, 避免下一行误判已完成 */
                (void)xSemaphoreTake(s_tx_done, 0);
            }
        } else {
            osDelay(2);  /* DMA 启动失败(总线忙/错误), 短等重试下一行 */
        }
    }
}

/**
  * @brief USART1 DMA 发送完成中断回调: 释放信号量唤醒 LogTask 发下一行
  * @note  仅响应 USART1(日志口)。若将来 TelemetryTask 也用 USART1 DMA 发波形,
  *        需在此区分来源或改用互斥协调, 否则会误唤醒 LogTask。
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && s_tx_done) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(s_tx_done, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* ===== 旧接口保留(均经 printf -> fputc -> 队列, 不阻塞调用方) ===== */

int PrintLog(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vprintf(fmt, args);  /* vprintf -> fputc -> 行缓冲入队 */
    va_end(args);
    return n;
}

void Float2Str(char *str, float va)
{
    int flag = va < 0;
    int head = (int)va;
    int point = (int)((va - head) * 1000);
    head = abs(head);
    point = abs(point);
    if (flag)
        sprintf(str, "-%d.%d", head, point);
    else
        sprintf(str, "%d.%d", head, point);
}

/* MicroLIB 安全的浮点打印: 用 %d 整数拼接, 避免 printf("%f") 栈崩溃。 */
void Log_PrintFloat1(const char *label, float va)
{
    /* 1 位小数: 放大 10 倍, 四舍五入取整 */
    int neg = va < 0.0f;
    if (neg) va = -va;
    int scaled = (int)(va * 10.0f + 0.5f);
    int head = scaled / 10;
    int frac = scaled % 10;
    if (neg)
        printf("%s-%d.%d", label, head, frac);
    else
        printf("%s%d.%d", label, head, frac);
}

void Log_PrintFloat2(const char *label, float va)
{
    /* 2 位小数: 放大 100 倍, 四舍五入取整 */
    int neg = va < 0.0f;
    if (neg) va = -va;
    int scaled = (int)(va * 100.0f + 0.5f);
    int head = scaled / 100;
    int frac = scaled % 100;
    if (neg)
        printf("%s-%d.%02d", label, head, frac);
    else
        printf("%s%d.%02d", label, head, frac);
}
