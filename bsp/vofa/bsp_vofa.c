/**
  ******************************************************************************
  * @file    bsp_vofa.c
  * @brief   VOFA+ JustFloat 协议波形发送实现
  ******************************************************************************
  */

#include "bsp_vofa.h"
#include "stm32f4xx_hal.h"
#include "string.h"

extern UART_HandleTypeDef huart1;

void VOFA_SendFloats(float *data, uint8_t n)
{
    static uint8_t buf[(VOFA_MAX_CHANNELS * 4) + 4];

    if (n == 0 || n > VOFA_MAX_CHANNELS)
        return;
    if (data == NULL)
        return;

    /* 拷贝float数据(小端, STM32原生即小端) */
    memcpy(buf, data, (uint32_t)n * 4);

    /* 追加JustFloat帧尾 */
    uint16_t tail = n * 4;
    buf[tail]     = VOFA_JUSTFLOAT_TAIL0;
    buf[tail + 1] = VOFA_JUSTFLOAT_TAIL1;
    buf[tail + 2] = VOFA_JUSTFLOAT_TAIL2;
    buf[tail + 3] = VOFA_JUSTFLOAT_TAIL3;

    uint16_t total = (uint16_t)(tail + 4);

    /* 优先 DMA 发送(非阻塞, DMA搬运期间CPU可执行其他任务)。
     * 若 USART1 未配置 TX DMA (huart1.hdmatx == NULL), 回退阻塞发送。
     * DMA 发送用 static buf 保证发送期间缓冲区有效(DMA异步搬运, 栈变量可能失效)。 */
    if (huart1.hdmatx != NULL)
    {
        /* 等待上一次 DMA 发送完成(防止覆盖未发完的缓冲区) */
        uint32_t tickstart = HAL_GetTick();
        while (huart1.gState != HAL_UART_STATE_READY)
        {
            if ((HAL_GetTick() - tickstart) > 20U)  /* 20ms 超时, 异常时放弃本帧 */
            {
                /* 超时必须复位 gState, 否则永久卡在 BUSY_TX 导致后续帧全丢。
                 * 用 AbortTransmit 仅中止 TX: HAL_UART_Abort 会一并清除 RXNEIE,
                 * 导致 USART1 的 VOFA 命令接收(HAL_UART_Receive_IT 逐字节中断)永久失效。 */
                HAL_UART_AbortTransmit(&huart1);
                return;
            }
        }
        if (HAL_UART_Transmit_DMA(&huart1, buf, total) != HAL_OK)
        {
            /* DMA 启动失败(罕见), 回退阻塞 */
            HAL_UART_Transmit(&huart1, buf, total, 20);
        }
    }
    else
    {
        /* 无 DMA, 阻塞发送, 超时按字节数线性估算(每字节≈10/115200s, 留余量) */
        HAL_UART_Transmit(&huart1, buf, total, 20);
    }
}

void VOFA_SendFloat(float v)
{
    VOFA_SendFloats(&v, 1);
}
