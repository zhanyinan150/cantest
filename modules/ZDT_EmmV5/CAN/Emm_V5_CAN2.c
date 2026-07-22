/**
  ******************************************************************************
  * @file    Emm_V5_CAN2.c
  * @brief   Emm_V5.0 步进闭环 CAN2 驱动 (直接 HAL CAN, 不依赖 bsp_can)
  ******************************************************************************
  * 参考 UART 版 Emm_V5.c/h, 把 HAL_UART_Transmit 换成 HAL_CAN_AddTxMessage。
  * 命令字节布局与 UART 版完全一致, 仅发送通路改为 CAN2 扩展帧。
  ******************************************************************************
  */

#include "Emm_V5_CAN2.h"
#include "can.h"          /* hcan2 */
#include "cmsis_os2.h"    /* osDelay */
#include "stdio.h"

/* ==================== 内部参数 ==================== */
#define CAN2_TX_TIMEOUT_MS   10      /* 等发送邮箱空闲的超时(ms) */
#define CAN2_MULTIFRAME_GAP  2       /* 多帧命令之间间隔(ms) */

/* 帧发送日志开关 (默认关): 打开后每帧 printf 实际 CAN ID/DLC/数据 */
static bool s_frame_log_enable = false;
void Emm_V5_CAN2_SetFrameLog(bool en) { s_frame_log_enable = en; }

/* ==================== 内部: 单帧发送 ==================== */

/**
  * @brief  发送一个 CAN2 扩展帧
  * @param  ext_id 29位扩展ID
  * @param  data   数据指针
  * @param  dlc    数据长度 (1~8)
  * @retval true=成功加入发送邮箱
  */
static bool Emm_V5_CAN2_TransmitFrame(uint32_t ext_id, uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef tx = {0};
    tx.ExtId = ext_id;
    tx.IDE   = CAN_ID_EXT;
    tx.RTR   = CAN_RTR_DATA;
    tx.DLC   = dlc;

    /* 等待有空闲邮箱 (3个邮箱可能都被占) */
    uint32_t timeout = CAN2_TX_TIMEOUT_MS;
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan2) == 0) {
        if (timeout == 0) return false;
        timeout--;
        osDelay(1);
    }

    uint32_t mailbox = 0;
    return (HAL_CAN_AddTxMessage(&hcan2, &tx, data, &mailbox) == HAL_OK);
}

/* ==================== 内部: 通用发送 (含分包) ==================== */

/**
  * @brief  通用发送: cmd[0]=地址, 后续为命令字节
  * @note   >8 数据字节自动分包:
  *           第1包 ExtId=(addr<<8)+0, 数据 cmd[1..8]
  *           第2包 ExtId=(addr<<8)+1, 数据 cmd[9..16]
  *           ...
  *         <=8 数据字节: 单帧, ExtId=addr<<8, 序号=0
  */
bool Emm_V5_CAN2_SendCmd(uint8_t *cmd, uint16_t len)
{
    if (len == 0 || len > 64) return false;

    uint8_t  addr          = cmd[0];
    uint8_t  packet_index  = 0;
    uint16_t data_offset   = 1;   /* 跳过 cmd[0] (地址), 数据从 cmd[1] 开始 */

    while (data_offset < len) {
        uint8_t  frame_len = (len - data_offset > 8) ? 8 : (uint8_t)(len - data_offset);
        uint32_t can_id    = ((uint32_t)addr << 8) + ((len - 1 > 8) ? packet_index : 0);

        /* 帧日志: 打印即将下发的真实 CAN 帧 */
        if (s_frame_log_enable) {
            printf("[CAN2 TX] addr=%u ID=0x%06lX DLC=%u pkt=%u DATA:",
                   (unsigned)addr, (unsigned long)can_id,
                   (unsigned)frame_len, (unsigned)packet_index);
            for (uint8_t i = 0; i < frame_len; i++)
                printf(" %02X", cmd[data_offset + i]);
            printf("\r\n");
        }

        if (!Emm_V5_CAN2_TransmitFrame(can_id, &cmd[data_offset], frame_len))
            return false;

        data_offset += frame_len;
        packet_index++;

        if (len - 1 > 8)
            osDelay(CAN2_MULTIFRAME_GAP);   /* 多帧命令间隔, 避免总线拥塞 */
    }
    return true;
}

/* ==================== 初始化 ==================== */

