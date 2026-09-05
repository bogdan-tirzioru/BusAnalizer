/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint32_t reasons;
  uint32_t expected_counter;
  uint32_t received_counter;
  uint32_t tx_queued;
  uint32_t rx_count;
  uint32_t identifier;
  uint32_t dlc;
  uint32_t bad_byte_index;
  uint8_t bad_byte_expected;
  uint8_t bad_byte_received;
  uint8_t data[8];
} GeneratorCanBadSnapshot;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define GENERATOR_CAN_BITRATE_500K  500000UL
#define GENERATOR_CAN_BITRATE_1M   1000000UL
#define GENERATOR_CAN_DBITRATE_2M  2000000UL
#define GENERATOR_CAN_DBITRATE_5M  5000000UL

#define GENERATOR_BAD_SEQUENCE  (1U << 0)
#define GENERATOR_BAD_FUTURE    (1U << 1)
#define GENERATOR_BAD_ID        (1U << 2)
#define GENERATOR_BAD_DLC       (1U << 3)
#define GENERATOR_BAD_HEADER    (1U << 4)
#define GENERATOR_BAD_ESI       (1U << 5)
#define GENERATOR_BAD_PAYLOAD   (1U << 6)

#define GENERATOR_PROTOCOL_IR_MASK  (FDCAN_IR_PEA | FDCAN_IR_PED)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

FDCAN_HandleTypeDef hfdcan1;
FDCAN_HandleTypeDef hfdcan2;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */
static uint32_t can_tx_count = 0;
static uint32_t can_rx_count = 0;
static uint32_t can_tx_error = 0;

static uint32_t last_tx_tick = 0;
static uint32_t last_report_tick = 0;

static FDCAN_TxHeaderTypeDef txHeader = {0};
static uint8_t generator_can_fd = 1U;
static uint32_t generator_nominal_bitrate = GENERATOR_CAN_BITRATE_1M;
static uint32_t generator_data_bitrate = GENERATOR_CAN_DBITRATE_5M;
static uint32_t generator_payload_length = 64U;

/*
 * Independent validation of the traffic seen by generator FDCAN2.
 *
 * FDCAN1 always transmits standard ID 0x100 with a monotonically increasing
 * little-endian counter in data[0..3].  The rest of the active payload uses
 * the deterministic pattern (counter + byte index) & 0xFF.
 *
 * Runtime configuration can switch both generator controllers between:
 *   Classic CAN : 500 kbit/s or 1 Mbit/s, 8-byte payload
 *   CAN FD+BRS  : 500 kbit/s or 1 Mbit/s nominal,
 *                 2 Mbit/s or 5 Mbit/s data, 64-byte payload
 *
 * No printf is performed in the RX hot path.  Only counters and the most
 * recent mismatch are retained and printed in the once-per-second report.
 */
static uint8_t can_rx_have_counter = 0U;
static uint32_t can_rx_expected_counter = 0U;
static uint32_t can_rx_first_counter = 0U;
static uint32_t can_rx_last_counter = 0U;

static uint32_t can_rx_good_frames = 0U;
static uint32_t can_rx_bad_frames = 0U;
static uint32_t can_rx_sequence_errors = 0U;
static uint32_t can_rx_missing_frames = 0U;
static uint32_t can_rx_duplicate_events = 0U;
static uint32_t can_rx_backward_events = 0U;
static uint32_t can_rx_future_events = 0U;
static uint32_t can_rx_id_errors = 0U;
static uint32_t can_rx_dlc_errors = 0U;
static uint32_t can_rx_header_errors = 0U;
static uint32_t can_rx_esi_errors = 0U;
static uint32_t can_rx_payload_errors = 0U;
static uint32_t can_rx_payload_byte_errors = 0U;
static uint32_t can_rx_read_errors = 0U;
static uint32_t can_rx_fifo_lost_events = 0U;
static uint32_t can_rx_max_fifo_fill = 0U;

static GeneratorCanBadSnapshot can_rx_first_bad = {0};
static GeneratorCanBadSnapshot can_rx_last_bad = {0};

static uint32_t can_rx_protocol_events = 0U;
static uint32_t can_rx_protocol_lec_events = 0U;
static uint32_t can_rx_protocol_dlec_events = 0U;
static uint32_t can_rx_protocol_ir_events = 0U;
static uint32_t can_rx_protocol_last_psr = 0U;
static uint32_t can_rx_protocol_last_ecr = 0U;
static uint32_t can_rx_protocol_last_ir = 0U;
static uint32_t can_rx_protocol_last_tx = 0U;
static uint32_t can_rx_protocol_last_rx = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_FDCAN2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void Generator_CAN_ConfigureTxHeader(void);
static void Generator_CAN_ResetStatistics(void);
static uint8_t Generator_CAN_CounterIsAfter(uint32_t value,
                                            uint32_t reference);
