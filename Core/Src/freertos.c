/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "can.h"
#include "usart.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CAN_DEMO_NODE_ID              0U
#define CAN_DEMO_BASE_STD_ID          0x100U
#define CAN_DEMO_MAX_NODE_ID          126U
#define CAN_DEMO_TX_PERIOD_MS         600U
#define CAN_DEMO_POLL_PERIOD_MS       10U
#define CAN_DEMO_START_OFFSET_STEP_MS 10U
#define CAN_DEMO_PAYLOAD_MAGIC        0xA5U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static uint8_t CAN_DemoGetNodeId(void);
static HAL_StatusTypeDef CAN_DemoConfigFilter(void);
static HAL_StatusTypeDef CAN_DemoSend(uint8_t node_id, uint32_t sequence);
static void CAN_DemoDrainRx(void);
static void CAN_DemoPrintData(const uint8_t *data, uint32_t length);
static void CAN_DemoPrintStartup(uint8_t node_id);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  uint8_t node_id = CAN_DemoGetNodeId();
  uint32_t sequence = 0U;
  uint32_t next_tx_tick;
  uint8_t can_ready = 0U;
  uint32_t diag_counter = 0U;

  CAN_DemoPrintStartup(node_id);

  /* ---- Step 1: Loopback test (no external bus needed) ---- */
  printf("[DIAG] === LOOPBACK TEST (internal, no transceiver needed) ===\r\n");
  {
    /* Temporarily re-init CAN in loopback mode */
    hcan1.Init.Mode = CAN_MODE_LOOPBACK;
    HAL_StatusTypeDef lb_init = HAL_CAN_Init(&hcan1);
    printf("[DIAG] Loopback HAL_CAN_Init status=%ld\r\n", (long)lb_init);
    printf("[DIAG] Loopback MCR=0x%08lX MSR=0x%08lX\r\n",
           (unsigned long)CAN1->MCR, (unsigned long)CAN1->MSR);

    if (lb_init == HAL_OK)
    {
      /* Config filter */
      if (CAN_DemoConfigFilter() == HAL_OK)
      {
        printf("[DIAG] Loopback filter OK\r\n");
      }
      else
      {
        printf("[DIAG] Loopback filter FAILED\r\n");
      }

      HAL_StatusTypeDef lb_start = HAL_CAN_Start(&hcan1);
      printf("[DIAG] Loopback HAL_CAN_Start status=%ld state=%lu\r\n",
             (long)lb_start, (unsigned long)HAL_CAN_GetState(&hcan1));
      printf("[DIAG] Loopback MCR=0x%08lX MSR=0x%08lX ESR=0x%08lX\r\n",
             (unsigned long)CAN1->MCR, (unsigned long)CAN1->MSR, (unsigned long)CAN1->ESR);

      if (lb_start == HAL_OK)
      {
        /* Send a test frame in loopback */
        CAN_TxHeaderTypeDef tx_hdr = {0};
        uint32_t tx_mailbox;
        uint8_t tx_data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
        tx_hdr.StdId = 0x123;
        tx_hdr.IDE = CAN_ID_STD;
        tx_hdr.RTR = CAN_RTR_DATA;
        tx_hdr.DLC = 8;

        HAL_StatusTypeDef tx_status = HAL_CAN_AddTxMessage(&hcan1, &tx_hdr, tx_data, &tx_mailbox);
        printf("[DIAG] Loopback TX status=%ld mailbox=%lu\r\n", (long)tx_status, (unsigned long)tx_mailbox);

        /* Wait a bit for loopback RX */
        osDelay(50);

        uint32_t fifo_fill = HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0);
        printf("[DIAG] Loopback RX FIFO0 fill=%lu\r\n", (unsigned long)fifo_fill);

        if (fifo_fill > 0U)
        {
          CAN_RxHeaderTypeDef rx_hdr;
          uint8_t rx_data[8] = {0};
          if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx_hdr, rx_data) == HAL_OK)
          {
            printf("[DIAG] Loopback RX OK! id=0x%03lX dlc=%lu data=",
                   (unsigned long)rx_hdr.StdId, (unsigned long)rx_hdr.DLC);
            CAN_DemoPrintData(rx_data, rx_hdr.DLC);
            printf("\r\n");
            printf("[DIAG] *** LOOPBACK TEST PASSED - CAN controller works! ***\r\n");
          }
          else
          {
            printf("[DIAG] Loopback RX read FAILED\r\n");
          }
        }
        else
        {
          printf("[DIAG] *** LOOPBACK TEST FAILED - no RX frame! ***\r\n");
          printf("[DIAG] This means CAN controller itself is broken or misconfigured\r\n");
        }

        HAL_CAN_Stop(&hcan1);
      }
      else
      {
        printf("[DIAG] *** LOOPBACK START FAILED - CAN controller cannot leave init mode ***\r\n");
        printf("[DIAG] MSR INAK bit = %lu (should be 0 after start)\r\n",
               (unsigned long)((CAN1->MSR & CAN_MSR_INAK) >> 0));
        printf("[DIAG] This is a CRITICAL hardware issue:\r\n");
        printf("[DIAG]   - Check PD0/PD1 are not shorted to ground or VCC\r\n");
        printf("[DIAG]   - Check no other peripheral is claiming PA11/PA12\r\n");
        printf("[DIAG]   - Check AF mapping: PD0=AF9(CAN1_RX), PD1=AF9(CAN1_TX)\r\n");
      }
    }
    else
    {
      printf("[DIAG] *** LOOPBACK INIT FAILED ***\r\n");
    }

    /* Stop loopback, restore normal mode */
    HAL_CAN_Stop(&hcan1);
    hcan1.Init.Mode = CAN_MODE_NORMAL;
  }
  printf("[DIAG] === LOOPBACK TEST END ===\r\n\r\n");

  /* ---- Step 2: Detailed CAN register dump before normal init ---- */
  printf("[DIAG] CAN1 registers BEFORE normal init:\r\n");
  printf("[DIAG]   MCR=0x%08lX MSR=0x%08lX TSR=0x%08lX\r\n",
         (unsigned long)CAN1->MCR,
         (unsigned long)CAN1->MSR,
         (unsigned long)CAN1->TSR);
  printf("[DIAG]   ESR=0x%08lX BTR=0x%08lX IER=0x%08lX\r\n",
         (unsigned long)CAN1->ESR,
         (unsigned long)CAN1->BTR,
         (unsigned long)CAN1->IER);
  printf("[DIAG]   RF0R=0x%08lX RF1R=0x%08lX FMR=0x%08lX\r\n",
         (unsigned long)CAN1->RF0R,
         (unsigned long)CAN1->RF1R,
         (unsigned long)CAN1->FMR);

  /* ---- Baudrate verification ---- */
  {
    uint32_t btr = CAN1->BTR;
    uint32_t prescaler = (btr & CAN_BTR_BRP) >> 0;
    uint32_t ts1 = (btr & CAN_BTR_TS1) >> 16;
    uint32_t ts2 = (btr & CAN_BTR_TS2) >> 20;
    uint32_t sjw = (btr & CAN_BTR_SJW) >> 24;
    uint32_t total_tq = 1U + (ts1 + 1U) + (ts2 + 1U);
    uint32_t baud = 42000000U / ((prescaler + 1U) * total_tq);
    printf("[DIAG] BTR=0x%08lX: Prescaler=%lu TS1=%lu TS2=%lu SJW=%lu total_TQ=%lu\r\n",
           (unsigned long)btr,
           (unsigned long)(prescaler + 1U),
           (unsigned long)(ts1 + 1U),
           (unsigned long)(ts2 + 1U),
           (unsigned long)(sjw + 1U),
           (unsigned long)total_tq);
    printf("[DIAG] Calculated baudrate = %lu bps (target 1Mbps)\r\n", (unsigned long)baud);
  }

  /* ---- Step 3: Re-init CAN in NORMAL mode ---- */
  printf("[CAN INIT] Re-initializing CAN in NORMAL mode...\r\n");
  HAL_StatusTypeDef reinit_status = HAL_CAN_Init(&hcan1);
  printf("[CAN INIT] HAL_CAN_Init status=%ld\r\n", (long)reinit_status);
  printf("[CAN INIT] After init: MCR=0x%08lX MSR=0x%08lX\r\n",
         (unsigned long)CAN1->MCR, (unsigned long)CAN1->MSR);

  /* Check INAK status after init */
  {
    uint32_t inak = (CAN1->MSR & CAN_MSR_INAK);
    printf("[DIAG] MSR INAK=%lu (1=still in init mode, 0=left init mode)\r\n", (unsigned long)inak);
    if (inak != 0U)
    {
      printf("[DIAG] *** WARNING: CAN stuck in init mode! ***\r\n");
      printf("[DIAG] INAK stays 1 when CAN_RX pin does not see recessive level (HIGH)\r\n");
      printf("[DIAG] Possible causes:\r\n");
      printf("[DIAG]   1) CAN transceiver not powered or not connected\r\n");
      printf("[DIAG]   2) CANH/CANL shorted or swapped\r\n");
      printf("[DIAG]   3) No 120ohm termination on bus\r\n");
      printf("[DIAG]   4) PD0/PD1 GPIO conflict with another peripheral\r\n");
    }
  }

  /* ---- Step 4: GPIO pin check ---- */
  printf("[DIAG] GPIO pin levels (PD0=CAN1_RX, PD1=CAN1_TX):\r\n");
  printf("[DIAG]   PA11=%u PA12=%u\r\n",
         (unsigned int)HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0),
         (unsigned int)HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1));
  printf("[DIAG]   (CAN_RX idle should be HIGH=1, LOW=0 means no transceiver or bus stuck dominant)\r\n");

  /* ---- Step 5: Filter config ---- */
  printf("[CAN INIT] attempting filter config...\r\n");
  if (CAN_DemoConfigFilter() != HAL_OK)
  {
    printf("[CAN INIT] filter failed, err=0x%08lX\r\n", (unsigned long)HAL_CAN_GetError(&hcan1));
  }
  else
  {
    printf("[CAN INIT] filter configured OK\r\n");

    /* Dump filter registers after config */
    printf("[DIAG] Filter regs: FMR=0x%08lX FM1R=0x%08lX FS1R=0x%08lX\r\n",
           (unsigned long)CAN1->FMR,
           (unsigned long)CAN1->FM1R,
           (unsigned long)CAN1->FS1R);
    printf("[DIAG] Filter regs: FFA1R=0x%08lX FA1R=0x%08lX\r\n",
           (unsigned long)CAN1->FFA1R,
           (unsigned long)CAN1->FA1R);
    printf("[DIAG] Filter0: FR1=0x%08lX FR2=0x%08lX\r\n",
           (unsigned long)CAN1->sFilterRegister[0].FR1,
           (unsigned long)CAN1->sFilterRegister[0].FR2);

    printf("[CAN INIT] attempting HAL_CAN_Start...\r\n");

    HAL_StatusTypeDef start_status = HAL_CAN_Start(&hcan1);
    if (start_status != HAL_OK)
    {
      uint32_t error_code = HAL_CAN_GetError(&hcan1);
      printf("[CAN INIT] start failed, status=%ld state=%lu err=0x%08lX\r\n",
             (long)start_status,
             (unsigned long)HAL_CAN_GetState(&hcan1),
             (unsigned long)error_code);

      printf("[DIAG] MSR=0x%08lX INAK=%lu SLAK=%lu\r\n",
             (unsigned long)CAN1->MSR,
             (unsigned long)(CAN1->MSR & CAN_MSR_INAK),
             (unsigned long)((CAN1->MSR & CAN_MSR_SLAK) >> 2));

      if (error_code & HAL_CAN_ERROR_ACK)
        printf("[CAN DIAG] ACK error - no other node responding\r\n");
      if (error_code & HAL_CAN_ERROR_BR)
        printf("[CAN DIAG] Bit recessive error\r\n");
      if (error_code & HAL_CAN_ERROR_BD)
        printf("[CAN DIAG] Bit dominant error\r\n");
      if (error_code & HAL_CAN_ERROR_CRC)
        printf("[CAN DIAG] CRC error\r\n");

      printf("[CAN DIAG] Check: 1) Transceiver power 2) CANH/CANL wiring 3) Termination resistors 4) Baudrate match\r\n");
    }
    else
    {
      can_ready = 1U;
      printf("[CAN INIT] started OK, tx_id=0x%03lX\r\n",
             (unsigned long)(CAN_DEMO_BASE_STD_ID + node_id));
    }
  }

  /* ---- Post-start register dump ---- */
  printf("[DIAG] CAN1 registers AFTER start:\r\n");
  printf("[DIAG]   MCR=0x%08lX MSR=0x%08lX TSR=0x%08lX\r\n",
         (unsigned long)CAN1->MCR,
         (unsigned long)CAN1->MSR,
         (unsigned long)CAN1->TSR);
  printf("[DIAG]   ESR=0x%08lX BTR=0x%08lX IER=0x%08lX\r\n",
         (unsigned long)CAN1->ESR,
         (unsigned long)CAN1->BTR,
         (unsigned long)CAN1->IER);
  printf("[DIAG]   RF0R=0x%08lX RF1R=0x%08lX\r\n",
         (unsigned long)CAN1->RF0R,
         (unsigned long)CAN1->RF1R);

  /* Decode ESR */
  {
    uint32_t esr = CAN1->ESR;
    printf("[DIAG] ESR detail: REC=%lu TEC=%lu LEC=%lu\r\n",
           (unsigned long)((esr & CAN_ESR_REC) >> 24),
           (unsigned long)((esr & CAN_ESR_TEC) >> 16),
           (unsigned long)((esr & CAN_ESR_LEC) >> 4));
    if (esr & CAN_ESR_EWGF) printf("[DIAG]   EWGF=1 (Error Warning)\r\n");
    if (esr & CAN_ESR_EPVF) printf("[DIAG]   EPVF=1 (Error Passive)\r\n");
    if (esr & CAN_ESR_BOFF) printf("[DIAG]   BOFF=1 (Bus-Off!)\r\n");
  }

  if (can_ready != 0U)
  {
    uint32_t offset_ms = ((node_id * 7U) % 10U) * 100U;
    printf("[CAN INIT] TX offset: %lu ms\r\n", (unsigned long)offset_ms);
    osDelay(offset_ms);
  }
  else
  {
    printf("[CAN INIT] CAN not ready, retrying in 3 seconds...\r\n");
    osDelay(3000);

    printf("[CAN INIT] retry: attempting HAL_CAN_Start...\r\n");
    HAL_StatusTypeDef retry_status = HAL_CAN_Start(&hcan1);
    if (retry_status == HAL_OK)
    {
      can_ready = 1U;
      printf("[CAN INIT] retry succeeded! tx_id=0x%03lX\r\n",
             (unsigned long)(CAN_DEMO_BASE_STD_ID + node_id));
    }
    else
    {
      printf("[CAN INIT] retry failed, status=%ld state=%lu err=0x%08lX\r\n",
             (long)retry_status,
             (unsigned long)HAL_CAN_GetState(&hcan1),
             (unsigned long)HAL_CAN_GetError(&hcan1));
    }
  }

  next_tx_tick = osKernelGetTickCount();

  for(;;)
  {
    uint32_t now = osKernelGetTickCount();

    if (can_ready != 0U)
    {
      CAN_DemoDrainRx();

      if ((int32_t)(now - next_tx_tick) >= 0)
      {
        HAL_StatusTypeDef status = CAN_DemoSend(node_id, sequence);

        if (status == HAL_OK)
        {
          sequence++;
        }

        next_tx_tick += CAN_DEMO_TX_PERIOD_MS;
      }

      /* Periodic diagnostics every 5 seconds */
      diag_counter++;
      if (diag_counter >= 500U)
      {
        diag_counter = 0U;
        uint32_t esr = CAN1->ESR;
        printf("[DIAG] --- Periodic ---\r\n");
        printf("[DIAG] State=%lu ESR=0x%08lX REC=%lu TEC=%lu LEC=%lu\r\n",
               (unsigned long)HAL_CAN_GetState(&hcan1),
               (unsigned long)esr,
               (unsigned long)((esr & CAN_ESR_REC) >> 24),
               (unsigned long)((esr & CAN_ESR_TEC) >> 16),
               (unsigned long)((esr & CAN_ESR_LEC) >> 4));
        printf("[DIAG] TSR=0x%08lX RF0R=0x%08lX FIFO0_fill=%lu free_tx=%lu\r\n",
               (unsigned long)CAN1->TSR,
               (unsigned long)CAN1->RF0R,
               (unsigned long)HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0),
               (unsigned long)HAL_CAN_GetTxMailboxesFreeLevel(&hcan1));
        printf("[DIAG] PD0(RX)=%u PD1(TX)=%u\r\n",
               (unsigned int)HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0),
               (unsigned int)HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1));
      }
    }

    osDelay(CAN_DEMO_POLL_PERIOD_MS);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
