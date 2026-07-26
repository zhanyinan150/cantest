/**
  ******************************************************************************
  * @file    uart_callback.c
  * @brief   UART接收回调统一分发 (BSP层)
  ******************************************************************************
  * HAL_UART_RxCpltCallback 是弱函数, 全工程只能有一个强定义。
  * 本文件作为全局回调路由:
  *   USART1 → 内建 VOFA+ 逐字节接收, 拼行后入命令消息队列
  *   其它串口 → 转发给上层模块通过 UART_Callback_Register() 注册的回调
  *
  * 分层约束: 不得 #include 任何 modules/ 头文件。业务模块(如步进电机)
  *           应自行注册回调, 而非由本文件直接调用其符号。
  ******************************************************************************
  */

#include "uart_callback.h"
#include "usart.h"
#include "FreeRTOS.h"   /* configASSERT */
#include "task.h"       /* taskDISABLE_INTERRUPTS (configASSERT 宏体依赖) */
#include "string.h"

/* ---- VOFA 命令接收 (VOFA_RX_BUF_SIZE 见 uart_callback.h) ---- */
#define VOFA_CMD_QUEUE_DEPTH  4   /* 命令队列深度, 缓冲突发连发 */

static uint8_t vofa_rx_idx = 0;
static char    vofa_rx_line[VOFA_RX_BUF_SIZE];

/* 命令消息队列: 每个元素是一条命令行 char[VOFA_RX_BUF_SIZE] */
static osMessageQueueId_t s_cmd_queue = NULL;

/* ---- 注册式回调分发表 ---- */
#define UART_DISPATCH_SLOTS  4   /* 同时支持的业务串口数 */

typedef struct {
    USART_TypeDef           *instance;   /* 外设实例, NULL=空槽 */
    UART_RxCompleteHandler_t handler;
    UART_RxEventHandler_t    event;      /* IDLE 切帧回调, 可为 NULL */
    UART_RxRestartHandler_t  restart;    /* 错误恢复回调, 可为 NULL */
} UART_DispatchSlot_t;

static UART_DispatchSlot_t s_slots[UART_DISPATCH_SLOTS];

/**
  * @brief  取得(或新建)某实例的槽位, 槽位耗尽返回 NULL
  */
static UART_DispatchSlot_t *s_slot_of(USART_TypeDef *instance)
{
    if (instance == NULL)
        return NULL;
    for (uint8_t i = 0; i < UART_DISPATCH_SLOTS; i++) {
        if (s_slots[i].instance == instance)
            return &s_slots[i];
    }
    for (uint8_t i = 0; i < UART_DISPATCH_SLOTS; i++) {
        if (s_slots[i].instance == NULL) {
            s_slots[i].instance = instance;
            return &s_slots[i];
        }
    }
    return NULL;  /* 槽位已满: 静默丢弃, BSP层不应直接 printf */
}

/**
  * @brief  注册某串口实例的接收完成回调
  */
void UART_Callback_Register(USART_TypeDef *instance, UART_RxCompleteHandler_t handler)
{
    UART_DispatchSlot_t *s = s_slot_of(instance);
    if (s) s->handler = handler;
}

/**
  * @brief  注册某串口实例的接收事件回调 (IDLE 切帧)
  */
void UART_Callback_RegisterEvent(USART_TypeDef *instance, UART_RxEventHandler_t handler)
{
    UART_DispatchSlot_t *s = s_slot_of(instance);
    if (s) s->event = handler;
}

/**
  * @brief  注册某串口实例的错误恢复回调
  */
void UART_Callback_RegisterRestart(USART_TypeDef *instance, UART_RxRestartHandler_t handler)
{
    UART_DispatchSlot_t *s = s_slot_of(instance);
    if (s) s->restart = handler;
}

/**
  * @brief  按实例查找已注册槽位(只查不建)
  */
static UART_DispatchSlot_t *s_find(USART_TypeDef *instance)
{
    for (uint8_t i = 0; i < UART_DISPATCH_SLOTS; i++) {
        if (s_slots[i].instance == instance)
            return &s_slots[i];
    }
    return NULL;
}

/**
  * @brief  初始化UART回调分发模块
  * @note   启动 USART1 逐字节中断接收, 并创建命令消息队列
  */
void UART_Callback_Init(void)
{
    if (s_cmd_queue == NULL) {
        s_cmd_queue = osMessageQueueNew(VOFA_CMD_QUEUE_DEPTH,
                                        VOFA_RX_BUF_SIZE,
                                        NULL);
        /* 队列创建失败(堆不足)会致 CommandTask 忙等锁死, 开机即断言捕获 */
        configASSERT(s_cmd_queue != NULL);
    }
    HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
}

osMessageQueueId_t UART_Callback_GetCmdQueue(void)
{
    return s_cmd_queue;
}

/**
  * @brief  USART接收完成回调(全局唯一强定义)
  *         USART1 内建 VOFA 拼行入队; 其它串口转发给已注册的业务回调
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        /* ---- VOFA+ 逐字节接收, 拼行后入命令队列 ---- */
        uint8_t b = uart1_rx_byte;
        if (b == '\n' || b == '\r') {
            if (vofa_rx_idx > 0) {            /* 非空行才入队 */
                vofa_rx_line[vofa_rx_idx] = '\0';
                /* 入队: osMessageQueuePut 值拷贝入队列内部存储, vofa_rx_line 为
                 * static 缓冲在 ISR 返回前不会被改写, 可直接传入无需暂存。
                 * 队列满则丢弃本条(0超时, ISR不可阻塞)。 */
                if (s_cmd_queue != NULL) {
                    osMessageQueuePut(s_cmd_queue, vofa_rx_line, 0, 0);
                }
                vofa_rx_idx = 0;
            }
        } else {
            if (vofa_rx_idx < VOFA_RX_BUF_SIZE - 1) {
                vofa_rx_line[vofa_rx_idx++] = (char)b;
            } else {
                vofa_rx_idx = 0;              /* 溢出丢弃, 防越界 */
            }
        }
        HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1); /* 重新使能接收 */
        return;
    }

    /* ---- 其它串口: 转发给业务模块注册的回调 ---- */
    UART_DispatchSlot_t *s = s_find(huart->Instance);
    if (s && s->handler)
        s->handler(huart);
}

/**
  * @brief  UART 接收事件回调(全局唯一强定义): IDLE 空闲切帧 / DMA 收满
  * @note   由 HAL_UARTEx_ReceiveToIdle_DMA 触发, 转发给注册的事件回调并带上
  *         实际字节数, 上层据此判断整帧/残帧。
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    UART_DispatchSlot_t *s = s_find(huart->Instance);
    if (s && s->event)
        s->event(huart, Size);
}

/**
  * @brief  UART 错误回调(全局唯一强定义): 清错误标志 + 重新武装接收
  * @note   不实现本函数时, 一次 ORE/FE/NE(上电噪声、波特率抖动、对端复位)
  *         就会让 HAL 中止接收且再不恢复, 该串口永久失联。
  *         USART1 内建恢复(逐字节 IT); 其它串口调用注册的 restart 回调。
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    /* 清 ORE/NE/FE/PE: F4 的 SR 读后再读 DR 即可清标志 */
    volatile uint32_t tmp;
    tmp = huart->Instance->SR;
    tmp = huart->Instance->DR;
    (void)tmp;
    huart->ErrorCode = HAL_UART_ERROR_NONE;

    if (huart->Instance == USART1) {
        HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
        return;
    }

    UART_DispatchSlot_t *s = s_find(huart->Instance);
    if (s && s->restart)
        s->restart(huart);
}
