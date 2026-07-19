/**
  ******************************************************************************
  * @file    laser.c
  * @brief   激光 ToF 测距传感器模块 (双传感器, IDLE 帧同步 + DMA)
  ******************************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "laser.h"
#include "usart.h"
#include "stdio.h"
#include "string.h"

/* 每侧接收缓冲: DMA 写入此缓冲, IDLE 时解析 */
/* 用双缓冲思想: DMA 用 rx_buf 收, 解析用 rx_buf(IDLE 时 DMA 已停)。
 * 简单起见用单缓冲 + IDLE 时拷贝解析, 解析后重启 DMA。 */
static uint8_t s_rx_buf[LASER_SIDE_COUNT][LASER_FRAME_LEN];

/* 每侧帧数据 (解析结果) */
static LaserFrame s_frames[LASER_SIDE_COUNT];

/* 每侧关联的 UART 句柄 */
static UART_HandleTypeDef *const s_huart[LASER_SIDE_COUNT] = {
    &huart2,   /* FRONT */
    &huart3,   /* BACK  */
};

/* 阈值监测配置 */
static struct {
    LaserThresholdCb cb;
    uint16_t thresh_mm;
    bool armed;        /* 边沿触发: 上次是否已触发(防止持续触发) */
} s_thresh[LASER_SIDE_COUNT];

static osThreadId_t s_monitor_task = NULL;

/* ---- 内部辅助 ---- */
static LaserSide side_from_huart(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) return LASER_SIDE_FRONT;
    if (huart->Instance == USART3) return LASER_SIDE_BACK;
    return LASER_SIDE_COUNT;
}

/* 解析单帧到指定侧的 LaserFrame。帧完整性已由调用方校验。
 * 在 USART IDLE 中断上下文执行(非任务), 故只做纯内存操作, 不含 printf/阻塞。
 * 每个测量点 15 字节, 布局: distance(2) noise(2) peak(4) confidence(1) intg(4) reftof(2),
 * 全部小端(低字节在前), 与上位机 Python 解析脚本逐字节对应。 */
static void parse_frame(LaserSide side, const uint8_t *frame)
{
    LaserFrame *f = &s_frames[side];
    for (uint8_t p = 0; p < LASER_POINT_COUNT; p++) {
        /* 第 p 个测量点在帧中的起始偏移 = 头部10字节 + p×15 */
        uint16_t i = LASER_FRAME_HEAD + p * LASER_POINT_LEN;
        /* 小端解析: 低字节 | (高字节 << 8), 多字节字段同理逐字节拼 */
        f->points[p].distance_mm = (uint16_t)(frame[i] | (frame[i + 1] << 8));
        f->points[p].noise       = (uint16_t)(frame[i + 2] | (frame[i + 3] << 8));
        f->points[p].peak        = (uint32_t)frame[i + 4] | ((uint32_t)frame[i + 5] << 8) |
                                   ((uint32_t)frame[i + 6] << 16) | ((uint32_t)frame[i + 7] << 24);
        f->points[p].confidence  = frame[i + 8];
        f->points[p].intg        = (uint32_t)frame[i + 9] | ((uint32_t)frame[i + 10] << 8) |
                                   ((uint32_t)frame[i + 11] << 16) | ((uint32_t)frame[i + 12] << 24);
        f->points[p].reftof      = (uint16_t)(frame[i + 13] | (frame[i + 14] << 8));
    }
    f->frame_counter++;
    f->last_tick = HAL_GetTick();
    f->valid = true;
}

/* ---- IDLE 帧同步入口 ----
 * IDLE 中断表示一帧结束。此时 DMA 已把数据写入 s_rx_buf, 但 DMA 可能未触发 RxCplt
 * (Normal 模式收满才触发)。这里用 IDLE 作为帧边界, 检查收到的字节数。
 * 实现: 用 __HAL_UART_GET_FLAG(IDLE) 在 USART IRQHandler 里调本函数,
 *       停 DMA → 读 NDTR 算实际长度 → 校验 → 解析 → 重启 DMA。 */
void Laser_OnIdle(UART_HandleTypeDef *huart)
{
    LaserSide side = side_from_huart(huart);
    if (side >= LASER_SIDE_COUNT) return;

    /* 清 IDLE 标志 (读 SR 再读 DR, 或用 HAL 宏) */
    __HAL_UART_CLEAR_IDLEFLAG(huart);

    /* 停止当前 DMA。NDTR 是 DMA 剩余未传输计数, "申请长度 - 剩余" = 实际收到字节数 */
    HAL_UART_DMAStop(huart);
    uint16_t rx_len = LASER_FRAME_LEN - __HAL_DMA_GET_COUNTER(huart->hdmarx);

    /* 帧校验: 长度==195 且首字节==0xAA。不满足则丢弃(帧同步会自动恢复:
     * 残帧/错位帧被跳过, 下一帧的 0xAA 起始符会重新对齐)。 */
    if (rx_len == LASER_FRAME_LEN && s_rx_buf[side][0] == LASER_START_BYTE) {
        parse_frame(side, s_rx_buf[side]);
    } else {
        s_frames[side].bad_frame_counter++;
    }

    /* 重启 DMA 接收下一帧 */
    HAL_UART_Receive_DMA(huart, s_rx_buf[side], LASER_FRAME_LEN);
}

