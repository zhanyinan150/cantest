#include "stm32f4xx_hal.h"
#include "Emm_V5.h"
#include "cmsis_os.h" // FreeRTOS API
#include "semphr.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
// 函数前向声明，避免隐式声明警告
void Emm_V5_Set_Encoder_Zero(uint8_t id);
void Emm_V5_Get_All_Encoders(int32_t encoder[4]);
int32_t Emm_V5_Read_Encoder(uint8_t addr);

static SemaphoreHandle_t usart6_rx_sem = NULL;

// 编码器累计值，用于处理周期性清零问题
static int32_t accumulated_encoder[4] = {0};
static int32_t last_raw_encoder[4] = {0};
static const int32_t ENCODER_MAX = 65535;  // 编码器一圈的最大值
static const int32_t ENCODER_HALF = 32768; // 编码器最大值的一半，用于判断溢出

// 添加软件零点偏移变量
static int32_t encoder_zero_offset[4] = {0};

/**********************************************************
***	Emm_V5.0步进闭环控制例程
***	编写作者：ZHANGDATOU
***	技术支持：张大头闭环技术
***	淘宝店铺：https://zhangdatou.taobao.com
***	CSDN博客：https://blog.csdn.net/zhangdatou666
***	qq交流群：262438510
**********************************************************/

