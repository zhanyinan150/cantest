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
  *   1. MX_USART3_UART_Init() 已由 CubeMX 生成调用 (main.c)
  *   2. UART_Callback_Init() 已调用 (注册 USART1 + 创建命令队列)
  *   3. K230_Init() 注册 USART3 回调并启动 DMA 接收
  *
  * 通信协议详见 K230.h 文件头注释。
  *
  * K230 三阶段通讯协议层(从 action.c 移植): 发命令->等ACK->等数据->回ACK
  *   的三阶段封装, 带 ACK/数据超时重试(最多 K230_RETRY_MAX 次)。
  *   k230_phase1_bean / k230_phase2_front / k230_phase3_side 供 action.c 调用。
  ******************************************************************************
  */

#include "K230.h"
#include "usart.h"           /* huart3 */
#include "uart_callback.h"   /* UART_Callback_Register */
#include "cmsis_os2.h"        /* osDelay (协议层重试延时) */
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_log.h"          /* printf -> 日志队列 -> LogTask DMA 发 USART1 */
#include <stdio.h>            /* snprintf (协议层格式化输出) */

/* ==================== 模块变量 ==================== */

uint8_t K230_Rx[K230_RX_BUF_SIZE];           /* DMA 接收缓冲 */
static uint8_t Command_Data[K230_RX_BUF_SIZE]; /* 发送缓冲 (k230_write 用) */

__IO uint8_t bean_color[3]      = {0};        /* 解析后的豆子颜色 */
__IO uint8_t number_position[5] = {0};        /* 解析后的数字位置(左到右) */
__IO uint8_t bean_flag   = 0;                 /* 豆子数据就绪标志 */
__IO uint8_t bean_locked = 0;                 /* bean_color 锁 */
__IO uint8_t front_number[3]      = {0};      /* 正面3个数字 */
__IO uint8_t front_number_flag    = 0;        /* 正面数字就绪标志 */
__IO uint8_t full_number_flag     = 0;        /* 完整5数字就绪标志 */
__IO uint8_t count       = 0;                 /* 数字数据帧计数 */
__IO uint8_t k230_ack_flag = 0;               /* K230 ACK (0x0A) 就绪标志 */
__IO uint8_t k230_ready_flag = 0;             /* K230 就绪 (0x0B): 模型加载完成 */

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

/* 接收统计(诊断用): 可在调试器里看是否有帧在丢 */
__IO uint32_t k230_rx_frames  = 0;   /* 成功解析的整帧数 */
__IO uint32_t k230_rx_badxor  = 0;   /* XOR 校验失败帧数 */
__IO uint32_t k230_rx_badlen  = 0;   /* 长度不等于 8 的残帧数 */
__IO uint32_t k230_rx_rearm_err = 0; /* 重新武装 DMA 失败次数 */
__IO uint32_t k230_rx_badphase  = 0; /* 阶段号不符被丢弃的帧数 */
__IO uint32_t k230_last_rx_tick = 0; /* 最近一次收到合法帧的 tick */

/* 当前阶段号, 填进发出帧的 byte[1], 并用于校验收到帧的 byte[1]。
 * 由 k230_phase1/2/3 在各自入口设置; k230_wait_ready 期间为 NONE。 */
static __IO uint8_t s_cur_phase = K230_PHASE_NONE;

/**
  * @brief  武装 USART3 的 IDLE 空闲切帧 DMA 接收
  * @note   用 ReceiveToIdle 而非定长 Receive_DMA: 定长模式下一旦丢/多一个字节,
  *         DMA 组帧相位就永久错位, XOR 永远失败且无法自愈; IDLE 模式靠帧间
  *         空隙(K230 每帧之间必有间隔)重新对齐, 掉字节后下一帧即恢复。
  *         返回值必须检查——失败而不重试会导致 K230 通讯永久中断。
  */
