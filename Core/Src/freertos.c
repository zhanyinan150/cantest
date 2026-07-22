/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "can.h"
#include "usart.h"
#include <stdio.h>
#include "app_init.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* appInitTask 属性: 放 USER CODE 区抗 CubeMX 重新生成覆盖 */
osThreadId_t appInitTaskHandle;
const osThreadAttr_t appInitTask_attributes = {
  .name = "appInitTask",
  .stack_size = 512 * 4,   /* 2KB: 跑 App_Init, printf→fputc→HAL_UART_Transmit 链深 */
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void AppInitTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* appInitTask: 调度器启动后跑 App_Init(), 完成后 vTaskDelete 自删除。
   * osThreadNew 在调度器前创建任务本身是安全的(同 defaultTask); 卡死的是
   * "调度器前直接调用 App_Init"(其 HAL_Delay 依赖 uwTick), 故改为独立任务而非裸调用。
   * 注: MX_USB_DEVICE_Init 已在 main.c USER CODE 2(调度器前)完成, 属硬件外设初始化,
   * 不依赖 FreeRTOS, 故无需放任务上下文; App_Init 首行 osDelay(500) 等 PC 侧 CDC 枚举。 */
  appInitTaskHandle = osThreadNew(AppInitTask, NULL, &appInitTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */
  /* defaultTask 纯保活: 不承担任何初始化。
   * 硬件外设初始化(含 USB)在 main.c, 应用初始化在 appInitTask, defaultTask 仅空转。
   * 栈 128word(512B) 足够 osDelay。 */
  for (;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN AppInitTask */
/**
  * @brief  应用初始化任务: 调度器启动后执行 App_Init() → 自删除。
  *         USB 已在 main.c(调度器前)初始化, App_Init 首行 osDelay(500) 等 PC 枚举 CDC。
  *         完成后 vTaskDelete 释放栈与TCB。放 USER CODE 区抗 CubeMX 覆盖。
  */
static void AppInitTask(void *argument)
{
  (void)argument;
  App_Init();
  vTaskDelete(NULL);
}
/* USER CODE END AppInitTask */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* printf 重定向在 bsp/printf/bsp_printf.c (fputc)。
 * 旧 CAN demo 已随四层架构移植移除, 业务逻辑见 app/app_init.c。 */

/* FreeRTOS 栈溢出钩子(configCHECK_FOR_STACK_OVERFLOW=2): 栈溢出时打印任务名,
 * 替代直接跳NULL的HardFault, 直接定位是哪个任务栈不够。 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  printf("\r\n*** STACK OVERFLOW in task: %s ***\r\n", pcTaskName);
  while (1) {}
}
/* USER CODE END Application */

