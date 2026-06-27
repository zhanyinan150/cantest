/**
  ******************************************************************************
  * @file    telemetry.c
  * @brief   遥测波形通道注册表 (BSP层)
  ******************************************************************************
  * 详见 telemetry.h
  ******************************************************************************
  */

#include "telemetry.h"
#include "bsp_vofa.h"
#include <string.h>

#define TELEMETRY_MAX_GROUPS  8   /* 最多注册通道组数 */

typedef struct {
    const char         *name;
    Telemetry_Getter_t  getter;
} Telemetry_Entry_t;

static Telemetry_Entry_t s_table[TELEMETRY_MAX_GROUPS];
static uint32_t           s_count = 0;

int Telemetry_Register(const char *name, Telemetry_Getter_t getter)
{
    if (getter == NULL)
        return -1;
    if (s_count >= TELEMETRY_MAX_GROUPS)
        return -1;
    s_table[s_count].name   = name;
    s_table[s_count].getter = getter;
    s_count++;
    return 0;
}

void Telemetry_SampleAndSend(void)
{
    static float frame[VOFA_MAX_CHANNELS];
    uint8_t total = 0;

    for (uint32_t i = 0; i < s_count && total < VOFA_MAX_CHANNELS; i++) {
        uint8_t n = s_table[i].getter(&frame[total], VOFA_MAX_CHANNELS - total);
        if (n > VOFA_MAX_CHANNELS - total)
            n = VOFA_MAX_CHANNELS - total;
        total += n;
    }

    if (total > 0)
        VOFA_SendFloats(frame, total);
}

uint32_t Telemetry_Count(void)
{
    return s_count;
}
