#ifndef __EMM_V5_H
#define __EMM_V5_H
#include "stm32f4xx_hal.h"
#include "usart.h"
#include <stdbool.h>  // 添加stdbool.h，定义bool类型
/**********************************************************
***	Emm_V5.0步进闭环控制例程 (UART串口通讯版, 横移电机)
***	编写作者：ZHANGDATOU
***	技术支持：张大头闭环技术
***	淘宝店铺：https://zhangdatou.taobao.com
***	CSDN博客：https://blog.csdn.net/zhangdatou666
***	qq交流群：262438510
***
*** ===== 电机菜单设置 (首次上电在小屏幕配置, 详见说明书第4章) =====
*** 电机型号 : Emm42_V5.0 (Emm5.0固件)
*** P_Pul    : PUL_FOC      (FOC矢量闭环)
*** P_Serial : UART_FUN     (通讯端口复用为串口TTL/RS232/RS485)
*** En       : Hold         (一直使能, 由软件F3命令控制)
*** MStep    : 16           (细分; 位置模式每转脉冲 = 200×16 = 3200)
*** ID_Addr  : 1~4          (电机地址)
*** UartBaud : 115200       (须与主控UART5波特率一致)
*** Checksum : 0x6B         (固定校验字节)
*** Response : Receive      (只返回确认收到)
*** Cal      : 首次上电空载校准
***
*** ===== 串口通讯协议要点 (说明书 6.2.1) =====
*** 帧格式   : 地址 + 功能码 + 命令数据 + 0x6B
***             (串口版数据区含地址字节; CAN版地址在ExtId里, 数据区不含)
*** 速度范围 : 0~3000 RPM (F6速度模式, 0x0BB8; >3000驱动器返回E2)
*** 位置脉冲 : FD命令clk单位=细分脉冲, 16细分下3200脉冲/圈 (非编码器值65536!)
*** 加速度   : 0~255档, 0=直接启动; 公式 t=(256-acc)*50us 每1RPM
*** 注意     : 本文件函数通过 UART5 (huart5) 发送, 供横移模块使用
**********************************************************/

#define		ABS(x)		((x) > 0 ? (x) : -(x))

typedef enum {
	S_VER   = 0,			/* 读取固件版本和对应的硬件版本 */
	S_RL    = 1,			/* 读取相电阻和相电感 */
	S_PID   = 2,			/* 读取PID参数 */
	S_VBUS  = 3,			/* 读取总线电压 */
	S_CPHA  = 5,			/* 读取相电流 */
	S_ENCL  = 7,			/* 读取经过线性化校准后的编码器值 */
	S_TPOS  = 8,			/* 读取电机目标位置角度 */
	S_VEL   = 9,			/* 读取电机实时转速 */
	S_CPOS  = 10,			/* 读取电机实时位置角度 */
	S_PERR  = 11,			/* 读取电机位置误差角度 */
	S_FLAG  = 13,			/* 读取使能/到位/堵转状态标志位 */
	S_Conf  = 14,			/* 读取驱动参数 */
	S_State = 15,			/* 读取系统状态参数 */
	S_ORG   = 16,     /* 读取正在回零/回零失败状态标志位 */
}SysParams_t1;


/**********************************************************
*** 注意：每个函数的参数的具体说明，请查阅对应函数的注释说明
**********************************************************/
void Emm_V5_Reset_CurPos_To_Zero(uint8_t addr); // 将当前位置清零
void Emm_V5_Reset_Clog_Pro(uint8_t addr); // 解除堵转保护
void Emm_V5_Read_Sys_Params(uint8_t addr, SysParams_t1 s); // 读取参数
void Emm_V5_Modify_Ctrl_Mode(uint8_t addr, bool svF, uint8_t ctrl_mode); // 发送命令修改开环/闭环控制模式
void Emm_V5_En_Control(uint8_t addr, bool state, bool snF); // 电机使能控制
void Emm_V5_Vel_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, bool snF); // 速度模式控制
void Emm_V5_Pos_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, uint32_t clk, bool raF, bool snF); // 位置模式控制
void Emm_V5_Stop_Now(uint8_t addr, bool snF); // 让电机立即停止运动
void Emm_V5_Synchronous_motion(uint8_t addr); // 触发多机同步开始运动
void Emm_V5_Origin_Set_O(uint8_t addr, bool svF); // 设置单圈回零的零点位置
void Emm_V5_Origin_Modify_Params(uint8_t addr, bool svF, uint8_t o_mode, uint8_t o_dir, uint16_t o_vel, uint32_t o_tm, uint16_t sl_vel, uint16_t sl_ma, uint16_t sl_ms, bool potF); // 修改回零参数
void Emm_V5_Origin_Trigger_Return(uint8_t addr, uint8_t o_mode, bool snF); // 发送命令触发回零
void Emm_V5_Origin_Interrupt(uint8_t addr); // 强制中断并退出回零
void Emm_V5_Get_All_Encoders(int32_t encoder[4]);
void Emm_V5_Init(void);
void Emm_V5_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void Emm_V5_Reset_Encoder_Accumulation(uint8_t id);
int32_t Emm_V5_Read_Encoder(uint8_t addr);  /* 读取单电机编码器累计值 (多圈, 含回绕处理) */
void X_vel_Control(uint8_t addr, uint8_t dir, uint16_t vel, uint8_t acc, bool snF);
#endif
