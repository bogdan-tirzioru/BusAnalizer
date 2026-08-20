#include "can_sniffer.h"

#include "can_capture_buffer.h"
#include "main.h"
#include "stm32h7xx_hal_fdcan.h"

#include <stddef.h>
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
extern FDCAN_HandleTypeDef hfdcan2;

typedef struct
{
  FDCAN_HandleTypeDef *hfdcan;
  uint32_t rx_frames;
  uint32_t read_errors;
  uint32_t fifo_lost_events;
  uint32_t max_fifo_fill;
  uint32_t current_bitrate;
  uint32_t current_data_bitrate;
  bool capture_running;
  bool hardware_started;
  bool current_can_fd;
  uint8_t channel;
} CAN_SnifferChannel;

static CAN_SnifferChannel channels[CAN_SNIFFER_CHANNEL_COUNT];
static uint8_t process_first_channel;

static CAN_SnifferChannel *CAN_GetChannel(uint8_t channel)
{
  if (channel >= CAN_SNIFFER_CHANNEL_COUNT)
  {
    return NULL;
  }
  return &channels[channel];
}

static bool CAN_StopHardware(CAN_SnifferChannel *ctx)
{
  if (ctx->hardware_started)
  {
    if (HAL_FDCAN_Stop(ctx->hfdcan) != HAL_OK)
    {
      return false;
    }
    ctx->hardware_started = false;
  }
  return true;
}

static bool CAN_ConfigureAndStartHardware(CAN_SnifferChannel *ctx)
{
  /*
   * With zero explicit filters, the global filter routes all standard,
   * extended and remote frames to FIFO0. Both FDCAN instances remain in
   * bus-monitoring mode, so neither analyzer channel ACKs or drives the bus.
   */
  if ((HAL_FDCAN_ConfigGlobalFilter(ctx->hfdcan,
                                    FDCAN_ACCEPT_IN_RX_FIFO0,
                                    FDCAN_ACCEPT_IN_RX_FIFO0,
                                    FDCAN_FILTER_REMOTE,
                                    FDCAN_FILTER_REMOTE) != HAL_OK) ||
      (HAL_FDCAN_ConfigTimestampCounter(
           ctx->hfdcan, FDCAN_TIMESTAMP_PRESC_8) != HAL_OK) ||
      (HAL_FDCAN_EnableTimestampCounter(
           ctx->hfdcan, FDCAN_TIMESTAMP_INTERNAL) != HAL_OK))
  {
    ctx->hardware_started = false;
    return false;
  }

  /* Do not carry a stale FIFO-lost indication into a new channel run. */
  ctx->hfdcan->Instance->IR = FDCAN_IR_RF0L;

  if (HAL_FDCAN_Start(ctx->hfdcan) != HAL_OK)
  {
    ctx->hardware_started = false;
    return false;
  }

  ctx->hardware_started = true;
  return true;
}

static bool CAN_ApplyBitrateProfileChannel(CAN_SnifferChannel *ctx,
                                           uint32_t bitrate)
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

  if (!CAN_StopHardware(ctx))
  {
    return false;
  }
  ctx->capture_running = false;

  /*
   * All convenience profiles use 16 time quanta and an 87.5% sample point.
   * With the 80 MHz FDCAN kernel clock:
   *   prescaler 20 -> 250 kbit/s
   *   prescaler 10 -> 500 kbit/s
   *   prescaler  5 ->   1 Mbit/s
   */
  ctx->hfdcan->Init.FrameFormat =
      ctx->current_can_fd ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_CLASSIC;
  ctx->hfdcan->Init.Mode = FDCAN_MODE_BUS_MONITORING;
  ctx->hfdcan->Init.NominalPrescaler = prescaler;
  ctx->hfdcan->Init.NominalSyncJumpWidth = 2U;
  ctx->hfdcan->Init.NominalTimeSeg1 = 13U;
  ctx->hfdcan->Init.NominalTimeSeg2 = 2U;

  if (HAL_FDCAN_Init(ctx->hfdcan) != HAL_OK)
  {
    return false;
  }

  ctx->current_bitrate = bitrate;
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