void Emm_V5_CAN2_Init(void)
{
    /* 配置 CAN2 过滤器: bank 14 (CAN2 从 bank 14 开始), 接收所有扩展帧到 FIFO0
     * mask=0 -> 接收所有 ID (供后续 Read_Sys_Params 等读状态用) */
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank           = 14;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = 0;
    filter.FilterIdLow          = 0;
    filter.FilterMaskIdHigh     = 0;
    filter.FilterMaskIdLow      = 0;        /* mask=0, 全收 */
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = CAN_FILTER_ENABLE;
    filter.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan2, &filter);

    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* ==================== 控制命令 (字节布局同 UART 版) ==================== */

/**
  * @brief  将当前位置清零
  */
void Emm_V5_CAN2_Reset_CurPos_To_Zero(uint8_t addr)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0x0A;
    cmd[2] = 0x6D;
    cmd[3] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 4);
}

/**
  * @brief  解除堵转保护
  */
void Emm_V5_CAN2_Reset_Clog_Pro(uint8_t addr)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0x0E;
    cmd[2] = 0x52;
    cmd[3] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 4);
}

/**
  * @brief  读取系统参数
  */
void Emm_V5_CAN2_Read_Sys_Params(uint8_t addr, EmmV5_CAN2_SysParam_t s)
{
    uint8_t i = 0;
    uint8_t cmd[16] = {0};
    cmd[i] = addr; ++i;

    switch (s) {
        case EMM_V5_CAN2_S_VER  : cmd[i] = 0x1F; ++i; break;
        case EMM_V5_CAN2_S_RL   : cmd[i] = 0x20; ++i; break;
        case EMM_V5_CAN2_S_PID  : cmd[i] = 0x21; ++i; break;
        case EMM_V5_CAN2_S_VBUS : cmd[i] = 0x24; ++i; break;
        case EMM_V5_CAN2_S_CPHA : cmd[i] = 0x27; ++i; break;
        case EMM_V5_CAN2_S_ENCL : cmd[i] = 0x31; ++i; break;
        case EMM_V5_CAN2_S_TPOS : cmd[i] = 0x33; ++i; break;
        case EMM_V5_CAN2_S_VEL  : cmd[i] = 0x35; ++i; break;
        case EMM_V5_CAN2_S_CPOS : cmd[i] = 0x36; ++i; break;
        case EMM_V5_CAN2_S_PERR : cmd[i] = 0x37; ++i; break;
        case EMM_V5_CAN2_S_FLAG : cmd[i] = 0x3A; ++i; break;
        case EMM_V5_CAN2_S_ORG  : cmd[i] = 0x3B; ++i; break;
        case EMM_V5_CAN2_S_Conf : cmd[i] = 0x42; ++i; cmd[i] = 0x6C; ++i; break;
        case EMM_V5_CAN2_S_State: cmd[i] = 0x43; ++i; cmd[i] = 0x7A; ++i; break;
        default: break;
    }
    cmd[i] = 0x6B; ++i;
    Emm_V5_CAN2_SendCmd(cmd, i);
}

/**
  * @brief  修改开环/闭环控制模式
  * @param  svF       是否存储
  * @param  ctrl_mode 0=关闭脉冲, 1=开环, 2=闭环, 3=En/Dir复用
  */
void Emm_V5_CAN2_Modify_Ctrl_Mode(uint8_t addr, bool svF, uint8_t ctrl_mode)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0x46;
    cmd[2] = 0x69;
    cmd[3] = (uint8_t)svF;
    cmd[4] = ctrl_mode;
    cmd[5] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 6);
}

/**
  * @brief  使能信号控制
  * @param  state true=使能, false=关闭
  * @param  snF   多机同步标志
  */
void Emm_V5_CAN2_En_Control(uint8_t addr, bool state, bool snF)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0xF3;
    cmd[2] = 0xAB;
    cmd[3] = (uint8_t)state;
    cmd[4] = snF;
    cmd[5] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 6);
}

/**
  * @brief  速度模式
  * @param  dir 0=CW, 其余=CCW
  * @param  vel 0~3000 RPM
  * @param  acc 0~255, 0=直接启动
  * @param  snF 多机同步标志
  */