int fputc(int ch, FILE *f)
{
  (void)f;
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1U, HAL_MAX_DELAY);
  return ch;
}

int __io_putchar(int ch)
{
  return fputc(ch, stdout);
}

static uint8_t CAN_DemoGetNodeId(void)
{
  uint32_t mixed_uid;

  if ((CAN_DEMO_NODE_ID >= 1U) && (CAN_DEMO_NODE_ID <= CAN_DEMO_MAX_NODE_ID))
  {
    return (uint8_t)CAN_DEMO_NODE_ID;
  }

  mixed_uid = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
  return (uint8_t)((mixed_uid % CAN_DEMO_MAX_NODE_ID) + 1U);
}

static HAL_StatusTypeDef CAN_DemoConfigFilter(void)
{
  CAN_FilterTypeDef filter_config = {0};

  filter_config.FilterBank = 0;
  filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
  filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
  filter_config.FilterIdHigh = 0x0000;
  filter_config.FilterIdLow = 0x0000;
  filter_config.FilterMaskIdHigh = 0x0000;
  filter_config.FilterMaskIdLow = 0x0000;
  filter_config.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter_config.FilterActivation = CAN_FILTER_ENABLE;
  filter_config.SlaveStartFilterBank = 14;

  printf("[CAN FILTER] mode=32BIT mask=0x0000 (accept all)\r\n");
  return HAL_CAN_ConfigFilter(&hcan1, &filter_config);
}

