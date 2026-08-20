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
static uint32_t current_bitrate = CAN_SNIFFER_BITRATE_1M;
static uint32_t current_data_bitrate = CAN_SNIFFER_DATA_BITRATE_5M;
static bool hardware_started;
static bool current_can_fd = true;

static bool CAN_ConfigureAndStartHardware(void)
{
  /*
   * With zero explicit filters, the global filter routes all standard,
   * extended and remote frames to FIFO0. FDCAN remains in bus-monitoring
   * mode, so this analyzer never ACKs or otherwise drives the CAN bus.
   */
  if ((HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                    FDCAN_ACCEPT_IN_RX_FIFO0,
                                    FDCAN_ACCEPT_IN_RX_FIFO0,
                                    FDCAN_FILTER_REMOTE,
                                    FDCAN_FILTER_REMOTE) != HAL_OK) ||
      (HAL_FDCAN_ConfigTimestampCounter(
           &hfdcan1, FDCAN_TIMESTAMP_PRESC_8) != HAL_OK) ||
      (HAL_FDCAN_EnableTimestampCounter(
           &hfdcan1, FDCAN_TIMESTAMP_INTERNAL) != HAL_OK) ||
      (HAL_FDCAN_Start(&hfdcan1) != HAL_OK))
  {
    hardware_started = false;
    return false;
  }

  hardware_started = true;
  return true;
}

static bool CAN_ApplyBitrateProfile(uint32_t bitrate)
{
  uint32_t prescaler;

  if (bitrate == CAN_SNIFFER_BITRATE_250K)
  {
    prescaler = 20U;
  }
  else if (bitrate == CAN_SNIFFER_BITRATE_500K)
  {
    prescaler = 10U;
  }
  else if (bitrate == CAN_SNIFFER_BITRATE_1M)
  {
    prescaler = 5U;
  }
  else
  {
    return false;
  }

  if (hardware_started && (HAL_FDCAN_Stop(&hfdcan1) != HAL_OK))
  {
    return false;
  }
  hardware_started = false;

  /*
   * All convenience profiles use 16 time quanta and an 87.5% sample point.
   * With the 80 MHz FDCAN kernel clock:
   *   prescaler 20 -> 250 kbit/s
   *   prescaler 10 -> 500 kbit/s
   *   prescaler  5 ->   1 Mbit/s
   */
  hfdcan1.Init.FrameFormat = current_can_fd ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_BUS_MONITORING;
  hfdcan1.Init.NominalPrescaler = prescaler;
  hfdcan1.Init.NominalSyncJumpWidth = 2U;
  hfdcan1.Init.NominalTimeSeg1 = 13U;
  hfdcan1.Init.NominalTimeSeg2 = 2U;

  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    return false;
  }

  return true;
}

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

  if ((frame->flags & CAN_FRAME_FLAG_RTR) != 0U)
  {
    length = 0U;
  }
  else if ((frame->flags & CAN_FRAME_FLAG_FD) != 0U)
  {
    length = CAN_DlcToLength(frame->dlc);
  }
  else
  {
    length = (frame->dlc <= 8U) ? frame->dlc : 8U;
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
  CAN_CaptureBuffer_Init();
  rx_frames = 0U;
  read_errors = 0U;
  fifo_lost_events = 0U;
  max_fifo_fill = 0U;
  capture_running = false;
  hardware_started = false;
  current_bitrate = CAN_SNIFFER_BITRATE_1M;
  current_data_bitrate = CAN_SNIFFER_DATA_BITRATE_5M;
  current_can_fd = true;
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
  (void)CAN_Sniffer_StartListenOnly();
}

void CAN_Sniffer_Stop(void)
{
  CAN_Sniffer_Reset();
}

void CAN_Sniffer_Clear(void)
{
  CAN_CaptureBuffer_Clear();
}

bool CAN_Sniffer_IsRunning(void)
{
  return capture_running;
}

bool CAN_Sniffer_SetBitrate(uint32_t bitrate)
{
  uint32_t previous_bitrate;
  bool was_running;

  if ((bitrate != CAN_SNIFFER_BITRATE_250K) &&
      (bitrate != CAN_SNIFFER_BITRATE_500K) &&
      (bitrate != CAN_SNIFFER_BITRATE_1M))
  {
    return false;
  }
  if (bitrate == current_bitrate)
  {
    return true;
  }

  previous_bitrate = current_bitrate;
  was_running = capture_running;
  capture_running = false;

  if (!CAN_ApplyBitrateProfile(bitrate))
  {
    (void)CAN_ApplyBitrateProfile(previous_bitrate);
    if (was_running)
    {
      (void)CAN_Sniffer_StartListenOnly();
    }
    return false;
  }

  CAN_CaptureBuffer_Clear();
  current_bitrate = bitrate;
  if (was_running && !CAN_Sniffer_StartListenOnly())
  {
    return false;
  }
  return true;
}