static bool CAN_ReadFifo0Direct(CAN_SnifferChannel *ctx,
                                CAN_SnifferFrame *frame)
{
  uint32_t fifo_status;
  uint32_t get_index;
  uint32_t *element;
  uint32_t word0;
  uint32_t word1;
  uint8_t length;
  uint32_t i;

  fifo_status = ctx->hfdcan->Instance->RXF0S;
  if ((fifo_status & FDCAN_RXF0S_F0FL) == 0U)
  {
    return false;
  }

  get_index = (fifo_status & FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;
  element = (uint32_t *)(ctx->hfdcan->msgRam.RxFIFO0SA +
                         (get_index * ctx->hfdcan->Init.RxFifo0ElmtSize * 4U));
  word0 = element[0];
  word1 = element[1];

  frame->flags = (ctx->channel == 1U) ? CAN_FRAME_FLAG_CHANNEL_1 : 0U;

  if ((word0 & RX_ELEMENT_XTD_MASK) != 0U)
  {
    frame->id = word0 & RX_ELEMENT_EXTID_MASK;
    frame->flags |= CAN_FRAME_FLAG_EXTENDED;
  }
  else
  {
    frame->id = (word0 & RX_ELEMENT_STDID_MASK) >> 18;
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

  ctx->hfdcan->Instance->RXF0A = get_index;
  return true;
}

static void CAN_ProcessChannel(CAN_SnifferChannel *ctx)
{
  CAN_SnifferFrame frame;
  uint32_t fifo_status;
  uint32_t fill;

  if (!ctx->hardware_started)
  {
    return;
  }

  fifo_status = ctx->hfdcan->Instance->RXF0S;
  fill = fifo_status & FDCAN_RXF0S_F0FL;
  if (fill > ctx->max_fifo_fill)
  {
    ctx->max_fifo_fill = fill;
  }

  if ((fifo_status & FDCAN_RXF0S_RF0L) != 0U)
  {
    ctx->fifo_lost_events++;
    ctx->hfdcan->Instance->IR = FDCAN_IR_RF0L;
  }

  while ((ctx->hfdcan->Instance->RXF0S & FDCAN_RXF0S_F0FL) != 0U)
  {
    if (!CAN_ReadFifo0Direct(ctx, &frame))
    {
      ctx->read_errors++;
      break;
    }

    ctx->rx_frames++;
    if (ctx->capture_running)
    {
      (void)CAN_CaptureBuffer_Push(&frame);
    }
  }
}

void CAN_Sniffer_Init(void)
{
  memset(channels, 0, sizeof(channels));

  channels[0].hfdcan = &hfdcan1;
  channels[0].channel = 0U;
  channels[0].current_bitrate = CAN_SNIFFER_BITRATE_1M;
  channels[0].current_data_bitrate = CAN_SNIFFER_DATA_BITRATE_5M;
  channels[0].current_can_fd = true;

  channels[1].hfdcan = &hfdcan2;
  channels[1].channel = 1U;
  channels[1].current_bitrate = CAN_SNIFFER_BITRATE_1M;
  channels[1].current_data_bitrate = CAN_SNIFFER_DATA_BITRATE_5M;
  channels[1].current_can_fd = true;

  process_first_channel = 0U;
  CAN_CaptureBuffer_Init();
}

void CAN_Sniffer_Process(void)
{
  /* Alternate the first fully-drained FIFO to avoid a fixed channel bias. */
  if (process_first_channel == 0U)
  {
    CAN_ProcessChannel(&channels[0]);
    CAN_ProcessChannel(&channels[1]);
    process_first_channel = 1U;
  }
  else
  {
    CAN_ProcessChannel(&channels[1]);
    CAN_ProcessChannel(&channels[0]);
    process_first_channel = 0U;
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
  return CAN_Sniffer_IsChannelRunning(0U);
}

bool CAN_Sniffer_IsChannelRunning(uint8_t channel)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);
  return (ctx != NULL) ? ctx->capture_running : false;
}

bool CAN_Sniffer_SetBitrate(uint32_t bitrate)
{
  CAN_SnifferChannel *ctx = &channels[0];
  uint32_t previous_bitrate;
  bool was_running;

  if ((bitrate != CAN_SNIFFER_BITRATE_250K) &&
      (bitrate != CAN_SNIFFER_BITRATE_500K) &&
      (bitrate != CAN_SNIFFER_BITRATE_1M))
  {
    return false;
  }
  if (bitrate == ctx->current_bitrate)
  {
    return true;
  }

  previous_bitrate = ctx->current_bitrate;
  was_running = ctx->capture_running;

  if (!CAN_ApplyBitrateProfileChannel(ctx, bitrate))
  {
    (void)CAN_ApplyBitrateProfileChannel(ctx, previous_bitrate);
    if (was_running)
    {
      (void)CAN_Sniffer_StartListenOnly();
    }
    return false;
  }

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
  return CAN_Sniffer_SetBitTimingChannel(0U, prop_seg, phase_seg1,
                                         phase_seg2, sjw, brp);
}

bool CAN_Sniffer_SetBitTimingChannel(uint8_t channel,
                                     uint32_t prop_seg,
                                     uint32_t phase_seg1,
                                     uint32_t phase_seg2,
                                     uint32_t sjw,
                                     uint32_t brp)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);
  uint32_t time_seg1;
  uint32_t total_tq;

  if ((ctx == NULL) ||
      (prop_seg > 256U) || (phase_seg1 > 256U) ||
      ((prop_seg + phase_seg1) < 1U) ||
      ((prop_seg + phase_seg1) > 256U) ||
      (phase_seg2 < 1U) || (phase_seg2 > 128U) ||
      (sjw < 1U) || (sjw > 128U) || (sjw > phase_seg2) ||
      (brp < 1U) || (brp > 512U))
  {
    return false;
  }

  ctx->capture_running = false;
  if (!CAN_StopHardware(ctx))
  {
    return false;
  }

  time_seg1 = prop_seg + phase_seg1;
  ctx->hfdcan->Init.FrameFormat =
      ctx->current_can_fd ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_CLASSIC;
  ctx->hfdcan->Init.Mode = FDCAN_MODE_BUS_MONITORING;
  ctx->hfdcan->Init.NominalPrescaler = brp;
  ctx->hfdcan->Init.NominalSyncJumpWidth = sjw;
  ctx->hfdcan->Init.NominalTimeSeg1 = time_seg1;
  ctx->hfdcan->Init.NominalTimeSeg2 = phase_seg2;

  if (HAL_FDCAN_Init(ctx->hfdcan) != HAL_OK)
  {
    return false;
  }

  total_tq = 1U + time_seg1 + phase_seg2;
  ctx->current_bitrate = CAN_SNIFFER_FDCAN_CLOCK_HZ / (brp * total_tq);
  return true;
}