static void k230_arm_rx(UART_HandleTypeDef *huart)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(huart, K230_Rx, K230_RX_BUF_SIZE) != HAL_OK)
    {
        k230_rx_rearm_err++;
        return;
    }
    /* 关半满中断: 只关心 IDLE/收满事件, 否则 4 字节就会上报一次残帧 */
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
}

/**
  * @brief  USART3 接收事件回调 (中断上下文): 解析 K230 数据帧
  * @param  huart  USART3 句柄
  * @param  size   本次实际收到的字节数 (IDLE 切帧上报)
  * @note   通过 UART_Callback_RegisterEvent 注册, 由 bsp/uart/uart_callback.c
  *         的 HAL_UARTEx_RxEventCallback 分发, 不直接覆写弱函数。
  */
void k230_read(UART_HandleTypeDef *huart, uint16_t size)
{
    /* 残帧(丢字节/对端半途复位): 丢弃并重新武装, 下一帧靠 IDLE 自动对齐 */
    if (size != K230_RX_BUF_SIZE)
    {
        k230_rx_badlen++;
        k230_arm_rx(huart);
        return;
    }

    /* XOR 校验: byte[7] 应等于 byte[0]^...^byte[6], 不匹配则丢弃 */
    uint8_t cs = 0;
    for (uint8_t i = 0; i < K230_RX_BUF_SIZE - 1; i++)
        cs ^= K230_Rx[i];
    if (cs != K230_Rx[K230_RX_BUF_SIZE - 1])
    {
        k230_rx_badxor++;
        k230_arm_rx(huart);
        return;
    }

    k230_rx_frames++;
    /* 存活时戳: 只要收到一帧合法帧就刷新。osKernelGetTickCount 在 CMSIS-RTOS2
     * 里内部走 IS_IRQ() 分支调 xTaskGetTickCountFromISR, 中断上下文可安全调用。 */
    k230_last_rx_tick = osKernelGetTickCount();

    /* 阶段号校验: byte[1] 与主机当前阶段不符 = 上一阶段的迟到帧, 原地丢弃。
     * 不丢的话最要命的是 0x0A —— k230_ack_flag 是累加计数器, 迟到的 ACK 会被
     * 下一阶段白白消费, 主机以为对端应答了而实际没有, 从此步步错位。
     * byte[1]==0 放行: 未升级的 K230 脚本 byte[1] 恒为 0, 此时退化成升级前的
     * 行为(不校验), 保证新固件配旧脚本也能跑, 不会因协议不匹配直接锁死。
     * 就绪帧 0x0B 不属于任何阶段, 一律放行。 */
    if (K230_Rx[0] != 0x0B && K230_Rx[1] != K230_PHASE_NONE &&
        K230_Rx[1] != s_cur_phase)
    {
        k230_rx_badphase++;
        k230_arm_rx(huart);
        return;
    }

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
    else if (K230_Rx[0] == 0x03)
    {
        /* 正面3数字帧: [2..4] = 正面三个数字 */
        if (K230_Rx[2] != 0 && K230_Rx[3] != 0 && K230_Rx[4] != 0)
        {
            for (int i = 0; i < 3; i++)
                front_number[i] = K230_Rx[i + 2];
            front_number_flag = 1;
        }
    }
    else if (K230_Rx[0] == 0x01)
    {
        /* 完整5数字帧: [2..6] = 五个箱子的数字编号(左到右) */
        if (K230_Rx[2] != 0 && K230_Rx[3] != 0 && K230_Rx[4] != 0 &&
            K230_Rx[5] != 0 && K230_Rx[6] != 0)
        {
            count++;
            for (int i = 0; i < 5; i++)
                number_position[i] = K230_Rx[i + 2];
            full_number_flag = 1;
        }
    }
    else if (K230_Rx[0] == 0x0A)
    {
        /* ACK 计数而非置 1: Phase2 中 K230 会连发两个 0x0A(命令收到/识别完成),
         * 若两帧间隔短于任务轮询周期, 用布尔标志会丢掉第二个, 导致主机白等超时。 */
        if (k230_ack_flag < 255)
            k230_ack_flag++;
    }
    else if (K230_Rx[0] == 0x0B)
    {
        /* 就绪帧: K230 模型加载完成后循环上报, 直到收到 LOOK_BEAN。
         * 用布尔而非计数: 主机只需知道"已就绪", 多发几帧不需累加。 */
        k230_ready_flag = 1;
    }

    /* 重新武装接收, 等待下一帧 */
    k230_arm_rx(huart);
}