bool CAN_Sniffer_SetBitTiming(uint32_t prop_seg,
                              uint32_t phase_seg1,
                              uint32_t phase_seg2,
                              uint32_t sjw,
                              uint32_t brp)
{
  uint32_t time_seg1;
  uint32_t total_tq;

  if ((prop_seg > 256U) || (phase_seg1 > 256U) ||
      ((prop_seg + phase_seg1) < 1U) ||
      ((prop_seg + phase_seg1) > 256U) ||
      (phase_seg2 < 1U) || (phase_seg2 > 128U) ||
      (sjw < 1U) || (sjw > 128U) || (sjw > phase_seg2) ||
      (brp < 1U) || (brp > 512U))
  {
    return false;
  }

  capture_running = false;
  if (hardware_started && (HAL_FDCAN_Stop(&hfdcan1) != HAL_OK))
  {
    return false;
  }
  hardware_started = false;

  time_seg1 = prop_seg + phase_seg1;
  hfdcan1.Init.FrameFormat = current_can_fd ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_BUS_MONITORING;
  hfdcan1.Init.NominalPrescaler = brp;
  hfdcan1.Init.NominalSyncJumpWidth = sjw;
  hfdcan1.Init.NominalTimeSeg1 = time_seg1;
  hfdcan1.Init.NominalTimeSeg2 = phase_seg2;

  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    return false;
  }

  total_tq = 1U + time_seg1 + phase_seg2;
  current_bitrate = CAN_SNIFFER_FDCAN_CLOCK_HZ / (brp * total_tq);
  CAN_CaptureBuffer_Clear();
  return true;
}

bool CAN_Sniffer_SetDataBitTiming(uint32_t prop_seg,
                                  uint32_t phase_seg1,
                                  uint32_t phase_seg2,
                                  uint32_t sjw,
                                  uint32_t brp)
{
  uint32_t time_seg1;
  uint32_t total_tq;

  if ((prop_seg > 32U) || (phase_seg1 > 32U) ||
      ((prop_seg + phase_seg1) < 1U) ||
      ((prop_seg + phase_seg1) > 32U) ||
      (phase_seg2 < 1U) || (phase_seg2 > 16U) ||
      (sjw < 1U) || (sjw > 16U) || (sjw > phase_seg2) ||
      (brp < 1U) || (brp > 32U))
  {
    return false;
  }

  capture_running = false;
  if (hardware_started && (HAL_FDCAN_Stop(&hfdcan1) != HAL_OK))
  {
    return false;
  }
  hardware_started = false;

  time_seg1 = prop_seg + phase_seg1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  hfdcan1.Init.Mode = FDCAN_MODE_BUS_MONITORING;
  hfdcan1.Init.DataPrescaler = brp;
  hfdcan1.Init.DataSyncJumpWidth = sjw;
  hfdcan1.Init.DataTimeSeg1 = time_seg1;
  hfdcan1.Init.DataTimeSeg2 = phase_seg2;

  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    return false;
  }

  total_tq = 1U + time_seg1 + phase_seg2;
  current_data_bitrate = CAN_SNIFFER_FDCAN_CLOCK_HZ / (brp * total_tq);
  CAN_CaptureBuffer_Clear();
  return true;
}

bool CAN_Sniffer_StartListenOnly(void)
{
  return CAN_Sniffer_StartListenOnlyMode(current_can_fd);
}

bool CAN_Sniffer_StartListenOnlyMode(bool can_fd)
{
  capture_running = false;

  if (hardware_started)
  {
    if (HAL_FDCAN_Stop(&hfdcan1) != HAL_OK)
    {
      return false;
    }
    hardware_started = false;
  }

  hfdcan1.Init.FrameFormat = can_fd ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_BUS_MONITORING;
  if ((HAL_FDCAN_Init(&hfdcan1) != HAL_OK) ||
      !CAN_ConfigureAndStartHardware())
  {
    return false;
  }

  current_can_fd = can_fd;
  CAN_CaptureBuffer_Clear();
  capture_running = true;
  return true;
}

void CAN_Sniffer_Reset(void)
{
  capture_running = false;
  if (hardware_started)
  {
    (void)HAL_FDCAN_Stop(&hfdcan1);
    hardware_started = false;
  }
  CAN_CaptureBuffer_Clear();
}

uint32_t CAN_Sniffer_GetBitrate(void)
{
  return current_bitrate;
}

uint32_t CAN_Sniffer_GetDataBitrate(void)
{
  return current_data_bitrate;
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