bool Emm_V5_CAN2_Vel_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, bool snF)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0xF6;
    cmd[2] = dir;
    cmd[3] = (uint8_t)(vel >> 8);
    cmd[4] = (uint8_t)(vel >> 0);
    cmd[5] = acc;
    cmd[6] = snF;
    cmd[7] = 0x6B;
    return Emm_V5_CAN2_SendCmd(cmd, 8);
}

/**
  * @brief  位置模式
  * @param  dir 0=CW, 其余=CCW
  * @param  vel 0~3000 RPM
  * @param  acc 0~255, 0=直接启动
  * @param  clk 脉冲数 (0 ~ 2^32-1)
  * @param  raF false=相对运动, true=绝对运动
  * @param  snF 多机同步标志
  */
void Emm_V5_CAN2_Pos_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, uint32_t clk, bool raF, bool snF)
{
    uint8_t cmd[16] = {0};
    cmd[0]  = addr;
    cmd[1]  = 0xFD;
    cmd[2]  = dir;
    cmd[3]  = (uint8_t)(vel >> 8);
    cmd[4]  = (uint8_t)(vel >> 0);
    cmd[5]  = acc;
    cmd[6]  = (uint8_t)(clk >> 24);
    cmd[7]  = (uint8_t)(clk >> 16);
    cmd[8]  = (uint8_t)(clk >> 8);
    cmd[9]  = (uint8_t)(clk >> 0);
    cmd[10] = raF;
    cmd[11] = snF;
    cmd[12] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 13);
}

/**
  * @brief  立即停止 (所有控制模式通用)
  */
void Emm_V5_CAN2_Stop_Now(uint8_t addr, bool snF)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0xFE;
    cmd[2] = 0x98;
    cmd[3] = snF;
    cmd[4] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 5);
}

/**
  * @brief  触发多机同步运动
  */
bool Emm_V5_CAN2_Synchronous_motion(uint8_t addr)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0xFF;
    cmd[2] = 0x66;
    cmd[3] = 0x6B;
    return Emm_V5_CAN2_SendCmd(cmd, 4);
}

/**
  * @brief  设置单圈回零的零点位置
  */
void Emm_V5_CAN2_Origin_Set_O(uint8_t addr, bool svF)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0x93;
    cmd[2] = 0x88;
    cmd[3] = svF;
    cmd[4] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 5);
}

/**
  * @brief  修改回零参数
  */
void Emm_V5_CAN2_Origin_Modify_Params(uint8_t addr, bool svF, uint8_t o_mode, uint8_t o_dir,
                                       uint16_t o_vel, uint32_t o_tm,
                                       uint16_t sl_vel, uint16_t sl_ma, uint16_t sl_ms, bool potF)
{
    uint8_t cmd[32] = {0};
    cmd[0]  = addr;
    cmd[1]  = 0x4C;
    cmd[2]  = 0xAE;
    cmd[3]  = svF;
    cmd[4]  = o_mode;
    cmd[5]  = o_dir;
    cmd[6]  = (uint8_t)(o_vel >> 8);
    cmd[7]  = (uint8_t)(o_vel >> 0);
    cmd[8]  = (uint8_t)(o_tm >> 24);
    cmd[9]  = (uint8_t)(o_tm >> 16);
    cmd[10] = (uint8_t)(o_tm >> 8);
    cmd[11] = (uint8_t)(o_tm >> 0);
    cmd[12] = (uint8_t)(sl_vel >> 8);
    cmd[13] = (uint8_t)(sl_vel >> 0);
    cmd[14] = (uint8_t)(sl_ma >> 8);
    cmd[15] = (uint8_t)(sl_ma >> 0);
    cmd[16] = (uint8_t)(sl_ms >> 8);
    cmd[17] = (uint8_t)(sl_ms >> 0);
    cmd[18] = potF;
    cmd[19] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 20);
}

/**
  * @brief  触发回零
  */
void Emm_V5_CAN2_Origin_Trigger_Return(uint8_t addr, uint8_t o_mode, bool snF)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0x9A;
    cmd[2] = o_mode;
    cmd[3] = snF;
    cmd[4] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 5);
}

/**
  * @brief  强制中断并退出回零
  */
void Emm_V5_CAN2_Origin_Interrupt(uint8_t addr)
{
    uint8_t cmd[16] = {0};
    cmd[0] = addr;
    cmd[1] = 0x9C;
    cmd[2] = 0x48;
    cmd[3] = 0x6B;
    Emm_V5_CAN2_SendCmd(cmd, 4);
}