/**
  * @brief    将当前位置清零
  * @param    addr  ：电机地址
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Reset_CurPos_To_Zero(uint8_t addr)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x0A;                       // 功能码
  cmd[2] =  0x6D;                       // 辅助码
  cmd[3] =  0x6B;                       // 校验字节

  // 发送命令
	HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 4);
}

/**
  * @brief    解除堵转保护
  * @param    addr  ：电机地址
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Reset_Clog_Pro(uint8_t addr)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x0E;                       // 功能码
  cmd[2] =  0x52;                       // 辅助码
  cmd[3] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 4);
}

/**
  * @brief    读取系统参数
  * @param    addr  ：电机地址
  * @param    s     ：系统参数类型
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Read_Sys_Params(uint8_t addr, SysParams_t1 s)
{
  uint8_t i = 0;
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[i] = addr; ++i;                   // 地址

  switch(s)                             // 功能码
  {
    case S_VER  : cmd[i] = 0x1F; ++i; break;
    case S_RL   : cmd[i] = 0x20; ++i; break;
    case S_PID  : cmd[i] = 0x21; ++i; break;
    case S_VBUS : cmd[i] = 0x24; ++i; break;
    case S_CPHA : cmd[i] = 0x27; ++i; break;
    case S_ENCL : cmd[i] = 0x31; ++i; break;
    case S_TPOS : cmd[i] = 0x33; ++i; break;
    case S_VEL  : cmd[i] = 0x35; ++i; break;
    case S_CPOS : cmd[i] = 0x36; ++i; break;
    case S_PERR : cmd[i] = 0x37; ++i; break;
    case S_FLAG : cmd[i] = 0x3A; ++i; break;
    case S_ORG  : cmd[i] = 0x3B; ++i; break;
    case S_Conf : cmd[i] = 0x42; ++i; cmd[i] = 0x6C; ++i; break;
    case S_State: cmd[i] = 0x43; ++i; cmd[i] = 0x7A; ++i; break;
    default: break;
  }

  cmd[i] = 0x6B; ++i;                   // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, i);
}

/**
  * @brief    修改开环/闭环控制模式
  * @param    addr     ：电机地址
  * @param    svF      ：是否存储标志，false为不存储，true为存储
  * @param    ctrl_mode：控制模式（对应屏幕上的P_Pul菜单），0是关闭脉冲输入引脚，1是开环模式，2是闭环模式，3是让En端口复用为多圈限位开关输入引脚，Dir端口复用为到位输出高电平功能
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Modify_Ctrl_Mode(uint8_t addr, bool svF, uint8_t ctrl_mode)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x46;                       // 功能码
  cmd[2] =  0x69;                       // 辅助码
  cmd[3] =  svF;                        // 是否存储标志，false为不存储，true为存储
  cmd[4] =  ctrl_mode;                  // 控制模式（对应屏幕上的P_Pul菜单），0是关闭脉冲输入引脚，1是开环模式，2是闭环模式，3是让En端口复用为多圈限位开关输入引脚，Dir端口复用为到位输出高电平功能
  cmd[5] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 6);
}

/**
  * @brief    使能信号控制
  * @param    addr  ：电机地址
  * @param    state ：使能状态     ，true为使能电机，false为关闭电机
  * @param    snF   ：多机同步标志 ，false为不启用，true为启用
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_En_Control(uint8_t addr, bool state, bool snF)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0xF3;                       // 功能码
  cmd[2] =  0xAB;                       // 辅助码
  cmd[3] =  (uint8_t)state;             // 使能状态
  cmd[4] =  snF;                        // 多机同步运动标志
  cmd[5] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 6);
}

/**
  * @brief    速度模式
  * @param    addr：电机地址
  * @param    dir ：方向       ，0为CW，其余值为CCW
  * @param    vel ：速度       ，范围0 - 5000RPM
  * @param    acc ：加速度     ，范围0 - 255，注意：0是直接启动
  * @param    snF ：多机同步标志，false为不启用，true为启用
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Vel_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, bool snF)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0xF6;                       // 功能码
  cmd[2] =  dir;                        // 方向
  cmd[3] =  (uint8_t)(vel >> 8);        // 速度(RPM)高8位字节
  cmd[4] =  (uint8_t)(vel >> 0);        // 速度(RPM)低8位字节
  cmd[5] =  acc;                        // 加速度，注意：0是直接启动
  cmd[6] =  snF;                        // 多机同步运动标志
  cmd[7] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 8);
  osDelay(3);

}

/**
  * @brief    位置模式
  * @param    addr：电机地址
  * @param    dir ：方向        ，0为CW，其余值为CCW
  * @param    vel ：速度(RPM)   ，范围0 - 5000RPM
  * @param    acc ：加速度      ，范围0 - 255，注意：0是直接启动
  * @param    clk ：脉冲数      ，范围0- (2^32 - 1)个
  * @param    raF ：相位/绝对标志，false为相对运动，true为绝对值运动
  * @param    snF ：多机同步标志 ，false为不启用，true为启用
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Pos_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, uint32_t clk, bool raF, bool snF)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0]  =  addr;                      // 地址
  cmd[1]  =  0xFD;                      // 功能码
  cmd[2]  =  dir;                       // 方向
  cmd[3]  =  (uint8_t)(vel >> 8);       // 速度(RPM)高8位字节
  cmd[4]  =  (uint8_t)(vel >> 0);       // 速度(RPM)低8位字节
  cmd[5]  =  acc;                       // 加速度，注意：0是直接启动
  cmd[6]  =  (uint8_t)(clk >> 24);      // 脉冲数(bit24 - bit31)
  cmd[7]  =  (uint8_t)(clk >> 16);      // 脉冲数(bit16 - bit23)
  cmd[8]  =  (uint8_t)(clk >> 8);       // 脉冲数(bit8  - bit15)
  cmd[9]  =  (uint8_t)(clk >> 0);       // 脉冲数(bit0  - bit7 )
  cmd[10] =  raF;                       // 相位/绝对标志，false为相对运动，true为绝对值运动
  cmd[11] =  snF;                       // 多机同步运动标志，false为不启用，true为启用
  cmd[12] =  0x6B;                      // 校验字节

  HAL_UART_Transmit(&huart5, (uint8_t *)cmd, 13,50);
}

/**
  * @brief    立即停止（所有控制模式都通用）
  * @param    addr  ：电机地址
  * @param    snF   ：多机同步标志，false为不启用，true为启用
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Stop_Now(uint8_t addr, bool snF)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0xFE;                       // 功能码
  cmd[2] =  0x98;                       // 辅助码
  cmd[3] =  snF;                        // 多机同步运动标志
  cmd[4] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 5);
}

/**
  * @brief    多机同步运动
  * @param    addr  ：电机地址
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Synchronous_motion(uint8_t addr)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0xFF;                       // 功能码
  cmd[2] =  0x66;                       // 辅助码
  cmd[3] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 4);
}

/**
  * @brief    设置单圈回零的零点位置
  * @param    addr  ：电机地址
  * @param    svF   ：是否存储标志，false为不存储，true为存储
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Origin_Set_O(uint8_t addr, bool svF)
{
   uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x93;                       // 功能码
  cmd[2] =  0x88;                       // 辅助码
  cmd[3] =  svF;                        // 是否存储标志，false为不存储，true为存储
  cmd[4] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 5);
}

/**
  * @brief    修改回零参数
  * @param    addr  ：电机地址
  * @param    svF   ：是否存储标志，false为不存储，true为存储
  * @param    o_mode ：回零模式，0为单圈就近回零，1为单圈方向回零，2为多圈无限位碰撞回零，3为多圈有限位开关回零
  * @param    o_dir  ：回零方向，0为CW，其余值为CCW
  * @param    o_vel  ：回零速度，单位：RPM（转/分钟）
  * @param    o_tm   ：回零超时时间，单位：毫秒
  * @param    sl_vel ：无限位碰撞回零检测转速，单位：RPM（转/分钟）
  * @param    sl_ma  ：无限位碰撞回零检测电流，单位：Ma（毫安）
  * @param    sl_ms  ：无限位碰撞回零检测时间，单位：Ms（毫秒）
  * @param    potF   ：上电自动触发回零，false为不使能，true为使能
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Origin_Modify_Params(uint8_t addr, bool svF, uint8_t o_mode, uint8_t o_dir, uint16_t o_vel, uint32_t o_tm, uint16_t sl_vel, uint16_t sl_ma, uint16_t sl_ms, bool potF)
{
  uint8_t cmd[32] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x4C;                       // 功能码
  cmd[2] =  0xAE;                       // 辅助码
  cmd[3] =  svF;                        // 是否存储标志，false为不存储，true为存储
  cmd[4] =  o_mode;                     // 回零模式，0为单圈就近回零，1为单圈方向回零，2为多圈无限位碰撞回零，3为多圈有限位开关回零
  cmd[5] =  o_dir;                      // 回零方向
  cmd[6]  =  (uint8_t)(o_vel >> 8);     // 回零速度(RPM)高8位字节
  cmd[7]  =  (uint8_t)(o_vel >> 0);     // 回零速度(RPM)低8位字节
  cmd[8]  =  (uint8_t)(o_tm >> 24);     // 回零超时时间(bit24 - bit31)
  cmd[9]  =  (uint8_t)(o_tm >> 16);     // 回零超时时间(bit16 - bit23)
  cmd[10] =  (uint8_t)(o_tm >> 8);      // 回零超时时间(bit8  - bit15)
  cmd[11] =  (uint8_t)(o_tm >> 0);      // 回零超时时间(bit0  - bit7 )
  cmd[12] =  (uint8_t)(sl_vel >> 8);    // 无限位碰撞回零检测转速(RPM)高8位字节
  cmd[13] =  (uint8_t)(sl_vel >> 0);    // 无限位碰撞回零检测转速(RPM)低8位字节
  cmd[14] =  (uint8_t)(sl_ma >> 8);     // 无限位碰撞回零检测电流(Ma)高8位字节
  cmd[15] =  (uint8_t)(sl_ma >> 0);     // 无限位碰撞回零检测电流(Ma)低8位字节
  cmd[16] =  (uint8_t)(sl_ms >> 8);     // 无限位碰撞回零检测时间(Ms)高8位字节
  cmd[17] =  (uint8_t)(sl_ms >> 0);     // 无限位碰撞回零检测时间(Ms)低8位字节
  cmd[18] =  potF;                      // 上电自动触发回零，false为不使能，true为使能
  cmd[19] =  0x6B;                      // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 20);
}

/**
  * @brief    触发回零
  * @param    addr   ：电机地址
  * @param    o_mode ：回零模式，0为单圈就近回零，1为单圈方向回零，2为多圈无限位碰撞回零，3为多圈有限位开关回零
  * @param    snF   ：多机同步标志，false为不启用，true为启用
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Origin_Trigger_Return(uint8_t addr, uint8_t o_mode, bool snF)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x9A;                       // 功能码
  cmd[2] =  o_mode;                     // 回零模式，0为单圈就近回零，1为单圈方向回零，2为多圈无限位碰撞回零，3为多圈有限位开关回零
  cmd[3] =  snF;                        // 多机同步运动标志，false为不启用，true为启用
  cmd[4] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 5);
}

/**
  * @brief    强制中断并退出回零
  * @param    addr  ：电机地址
  * @retval   地址 + 功能码 + 命令状态 + 校验字节
  */
