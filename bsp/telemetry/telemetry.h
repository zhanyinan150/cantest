/**
  ******************************************************************************
  * @file    telemetry.h
  * @brief   遥测波形通道注册表 (BSP层)
  ******************************************************************************
  * 各模块(Modules)在初始化时调用 Telemetry_Register() 注册自己的波形通道 getter,
  * TelemetryTask 周期遍历所有注册项, 按注册顺序把各模块提供的 float 通道拼成
  * 一个 JustFloat 帧(最多 VOFA_MAX_CHANNELS 通道)经 VOFA_SendFloats 发送。
  *
  * 设计目的:
  *   - 解耦: TelemetryTask 无需知道有哪些模块/通道, 不再硬编码 lift 的 4 通道。
  *   - 分层: 模块自注册, 避免 app 层反向依赖各模块内部状态。
  *
  * getter 约定:
  *   uint8_t getter(float *out, uint8_t max): 模块向 out 写入本模块的通道值,
  *   返回实际写入的通道数(≤max)。在 TelemetryTask 上下文调用。
  ******************************************************************************
  */

#ifndef __TELEMETRY_H
#define __TELEMETRY_H

#include <stdint.h>

/** @brief 遥测通道 getter: 向 out 写入本模块通道值, 返回通道数(0 表示本周期无数据) */
typedef uint8_t (*Telemetry_Getter_t)(float *out, uint8_t max);

/**
  * @brief  注册一个遥测通道组
  * @param  name   通道组名(诊断用, 可为 NULL)
  * @param  getter 通道值 getter 函数
  * @retval 0 成功, -1 表满或参数非法
  * @note   由模块在 Init 阶段调用。通道拼接到全局帧, 总数不得超过 VOFA_MAX_CHANNELS。
  */
int Telemetry_Register(const char *name, Telemetry_Getter_t getter);

/**
  * @brief  采集所有注册通道, 拼成 JustFloat 帧发送
  * @note   由 TelemetryTask 周期调用。超出 VOFA_MAX_CHANNELS 的通道静默截断。
  */
void Telemetry_SampleAndSend(void);

/** @brief 已注册通道组数(诊断用) */
uint32_t Telemetry_Count(void);

#endif /* __TELEMETRY_H */
