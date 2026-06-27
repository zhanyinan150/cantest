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

#endif