static HAL_StatusTypeDef CAN_DemoSend(uint8_t node_id, uint32_t sequence)
{
  CAN_TxHeaderTypeDef tx_header = {0};
  uint32_t tx_mailbox;
  uint32_t tick = HAL_GetTick();
  uint8_t tx_data[8];

  tx_data[0] = node_id;
  tx_data[1] = (uint8_t)(sequence & 0xFFU);
  tx_data[2] = (uint8_t)((sequence >> 8) & 0xFFU);
  tx_data[3] = (uint8_t)((sequence >> 16) & 0xFFU);
  tx_data[4] = (uint8_t)(tick & 0xFFU);
  tx_data[5] = (uint8_t)((tick >> 8) & 0xFFU);
  tx_data[6] = (uint8_t)((tick >> 16) & 0xFFU);
  tx_data[7] = CAN_DEMO_PAYLOAD_MAGIC;

  tx_header.StdId = CAN_DEMO_BASE_STD_ID + node_id;
  tx_header.ExtId = 0U;
  tx_header.IDE = CAN_ID_STD;
  tx_header.RTR = CAN_RTR_DATA;
  tx_header.DLC = sizeof(tx_data);
  tx_header.TransmitGlobalTime = DISABLE;

  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U)
  {
    HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
    osDelay(1);
  }

  HAL_StatusTypeDef result = HAL_CAN_AddTxMessage(&hcan1, &tx_header, tx_data, &tx_mailbox);

  if (result == HAL_OK)
  {
    printf("[CAN TX] std_id=0x%03lX rtr=DATA dlc=%lu seq=%lu data=",
           (unsigned long)(CAN_DEMO_BASE_STD_ID + node_id),
           (unsigned long)sizeof(tx_data),
           (unsigned long)sequence);
    CAN_DemoPrintData(tx_data, sizeof(tx_data));
    printf(" free=%lu\r\n",
           (unsigned long)HAL_CAN_GetTxMailboxesFreeLevel(&hcan1));
  }
  else
  {
    printf("[CAN TX] failed, status=%ld state=%lu err=0x%08lX data=",
           (long)result,
           (unsigned long)HAL_CAN_GetState(&hcan1),
           (unsigned long)HAL_CAN_GetError(&hcan1));
    CAN_DemoPrintData(tx_data, sizeof(tx_data));
    printf("\r\n");
  }

  return result;
}