/* ==================== 公开接口 ==================== */

/**
  * @brief  初始化 K230 模块: 注册 USART3 回调 + 启动 DMA 接收
  * @note   在 App_Init 中 UART_Callback_Init 之后调用
  */
void K230_Init(void)
{
    __HAL_UART_CLEAR_OREFLAG(&huart3);
    UART_Callback_RegisterEvent(USART3, k230_read);
    UART_Callback_RegisterRestart(USART3, k230_arm_rx);  /* ORE/FE 后自动恢复 */
    k230_arm_rx(&huart3);
}

/**
  * @brief  向 K230 发送命令
  * @param  command  命令码: K230_CMD_LOOK_BEAN/NUMBER/SIDE/CLOSE
  *         2=看豆, 3=看正面数字, 1=看侧面数字, 6=关闭 (与K230端匹配)
  */
int k230_write(uint8_t command)
{
    Command_Data[0] = command;
    /* byte[1] = 当前阶段号。原来这里是个 (command==CLOSE)?0:1 的标志位, K230 端
     * 从来不读它, 现改为承载阶段号, 让对端能识别并丢弃跨阶段的迟到帧。 */
    Command_Data[1] = s_cur_phase;
    for (uint8_t i = 2; i < K230_RX_BUF_SIZE - 1; i++)
        Command_Data[i] = 0;

    /* XOR 校验码: byte[7] = byte[0]^...^byte[6] */
    uint8_t cs = 0;
    for (uint8_t i = 0; i < K230_RX_BUF_SIZE - 1; i++)
        cs ^= Command_Data[i];
    Command_Data[K230_RX_BUF_SIZE - 1] = cs;

    /* 返回值必须检查: 发送失败(总线忙/上一次 DMA 未完成)时静默丢命令,
     * K230 收不到就永远不会应答, 表现为"莫名超时", 极难定位。 */
    return (HAL_UART_Transmit(&huart3, Command_Data, K230_RX_BUF_SIZE, 100) == HAL_OK) ? 0 : -1;
}












//下面是实现怎么放豆子，我有一个思路是把最开始到箱子后的位置视为初始位置，然后以这个初始位置为坐标参考点，遍历数组放完第一个豆子后，先回到初始位置，再去重新遍历数组去放第二个豆子，第三个豆子只有一个爪子所以不用考虑，后面有机会过完省赛我再改成绝对模式去搞，这样就是可以用绝对位置去定位（但现在我还没用过绝对模式，后面再说）







/**
  * @brief  根据豆子颜色查找其在 number_position 中的位置(1~5), 执行 Action_1..5
  * @param  key  目标豆子颜色: BEAN_GREEN(0x06)/BEAN_YUN(0x07)/BEAN_YELLOW(0x08)
  * @note   颜色->目标值映射: 绿=2, 芸=3, 黄=1 (与 K230 返回的 number_position 编码一致)
  */
