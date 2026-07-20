// /**
//   ******************************************************************************
//   * @file    motor_uart_test.c
//   * @brief   UART5 步进电机测试: 3号电机(X轴) 水平往复 10cm
//   ******************************************************************************
//   * 电机 ID=3 (Emm_V5 步进闭环, UART5/115200, PC12=TX PD2=RX), 单电机无需同步。
//   *
//   * 流程:
//   *   1. 初始化: 清堵转保护 -> 设闭环模式 -> 使能
//   *   2. 位置模式往复: 每次走 10cm, 方向交替, 间隔 2 秒
//   *
//   * 10cm -> 脉冲换算 (X轴机械参数, 见 mech_params.h):
//   *   clk = (距离/周长) × 每转脉冲 × 减速比
//   *       = (10 / (π×1.4)) × 65536 × 1 ≈ 149023 脉冲
//   *
//   * 注意: UART5 在本工程未配置 DMA (usart.c::HAL_UART_MspInit 的 UART5 分支只配了
//   *       GPIO/时钟), 故 Emm_V5.c 中 HAL_UART_Transmit_DMA 系列函数发不出数据。
//   *       本模块所有命令统一用 HAL_UART_Transmit (阻塞) 发送, 不依赖 DMA。
//   ******************************************************************************
//   */

// #include "motor_uart_test.h"

// #include "usart.h"          /* huart5 */
// #include "cmsis_os2.h"
// #include "FreeRTOS.h"
// #include "task.h"
// #include "mech_params.h"    /* X轴机械参数 */
// #include "stdio.h"

// /* ==================== 任务参数 ==================== */
// #define MOTOR_UART_TASK_STACK_SIZE   512
// #define MOTOR_UART_TASK_PRIORITY     osPriorityNormal
// #define MOTOR_UART_STARTUP_DELAY     1000   /* 上电稳定(ms) */

// /* ==================== 运动参数 ==================== */
// #define MOTOR_UART_ADDR              3       /* 3号电机 = X轴 (mech_params.h) */
// #define TEST_DISTANCE_CM             10.0f   /* 每次移动距离(cm) */
// #define TEST_VEL_RPM                 300     /* 转速(RPM), Emm_V5 范围 0~5000 */
// #define TEST_ACC                     180     /* 加速度档位 0~255, 0=直接启动 */
// #define TEST_MOVE_DELAY_MS           2000    /* 两次位置命令间隔(ms)
//                                               * 10cm@300rpm≈0.5s, 留足余量 */

// /* 10cm 换算为脉冲数 (X轴: 周长 π×1.4cm, 减速比1, 每转65536脉冲) */
// #define TEST_CLK  ((uint32_t)((TEST_DISTANCE_CM / MOTOR_X_WHEEL_CIRCUMFERENCE_CM) \
//                               * MOTOR_XY_PULSE_PER_REV * MOTOR_X_GEAR_RATIO))

// /* ==================== 内部函数 ==================== */
// static void MotorUartTestTask(void *argument);

// /* ---- UART5 阻塞发送辅助 ----
//  * Emm_V5.c 多数函数用 HAL_UART_Transmit_DMA, 但 UART5 未配 DMA 致发送失败,
//  * 故本模块统一用阻塞发送。50ms 超时对最长 13 字节帧 (@115200≈1.2ms) 足够。 */
// static void uart5_send_blocking(const uint8_t *buf, uint16_t len)
// {
//     HAL_UART_Transmit(&huart5, (uint8_t *)buf, len, 50);
// }

// /* ---- 电机初始化/控制命令 (协议帧与 Emm_V5.c 一致, 改用阻塞发送) ---- */

// /* 解除堵转保护: addr + 0x0E + 0x52 + 0x6B */
// static void motor_reset_clog(uint8_t addr)
// {
//     uint8_t cmd[4] = { addr, 0x0E, 0x52, 0x6B };
//     uart5_send_blocking(cmd, 4);
// }