void Emm_V5_Origin_Interrupt(uint8_t addr)
{
  uint8_t cmd[16] = {0};

  // 装载命令
  cmd[0] =  addr;                       // 地址
  cmd[1] =  0x9C;                       // 功能码
  cmd[2] =  0x48;                       // 辅助码
  cmd[3] =  0x6B;                       // 校验字节

  // 发送命令
  HAL_UART_Transmit_DMA(&huart5, (uint8_t *)cmd, 5);
}

/**
  * @brief    初始化 UART5 接收信号量 + 清零编码器累计 (不预读总线)
  * @note     仅创建信号量并清零累计数组; 实际编码器读取由调用方按需进行。
  *           信号量名 usart6_rx_sem 为历史命名残留, 实际服务 UART5。
  *           必须在首次 HAL_UART_Receive_DMA(&huart5,...) 之前调用, 否则回调
  *           Emm_V5_UART_RxCpltCallback 因 sem==NULL 而失效。
  */
void Emm_V5_Init(void)
{
    if (usart6_rx_sem == NULL) {
        usart6_rx_sem = xSemaphoreCreateBinary();
    }
    for (int i = 0; i < 4; i++) {
        accumulated_encoder[i] = 0;
        last_raw_encoder[i] = 0;
        encoder_zero_offset[i] = 0;
    }
}