int Data_yinshe(uint8_t key)
{
    uint8_t target = 0;
    uint8_t pos = 0;
    uint8_t i;

    /* 1、绑定对应目标值 */
    if      (key == BEAN_GREEN)  target = 2;   /* 0x06 绿豆 */
    else if (key == BEAN_YUN)    target = 3;   /* 0x07 芸豆 */
    else if (key == BEAN_YELLOW) target = 1;   /* 0x08 黄豆 */
    else                 return -1;            /* 未知颜色 */

    /* 2、遍历 number_position, 查询目标值所在位置(1~5) */
    for (i = 0; i < 5; i++)
    {
        if (number_position[i] == target)
        {
            pos = i + 1;
            break;
        }
    }

    /* 3、五位位置 switch, 调对应动作组。
     * pos==0 表示 number_position 里没有该数字(K230 识别错/推理失败),
     * 必须把失败回报给调用方, 否则这颗豆子会被静默丢弃且毫无提示。 */
    // switch (pos)
    // {
    //     case 1: Action_1(); break;
    //     case 2: Action_2(); break;
    //     case 3: Action_3(); break;
    //     case 4: Action_4(); break;
    //     case 5: Action_5(); break;
    //     default: return -2;                    /* 目标数字不在 number_position 中 */
    // }
    return pos;
}

/* ==================== K230 通讯协议层 ==================== */
/* 从 action.c 移植: 发命令->等ACK->等数据->回ACK 的三阶段封装。
 * 调试输出走 printf -> 日志队列 -> LogTask DMA 发 USART1,
 * 不可直接 HAL_UART_Transmit(&huart1,...): 会与 LogTask 的 DMA 发送抢同一个
 * USART1, 正是 bsp_log.c 文件头列为 HardFault 根因的用法。 */
static void k230_dbg(const char *s)
{
    printf("%s", s);
}

/**
  * @brief  等待事件标志(计数器语义), 成功后消费一个事件
  * @note   k230_ack_flag 是计数器(K230 可能连发多个 ACK), bean_flag /
  *         full_number_flag 是 0/1, 两者都适用"非零即有事件, 取走减一"。
  */
static int k230_wait_flag(volatile uint8_t *flag, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (!(*flag))
    {
        osDelay(10);
        elapsed += 10;
        if (elapsed >= timeout_ms)
            return -1;
    }
    (*flag)--;          /* 消费一个事件, 不要粗暴清零以免丢掉已到达的下一个 */
    return 0;
}

/**
  * @brief  上电握手: 阻塞等 K230 就绪(0x0B), 超时也放行以免整局卡死
  * @note   K230 加载 kmodel 常需 5~10s, 远超 P1 的 ACK 等待窗口。若主机上电就
  *         发 LOOK_BEAN, K230 还没跑到接收就漏掉命令。改为主机先等就绪帧,
  *         K230 加载完循环发 0x0B, 谁先启动都能对上。
  * @retval 0=收到就绪帧, -1=超时(仍继续流程, K230 可能已就绪只是漏了就绪帧)
  */
int k230_wait_ready(void)
{
    s_cur_phase = K230_PHASE_NONE;   /* 握手期不属于任何阶段 */
    k230_ready_flag = 0;
    k230_dbg("[RDY] wait K230 ready (0x0B)...\r\n");
    if (k230_wait_flag(&k230_ready_flag, K230_READY_TIMEOUT_MS) == 0)
    {
        k230_dbg("[RDY] K230 ready\r\n");
        return 0;
    }
    k230_dbg("[RDY] ready timeout, proceed anyway\r\n");
    return -1;
}

/**
  * @brief  发一次命令 + 等一次 ACK, **不重试**
  * @note   重试策略一律由调用方决定。若这里也重试, 而调用方又套一层
  *         K230_RETRY_MAX 循环, 嵌套后实际重试 3x3=9 次(约 27s 才放弃),
  *         与宏名和文档说的"最多 3 次"对不上。
  */
