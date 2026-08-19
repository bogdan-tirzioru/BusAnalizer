#include "can_sniffer.h"

#include "can_capture_buffer.h"
#include "main.h"
#include "stm32h7xx_hal_fdcan.h"

#include <string.h>

#define RX_ELEMENT_STDID_MASK 0x1FFC0000U
#define RX_ELEMENT_EXTID_MASK 0x1FFFFFFFU
#define RX_ELEMENT_RTR_MASK   0x20000000U
#define RX_ELEMENT_XTD_MASK   0x40000000U
#define RX_ELEMENT_ESI_MASK   0x80000000U
#define RX_ELEMENT_TS_MASK    0x0000FFFFU
#define RX_ELEMENT_DLC_MASK   0x000F0000U
#define RX_ELEMENT_BRS_MASK   0x00100000U
#define RX_ELEMENT_FDF_MASK   0x00200000U

extern FDCAN_HandleTypeDef hfdcan1;

static volatile bool capture_running;
static uint32_t rx_frames;
static uint32_t read_errors;
static uint32_t fifo_lost_events;
static uint32_t max_fifo_fill;

static uint8_t CAN_DlcToLength(uint8_t dlc)
{
  static const uint8_t lengths[16] =
  {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
    8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U
  };
  return lengths[dlc & 0x0FU];
}

static bool CAN_ReadFifo0Direct(CAN_SnifferFrame *frame)
{
  uint32_t fifo_status;
  uint32_t get_index;
  uint32_t *element;
  uint32_t word0;
  uint32_t word1;
  uint8_t length;
  uint32_t i;

  fifo_status = hfdcan1.Instance->RXF0S;
  if ((fifo_status & FDCAN_RXF0S_F0FL) == 0U)
  {
    return false;
  }

  get_index = (fifo_status & FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;
  element = (uint32_t *)(hfdcan1.msgRam.RxFIFO0SA +
                         (get_index * hfdcan1.Init.RxFifo0ElmtSize * 4U));
  word0 = element[0];
  word1 = element[1];

  if ((word0 & RX_ELEMENT_XTD_MASK) != 0U)
  {
    frame->id = word0 & RX_ELEMENT_EXTID_MASK;
    frame->flags = CAN_FRAME_FLAG_EXTENDED;
  }
  else
  {
    frame->id = (word0 & RX_ELEMENT_STDID_MASK) >> 18;
    frame->flags = 0U;
  }

  if ((word0 & RX_ELEMENT_RTR_MASK) != 0U)
  {
    frame->flags |= CAN_FRAME_FLAG_RTR;
  }
  if ((word0 & RX_ELEMENT_ESI_MASK) != 0U)
  {
    frame->flags |= CAN_FRAME_FLAG_ESI;
  }
  if ((word1 & RX_ELEMENT_FDF_MASK) != 0U)
  {
    frame->flags |= CAN_FRAME_FLAG_FD;
  }
  if ((word1 & RX_ELEMENT_BRS_MASK) != 0U)
  {
    frame->flags |= CAN_FRAME_FLAG_BRS;
  }

  frame->timestamp = (uint16_t)(word1 & RX_ELEMENT_TS_MASK);
  frame->dlc = (uint8_t)((word1 & RX_ELEMENT_DLC_MASK) >> 16);
  memset(frame->data, 0, sizeof(frame->data));

  length = CAN_DlcToLength(frame->dlc);
  if (length > sizeof(frame->data))
  {
    length = sizeof(frame->data);
  }

  for (i = 0U; i < length; i++)
  {
    frame->data[i] = ((uint8_t *)&element[2])[i];
  }

  hfdcan1.Instance->RXF0A = get_index;
  return true;
}

void CAN_Sniffer_Init(void)
{
  FDCAN_FilterTypeDef filter;

  CAN_CaptureBuffer_Init();
  rx_frames = 0U;
  read_errors = 0U;
  fifo_lost_events = 0U;
  max_fifo_fill = 0U;
  capture_running = false;

  memset(&filter, 0, sizeof(filter));
  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0U;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = 0U;
  filter.FilterID2 = 0U;

  if ((HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK) ||
      (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                    FDCAN_ACCEPT_IN_RX_FIFO0,
                                    FDCAN_ACCEPT_IN_RX_FIFO0,
                                    FDCAN_FILTER_REMOTE,
                                    FDCAN_FILTER_REMOTE) != HAL_OK) ||
      (HAL_FDCAN_ConfigTimestampCounter(&hfdcan1, FDCAN_TIMESTAMP_PRESC_8) != HAL_OK) ||
      (HAL_FDCAN_EnableTimestampCounter(&hfdcan1, FDCAN_TIMESTAMP_INTERNAL) != HAL_OK) ||
      (HAL_FDCAN_Start(&hfdcan1) != HAL_OK))
  {
    Error_Handler();
  }

  capture_running = true;
}

void CAN_Sniffer_Process(void)
{
  CAN_SnifferFrame frame;
  uint32_t fifo_status;
  uint32_t fill;

  fifo_status = hfdcan1.Instance->RXF0S;
  fill = fifo_status & FDCAN_RXF0S_F0FL;
  if (fill > max_fifo_fill)
  {
    max_fifo_fill = fill;
  }
  if ((fifo_status & FDCAN_RXF0S_RF0L) != 0U)
  {
    fifo_lost_events++;
    hfdcan1.Instance->IR = FDCAN_IR_RF0L;
  }

  while ((hfdcan1.Instance->RXF0S & FDCAN_RXF0S_F0FL) != 0U)
  {
    if (!CAN_ReadFifo0Direct(&frame))
    {
      read_errors++;
      break;
    }

    rx_frames++;
    if (capture_running)
    {
      (void)CAN_CaptureBuffer_Push(&frame);
    }
  }
}

void CAN_Sniffer_Start(void)
{
  capture_running = true;
}

void CAN_Sniffer_Stop(void)
{
  capture_running = false;
}

void CAN_Sniffer_Clear(void)
{
  CAN_CaptureBuffer_Clear();
}

bool CAN_Sniffer_IsRunning(void)
{
  return capture_running;
}

uint32_t CAN_Sniffer_GetRxCount(void)
{
  return rx_frames;
}

uint32_t CAN_Sniffer_GetErrorCount(void)
{
  return read_errors;
}

uint32_t CAN_Sniffer_GetBufferedCount(void)
{
  return CAN_CaptureBuffer_GetCount();
}

uint32_t CAN_Sniffer_GetDroppedCount(void)
{
  return CAN_CaptureBuffer_GetDropped();
}

uint32_t CAN_Sniffer_GetFifoLostEvents(void)
{
  return fifo_lost_events;
}

uint32_t CAN_Sniffer_GetMaxFifoFill(void)
{
  return max_fifo_fill;
}
