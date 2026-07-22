/**
  ******************************************************************************
  * @file    cmd_register.c
  * @brief   VOFA/串口命令注册表 (BSP层)
  ******************************************************************************
  * 详见 cmd_register.h
  ******************************************************************************
  */

#include "cmd_register.h"
#include <string.h>

#define CMD_TABLE_SIZE   32   /* 最多注册命令数 (lift10+chassis8+lateral7+mission1=26, 留余量) */
#define CMD_NAME_MAX_LEN 12   /* 命令名最大长度(含'\0') */

typedef struct {
    char          name[CMD_NAME_MAX_LEN];
    CMD_Handler_t handler;
} CMD_Entry_t;

static CMD_Entry_t s_table[CMD_TABLE_SIZE];
static uint32_t    s_count = 0;

int CMD_Register(const char *name, CMD_Handler_t handler)
{
    if (name == NULL || handler == NULL)
        return -1;
    size_t len = strlen(name);
    if (len == 0 || len >= CMD_NAME_MAX_LEN)
        return -1;

    /* 同名覆盖 */
    for (uint32_t i = 0; i < s_count; i++) {
        if (strcmp(s_table[i].name, name) == 0) {
            s_table[i].handler = handler;
            return 0;
        }
    }
    if (s_count >= CMD_TABLE_SIZE)
        return -1;

    memcpy(s_table[s_count].name, name, len + 1);
    s_table[s_count].handler = handler;
    s_count++;
    return 0;
}

int CMD_Dispatch(const char *line)
{
    if (line == NULL)
        return -1;

    /* 拆出命令名(首个 token) */
    char name[CMD_NAME_MAX_LEN];
    const char *p = line;
    /* 跳过前导空格 */
    while (*p == ' ' || *p == '\t') p++;
    size_t n = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && n < CMD_NAME_MAX_LEN - 1) {
        name[n++] = *p++;
    }
    name[n] = '\0';
    if (n == 0)
        return -1;

    /* 跳过命令名后的空格, 剩余作为 arg */
    while (*p == ' ' || *p == '\t') p++;
    const char *arg = p;  /* 指向剩余字符串, 可为空串 */

    /* 查表分发 */
    for (uint32_t i = 0; i < s_count; i++) {
        if (strcmp(s_table[i].name, name) == 0) {
            s_table[i].handler(arg);
            return 0;
        }
    }
    return -1;  /* 未知命令 */
}

uint32_t CMD_Count(void)
{
    return s_count;
}