static int k230_send_cmd_once(uint8_t cmd)
{
    /* 存活示警: 只打印, 不改流程。现场靠这行区分"K230 挂了/没接线"和"识别慢" ——
     * 三阶段全 no ack 时最难判断的正是这个, 光看主机日志分不出来。 */
    if (k230_last_rx_tick == 0)
    {
        k230_dbg("[WARN] K230 never responded since boot\r\n");
    }
    else
    {
        uint32_t silent = osKernelGetTickCount() - k230_last_rx_tick;
        if (silent > K230_SILENT_WARN_MS)
        {
            char wb[48];
            snprintf(wb, sizeof(wb), "[WARN] K230 silent %ums\r\n",
                     (unsigned int)silent);
            k230_dbg(wb);
        }
    }

    k230_ack_flag = 0;                       /* 丢弃上一轮残留 ACK */
    if (k230_write(cmd) != 0)                /* 发送失败(总线忙) */
    {
        k230_dbg("[RTY] uart tx failed\r\n");
        osDelay(50);
        return -1;
    }
    return k230_wait_flag(&k230_ack_flag, K230_ACK_TIMEOUT_MS);
}

/* 发命令 + 等 K230 ACK, 超时重试最多 K230_RETRY_MAX 次 */
static int k230_send_cmd_and_wait_ack(uint8_t cmd)
{
    for (int retry = 0; retry < K230_RETRY_MAX; retry++)
    {
        if (k230_send_cmd_once(cmd) == 0)
            return 0;
        k230_dbg("[RTY] no ack, retry...\r\n");
    }
    return -1;
}

/* 发命令 + 等 ACK + 等数据, 任一步超时则重发命令, 最多 K230_RETRY_MAX 次。
 * 这里必须调 k230_send_cmd_once 而不是 k230_send_cmd_and_wait_ack -- 后者自带
 * 重试, 嵌在本函数的循环里就是 9 次。 */
static int k230_send_cmd_wait_ack_wait_data(uint8_t cmd, volatile uint8_t *data_flag)
{
    for (int retry = 0; retry < K230_RETRY_MAX; retry++)
    {
        /* 发命令前清数据标志: 否则上一轮/上电前残留的 1 会让 wait_flag 立刻
         * 返回成功, 而 bean_color/number_position 里是过期数据。 */
        *data_flag = 0;

        if (k230_send_cmd_once(cmd) != 0)
        {
            k230_dbg("[RTY] no ack, retry...\r\n");
            continue;
        }
        if (k230_wait_flag(data_flag, K230_DATA_TIMEOUT_MS) == 0)
            return 0;

        /* 数据超时: 先试最便宜的一招 —— 发 RESYNC 让 K230 把上次那帧再发一遍。
         * 绝不能发 CLOSE: 那是"数据已收到, 关摄像头进下一阶段"的意思, K230 收到
         * 会真的前进, 而主机其实还在重试本阶段, 两端就此错开一个阶段。
         * RESYNC 不改变任一端的阶段, 只补一次重传。 */
        k230_dbg("[RTY] data timeout, send RESYNC\r\n");
        k230_write(K230_CMD_RESYNC);
        if (k230_wait_flag(data_flag, K230_RESYNC_TIMEOUT_MS) == 0)
        {
            k230_dbg("[RTY] resync ok\r\n");
            return 0;
        }

        /* 重传也没来: K230 多半根本没在听(挂了, 或还卡在识别循环里)。下一轮把
         * 整条命令重发, 让它重开摄像头重跑一次识别 —— 贵但是唯一的退路。 */
        k230_dbg("[RTY] resync failed, resend cmd\r\n");
        osDelay(200);
    }
    return -1;
}

/* ---- 三阶段封装: 与 k230_competition.py 的 Phase1/2/3 一一对应 ---- */

/**
  * @brief  Phase1: 豆子颜色识别, 结果填入 bean_color[3]
  * @retval 0=成功, -1=重试耗尽
  */
