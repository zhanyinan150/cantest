/**
  ******************************************************************************
  * @file    laser.h
  * @brief   激光 ToF 测距传感器模块 (双传感器: 前=USART2, 后=USART3)
  ******************************************************************************
  * @attention
  * 接收两路激光 ToF 传感器的二进制数据帧, IDLE 中断帧同步 + DMA 接收:
  *   - 帧格式: [0xAA][9字节头][12测量点×15字节][5字节尾] = 195 字节
  *   - 测量点(15字节, 小端): distance(2,mm) noise(2) peak(4) confidence(1) intg(4) reftof(2)
  *   - IDLE 中断检测帧边界 → 校验 0xAA + 长度 → 解析 12 点距离 → 存最新帧
  *
  * 双传感器: LASER_SIDE_FRONT(USART2, PA2/PA3) + LASER_SIDE_BACK(USART3, PB10/PB11)
  *   ⚠️ USART3 原属 servo.c(休眠死代码), 改 230400 后 servo 失效(本就未接线)。
  *
  * 数据用途: 提供查询 API + 阈值触发回调, 供 mission 编排接入。
  ******************************************************************************
  */
#ifndef __LASER_H
#define __LASER_H

#include "main.h"
#include "cmsis_os2.h"
#include "stdbool.h"

/* 激光传感器侧 (前/后) */
typedef enum {
    LASER_SIDE_FRONT = 0,   /* USART2 */
    LASER_SIDE_BACK,        /* USART3 */
    LASER_SIDE_COUNT,
} LaserSide;

/* 协议常量 */
#define LASER_FRAME_LEN        195u   /* 帧总长 */
#define LASER_FRAME_HEAD       10u    /* 头部长度(含0xAA) */
#define LASER_POINT_LEN        15u    /* 每测量点字节数 */
#define LASER_POINT_COUNT      12u    /* 每帧测量点数 */
#define LASER_START_BYTE       0xAAu

/* 单个测量点解析结果 */
typedef struct {
    uint16_t distance_mm;   /* 距离(mm) */
    uint16_t noise;         /* 环境噪声 */
    uint32_t peak;          /* 接收强度 */
    uint8_t  confidence;    /* 置信度 */
    uint32_t intg;          /* 积分次数 */
    uint16_t reftof;        /* 温度表征值 */
} LaserPoint;

/* 单个传感器的最新帧数据 */
typedef struct {
    LaserPoint points[LASER_POINT_COUNT];  /* 12 个测量点 */
    uint32_t   frame_counter;              /* 累计接收帧数(诊断) */
    uint32_t   bad_frame_counter;          /* 校验失败帧数(诊断) */
    uint32_t   last_tick;                  /* 最近有效帧时间(HAL_GetTick) */
    bool       valid;                      /* 是否已收到至少一帧有效数据 */
} LaserFrame;

/**
 * @brief 激光模块初始化: 启动两路 DMA 接收 + 注册 IDLE 回调
 * @note  需在 MX_USART2/3_UART_Init 之后调用。会改 USART2/3 波特率为 230400。
 *        ⚠️ 调用后 servo.c(USART3) 失效, 因波特率被改。
 * @retval 0 成功, -1 失败
 */
int Laser_Init(void);

/**
 * @brief 获取指定侧激光的最新帧(只读指针, 不要长时间持有)
 * @note  返回内部帧指针, 调用者应快速拷贝所需字段。ISR 可能并发更新。
 */
const LaserFrame *Laser_GetFrame(LaserSide side);

/**
 * @brief 获取指定侧第 idx 个测量点的距离(mm)
 * @retval 距离(mm), 无效帧返回 0xFFFF
 */
uint16_t Laser_GetDistance(LaserSide side, uint8_t idx);

/**
 * @brief 获取指定侧最近测量点的距离(mm) (12点中最小值, 排除0)
 * @note  "最近"= 最小非零距离, 常用于避障判断。
 * @retval 最近距离(mm), 无有效点返回 0xFFFF
 */
uint16_t Laser_GetNearestDistance(LaserSide side);

/**
 * @brief 获取指定侧平均距离(mm) (排除0和异常点)
 * @retval 平均距离(mm), 无有效点返回 0xFFFF
 */
uint16_t Laser_GetAvgDistance(LaserSide side);

/**
 * @brief 距离阈值触发回调类型: 当某侧距离越过阈值时调用
 * @param side 触发的传感器侧
 * @param distance 当前距离(mm)
 * @note  在解析任务上下文调用(非ISR), 可做较重操作。由 Laser_SetThresholdCallback 注册。
 */
typedef void (*LaserThresholdCb)(LaserSide side, uint16_t distance);

/**
 * @brief 注册距离阈值触发回调 + 设置阈值
 * @param cb       回调(为NULL则禁用阈值监测)
 * @param side     监测的传感器侧
 * @param thresh_mm 阈值(mm): 距离 < 阈值 且上次未触发 → 触发回调(边沿触发)
 * @note  供 mission 编排接入: 如前激光<100mm 触发停止/下降。
 *        监测在 LaserMonitorTask(周期)中执行, 非ISR。
 */
void Laser_SetThresholdCallback(LaserThresholdCb cb, LaserSide side, uint16_t thresh_mm);

/**
 * @brief IDLE 中断帧同步入口 (由 USART2/3 的 HAL_UART_RxCpltCallback 或 IDLE 处理调用)
 * @note  内部判断帧完整性, 不阻塞。供 bsp/uart 回调分发调用。
 */
void Laser_OnIdle(UART_HandleTypeDef *huart);

#endif /* __LASER_H */