/**
  * @brief    DMA接收回调
  * @param    huart  ：UART句柄
  */
void Emm_V5_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART5) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (usart6_rx_sem) {
            xSemaphoreGiveFromISR(usart6_rx_sem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

// 读取单个电机编码器累计值（多圈, 含回绕处理）。阻塞 Q&A: 发3字节→DMA收5字节→信号量同步。
int32_t Emm_V5_Read_Encoder(uint8_t addr)
{
    uint8_t cmd[3] = {addr, 0x31, 0x6B}; // S_ENCL指令
    uint8_t rx_buf[5] = {0}; // 返回5字节：地址 + 0x31 + 高8位 + 低8位 + 校验字节
    int32_t raw_value = 0;
    int idx = addr - 1;

    // 先清空缓冲区和状态, 避免残留数据
    __HAL_UART_FLUSH_DRREGISTER(&huart5);
    HAL_UART_DMAStop(&huart5);

    // 发送读取编码器命令 (阻塞, 短超时)
    HAL_UART_Transmit(&huart5, cmd, 3, 5);

    // 启动DMA接收，返回5字节数据
    HAL_UART_Receive_DMA(&huart5, rx_buf, 5);

    if (usart6_rx_sem) {
        if (xSemaphoreTake(usart6_rx_sem, pdMS_TO_TICKS(30)) == pdTRUE) {
            // 校验地址 + 功能码
            if (rx_buf[0] == addr && rx_buf[1] == 0x31) {
                // 单圈编码器值 (0~65535), 高字节在前
                raw_value = (int32_t)((rx_buf[2] << 8) | rx_buf[3]);

                // 多圈累计: 单圈值在 0~65535 间循环, 需检测跨零点回绕, 累计真实行程。
                // diff = 本周期值 - 上周期值, 正常转动 |diff| < 半圈(32768)。
                int32_t diff = raw_value - last_raw_encoder[idx];

                if (last_raw_encoder[idx] > ENCODER_HALF && raw_value < ENCODER_HALF/2) {
                    // 驱动器周期性清零: 上次接近满量程, 这次突变为小值 (非正常回绕),
                    // 按正向跨零处理: 补上 (满量程-上次值) + 本次值
                    accumulated_encoder[idx] += (ENCODER_MAX - last_raw_encoder[idx]) + raw_value;
                } else if (diff < -ENCODER_HALF) {
                    // 正向回绕: 顺时针跨过 65535→0, 实际前进 (65536+diff) 步
                    accumulated_encoder[idx] += ENCODER_MAX + diff;
                } else if (diff > ENCODER_HALF) {
                    // 反向回绕: 逆时针跨过 0→65535, 实际后退 (65536-diff) 步
                    accumulated_encoder[idx] -= ENCODER_MAX - diff;
                } else {
                    accumulated_encoder[idx] += diff;
                }

                last_raw_encoder[idx] = raw_value;
                return accumulated_encoder[idx]; // 返回累计值
            }
            // 响应帧不匹配: 静默返回上次累计值
        }
        // 接收超时: 静默返回上次累计值
    }

    return accumulated_encoder[idx]; // 读取失败也返回上次累计值
}

/**
  * @brief    快速读取1-4号电机编码器累计值
  * @note     针对某个电机不可用的情况进行了优化
  * @param    encoder 存储编码器累计值的数组，大小为4
  */
void Emm_V5_Get_All_Encoders(int32_t encoder[4])
{
    static uint8_t debug_count = 0;
    bool debug_log = (++debug_count % 20) == 0; // 每20次输出一次日志
    if (debug_log) {
        LOGINFO("读取编码器...");
    }

    // 先清空串口缓冲区
    __HAL_UART_FLUSH_DRREGISTER(&huart5);
    HAL_UART_DMAStop(&huart5);
    osDelay(5); // 减少等待时间

    // 读取1、2、3、4号电机
    int32_t raw_encoder[4];
    raw_encoder[0] = Emm_V5_Read_Encoder(1);
    raw_encoder[1] = Emm_V5_Read_Encoder(2);
    raw_encoder[2] = Emm_V5_Read_Encoder(3);
    raw_encoder[3] = Emm_V5_Read_Encoder(4);

    // 应用零点偏移
    for (int i = 0; i < 4; i++) {
        encoder[i] = raw_encoder[i] - encoder_zero_offset[i];
    }

    if (debug_log) {
        LOGINFO("编码器原始值: [%d, %d, %d, %d]", raw_encoder[0], raw_encoder[1], raw_encoder[2], raw_encoder[3]);
        LOGINFO("编码器偏移后结果: [%d, %d, %d, %d]", encoder[0], encoder[1], encoder[2], encoder[3]);
    }
}

/**
  * @brief    重置编码器累计计数
  * @param    id 电机ID，1-4，如果为0则重置所有电机
  */
void Emm_V5_Reset_Encoder_Accumulation(uint8_t id)
{
    if (id == 0) {
        // 重置所有电机的累计值
        for (int i = 0; i < 4; i++) {
            accumulated_encoder[i] = 0;
            last_raw_encoder[i] = 0;
        }
    } else if (id <= 4) {
        // 重置指定电机的累计值
        accumulated_encoder[id-1] = 0;
        last_raw_encoder[id-1] = 0;
    }
}

/**
  * @brief    设置编码器软件零点（不改变原始编码器值，只设置偏移量）
  * @param    id 电机ID，1-4，如果为0则设置所有电机
  */
void Emm_V5_Set_Encoder_Zero(uint8_t id)
{
    int32_t current_values[4] = {0};

    // 先获取当前的编码器值
    Emm_V5_Get_All_Encoders(current_values);

    if (id == 0) {
        // 设置所有电机的零点偏移
        for (int i = 0; i < 4; i++) {
            encoder_zero_offset[i] = current_values[i];
            LOGINFO("电机%d设置零点偏移: %d", i+1, encoder_zero_offset[i]);
        }
    } else if (id <= 4) {
        // 设置指定电机的零点偏移
        encoder_zero_offset[id-1] = current_values[id-1];
        LOGINFO("电机%d设置零点偏移: %d", id, encoder_zero_offset[id-1]);
    }
}