// /* 设闭环控制模式: addr + 0x46 + 0x69 + svF + mode + 0x6B
//  * Emm_V5(UART5版) ctrl_mode: 0=关脉冲 1=开环 2=闭环 (与 CAN 版不同, 勿混) */
// static void motor_set_closed_loop(uint8_t addr)
// {
//     uint8_t cmd[6] = { addr, 0x46, 0x69, 1/*svF=存储*/, 2/*闭环*/, 0x6B };
//     uart5_send_blocking(cmd, 6);
// }

// /* 电机使能: addr + 0xF3 + 0xAB + state + snF + 0x6B (单电机 snF=0) */
// static void motor_enable(uint8_t addr)
// {
//     uint8_t cmd[6] = { addr, 0xF3, 0xAB, 1/*使能*/, 0/*不同步*/, 0x6B };
//     uart5_send_blocking(cmd, 6);
// }

// /* 位置模式控制: addr + 0xFD + dir + vel(H/L) + acc + clk(B0~B3) + raF + snF + 0x6B
//  * 单电机 snF=0(不同步), raF=0(相对运动, 每次走 clk 脉冲) */
// static void motor_pos_control(uint8_t addr, uint8_t dir, uint16_t vel,
//                               uint8_t acc, uint32_t clk)
// {
//     uint8_t cmd[13] = {
//         addr,
//         0xFD,
//         dir,
//         (uint8_t)(vel >> 8),
//         (uint8_t)(vel >> 0),
//         acc,
//         (uint8_t)(clk >> 24),
//         (uint8_t)(clk >> 16),
//         (uint8_t)(clk >> 8),
//         (uint8_t)(clk >> 0),
//         0,    /* raF=0 相对运动 */
//         0,    /* snF=0 不同步 */
//         0x6B
//     };
//     uart5_send_blocking(cmd, 13);
// }

// /* ==================== 公开接口 ==================== */

// void MotorUartTest_Init(void)
// {
//     printf("[motor_uart] UART5 115200, X axis addr=%d, dist=%dcm, clk=%lu\r\n",
//            MOTOR_UART_ADDR, (int)TEST_DISTANCE_CM, (unsigned long)TEST_CLK);

//     const osThreadAttr_t task_attr = {
//         .name = "MotorUartTestTask",
//         .stack_size = MOTOR_UART_TASK_STACK_SIZE * 4,
//         .priority = (osPriority_t)MOTOR_UART_TASK_PRIORITY,
//     };
//     osThreadNew(MotorUartTestTask, NULL, &task_attr);
// }

// /**
//   * @brief  测试任务: 3号电机位置模式往复, 每次 10cm, 方向交替
//   * @note   相对运动(raF=0): 第一次 CW 走 10cm, 第二次 CCW 回 10cm, 周而复始。
//   *         到位判断靠 TEST_MOVE_DELAY_MS 定时等待(非编码器反馈), 足够覆盖走完时间。
//   */
// static void MotorUartTestTask(void *argument)
// {
//     (void)argument;
//     osDelay(MOTOR_UART_STARTUP_DELAY);

//     /* 1. 电机准备: 清堵转 -> 闭环 -> 使能 (每步间隔让电机处理命令) */
//     motor_reset_clog(MOTOR_UART_ADDR);      osDelay(50);
//     motor_set_closed_loop(MOTOR_UART_ADDR); osDelay(50);
//     motor_enable(MOTOR_UART_ADDR);          osDelay(100);

//     printf("[motor_uart] motor ready, start reciprocating\r\n");

//     /* 2. 位置模式往复: 每次走 10cm, 方向交替, 间隔 2s */
//     uint8_t dir = 0;  /* 0=CW(右), 1=CCW(左), 每次发命令后翻转 */
//     for (;;) {
//         printf("[motor_uart] move dir=%d dist=%dcm clk=%lu\r\n",
//                dir, (int)TEST_DISTANCE_CM, (unsigned long)TEST_CLK);

//         motor_pos_control(MOTOR_UART_ADDR, dir, TEST_VEL_RPM, TEST_ACC, TEST_CLK);

//         dir ^= 1;  /* 翻转方向: 右->左->右->左... */
//         osDelay(TEST_MOVE_DELAY_MS);
//     }
// }