int k230_phase1_bean(void)
{
    char buf[80];

    s_cur_phase = K230_PHASE_BEAN;   /* 之后所有收发帧的 byte[1] 都带这个号 */
    k230_dbg("[P1] Send LOOK_BEAN\r\n");
    if (k230_send_cmd_wait_ack_wait_data(K230_CMD_LOOK_BEAN, &bean_flag) != 0)
    {
        k230_dbg("[P1] FAILED after retries!\r\n");
        return -1;
    }
    snprintf(buf, sizeof(buf), "[P1] Bean: %02X %02X %02X\r\n",
             bean_color[0], bean_color[1], bean_color[2]);
    k230_dbg(buf);
    k230_write(K230_CMD_CLOSE);      /* 回应答, K230 才会关摄像头进入下一阶段 */
    k230_dbg("[P1] ACK sent\r\n");
    return 0;
}

/**
  * @brief  Phase2: 正面数字识别(K230 端自存, 不回传数据帧, 只发两个 ACK)
  * @retval 0=成功, -1=重试耗尽
  */
int k230_phase2_front(void)
{
    s_cur_phase = K230_PHASE_FRONT;
    k230_dbg("[P2] Send LOOK_NUMBER\r\n");
    if (k230_send_cmd_and_wait_ack(K230_CMD_LOOK_NUMBER) != 0)
    {
        k230_dbg("[P2] FAILED after retries!\r\n");
        return -1;
    }
    k230_dbg("[P2] ACK received, wait recognition done...\r\n");

    /* K230 识别完再发一个 ACK(同样是 0x0A), 不发数据帧。
     * 注意不要在这里清零 k230_ack_flag: 若 K230 识别很快, 第二个 ACK 可能在
     * send_cmd_and_wait_ack 返回前就已到达并被计数, 清零会把它抹掉导致白等。 */
    for (int retry = 0; retry < K230_RETRY_MAX; retry++)
    {
        if (k230_wait_flag(&k230_ack_flag, K230_DATA_TIMEOUT_MS) == 0)
        {
            k230_dbg("[P2] Recognition done\r\n");
            k230_write(K230_CMD_CLOSE);
            k230_dbg("[P2] ACK sent\r\n");
            return 0;
        }
        /* 同 Phase1: 先发 RESYNC 要一次重传。不能发 CLOSE —— 会让 K230 误以为
         * 本阶段已完成而关摄像头前进, 而主机还在等它的识别完成 ACK。 */
        k230_dbg("[P2] recognition timeout, send RESYNC\r\n");
        k230_write(K230_CMD_RESYNC);
        if (k230_wait_flag(&k230_ack_flag, K230_RESYNC_TIMEOUT_MS) == 0)
        {
            k230_dbg("[P2] Recognition done (resync)\r\n");
            k230_write(K230_CMD_CLOSE);
            k230_dbg("[P2] ACK sent\r\n");
            return 0;
        }

        k230_dbg("[P2] resync failed, resend LOOK_NUMBER\r\n");
        osDelay(200);
        /* 只重发一次, 不用 send_cmd_and_wait_ack, 否则又是 3x3 嵌套 */
        k230_send_cmd_once(K230_CMD_LOOK_NUMBER);
    }

    k230_dbg("[P2] FAILED after retries!\r\n");
    return -1;
}

/**
  * @brief  Phase3: 侧面数字 + 推理第5位, 结果填入 number_position[5]
  * @retval 0=成功, -1=重试耗尽
  */
int k230_phase3_side(void)
{
    char buf[80];

    s_cur_phase = K230_PHASE_SIDE;
    k230_dbg("[P3] Send LOOK_SIDE\r\n");
    if (k230_send_cmd_wait_ack_wait_data(K230_CMD_LOOK_SIDE, &full_number_flag) != 0)
    {
        k230_dbg("[P3] FAILED after retries!\r\n");
        return -1;
    }
    snprintf(buf, sizeof(buf), "[P3] Numbers: %02X %02X %02X %02X %02X\r\n",
             number_position[0], number_position[1], number_position[2],
             number_position[3], number_position[4]);
    k230_dbg(buf);
    k230_write(K230_CMD_CLOSE);
    k230_dbg("[P3] ACK sent\r\n");
    return 0;
}