static void Generator_CAN_SaveBadSnapshot(
    GeneratorCanBadSnapshot *snapshot,
    uint32_t reasons,
    uint32_t expected_counter,
    uint32_t received_counter,
    uint32_t tx_queued,
    const FDCAN_RxHeaderTypeDef *header,
    const uint8_t *data,
    uint32_t bad_byte_index,
    uint8_t bad_byte_expected,
    uint8_t bad_byte_received);
static void Generator_CAN_MonitorRxProtocol(void);
static HAL_StatusTypeDef Generator_CAN_ConfigureHandle(
    FDCAN_HandleTypeDef *hfdcan,
    uint8_t can_fd,
    uint32_t nominal_bitrate,
    uint32_t data_bitrate);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Generator_CAN_SendText(const char *text)
{
  uint16_t length = 0U;

  if (text == NULL)
  {
    return;
  }

  while ((text[length] != '\0') && (length < 383U))
  {
    length++;
  }

  (void)Console_UART_Transmit(&huart1,
                              (const uint8_t *)text,
                              length,
                              0U);
}

static void Generator_CAN_ConfigureTxHeader(void)
{
  txHeader.Identifier = 0x100U;
  txHeader.IdType = FDCAN_STANDARD_ID;
  txHeader.TxFrameType = FDCAN_DATA_FRAME;
  txHeader.DataLength = (generator_can_fd != 0U) ?
                        FDCAN_DLC_BYTES_64 : FDCAN_DLC_BYTES_8;
  txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  txHeader.BitRateSwitch = (generator_can_fd != 0U) ?
                           FDCAN_BRS_ON : FDCAN_BRS_OFF;
  txHeader.FDFormat = (generator_can_fd != 0U) ?
                      FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
  txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  txHeader.MessageMarker = 0U;
}

static uint8_t Generator_CAN_CounterIsAfter(uint32_t value,
                                            uint32_t reference)
{
  uint32_t distance = value - reference;

  return ((distance != 0U) && (distance < 0x80000000U)) ? 1U : 0U;
}

static void Generator_CAN_SaveBadSnapshot(
    GeneratorCanBadSnapshot *snapshot,
    uint32_t reasons,
    uint32_t expected_counter,
    uint32_t received_counter,
    uint32_t tx_queued,
    const FDCAN_RxHeaderTypeDef *header,
    const uint8_t *data,
    uint32_t bad_byte_index,
    uint8_t bad_byte_expected,
    uint8_t bad_byte_received)
{
  uint32_t i;

  snapshot->reasons = reasons;
  snapshot->expected_counter = expected_counter;
  snapshot->received_counter = received_counter;
  snapshot->tx_queued = tx_queued;
  snapshot->rx_count = can_rx_count;
  snapshot->identifier = header->Identifier;
  snapshot->dlc = header->DataLength;
  snapshot->bad_byte_index = bad_byte_index;
  snapshot->bad_byte_expected = bad_byte_expected;
  snapshot->bad_byte_received = bad_byte_received;

  for (i = 0U; i < 8U; i++)
  {
    snapshot->data[i] = data[i];
  }
}

static void Generator_CAN_MonitorRxProtocol(void)
{
  uint32_t psr = hfdcan2.Instance->PSR;
  uint32_t ecr = hfdcan2.Instance->ECR;
  uint32_t ir = hfdcan2.Instance->IR & GENERATOR_PROTOCOL_IR_MASK;
  uint32_t lec = (psr & FDCAN_PSR_LEC) >> FDCAN_PSR_LEC_Pos;
  uint32_t dlec = (psr & FDCAN_PSR_DLEC) >> FDCAN_PSR_DLEC_Pos;
  uint8_t lec_error = 0U;
  uint8_t dlec_error = 0U;

  /* Reading PSR consumes the LEC/DLEC change latches.  Poll them in the hot
   * loop so short receive-side protocol errors are retained in our counters. */
  if ((lec != FDCAN_PROTOCOL_ERROR_NONE) &&
      (lec != FDCAN_PROTOCOL_ERROR_NO_CHANGE))
  {
    lec_error = 1U;
  }

  if ((dlec != FDCAN_PROTOCOL_ERROR_NONE) &&
      (dlec != FDCAN_PROTOCOL_ERROR_NO_CHANGE))
  {
    dlec_error = 1U;
  }

  if ((lec_error != 0U) || (dlec_error != 0U) || (ir != 0U))
  {
    can_rx_protocol_events++;
    can_rx_protocol_lec_events += lec_error;
    can_rx_protocol_dlec_events += dlec_error;
    can_rx_protocol_ir_events += (ir != 0U) ? 1U : 0U;
    can_rx_protocol_last_psr = psr;
    can_rx_protocol_last_ecr = ecr;
    can_rx_protocol_last_ir = ir;
    can_rx_protocol_last_tx = can_tx_count;
    can_rx_protocol_last_rx = can_rx_count;

    /* PEA/PED are sticky. Clear only the sampled flags so a later protocol
     * episode is counted independently. */
    if (ir != 0U)
    {
      hfdcan2.Instance->IR = ir;
    }
  }
}