/* ---- 查询 API ---- */
const LaserFrame *Laser_GetFrame(LaserSide side)
{
    if (side >= LASER_SIDE_COUNT) return NULL;
    return &s_frames[side];
}

uint16_t Laser_GetDistance(LaserSide side, uint8_t idx)
{
    if (side >= LASER_SIDE_COUNT || idx >= LASER_POINT_COUNT) return 0xFFFF;
    if (!s_frames[side].valid) return 0xFFFF;
    return s_frames[side].points[idx].distance_mm;
}

uint16_t Laser_GetNearestDistance(LaserSide side)
{
    if (side >= LASER_SIDE_COUNT || !s_frames[side].valid) return 0xFFFF;
    uint16_t nearest = 0xFFFF;
    for (uint8_t i = 0; i < LASER_POINT_COUNT; i++) {
        uint16_t d = s_frames[side].points[i].distance_mm;
        /* distance==0 表示该点无效(无回波/超量程), 排除避免误判为"最近" */
        if (d != 0 && d < nearest) nearest = d;
    }
    return nearest;
}

uint16_t Laser_GetAvgDistance(LaserSide side)
{
    if (side >= LASER_SIDE_COUNT || !s_frames[side].valid) return 0xFFFF;
    uint32_t sum = 0, cnt = 0;
    for (uint8_t i = 0; i < LASER_POINT_COUNT; i++) {
        uint16_t d = s_frames[side].points[i].distance_mm;
        if (d != 0) { sum += d; cnt++; }  /* 同样排除无效点(0) */
    }
    if (cnt == 0) return 0xFFFF;
    return (uint16_t)(sum / cnt);
}

/* ---- 阈值监测任务 ----
 * 周期检查各侧最近距离, 越过阈值边沿触发回调。供 mission 接入。 */
static void LaserMonitorTask(void *argument)
{
    (void)argument;
    osDelay(3000);  /* 等激光传感器上电稳定出帧 */
    TickType_t xLast = xTaskGetTickCount();
    for (;;) {
        for (LaserSide s = 0; s < LASER_SIDE_COUNT; s++) {
            if (s_thresh[s].cb == NULL || !s_frames[s].valid) continue;
            uint16_t d = Laser_GetNearestDistance(s);
            if (d != 0xFFFF && d < s_thresh[s].thresh_mm) {
                if (!s_thresh[s].armed) {
                    s_thresh[s].armed = true;
                    s_thresh[s].cb(s, d);
                }
            } else {
                /* 距离恢复到阈值外 → 重新装订, 允许下次再次触发 */
                s_thresh[s].armed = false;
            }
        }
        vTaskDelayUntil(&xLast, pdMS_TO_TICKS(50));  /* 50ms 监测周期 */
    }
}

void Laser_SetThresholdCallback(LaserThresholdCb cb, LaserSide side, uint16_t thresh_mm)
{
    if (side >= LASER_SIDE_COUNT) return;
    s_thresh[side].cb = cb;
    s_thresh[side].thresh_mm = thresh_mm;
    s_thresh[side].armed = false;
}

/* ---- 初始化 ---- */
int Laser_Init(void)
{
    memset(s_frames, 0, sizeof(s_frames));
    memset(s_thresh, 0, sizeof(s_thresh));

    /* 波特率(230400)已在 MX_USART2/3_UART_Init(CubeMX) 配好, 此处仅使能 NVIC + IDLE + DMA。
     * 使能 USART2/3 NVIC 中断 (HAL_UART_Init 不使能 NVIC, 需手动; IDLE 帧同步依赖) */
    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);

    /* 使能 IDLE 中断 + 启动 DMA 接收 */
    for (LaserSide s = 0; s < LASER_SIDE_COUNT; s++) {
        UART_HandleTypeDef *h = s_huart[s];
        __HAL_UART_ENABLE_IT(h, UART_IT_IDLE);
        if (HAL_UART_Receive_DMA(h, s_rx_buf[s], LASER_FRAME_LEN) != HAL_OK) {
            printf("[laser] 侧%d DMA接收启动失败\r\n", (int)s);
            return -1;
        }
    }

    /* 创建阈值监测任务 */
    const osThreadAttr_t attr = {
        .name = "LaserMon",
        .stack_size = 384 * 4,
        .priority = osPriorityBelowNormal,
    };
    s_monitor_task = osThreadNew(LaserMonitorTask, NULL, &attr);
    if (s_monitor_task == NULL) {
        printf("[laser] 监测任务创建失败\r\n");
        return -1;
    }

    printf("[laser] 初始化完成 (USART2前+USART3后, 230400bps, IDLE帧同步)\r\n");
    return 0;
}