bool CAN_Sniffer_SetDataBitTiming(uint32_t prop_seg,
                                  uint32_t phase_seg1,
                                  uint32_t phase_seg2,
                                  uint32_t sjw,
                                  uint32_t brp)
{
  return CAN_Sniffer_SetDataBitTimingChannel(0U, prop_seg, phase_seg1,
                                             phase_seg2, sjw, brp);
}

bool CAN_Sniffer_SetDataBitTimingChannel(uint8_t channel,
                                         uint32_t prop_seg,
                                         uint32_t phase_seg1,
                                         uint32_t phase_seg2,
                                         uint32_t sjw,
                                         uint32_t brp)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);
  uint32_t time_seg1;
  uint32_t total_tq;

  if ((ctx == NULL) ||
      (prop_seg > 32U) || (phase_seg1 > 32U) ||
      ((prop_seg + phase_seg1) < 1U) ||
      ((prop_seg + phase_seg1) > 32U) ||
      (phase_seg2 < 1U) || (phase_seg2 > 16U) ||
      (sjw < 1U) || (sjw > 16U) || (sjw > phase_seg2) ||
      (brp < 1U) || (brp > 32U))
  {
    return false;
  }

  ctx->capture_running = false;
  if (!CAN_StopHardware(ctx))
  {
    return false;
  }

  time_seg1 = prop_seg + phase_seg1;
  ctx->hfdcan->Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  ctx->hfdcan->Init.Mode = FDCAN_MODE_BUS_MONITORING;
  ctx->hfdcan->Init.DataPrescaler = brp;
  ctx->hfdcan->Init.DataSyncJumpWidth = sjw;
  ctx->hfdcan->Init.DataTimeSeg1 = time_seg1;
  ctx->hfdcan->Init.DataTimeSeg2 = phase_seg2;

  if (HAL_FDCAN_Init(ctx->hfdcan) != HAL_OK)
  {
    return false;
  }

  total_tq = 1U + time_seg1 + phase_seg2;
  ctx->current_data_bitrate =
      CAN_SNIFFER_FDCAN_CLOCK_HZ / (brp * total_tq);
  return true;
}