static void Generator_CAN_ResetStatistics(void)
{
  can_tx_count = 0U;
  can_rx_count = 0U;
  can_tx_error = 0U;

  can_rx_have_counter = 0U;
  can_rx_expected_counter = 0U;
  can_rx_first_counter = 0U;
  can_rx_last_counter = 0U;

  can_rx_good_frames = 0U;
  can_rx_bad_frames = 0U;
  can_rx_sequence_errors = 0U;
  can_rx_missing_frames = 0U;
  can_rx_duplicate_events = 0U;
  can_rx_backward_events = 0U;
  can_rx_future_events = 0U;
  can_rx_id_errors = 0U;
  can_rx_dlc_errors = 0U;
  can_rx_header_errors = 0U;
  can_rx_esi_errors = 0U;
  can_rx_payload_errors = 0U;
  can_rx_payload_byte_errors = 0U;
  can_rx_read_errors = 0U;
  can_rx_fifo_lost_events = 0U;
  can_rx_max_fifo_fill = 0U;

  can_rx_first_bad = (GeneratorCanBadSnapshot){0};
  can_rx_last_bad = (GeneratorCanBadSnapshot){0};
  can_rx_first_bad.bad_byte_index = 0xFFFFFFFFU;
  can_rx_last_bad.bad_byte_index = 0xFFFFFFFFU;

  can_rx_protocol_events = 0U;
  can_rx_protocol_lec_events = 0U;
  can_rx_protocol_dlec_events = 0U;
  can_rx_protocol_ir_events = 0U;
  can_rx_protocol_last_psr = 0U;
  can_rx_protocol_last_ecr = 0U;
  can_rx_protocol_last_ir = 0U;
  can_rx_protocol_last_tx = 0U;
  can_rx_protocol_last_rx = 0U;

  /* Establish a clean receive-side protocol baseline before frame 0. */
  (void)hfdcan2.Instance->PSR;
  hfdcan2.Instance->IR = GENERATOR_PROTOCOL_IR_MASK;

  last_tx_tick = HAL_GetTick();
  last_report_tick = HAL_GetTick();
}

static HAL_StatusTypeDef Generator_CAN_ConfigureHandle(
    FDCAN_HandleTypeDef *hfdcan,
    uint8_t can_fd,
    uint32_t nominal_bitrate,
    uint32_t data_bitrate)
{
  uint32_t nominal_prescaler;
  uint32_t data_prescaler = 1U;
  uint32_t data_sjw = 3U;
  uint32_t data_seg1 = 12U;
  uint32_t data_seg2 = 3U;

  if (nominal_bitrate == GENERATOR_CAN_BITRATE_500K)
  {
    nominal_prescaler = 10U;
  }
  else if (nominal_bitrate == GENERATOR_CAN_BITRATE_1M)
  {
    nominal_prescaler = 5U;
  }
  else
  {
    return HAL_ERROR;
  }

  if (can_fd != 0U)
  {
    if (data_bitrate == GENERATOR_CAN_DBITRATE_2M)
    {
      /* 80 MHz / (2 * (1 + 15 + 4)) = 2 Mbit/s, SP=80%. */
      data_prescaler = 2U;
      data_sjw = 4U;
      data_seg1 = 15U;
      data_seg2 = 4U;
    }
    else if (data_bitrate == GENERATOR_CAN_DBITRATE_5M)
    {
      /* 80 MHz / (1 * (1 + 12 + 3)) = 5 Mbit/s, SP=81.25%. */
      data_prescaler = 1U;
      data_sjw = 3U;
      data_seg1 = 12U;
      data_seg2 = 3U;
    }
    else
    {
      return HAL_ERROR;
    }
  }

  hfdcan->Init.FrameFormat = (can_fd != 0U) ?
                             FDCAN_FRAME_FD_BRS : FDCAN_FRAME_CLASSIC;
  hfdcan->Init.NominalPrescaler = nominal_prescaler;
  hfdcan->Init.NominalSyncJumpWidth = 2U;
  hfdcan->Init.NominalTimeSeg1 = 13U;
  hfdcan->Init.NominalTimeSeg2 = 2U;
  hfdcan->Init.DataPrescaler = data_prescaler;
  hfdcan->Init.DataSyncJumpWidth = data_sjw;
  hfdcan->Init.DataTimeSeg1 = data_seg1;
  hfdcan->Init.DataTimeSeg2 = data_seg2;

  return HAL_FDCAN_Init(hfdcan);
}

