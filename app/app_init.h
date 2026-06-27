/**
  ******************************************************************************
  * @file    app_init.h
  * @brief   应用层入口 (App层)
  ******************************************************************************
  * 应用层负责系统初始化编排与任务创建, 依赖方向: App → Modules → BSP → Core。
  * freertos.c 的 StartDefaultTask 仅调用 App_Init(), 不再直接持有业务逻辑,
  * 以保证 CubeMX 重新生成 freertos.c 时不会覆盖应用代码。
  ******************************************************************************
  */

#ifndef __APP_INIT_H
#define __APP_INIT_H

/**
  * @brief  应用层初始化: 子系统初始化 + 任务创建
  * @note   由 StartDefaultTask 在 MX_USB_DEVICE_Init() 之后调用
  */
void App_Init(void);

#endif /* __APP_INIT_H */
