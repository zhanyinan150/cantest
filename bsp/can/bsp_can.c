#include "can.h"
#include "bsp_can.h"
#include "main.h"
#include "string.h"
#include "stdlib.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

#define CAN_MX_REGISTER_CNT 16
#define CAN_DEBUG  // 是否启用CAN调试信息

CANInstance *can_instance[CAN_MX_REGISTER_CNT] = {NULL};
volatile uint8_t idx;
osMutexId_t can_tx_mutex;

static void CANAddFilter(CANInstance *_instance)
{
    CAN_FilterTypeDef can_filter_conf = {0};
    static uint8_t can1_filter_idx = 0, can2_filter_idx = 14;

    can_filter_conf.FilterActivation = CAN_FILTER_ENABLE;
    can_filter_conf.SlaveStartFilterBank = 14;

    if (_instance->use_ext_id)
    {
        /* 32-bit mask filter, match ExtId high 8 bits (Emm_V5 address byte = ExtId[15:8])
         * STM32F4 bxCAN 32-bit extended filter register layout:
         *   FilterIdHigh[15:0]  = EXID[28:13]
         *   FilterIdLow[15:3]   = EXID[12:0]
         *   FilterIdLow[2]      = IDE (1=extended)
         *   FilterIdLow[1]      = RTR
         *   FilterIdLow[0]      = 0
         * Emm_V5 CAN ID = (addr << 8) | packet_index, so addr is in EXID[15:8]. */
        uint32_t extid = _instance->rx_id;
        can_filter_conf.FilterMode = CAN_FILTERMODE_IDMASK;
        can_filter_conf.FilterScale = CAN_FILTERSCALE_32BIT;
        can_filter_conf.FilterFIFOAssignment = CAN_RX_FIFO0;
        can_filter_conf.FilterIdHigh     = (uint16_t)(extid >> 13);
        can_filter_conf.FilterIdLow      = (uint16_t)((extid & 0x1FFF) << 3) | 0x0004; /* IDE=1, RTR=0 */
        /* Mask: only check EXID[15:8] (addr byte) + IDE bit */
        uint32_t mask = 0xFF00;
        can_filter_conf.FilterMaskIdHigh = (uint16_t)(mask >> 13);
        can_filter_conf.FilterMaskIdLow  = (uint16_t)((mask & 0x1FFF) << 3) | 0x0004;
    }
    else
    {
        /* 16-bit list mode, match exact StdId (e.g. M2006 feedback 0x201)
         * Each 16-bit slot = STID[10:0]<<5 | IDE(0) | RTR(0) | 0
         * Slot0=FilterIdHigh, Slot1=FilterIdLow, Slot2=FilterMaskIdHigh, Slot3=FilterMaskIdLow */
        uint16_t slot = (uint16_t)(_instance->rx_id << 5);
        can_filter_conf.FilterMode = CAN_FILTERMODE_IDLIST;
        can_filter_conf.FilterScale = CAN_FILTERSCALE_16BIT;
        can_filter_conf.FilterFIFOAssignment = CAN_RX_FIFO0;
        can_filter_conf.FilterIdHigh     = slot;
        can_filter_conf.FilterIdLow      = slot;
        can_filter_conf.FilterMaskIdHigh = 0;
        can_filter_conf.FilterMaskIdLow  = 0;
    }

    if (_instance->can_handle == &hcan1) {
        if (can1_filter_idx >= 14) {
            LOGERROR("[bsp_can] CAN1 filter bank exhausted (max 14)");
            return;
        }
        can_filter_conf.FilterBank = can1_filter_idx++;
    } else {
        if (can2_filter_idx >= 28) {
            LOGERROR("[bsp_can] CAN2 filter bank exhausted (max 28)");
            return;
        }
        can_filter_conf.FilterBank = can2_filter_idx++;
    }

    HAL_CAN_ConfigFilter(_instance->can_handle, &can_filter_conf);
}

static void CANServiceInit()
{
    can_tx_mutex = osMutexNew(NULL);
}

