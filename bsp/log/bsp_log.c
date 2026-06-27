#include "bsp_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void BSPLogInit(void)
{
}

int PrintLog(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vprintf(fmt, args);
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

/* MicroLIB 安全的浮点打印: 用 %d 整数拼接, 避免 printf("%f") 栈崩溃。
 * 仅 printf 一行文本, 整数运算 + 整数格式化, 不触发浮点格式化路径。 */
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
