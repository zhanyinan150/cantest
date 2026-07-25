/**
  ******************************************************************************
  * @file    K230.c
  * @brief   K230 视觉模块通信 (Modules层)
  ******************************************************************************
  * 移植自参考工程 crane_1 的 K230.c, 适配本工程 BSP 分层:
  *   - UART 接收改为通过 bsp/uart/uart_callback.c 注册回调,
  *     不再直接覆写 HAL_UART_RxCpltCallback (全工程唯一强定义在 uart_callback.c)
  *   - 动作组 Action_1..Action_10 以 __weak 空桩提供, 用户在 action.c 中
  *     以同名强定义覆写即可, 无需改 K230.c
  *
  * 依赖前提(须在 App_Init 中先完成):
  *   1. MX_USART2_UART_Init() 已由 CubeMX 生成调用 (main.c)
  *   2. UART_Callback_Init() 已调用 (注册 USART1 + 创建命令队列)
  *   3. K230_Init() 注册 USART2 回调并启动 DMA 接收
  *
  * 通信协议详见 K230.h 文件头注释。
  ******************************************************************************
  */

#include "K230.h"
#include "usart.h"           /* huart2 */
#include "uart_callback.h"   /* UART_Callback_Register */

/* ==================== 模块变量 ==================== */

uint8_t K230_Rx[K230_RX_BUF_SIZE];           /* DMA 接收缓冲 */
static uint8_t Command_Data[K230_RX_BUF_SIZE]; /* 发送缓冲 (k230_write 用) */

__IO uint8_t bean_color[3]      = {0};        /* 解析后的豆子颜色 */
__IO uint8_t number_position[5] = {0};        /* 解析后的数字位置 */
__IO uint8_t bean_flag   = 0;                 /* 豆子数据就绪标志 */
__IO uint8_t bean_locked = 0;                 /* bean_color 锁 */
__IO uint8_t count       = 0;                 /* 数字数据帧计数 */

/* ==================== 动作组弱定义桩 ==================== */
/* 用户在 action.c 中以同名强定义覆写, 链接器自动替换弱定义。
 * 例如 action.c 中: void Action_1(void) { Motor_XYZ(...); Servo_Open(1); ... } */
__weak void Action_1(void)  {}
__weak void Action_2(void)  {}
__weak void Action_3(void)  {}
__weak void Action_4(void)  {}
__weak void Action_5(void)  {}
__weak void Action_6(void)  {}
__weak void Action_7(void)  {}
__weak void Action_8(void)  {}
__weak void Action_9(void)  {}
__weak void Action_10(void) {}

/* ==================== 内部函数 ==================== */

/**
  * @brief  USART2 DMA 接收完成回调 (中断上下文)
  *         解析 K230 发来的 8 字节数据帧, 重新启动 DMA 接收下一帧
  * @note   通过 UART_Callback_Register 注册, 由 bsp/uart/uart_callback.c
  *         的 HAL_UART_RxCpltCallback 分发调用, 不直接覆写弱函数
  */
void k230_read(UART_HandleTypeDef *huart)
{
    (void)huart;

    if (K230_Rx[0] == 0x02)
    {
        /* 豆子颜色帧: [2..4] = 三颗豆子颜色 */
        if (!bean_locked && (K230_Rx[2] != 0 && K230_Rx[3] != 0 && K230_Rx[4] != 0))
        {
            for (int i = 0; i < 3; i++)
                bean_color[i] = K230_Rx[i + 2];
            bean_locked = 1;    /* 锁住, 后续豆子数据忽略; 需外部清零解锁 */
            bean_flag   = 1;
        }
    }
    else if (K230_Rx[0] == 0x01)
    {
        /* 数字位置帧: [2..6] = 五个箱子的数字编号 */
        if (K230_Rx[2] != 0 && K230_Rx[3] != 0 && K230_Rx[4] != 0 &&
            K230_Rx[5] != 0 && K230_Rx[6] != 0)
        {
            count++;
            for (int i = 0; i < 5; i++)
                number_position[i] = K230_Rx[i + 2];
        }
    }

    /* 重新启动 DMA 接收 (DMA_NORMAL 模式, 每收满 8 字节触发一次回调) */
    HAL_UART_Receive_DMA(&huart2, K230_Rx, K230_RX_BUF_SIZE);
}


/* ==================== 公开接口 ==================== */

/**
  * @brief  初始化 K230 模块: 注册 USART2 回调 + 启动 DMA 接收
  * @note   在 App_Init 中 UART_Callback_Init 之后调用
  */
void K230_Init(void)
{
    UART_Callback_Register(USART2, k230_read);
    HAL_UART_Receive_DMA(&huart2, K230_Rx, K230_RX_BUF_SIZE);
}

/**
  * @brief  向 K230 发送命令
  * @param  command  命令码: K230_CMD_LOOK_BEAN/NUMBER/SIDE/CLOSE
  *         1=开启看豆, 2=开启看中间数字, 3=看侧面数字, 6=关闭摄像头
  */
void k230_write(uint8_t command)
{
    Command_Data[0] = command;
    Command_Data[1] = (command == K230_CMD_CLOSE) ? 0 : 1;
    for (uint8_t i = 2; i < K230_RX_BUF_SIZE; i++)
        Command_Data[i] = 0;

    HAL_UART_AbortTransmit(&huart2);  /* 复位 gState, 防止上次 DMA 未完成中断未触发导致卡死 */
    HAL_UART_Transmit_DMA(&huart2, Command_Data, K230_RX_BUF_SIZE);
}












//下面是实现怎么放豆子，我有一个思路是把最开始到箱子后的位置视为初始位置，然后以这个初始位置为坐标参考点，遍历数组放完第一个豆子后，先回到初始位置，再去重新遍历数组去放第二个豆子，第三个豆子只有一个爪子所以不用考虑，后面有机会过完省赛我再改成绝对模式去搞，这样就是可以用绝对位置去定位（但现在我还没用过绝对模式，后面再说）







/**
  * @brief  根据豆子颜色查找其在 number_position 中的位置(1~5), 执行 Action_1..5
  * @param  key  目标豆子颜色: BEAN_GREEN(0x06)/BEAN_YELLOW(0x07)/BEAN_YUN(0x08)
  * @note   颜色->目标值映射: 绿=2, 黄=1, 云=3 (与 K230 返回的 number_position 编码一致)
  */
void Data_Handle1(uint8_t key)
{
    uint8_t target = 0;
    uint8_t pos = 0;
    uint8_t i;

    /* 1、绑定对应目标值 */
    if      (key == BEAN_GREEN)  target = 2;   /* 0x06 绿豆 */
    else if (key == BEAN_YELLOW) target = 1;   /* 0x07 黄豆 */
    else if (key == BEAN_YUN)    target = 3;   /* 0x08 云豆 */
    else                 return;

    /* 2、遍历 number_position, 查询目标值所在位置(1~5) */
    for (i = 0; i < 5; i++)
    {
        if (number_position[i] == target)
        {
            pos = i + 1;
            break;
        }
    }

    /* 3、五位位置 switch, 调对应动作组 */
    switch (pos)
    {
        case 1: Action_1(); break;
        case 2: Action_2(); break;
        case 3: Action_3(); break;
        case 4: Action_4(); break;
        case 5: Action_5(); break;
        default: break;
    }
}