static void CAN_DemoDrainRx(void)
{
  uint32_t frames_read = 0U;

  while ((HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0U) && (frames_read < 16U))
  {
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8] = {0};

    if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK)
    {
      printf("[CAN RX] read failed, err=0x%08lX\r\n", (unsigned long)HAL_CAN_GetError(&hcan1));
      break;
    }

    if (rx_header.IDE == CAN_ID_STD)
    {
      uint8_t sender_node = 0U;
      uint32_t sender_seq = 0U;
      uint32_t sender_tick = 0U;

      if (rx_header.DLC >= 4U)
      {
        sender_node = rx_data[0];
        sender_seq = ((uint32_t)rx_data[3] << 16) | ((uint32_t)rx_data[2] << 8) | rx_data[1];
      }
      if (rx_header.DLC >= 7U)
      {
        sender_tick = ((uint32_t)rx_data[6] << 16) | ((uint32_t)rx_data[5] << 8) | rx_data[4];
      }

      printf("[CAN RX] id=0x%03lX node=%u seq=%lu tick=%lu dlc=%lu data=",
             (unsigned long)rx_header.StdId,
             (unsigned int)sender_node,
             (unsigned long)sender_seq,
             (unsigned long)sender_tick,
             (unsigned long)rx_header.DLC);
      CAN_DemoPrintData(rx_data, rx_header.DLC);
      printf("\r\n");
    }
    else
    {
      printf("[CAN RX] ext_id=0x%08lX dlc=%lu data=",
             (unsigned long)rx_header.ExtId,
             (unsigned long)rx_header.DLC);
      CAN_DemoPrintData(rx_data, rx_header.DLC);
      printf("\r\n");
    }

    frames_read++;
  }

  if (frames_read > 0U)
  {
    printf("[CAN RX] processed %lu frame(s)\r\n", (unsigned long)frames_read);
  }
}

static void CAN_DemoPrintData(const uint8_t *data, uint32_t length)
{
  uint32_t i;

  if (length > 8U)
  {
    length = 8U;
  }

  for (i = 0U; i < length; i++)
  {
    printf("%02X", data[i]);
    if ((i + 1U) < length)
    {
      printf(" ");
    }
  }
}

static void CAN_DemoPrintStartup(uint8_t node_id)
{
  printf("\r\n[BOOT] cantest CAN demo\r\n");
  printf("[BOOT] UID=%08lX-%08lX-%08lX node=%u tx_id=0x%03lX\r\n",
         (unsigned long)HAL_GetUIDw2(),
         (unsigned long)HAL_GetUIDw1(),
         (unsigned long)HAL_GetUIDw0(),
         (unsigned int)node_id,
         (unsigned long)(CAN_DEMO_BASE_STD_ID + node_id));
  printf("[BOOT] USART1=115200  CAN1=1Mbps  TX_PERIOD=%lums\r\n",
         (unsigned long)CAN_DEMO_TX_PERIOD_MS);
}

/* USER CODE END Application */

