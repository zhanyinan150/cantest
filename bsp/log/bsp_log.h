#ifndef _BSP_LOG_H
#define _BSP_LOG_H

#include <stdio.h>
#include <stdbool.h>

/* ---- 日志队列参数 ---- */
#define LOG_QUEUE_DEPTH    16    /* 队列深度(行数), 满则丢弃新日志 */
#define LOG_LINE_MAX_LEN   128   /* 单行最大字节(含'\0'和\r\n) */

/**
 * @brief 初始化日志系统: 创建日志队列 + LogTask (DMA 发 USART1)
 * @note  需在调度器启动后(如 App_Init 开头)调用。之后所有 printf/LOG 经队列
 *        由 LogTask 集中 DMA 输出到 USART1, 避免多任务并发抢 USART1。
 *        调用之前(启动早期)的 printf 因队列未建而丢弃, 不阻塞。
 */
void BSPLogInit(void);

/**
 * @brief 入队一行已格式化文本(应含\r\n), 供 fputc 行缓冲 flush 用
 * @note  非阻塞, 队列满则丢弃。队列未初始化时静默丢弃。
 */
void Log_EnqueueLine(const char *line);

/**
 * @brief 格式化入队(供 LOG 宏用), 自动追加\r\n
 * @note  非阻塞, 队列满则丢弃。调用线程栈上 vsnprintf 格式化, 不占 LogTask 栈。
 */
void Log_Enqueue(const char *level, const char *fmt, ...);

#define LOG_PROTO(type, format, ...) Log_Enqueue(type, format, ##__VA_ARGS__)
#define LOG_CLEAR()

#define LOG(format, ...) LOG_PROTO("", format, ##__VA_ARGS__)

#if DISABLE_LOG_SYSTEM
#define LOGINFO(format, ...)
#define LOGWARNING(format, ...)
#define LOGERROR(format, ...)
#else
#define LOGINFO(format, ...)    LOG_PROTO("I:", format, ##__VA_ARGS__)
#define LOGWARNING(format, ...) LOG_PROTO("W:", format, ##__VA_ARGS__)
#define LOGERROR(format, ...)   LOG_PROTO("E:", format, ##__VA_ARGS__)
#endif

int PrintLog(const char *fmt, ...);
void Float2Str(char *str, float va);

/* MicroLIB 下 printf("%f") 会触发 HardFault(浮点格式化栈崩溃)。
 * 提供整数化的浮点打印辅助, 避免在 printf 中直接使用 %f。
 *   Log_PrintFloat1(label, va)  打印 label + 1位小数浮点, 如 "位移=12.3"
 *   Log_PrintFloat2(label, va)  打印 label + 2位小数浮点, 如 "位移=12.34"
 * 实现见 bsp_log.c, 内部用 %d 整数拼接, MicroLIB 安全。 */
void Log_PrintFloat1(const char *label, float va);
void Log_PrintFloat2(const char *label, float va);

#endif /* _BSP_LOG_H */
