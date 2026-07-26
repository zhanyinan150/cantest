# cantest — XYZ 起重机控制系统

基于 STM32F407 + FreeRTOS 的 XYZ 三轴起重机,搭配 K230 视觉识别。步进闭环采用张大头 Emm_V5 驱动器,升降采用大疆 M2006。

## 硬件平台

| 子系统 | 器件 | 通讯 | 地址/ID |
|--------|------|------|---------|
| X 轴(左右) | Emm_V5 步进闭环 | UART5 | ID=3 |
| Y 轴(前后) | Emm_V5 步进闭环 ×2 同步 | CAN2 扩展帧 | ID=1, 2 |
| Z 轴(升降) | 大疆 M2006 (C610 电调) | CAN1 标准帧 | 0x201 |
| 舵机 | LobotServoController | UART | - |
| 视觉 | K230 | USART3 (PB10/PB11) | UART3 (pin50/51) |
| 日志 | - | USART1 DMA (PA9) | - |

- 主控:STM32F407VG,168 MHz,FreeRTOS + Keil MDK-ARM
- 供电:步进 12/24V,M2006 12V,主控 3.3V

## 目录结构

```
app/              应用层 (App_Init 系统初始化编排)
bsp/              板级支持
├─ can/           CAN 驱动注册表 (bsp_can)
├─ cmd/           VOFA 命令注册表 (CMD_Register/Dispatch)
├─ log/           日志系统 (队列 + LogTask DMA)
├─ printf/        printf 重定向 (fputc -> 日志队列)
├─ uart/          UART 回调分发
├─ vofa/          VOFA+ JustFloat 波形
├─ telemetry/     遥测通道注册
└─ dwt/           DWT 计时
modules/          功能模块
├─ motor/         XYZ 三轴联动 (Motor_XYZ)
├─ lift/          Z 轴升降 (M2006 位置闭环)
├─ duoji/         舵机控制 (LobotServoController)
├─ ZDT_EmmV5/     Emm_V5 步进驱动 (UART 版 + CAN 版)
├─ dji_motor/     大疆电机驱动 (M2006/C610)
├─ motor_test/    电机测试任务
├─ motor_uart_test/ UART 电机测试
├─ servo/         舵机
├─ chassis/       底盘 (停用,地址与 Y 轴冲突)
├─ PID/  ADRC/    控制算法
├─ action/  k230/ 占位骨架
└─ ZDT_X42/       X42 驱动 (预留)
view/             K230 Python 视觉脚本 (按键拍照/切换摄像头)
Core/             STM32 HAL
Drivers/          HAL / CMSIS
Middlewares/      FreeRTOS / USB Device
USB_DEVICE/       USB CDC
MDK-ARM/          Keil 工程
硬件开发/          硬件资料
```

## 构建

Keil MDK-ARM 命令行编译:

```bash
cd MDK-ARM
E:/UV4/UV4.exe -j0 -b cantest.uvprojx -o build_log.txt
```

退出码:`0`=无警告,`1`=有警告(非错误),`2`=有错误。需读 `build_log.txt` 里的 `"cantest.axf" - N Error(s), M Warning(s)` 确认 Error 数为 0。

## Emm_V5 电机配置(首次上电菜单设置)

| 菜单 | 值 | 说明 |
|------|-----|------|
| P_Pul | PUL_FOC | FOC 矢量闭环 |
| P_Serial | CAN1_MAP / UART_FUN | CAN2 电机用 CAN1_MAP,UART5 电机用 UART_FUN |
| En | Hold | 一直使能,由软件 F3 命令控制 |
| MStep | 16 | 细分(决定位置模式每转脉冲 = 200×16 = 3200) |
| ID_Addr | 1 / 2 / 3 | 电机地址 |
| CAN_Baud | 1000000 | 1 Mbps(须与主控 CAN2 一致) |
| UartBaud | 115200 | UART5 电机 |
| Checksum | 0x6B | 固定校验字节 |
| Response | Receive | 只返回确认收到 |
| Cal | - | 首次上电空载校准(否则位置闭环误差 ±0.75°~1.5°) |

## 串口命令(USB CDC / VOFA)

| 命令 | 格式 | 说明 |
|------|------|------|
| `mxyz` | `mxyz xdir xvel xacc xdist ydir yvel yacc ydist zdir zvel zacc zdist` | XYZ 三轴联动 |

> ⚠️ 命令通道当前**未接通**:`CMD_Dispatch` 无消费者(CommandTask 不存在),
> `mxyz` 与 lift 的 10 条命令均注册了但无法触发。需要时补一个消费
> `UART_Callback_GetCmdQueue()` 的任务即可激活。
>
> ⚠️ `Motor_XYZ` 当前**非阻塞**:到位轮询代码在 `motor.c` 里被整段注释,
> 发完命令即返回,不检测堵转/超时。详见审核文档 U-3。

- 方向:`0`=CW,`1`=CCW(实际运动方向按电机安装)
- 速度:0-3000 RPM,加速度:0-255 档(0=直接启动)
- 距离:cm,某轴不动作传 0
- 示例:`mxyz 1 300 180 10 1 300 180 10 0 0 0 5` → X 右 10cm + Y 前 10cm + Z 升 5cm

## Emm_V5 协议要点

