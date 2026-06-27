#include "stdio.h"
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart1; // 假设使用的是USART1

/* VOFA独占USART1开关: 置1时printf静默(不向USART1发字节), 避免文本污染
 * VOFA JustFloat二进制波形流。需要恢复printf日志时改为0。 */
#define VOFA_UART1_EXCLUSIVE 0

int fputc(int ch, FILE *f) {
#if VOFA_UART1_EXCLUSIVE
    (void)ch; (void)f;
    return ch;  /* VOFA模式: printf静默, USART1专供波形 */
#else
    /* 有限超时(10ms), 避免 UART 异常时 HAL_MAX_DELAY 永久阻塞,
     * 尤其在高优先级任务(如 DJIMotorTask)中调用 printf 导致系统死锁。 */
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
#endif
}
