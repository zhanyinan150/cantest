#ifndef _BSP_LOG_H
#define _BSP_LOG_H

#include <stdio.h>

void BSPLogInit(void);

#define LOG_PROTO(type, format, ...) \
        printf("  %s" format "\r\n", type, ##__VA_ARGS__)

#define LOG_CLEAR()

#define LOG(format, ...) LOG_PROTO("", format, ##__VA_ARGS__)

#if DISABLE_LOG_SYSTEM
#define LOGINFO(format, ...)
#define LOGWARNING(format, ...)
#define LOGERROR(format, ...)
#else
#define LOGINFO(format, ...) LOG_PROTO("I:", format, ##__VA_ARGS__)
#define LOGWARNING(format, ...) LOG_PROTO("W:", format, ##__VA_ARGS__)
#define LOGERROR(format, ...) LOG_PROTO("E:", format, ##__VA_ARGS__)
#endif

int PrintLog(const char *fmt, ...);
void Float2Str(char *str, float va);

/* MicroLIB 下 printf("%f") 会触发 HardFault(浮点格式化栈崩溃)。
 * 提供整数化的浮点打印辅助, 避免在 printf 中直接使用 %f。
 *   LOG_F1(label, va)  打印 label + 1位小数浮点, 如 "位移=12.3"
 *   LOG_F2(label, va)  打印 label + 2位小数浮点, 如 "位移=12.34"
 * 实现见 bsp_log.c, 内部用 %d 整数拼接, MicroLIB 安全。 */
void Log_PrintFloat1(const char *label, float va);
void Log_PrintFloat2(const char *label, float va);

#endif