| 项目 | 说明 |
|------|------|
| 速度模式 F6 | 0-3000 RPM (`0x0BB8`),>3000 返回 E2 |
| 位置模式 FD | clk 单位 = 细分脉冲,16 细分下 **3200 脉冲/圈**(非编码器值 65536) |
| 加速度 | 0-255 档,0=直接启动;公式 `t=(256-acc)*50us` 每 1 RPM |
| CAN 帧 | 扩展帧,ExtId = `(addr<<8) | packet`,>8 字节分包,packet 从 0 递增 |
| 状态标志 0x3A | `&0x01`使能 `&0x02`到位 `&0x04`堵转 `&0x08`堵转保护 |
| 到位返回 | Response 设 Reached/Both,到位主动返回 `addr+FD+9F+6B` |
| 多机同步 | snF=1 缓存命令,广播 `FF 66 6B` 同步启动 |

> ⚠️ 位置命令 FD 的脉冲数是**细分脉冲**(3200/圈),不是编码器反馈值(65536/圈)。读取编码器 `0x31`/`0x33` 返回 0-65535 才是编码器单位,两者勿混。

## 日志系统

USART1 DMA 队列驱动,避免多任务并发抢 USART1 致 HardFault:

```
任意任务 printf/LOG -> fputc 行缓冲 -> osMessageQueue -> LogTask (DMA 发 USART1) -> TxCplt 信号量
```

- `BSPLogInit()` 初始化(创建队列 + LogTask)
- `LOGINFO` / `LOGWARNING` / `LOGERROR` 宏(非阻塞,队列满则丢)
- `printf` 也走队列(fputc 行缓冲入队)
- 输出到 PA9(USART1),115200 或 USB CDC

> ⚠️ **不要在任何地方直接 `HAL_UART_Transmit(&huart1, ...)`** —— 会与 LogTask 的 DMA
> 抢同一个 USART1。日志/调试输出一律走 `printf` 或 `LOG*` 宏。
>
> USART1/USART3/UART5 的 NVIC 全局中断必须使能(`usart.c` MspInit + `stm32f4xx_it.c`)。
> 缺了它 `HAL_UART_Transmit_DMA` 的 TC 中断进不去,`gState` 会永久卡在 BUSY_TX,
> 日志发完第一行就彻底停摆。用 CubeMX 重新生成后请核对这三个中断仍然勾着。

## K230 视觉

`view/` 目录 Python 脚本(K230 CanMV), 通讯走 UART3 (pin50/51) <-> STM32 USART3 (PB10/PB11), 115200 8N1, 8字节定长帧 + XOR 校验。

STM32 侧收帧用 `HAL_UARTEx_ReceiveToIdle_DMA` 按 IDLE 空闲切帧(不是定长 DMA),
丢字节后靠帧间空隙自动重新对齐; K230 侧用累积缓冲 + 滑窗重同步。两侧都能自愈错帧。

### 通讯协议 (STM32 主机 / K230 从机)

| 阶段 | STM32 | K230 |
|------|-------|------|
| P1 豆子 | 发 LOOK_BEAN(0x02) -> 等ACK(0x0A) -> 等豆子数据 -> 发ACK(0x06) | 等命令 -> ACK -> 识别(cam2) -> 发豆子帧(0x02) -> 等ACK |
| P2 正面数字 | 发 LOOK_NUMBER(0x03) -> 等ACK(0x0A) -> 等识别完成ACK(0x0A) -> 发ACK(0x06) | 等命令 -> ACK -> 识别(cam1) -> 发ACK(0x0A,不发数据) -> 等ACK |
| P3 侧面数字 | 发 LOOK_SIDE(0x01) -> 等ACK(0x0A) -> 等完整5数字 -> 发ACK(0x06) | 等命令 -> ACK -> 识别(cam0) -> 推理第5数字 -> 发完整帧(0x01) -> 等ACK |

### 脚本列表

- `k230_competition.py` - 比赛主程序: 真实摄像头+YOLO+三阶段协议+防抖+推理第5数字
- `k230_competition_mock.py` - 通讯协议测试(假数据,无摄像头)
- `k230_comm_test.py` - ACK握手通讯测试(旧协议,K230主动发)
- `k230_10s_switch.py` - YOLO 检测 + 10s 自动切摄像头 + GPIO53 按键
- `k230_1photo_switch.py` - 单摄像头按键拍照 + 切换
- `k230_photo_capture.py` - 批量拍照(数据集采集)
- `k230_uart_test.py` - 纯UART收发测试

按键硬件:GPIO53 下拉 + 高电平有效(按下=1),`PULL_DOWN`。

## 注意事项

> 完整的代码审核结果、已修复项与待确认项见 [代码审核与修复记录.md](代码审核与修复记录.md)。

0. **当前启用状态**:测试阶段 `app_init.c` 里 `Lift_Init` / `Emm_V5_CAN_Init` / `Motor_Init`
   均被 `#if 0` 停用(没接电机会崩),实际只跑 K230 通讯(`K230_Init` + `Action_Init`)。
   恢复电机前请先读审核文档「未修复」一节,尤其是轮径标定和 Y 轴方向。
1. **地址冲突**:Y 轴步进 ID=1,2 与底盘左右轮冲突,二者不可同时启用。
2. **位置模式运动模式**:Emm42_V5.0 只有 `00`(相对上次目标)/`01`(绝对),无 `02`(相对当前)。距离移动用 `00`,两次位置命令间不可插速度模式,否则目标基准漂移。
3. **升降轮径**:`LIFT_WHEEL_DIAMETER_M=0.032`(3.2cm),按实物量取后改 `mech_params.h`。
4. **细分变更**:改电机细分时同步改 `mech_params.h` 的 `*_MICROSTEP`,每转脉冲自动 = 200×细分。