bool CAN_Sniffer_StartListenOnly(void)
{
  return CAN_Sniffer_StartListenOnlyMode(channels[0].current_can_fd);
}

bool CAN_Sniffer_StartListenOnlyMode(bool can_fd)
{
  return CAN_Sniffer_StartListenOnlyModeChannel(0U, can_fd);
}

bool CAN_Sniffer_StartListenOnlyModeChannel(uint8_t channel, bool can_fd)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);

  if (ctx == NULL)
  {
    return false;
  }

  ctx->capture_running = false;
  if (!CAN_StopHardware(ctx))
  {
    return false;
  }

  ctx->hfdcan->Init.FrameFormat = can_fd ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_CLASSIC;
  ctx->hfdcan->Init.Mode = FDCAN_MODE_BUS_MONITORING;

  if ((HAL_FDCAN_Init(ctx->hfdcan) != HAL_OK) ||
      !CAN_ConfigureAndStartHardware(ctx))
  {
    return false;
  }

  ctx->current_can_fd = can_fd;
  ctx->capture_running = true;
  return true;
}

void CAN_Sniffer_Reset(void)
{
  CAN_Sniffer_ResetChannel(0U);
}

void CAN_Sniffer_ResetChannel(uint8_t channel)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);

  if (ctx == NULL)
  {
    return;
  }

  ctx->capture_running = false;
  (void)CAN_StopHardware(ctx);
}

void CAN_Sniffer_ResetAll(void)
{
  uint8_t channel;

  for (channel = 0U; channel < CAN_SNIFFER_CHANNEL_COUNT; channel++)
  {
    CAN_Sniffer_ResetChannel(channel);
  }
  CAN_CaptureBuffer_Clear();
}

uint32_t CAN_Sniffer_GetBitrate(void)
{
  return channels[0].current_bitrate;
}

uint32_t CAN_Sniffer_GetDataBitrate(void)
{
  return channels[0].current_data_bitrate;
}

uint32_t CAN_Sniffer_GetRxCount(void)
{
  return channels[0].rx_frames + channels[1].rx_frames;
}

uint32_t CAN_Sniffer_GetChannelRxCount(uint8_t channel)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);
  return (ctx != NULL) ? ctx->rx_frames : 0U;
}

uint32_t CAN_Sniffer_GetChannelErrorCount(uint8_t channel)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);
  return (ctx != NULL) ? ctx->read_errors : 0U;
}

uint32_t CAN_Sniffer_GetChannelFifoLostEvents(uint8_t channel)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);
  return (ctx != NULL) ? ctx->fifo_lost_events : 0U;
}

uint32_t CAN_Sniffer_GetChannelMaxFifoFill(uint8_t channel)
{
  CAN_SnifferChannel *ctx = CAN_GetChannel(channel);
  return (ctx != NULL) ? ctx->max_fifo_fill : 0U;
}

uint32_t CAN_Sniffer_GetErrorCount(void)
{
  return channels[0].read_errors + channels[1].read_errors;
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
  return channels[0].fifo_lost_events;
}

uint32_t CAN_Sniffer_GetMaxFifoFill(void)
{
  return channels[0].max_fifo_fill;
}