CANInstance *CANRegister(CAN_Init_Config_s *config)
{
    if (!idx)
    {
        CANServiceInit();
    }
    if (idx >= CAN_MX_REGISTER_CNT)
    {
#ifdef CAN_DEBUG
        LOGERROR("[bsp_can] CAN instance exceeded MAX num");
        while (1) {}
#else
        return NULL;
#endif
    }
    for (size_t i = 0; i < idx; i++)
    {
        if (can_instance[i]->rx_id == config->rx_id && can_instance[i]->can_handle == config->can_handle)
        {
#ifdef CAN_DEBUG
            LOGERROR("[bsp_can] CAN id crash, tx [%d] or rx [%d] already registered", config->tx_id, config->rx_id);
            while (1) {}
#else
            return NULL;
#endif
        }
    }

    CANInstance *instance = (CANInstance *)pvPortMalloc(sizeof(CANInstance));
    if (instance == NULL)
    {
        return NULL;
    }
    memset(instance, 0, sizeof(CANInstance));

    instance->txconf.StdId = config->tx_id;
    instance->txconf.ExtId = config->tx_id;
    instance->txconf.IDE = config->use_ext_id ? CAN_ID_EXT : CAN_ID_STD;
    instance->txconf.RTR = CAN_RTR_DATA;
    instance->txconf.DLC = 0x08;
    instance->can_handle = config->can_handle;
    instance->tx_id = config->tx_id;
    instance->rx_id = config->rx_id;
    instance->can_module_callback = config->can_module_callback;
    instance->id = config->id;
    instance->use_ext_id = config->use_ext_id;

    memset(instance->rx_buff, 0, sizeof(instance->rx_buff));
    memset(instance->tx_buff, 0, sizeof(instance->tx_buff));

    instance->rx_event = osEventFlagsNew(NULL);
    instance->rx_counter = 0;

    CANAddFilter(instance);
    can_instance[idx++] = instance;

    return instance;
}

uint8_t CANTransmit(CANInstance *_instance, float timeout)
{
    if (_instance == NULL || _instance->can_handle == NULL)
    {
        return 0;
    }

    uint32_t mb = 0;
    HAL_StatusTypeDef ret;

    /* Acquire TX mutex for thread safety, 超时则放弃本次发送避免无锁竞态 */
    if (can_tx_mutex)
    {
        if (osMutexAcquire(can_tx_mutex, (uint32_t)(timeout)) != osOK)
            return 0;
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(_instance->can_handle) == 0)
    {
        if (can_tx_mutex)
        {
            osMutexRelease(can_tx_mutex);
        }
        return 0;
    }

    ret = HAL_CAN_AddTxMessage(_instance->can_handle, &_instance->txconf, _instance->tx_buff, &mb);

    if (can_tx_mutex)
    {
        osMutexRelease(can_tx_mutex);
    }

    return (ret == HAL_OK) ? 1 : 0;
}

void CANSetDLC(CANInstance *_instance, uint8_t length)
{
    if (length > 8 || length == 0)
    {
#ifdef CAN_DEBUG
        LOGERROR("[bsp_can] CAN DLC error!");
        while (1) {}
#else
        return;
#endif
    }
    _instance->txconf.DLC = length;
}

static void CANFIFOxCallback(CAN_HandleTypeDef *_hcan, uint32_t fifox)
{
    CAN_RxHeaderTypeDef rxconf; /* Stack variable: safe for nested interrupts */
    uint8_t can_rx_buff[8];
    while (HAL_CAN_GetRxFifoFillLevel(_hcan, fifox))
    {
        HAL_CAN_GetRxMessage(_hcan, fifox, &rxconf, can_rx_buff);

        for (size_t i = 0; i < idx; ++i)
        {
            bool match = false;

            if (_hcan == can_instance[i]->can_handle)
            {
                if (rxconf.IDE == CAN_ID_EXT && can_instance[i]->use_ext_id)
                {
                    match = (rxconf.ExtId == can_instance[i]->rx_id);
                }
                else if (rxconf.IDE == CAN_ID_STD && !can_instance[i]->use_ext_id)
                {
                    match = (rxconf.StdId == can_instance[i]->rx_id);
                }
            }
            if (match)
            {
                can_instance[i]->rx_len = rxconf.DLC;
                memcpy(can_instance[i]->rx_buff, can_rx_buff, rxconf.DLC);
                can_instance[i]->rx_counter++;
                if (can_instance[i]->rx_event) {
                    osEventFlagsSet(can_instance[i]->rx_event, 0x01);
                }

                if (can_instance[i]->can_module_callback != NULL)
                {
                    can_instance[i]->can_module_callback(can_instance[i]);
                }
                break; /* Continue processing remaining FIFO frames */
            }
        }
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANFIFOxCallback(hcan, CAN_RX_FIFO0);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANFIFOxCallback(hcan, CAN_RX_FIFO1);
}
