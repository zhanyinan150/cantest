#ifndef __EMM_V5_CAN2_H
#define __EMM_V5_CAN2_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/**********************************************************
***	Emm_V5.0 步进闭环 CAN2 驱动 (直接 HAL CAN, 不依赖 bsp_can)
***	参考 UART 版 Emm_V5.c/h, 通信改为 CAN2 扩展帧。
***
*** CAN 帧格式 (与 Emm_V5 CAN 协议一致):
***   - 扩展帧 (IDE=EXT), 29 位 ID
***   - ExtId = (电机地址 << 8) | 分包序号
***   - 数据区 = 命令字节 (跳过首字节地址, 地址编码进 ExtId)
***   - 命令 <=8 数据字节: 单帧, ExtId = addr<<8, 序号=0
***   - 命令 >8 数据字节: 自动分包, 第1包序号0, 第2包序号1...
***
*** 使用前调用 Emm_V5_CAN2_Init() 完成 CAN2 过滤器+启动。
*** 需先在 main() 里调用 MX_CAN2_Init() (CubeMX 生成)。
**********************************************************/

#define EmmV5_CAN2_ABS(x) ((x) > 0 ? (x) : -(x))

/* 系统参数类型 (与 UART 版 SysParams_t1 一一对应) */
typedef enum {
    EMM_V5_CAN2_S_VER   = 0,   /* 读取固件版本和对应的硬件版本 */
    EMM_V5_CAN2_S_RL    = 1,   /* 读取相电阻和相电感 */
    EMM_V5_CAN2_S_PID   = 2,   /* 读取PID参数 */
    EMM_V5_CAN2_S_VBUS  = 3,   /* 读取总线电压 */
    EMM_V5_CAN2_S_CPHA  = 5,   /* 读取相电流 */
    EMM_V5_CAN2_S_ENCL  = 7,   /* 读取线性化校准后的编码器值 */
    EMM_V5_CAN2_S_TPOS  = 8,   /* 读取电机目标位置角度 */
    EMM_V5_CAN2_S_VEL   = 9,   /* 读取电机实时转速 */
    EMM_V5_CAN2_S_CPOS  = 10,  /* 读取电机实时位置角度 */
    EMM_V5_CAN2_S_PERR  = 11,  /* 读取电机位置误差角度 */
    EMM_V5_CAN2_S_FLAG  = 13,  /* 读取使能/到位/堵转状态标志位 */
    EMM_V5_CAN2_S_Conf  = 14,  /* 读取驱动参数 */
    EMM_V5_CAN2_S_State = 15,  /* 读取系统状态参数 */
    EMM_V5_CAN2_S_ORG   = 16,  /* 读取回零状态标志位 */
} EmmV5_CAN2_SysParam_t;

/* 电机状态标志位掩码 (S_FLAG 响应字节) */
#define EMM_V5_CAN2_FLAG_ENABLE      0x01   /* 使能 */
#define EMM_V5_CAN2_FLAG_ARRIVED     0x02   /* 到位 */
#define EMM_V5_CAN2_FLAG_STALL       0x04   /* 堵转 */
#define EMM_V5_CAN2_FLAG_STALL_PROT  0x08   /* 堵转保护 */

/**
  * @brief  初始化 CAN2: 配置过滤器 + 启动 + 使能接收中断
  * @note   内部使用 hcan2 (CAN2 外设)。须先在 main() 调用 MX_CAN2_Init()。
  *         过滤器: bank 14, 接收所有扩展帧到 FIFO0 (供后续读状态用)。
  */
void Emm_V5_CAN2_Init(void);

/**
  * @brief  打开/关闭帧发送日志 (USART1 打印每帧)
  * @param  en true=使能, false=关闭(默认)
  */
void Emm_V5_CAN2_SetFrameLog(bool en);

/**
  * @brief  通用发送: cmd[0]=地址, 后续为命令字节, len=总长度
  * @note   自动处理 >8 字节的分包。返回是否全部发送成功。
  */
bool Emm_V5_CAN2_SendCmd(uint8_t *cmd, uint16_t len);

/* ---- 控制命令 (参数定义与 UART 版一致) ---- */
void Emm_V5_CAN2_Reset_CurPos_To_Zero(uint8_t addr);                                    /* 将当前位置清零 */
void Emm_V5_CAN2_Reset_Clog_Pro(uint8_t addr);                                          /* 解除堵转保护 */
void Emm_V5_CAN2_Read_Sys_Params(uint8_t addr, EmmV5_CAN2_SysParam_t s);                /* 读取参数 */
void Emm_V5_CAN2_Modify_Ctrl_Mode(uint8_t addr, bool svF, uint8_t ctrl_mode);           /* 修改开环/闭环模式 */
void Emm_V5_CAN2_En_Control(uint8_t addr, bool state, bool snF);                        /* 使能控制 */
bool Emm_V5_CAN2_Vel_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, bool snF);  /* 速度模式 */
void Emm_V5_CAN2_Pos_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, uint32_t clk, bool raF, bool snF);  /* 位置模式 */
void Emm_V5_CAN2_Stop_Now(uint8_t addr, bool snF);                                      /* 立即停止 */
bool Emm_V5_CAN2_Synchronous_motion(uint8_t addr);                                      /* 触发多机同步 */
void Emm_V5_CAN2_Origin_Set_O(uint8_t addr, bool svF);                                  /* 设置回零零点 */
void Emm_V5_CAN2_Origin_Modify_Params(uint8_t addr, bool svF, uint8_t o_mode, uint8_t o_dir, uint16_t o_vel, uint32_t o_tm, uint16_t sl_vel, uint16_t sl_ma, uint16_t sl_ms, bool potF);  /* 修改回零参数 */
void Emm_V5_CAN2_Origin_Trigger_Return(uint8_t addr, uint8_t o_mode, bool snF);         /* 触发回零 */
void Emm_V5_CAN2_Origin_Interrupt(uint8_t addr);                                        /* 中断回零 */

#endif /* __EMM_V5_CAN2_H */