void Generator_CAN_PrintStatus(void)
{
  char msg[256];
  int length;

  if (generator_can_fd != 0U)
  {
    length = snprintf(
        msg,
        sizeof(msg),
        "CAN STATUS mode=FD+BRS nominal=%lu data=%lu payload=64 "
        "N[brp=%lu sjw=%lu seg1=%lu seg2=%lu] "
        "D[brp=%lu sjw=%lu seg1=%lu seg2=%lu]\r\n",
        (unsigned long)generator_nominal_bitrate,
        (unsigned long)generator_data_bitrate,
        (unsigned long)hfdcan1.Init.NominalPrescaler,
        (unsigned long)hfdcan1.Init.NominalSyncJumpWidth,
        (unsigned long)hfdcan1.Init.NominalTimeSeg1,
        (unsigned long)hfdcan1.Init.NominalTimeSeg2,
        (unsigned long)hfdcan1.Init.DataPrescaler,
        (unsigned long)hfdcan1.Init.DataSyncJumpWidth,
        (unsigned long)hfdcan1.Init.DataTimeSeg1,
        (unsigned long)hfdcan1.Init.DataTimeSeg2);
  }
  else
  {
    length = snprintf(
        msg,
        sizeof(msg),
        "CAN STATUS mode=CLASSIC nominal=%lu payload=8 "
        "N[brp=%lu sjw=%lu seg1=%lu seg2=%lu]\r\n",
        (unsigned long)generator_nominal_bitrate,
        (unsigned long)hfdcan1.Init.NominalPrescaler,
        (unsigned long)hfdcan1.Init.NominalSyncJumpWidth,
        (unsigned long)hfdcan1.Init.NominalTimeSeg1,
        (unsigned long)hfdcan1.Init.NominalTimeSeg2);
  }

  if (length > 0)
  {
    (void)Console_UART_Transmit(&huart1,
                                (const uint8_t *)msg,
                                (uint16_t)length,
                                0U);
  }
}

HAL_StatusTypeDef Generator_CAN_ApplyConfig(
    uint8_t can_fd,
    uint32_t nominal_bitrate,
    uint32_t data_bitrate)
{
  HAL_StatusTypeDef status;

  if ((nominal_bitrate != GENERATOR_CAN_BITRATE_500K) &&
      (nominal_bitrate != GENERATOR_CAN_BITRATE_1M))
  {
    return HAL_ERROR;
  }

  if ((can_fd != 0U) &&
      (data_bitrate != GENERATOR_CAN_DBITRATE_2M) &&
      (data_bitrate != GENERATOR_CAN_DBITRATE_5M))
  {
    return HAL_ERROR;
  }

  /* Stop the transmitter first so CAN2 is still available to ACK it. */
  if (HAL_FDCAN_Stop(&hfdcan1) != HAL_OK)
  {
    Generator_CAN_SendText("CAN config failed: cannot stop FDCAN1\r\n");
    return HAL_ERROR;
  }

  if (HAL_FDCAN_Stop(&hfdcan2) != HAL_OK)
  {
    Generator_CAN_SendText("CAN config failed: cannot stop FDCAN2\r\n");
    return HAL_ERROR;
  }

  status = Generator_CAN_ConfigureHandle(&hfdcan1,
                                         can_fd,
                                         nominal_bitrate,
                                         data_bitrate);
  if (status != HAL_OK)
  {
    Generator_CAN_SendText("CAN config failed: FDCAN1 init\r\n");
    return status;
  }

  status = Generator_CAN_ConfigureHandle(&hfdcan2,
                                         can_fd,
                                         nominal_bitrate,
                                         data_bitrate);
  if (status != HAL_OK)
  {
    Generator_CAN_SendText("CAN config failed: FDCAN2 init\r\n");
    return status;
  }

  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan2,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK)
  {
    Generator_CAN_SendText("CAN config failed: FDCAN2 filter\r\n");
    return HAL_ERROR;
  }

  /* Receiver/ACK node always starts before the traffic generator. */
  if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK)
  {
    Generator_CAN_SendText("CAN config failed: cannot start FDCAN2\r\n");
    return HAL_ERROR;
  }

  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    (void)HAL_FDCAN_Stop(&hfdcan2);
    Generator_CAN_SendText("CAN config failed: cannot start FDCAN1\r\n");
    return HAL_ERROR;
  }

  generator_can_fd = (can_fd != 0U) ? 1U : 0U;
  generator_nominal_bitrate = nominal_bitrate;
  generator_data_bitrate = (can_fd != 0U) ? data_bitrate : 0U;
  generator_payload_length = (can_fd != 0U) ? 64U : 8U;

  Generator_CAN_ConfigureTxHeader();
  Generator_CAN_ResetStatistics();
  Generator_CAN_PrintStatus();

  return HAL_OK;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_FDCAN1_Init();
  MX_FDCAN2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  const char msg[] =
      "\r\n"
      "================================\r\n"
      " CAN / CAN FD traffic generator\r\n"
      "================================\r\n"
      "Boot OK\r\n"
      "CPU          : 400 MHz\r\n"
      "Default      : CAN FD+BRS 1 Mbit/s / 5 Mbit/s\r\n"
      "Pattern      : ID=100 counter + deterministic pattern\r\n"
      "CAN2         : independent RX validator / ACK node enabled\r\n"
      "Type ? for runtime commands\r\n"
      "\r\n";

  HAL_UART_Transmit(&huart1,
                    (uint8_t *)msg,
                    sizeof(msg) - 1,
                    HAL_MAX_DELAY);

  Generator_CAN_ConfigureTxHeader();

  /* CAN2 accepts all non-matching standard/extended frames into FIFO0 */
  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan2,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK)
  {
      Error_Handler();
  }

  /*
   * Start receiver first.
   * CAN2 must be active before CAN1 starts transmitting,
   * because CAN2 provides the ACK.
   */
  if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK)
  {
      Error_Handler();
  }

  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
      Error_Handler();
  }

  Generator_CAN_ResetStatistics();
  Generator_CAN_PrintStatus();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

      uint32_t now = HAL_GetTick();

      while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) > 0)
      {
          uint8_t txData[64];

          txData[0] = (uint8_t)(can_tx_count);
          txData[1] = (uint8_t)(can_tx_count >> 8);
          txData[2] = (uint8_t)(can_tx_count >> 16);
          txData[3] = (uint8_t)(can_tx_count >> 24);

          for (uint32_t i = 4U; i < 64U; i++)
          {
              txData[i] = (uint8_t)(can_tx_count + i);
          }

          if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1,
                                            &txHeader,
                                            txData) == HAL_OK)
          {
              can_tx_count++;
          }
          else
          {
              can_tx_error++;
              break;
          }
      }

      /*
       * Observe FIFO pressure before draining it.  This allows a sequence gap
       * at CAN2 to be distinguished from a generator-side FIFO overflow.
       */
      uint32_t rx_fill =
          HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO0);

      if (rx_fill > can_rx_max_fifo_fill)
      {
          can_rx_max_fifo_fill = rx_fill;
      }

      if (__HAL_FDCAN_GET_FLAG(&hfdcan2,
                               FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST))
      {
          can_rx_fifo_lost_events++;
          __HAL_FDCAN_CLEAR_FLAG(&hfdcan2,
                                 FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST);
      }

      /* Drain and validate everything received by CAN2. */
      while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2,
                                          FDCAN_RX_FIFO0) > 0)
      {
          FDCAN_RxHeaderTypeDef rxHeader;
          uint8_t rxData[64] = {0U};

          if (HAL_FDCAN_GetRxMessage(&hfdcan2,
                                     FDCAN_RX_FIFO0,
                                     &rxHeader,
                                     rxData) == HAL_OK)
          {
              uint32_t counter =
                    ((uint32_t)rxData[0])
                  | ((uint32_t)rxData[1] << 8)
                  | ((uint32_t)rxData[2] << 16)
                  | ((uint32_t)rxData[3] << 24);

              uint32_t expected_before = can_rx_expected_counter;
              uint32_t tx_queued_before = can_tx_count;
              uint32_t tx_distance = tx_queued_before - counter;
              uint32_t reasons = 0U;
              uint32_t bad_byte_index = 0xFFFFFFFFU;
              uint8_t bad_byte_expected = 0U;
              uint8_t bad_byte_received = 0U;
              uint32_t expected_dlc = (generator_can_fd != 0U) ?
                                      FDCAN_DLC_BYTES_64 : FDCAN_DLC_BYTES_8;
              uint32_t expected_format = (generator_can_fd != 0U) ?
                                         FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
              uint32_t expected_brs = (generator_can_fd != 0U) ?
                                      FDCAN_BRS_ON : FDCAN_BRS_OFF;

              can_rx_count++;

              if (can_rx_have_counter == 0U)
              {
                  can_rx_have_counter = 1U;
                  can_rx_first_counter = counter;
              }

              /* A received counter must refer to a frame already accepted by
               * the TX queue.  Modular subtraction keeps this valid at wrap. */
              if ((tx_distance == 0U) || (tx_distance >= 0x80000000U))
              {
                  can_rx_future_events++;
                  reasons |= GENERATOR_BAD_FUTURE;
              }

              if ((counter == expected_before) &&
                  ((reasons & GENERATOR_BAD_FUTURE) == 0U))
              {
                  can_rx_expected_counter++;
              }
              else if (counter != expected_before)
              {
                  can_rx_sequence_errors++;
                  reasons |= GENERATOR_BAD_SEQUENCE;

                  if ((reasons & GENERATOR_BAD_FUTURE) != 0U)
                  {
                      /* An impossible counter must not poison synchronisation. */
                  }
                  else if ((can_rx_count > 1U) &&
                      (counter == can_rx_last_counter))
                  {
                      can_rx_duplicate_events++;
                      /* A duplicate does not advance the expected counter. */
                  }
                  else if (Generator_CAN_CounterIsAfter(
                               counter, expected_before) != 0U)
                  {
                      can_rx_missing_frames += counter - expected_before;
                      can_rx_expected_counter = counter + 1U;
                  }
                  else
                  {
                      can_rx_backward_events++;
                      /* Keep the expected counter so a late/stale frame does
                       * not hide a subsequent valid frame. */
                  }
              }

              can_rx_last_counter = counter;

              if ((rxHeader.Identifier != 0x100U) ||
                  (rxHeader.IdType != FDCAN_STANDARD_ID))
              {
                  can_rx_id_errors++;
                  reasons |= GENERATOR_BAD_ID;
              }

              if (rxHeader.DataLength != expected_dlc)
              {
                  can_rx_dlc_errors++;
                  reasons |= GENERATOR_BAD_DLC;
              }

              if ((rxHeader.RxFrameType != FDCAN_DATA_FRAME) ||
                  (rxHeader.FDFormat != expected_format) ||
                  (rxHeader.BitRateSwitch != expected_brs))
              {
                  can_rx_header_errors++;
                  reasons |= GENERATOR_BAD_HEADER;
              }

              if (rxHeader.ErrorStateIndicator != FDCAN_ESI_ACTIVE)
              {
                  can_rx_esi_errors++;
                  reasons |= GENERATOR_BAD_ESI;
              }

              for (uint32_t i = 4U; i < generator_payload_length; i++)
              {
                  uint8_t expected_byte = (uint8_t)(counter + i);

                  if (rxData[i] != expected_byte)
                  {
                      if ((reasons & GENERATOR_BAD_PAYLOAD) == 0U)
                      {
                          can_rx_payload_errors++;
                          bad_byte_index = i;
                          bad_byte_expected = expected_byte;
                          bad_byte_received = rxData[i];
                      }

                      can_rx_payload_byte_errors++;
                      reasons |= GENERATOR_BAD_PAYLOAD;
                  }
              }

              if (reasons == 0U)
              {
                  can_rx_good_frames++;
              }
              else
              {
                  can_rx_bad_frames++;

                  if (can_rx_bad_frames == 1U)
                  {
                      Generator_CAN_SaveBadSnapshot(
                          &can_rx_first_bad,
                          reasons,
                          expected_before,
                          counter,
                          tx_queued_before,
                          &rxHeader,
                          rxData,
                          bad_byte_index,
                          bad_byte_expected,
                          bad_byte_received);
                  }

                  Generator_CAN_SaveBadSnapshot(
                      &can_rx_last_bad,
                      reasons,
                      expected_before,
                      counter,
                      tx_queued_before,
                      &rxHeader,
                      rxData,
                      bad_byte_index,
                      bad_byte_expected,
                      bad_byte_received);
              }
          }
          else
          {
              can_rx_read_errors++;
          }
      }

      Generator_CAN_MonitorRxProtocol();

      /* Print status once per second. */
      if ((now - last_report_tick) >= 1000)
      {
          last_report_tick = now;

          FDCAN_ProtocolStatusTypeDef ps1 = {0};
          FDCAN_ProtocolStatusTypeDef ps2 = {0};

          FDCAN_ErrorCountersTypeDef ec1 = {0};
          FDCAN_ErrorCountersTypeDef ec2 = {0};

          HAL_FDCAN_GetProtocolStatus(&hfdcan1, &ps1);
          HAL_FDCAN_GetProtocolStatus(&hfdcan2, &ps2);

          HAL_FDCAN_GetErrorCounters(&hfdcan1, &ec1);
          HAL_FDCAN_GetErrorCounters(&hfdcan2, &ec2);

          uint32_t txFree =
              HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1);

          uint32_t rxFill =
              HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO0);

          char status_msg[320];

          int status_len = snprintf(
              status_msg,
              sizeof(status_msg),
              "TXQ=%lu RX=%lu ADDERR=%lu free=%lu rxFill=%lu "
              "| C1 TEC=%lu REC=%lu EP=%lu BO=%lu LEC=%lu "
              "| C2 TEC=%lu REC=%lu EP=%lu BO=%lu LEC=%lu\r\n",
              (unsigned long)can_tx_count,
              (unsigned long)can_rx_count,
              (unsigned long)can_tx_error,
              (unsigned long)txFree,
              (unsigned long)rxFill,

              (unsigned long)ec1.TxErrorCnt,
              (unsigned long)ec1.RxErrorCnt,
              (unsigned long)ps1.ErrorPassive,
              (unsigned long)ps1.BusOff,
              (unsigned long)ps1.LastErrorCode,

              (unsigned long)ec2.TxErrorCnt,
              (unsigned long)ec2.RxErrorCnt,
              (unsigned long)ps2.ErrorPassive,
              (unsigned long)ps2.BusOff,
              (unsigned long)ps2.LastErrorCode);

          HAL_UART_Transmit(&huart1,
                            (uint8_t *)status_msg,
                            status_len,
                            HAL_MAX_DELAY);

          char check_msg[384];
          uint32_t crosscheck_errors =
              can_rx_bad_frames +
              can_rx_read_errors +
              can_rx_fifo_lost_events +
              can_rx_protocol_events;
          const char *crosscheck_state;

          if (crosscheck_errors != 0U)
          {
              crosscheck_state = "FAIL";
          }
          else if ((can_tx_count == can_rx_count) &&
                   (can_rx_expected_counter == can_tx_count))
          {
              crosscheck_state = "PASS";
          }
          else
          {
              crosscheck_state = "RUN";
          }

          int check_len = snprintf(
              check_msg,
              sizeof(check_msg),
              "GENX %s tx=%lu rx=%lu d=%lu next=%lu ok=%lu bad=%lu\r\n"
              "SEQ first=%lu last=%lu err=%lu miss=%lu dup=%lu back=%lu fut=%lu\r\n"
              "RX id=%lu dlc=%lu hdr=%lu esi=%lu pf=%lu pb=%lu "
              "read=%lu lost=%lu max=%lu\r\n",
              crosscheck_state,
              (unsigned long)can_tx_count,
              (unsigned long)can_rx_count,
              (unsigned long)(can_tx_count - can_rx_count),
              (unsigned long)can_rx_expected_counter,
              (unsigned long)can_rx_good_frames,
              (unsigned long)can_rx_bad_frames,
              (unsigned long)can_rx_first_counter,
              (unsigned long)can_rx_last_counter,
              (unsigned long)can_rx_sequence_errors,
              (unsigned long)can_rx_missing_frames,
              (unsigned long)can_rx_duplicate_events,
              (unsigned long)can_rx_backward_events,
              (unsigned long)can_rx_future_events,
              (unsigned long)can_rx_id_errors,
              (unsigned long)can_rx_dlc_errors,
              (unsigned long)can_rx_header_errors,
              (unsigned long)can_rx_esi_errors,
              (unsigned long)can_rx_payload_errors,
              (unsigned long)can_rx_payload_byte_errors,
              (unsigned long)can_rx_read_errors,
              (unsigned long)can_rx_fifo_lost_events,
              (unsigned long)can_rx_max_fifo_fill);

          HAL_UART_Transmit(&huart1,
                            (uint8_t *)check_msg,
                            check_len,
                            HAL_MAX_DELAY);

          if ((can_rx_bad_frames != 0U) ||
              (can_rx_protocol_events != 0U))
          {
              char detail_msg[384];

              int detail_len = snprintf(
                  detail_msg,
                  sizeof(detail_msg),
                  "FIRST w=%02lX e=%lu g=%lu tq=%lu r=%lu "
                  "id=%03lX dlc=%lu b=%ld:%02X/%02X\r\n"
                  "LAST w=%02lX e=%lu g=%lu tq=%lu r=%lu "
                  "id=%03lX dlc=%lu b=%ld:%02X/%02X\r\n"
                  "PROTO ev=%lu l=%lu dl=%lu irn=%lu "
                  "p=%08lX e=%08lX i=%08lX t=%lu r=%lu\r\n",
                  (unsigned long)can_rx_first_bad.reasons,
                  (unsigned long)can_rx_first_bad.expected_counter,
                  (unsigned long)can_rx_first_bad.received_counter,
                  (unsigned long)can_rx_first_bad.tx_queued,
                  (unsigned long)can_rx_first_bad.rx_count,
                  (unsigned long)can_rx_first_bad.identifier,
                  (unsigned long)can_rx_first_bad.dlc,
                  (long)(int32_t)can_rx_first_bad.bad_byte_index,
                  can_rx_first_bad.bad_byte_expected,
                  can_rx_first_bad.bad_byte_received,
                  (unsigned long)can_rx_last_bad.reasons,
                  (unsigned long)can_rx_last_bad.expected_counter,
                  (unsigned long)can_rx_last_bad.received_counter,
                  (unsigned long)can_rx_last_bad.tx_queued,
                  (unsigned long)can_rx_last_bad.rx_count,
                  (unsigned long)can_rx_last_bad.identifier,
                  (unsigned long)can_rx_last_bad.dlc,
                  (long)(int32_t)can_rx_last_bad.bad_byte_index,
                  can_rx_last_bad.bad_byte_expected,
                  can_rx_last_bad.bad_byte_received,
                  (unsigned long)can_rx_protocol_events,
                  (unsigned long)can_rx_protocol_lec_events,
                  (unsigned long)can_rx_protocol_dlec_events,
                  (unsigned long)can_rx_protocol_ir_events,
                  (unsigned long)can_rx_protocol_last_psr,
                  (unsigned long)can_rx_protocol_last_ecr,
                  (unsigned long)can_rx_protocol_last_ir,
                  (unsigned long)can_rx_protocol_last_tx,
                  (unsigned long)can_rx_protocol_last_rx);

              HAL_UART_Transmit(&huart1,
                                (uint8_t *)detail_msg,
                                detail_len,
                                HAL_MAX_DELAY);
          }
      }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
  PeriphClkInitStruct.PLL2.PLL2M = 1;
  PeriphClkInitStruct.PLL2.PLL2N = 20;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = ENABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 5;
  hfdcan1.Init.NominalSyncJumpWidth = 2;
  hfdcan1.Init.NominalTimeSeg1 = 13;
  hfdcan1.Init.NominalTimeSeg2 = 2;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 3;
  hfdcan1.Init.DataTimeSeg1 = 12;
  hfdcan1.Init.DataTimeSeg2 = 3;
  hfdcan1.Init.MessageRAMOffset = 0;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr = 0;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxFifo1ElmtsNbr = 0;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr = 0;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 32;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_64;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief FDCAN2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN2_Init(void)
{

  /* USER CODE BEGIN FDCAN2_Init 0 */

  /* USER CODE END FDCAN2_Init 0 */

  /* USER CODE BEGIN FDCAN2_Init 1 */

  /* USER CODE END FDCAN2_Init 1 */
  hfdcan2.Instance = FDCAN2;
  hfdcan2.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan2.Init.AutoRetransmission = DISABLE;
  hfdcan2.Init.TransmitPause = DISABLE;
  hfdcan2.Init.ProtocolException = DISABLE;
  hfdcan2.Init.NominalPrescaler = 5;
  hfdcan2.Init.NominalSyncJumpWidth = 2;
  hfdcan2.Init.NominalTimeSeg1 = 13;
  hfdcan2.Init.NominalTimeSeg2 = 2;
  hfdcan2.Init.DataPrescaler = 1;
  hfdcan2.Init.DataSyncJumpWidth = 3;
  hfdcan2.Init.DataTimeSeg1 = 12;
  hfdcan2.Init.DataTimeSeg2 = 3;
  hfdcan2.Init.MessageRAMOffset = 576;
  hfdcan2.Init.StdFiltersNbr = 0;
  hfdcan2.Init.ExtFiltersNbr = 0;
  hfdcan2.Init.RxFifo0ElmtsNbr = 32;
  hfdcan2.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_64;
  hfdcan2.Init.RxFifo1ElmtsNbr = 0;
  hfdcan2.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_64;
  hfdcan2.Init.RxBuffersNbr = 0;
  hfdcan2.Init.RxBufferSize = FDCAN_DATA_BYTES_64;
  hfdcan2.Init.TxEventsNbr = 0;
  hfdcan2.Init.TxBuffersNbr = 0;
  hfdcan2.Init.TxFifoQueueElmtsNbr = 0;
  hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan2.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&hfdcan2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN2_Init 2 */

  /* USER CODE END FDCAN2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, CAN1_STBM_Pin|CAN2_STBM_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : CAN1_STBM_Pin CAN2_STBM_Pin */
  GPIO_InitStruct.Pin = CAN1_STBM_Pin|CAN2_STBM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
