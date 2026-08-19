#include "baii_protocol_platform.h"

#include "can_sniffer.h"
#include "main.h"

#include <string.h>

#define CAN1_KERNEL_CLOCK_HZ 8000000UL
#define CAN1_BITRATE         500000UL
#define CAN1_SAMPLE_POINT    875U

static void FillCanConfig(BAII_CanConfig *config)
{
  memset(config, 0, sizeof(*config));
  config->channel = BAII_CAN_CHANNEL_1;
  config->mode = BAII_CAN_MODE_LISTEN_ONLY;
  config->frame_format = BAII_CAN_FORMAT_CLASSIC;
  config->fdcan_clock_hz = CAN1_KERNEL_CLOCK_HZ;
  config->nominal_bitrate = CAN1_BITRATE;
  config->data_bitrate = CAN1_BITRATE;
  config->nominal_sample_point_permille = CAN1_SAMPLE_POINT;
  config->data_sample_point_permille = CAN1_SAMPLE_POINT;
  config->nominal_prescaler = 1U;
  config->nominal_time_seg1 = 13U;
  config->nominal_time_seg2 = 2U;
  config->nominal_sjw = 1U;
}

BAII_StatusCode BAII_Platform_GetInfo(BAII_DeviceInfo *info)
{
  if (info == NULL)
  {
    return BAII_STATUS_INVALID_PARAM;
  }

  memset(info, 0, sizeof(*info));
  info->firmware_major = 1U;
  info->firmware_minor = 1U;
  info->firmware_patch = 0U;
  info->capabilities = BAII_CAP_CAN_CONFIG |
                       BAII_CAP_CAPTURE_STATUS |
                       BAII_CAP_STREAMING;
  info->fdcan_clock_hz = CAN1_KERNEL_CLOCK_HZ;
  info->device_id = HAL_GetUIDw0();
  info->can_channel_count = 1U;
  info->rtc_valid = 0U;
  return BAII_STATUS_OK;
}

BAII_StatusCode BAII_Platform_GetStatus(BAII_DeviceStatus *status)
{
  if (status == NULL)
  {
    return BAII_STATUS_INVALID_PARAM;
  }

  memset(status, 0, sizeof(*status));
  status->uptime_ms = HAL_GetTick();
  status->can_rx_frames = CAN_Sniffer_GetRxCount();
  status->sram_buffered_frames = CAN_Sniffer_GetBufferedCount();
  status->sram_dropped_frames = CAN_Sniffer_GetDroppedCount();
  status->fdcan_fifo_lost_events = CAN_Sniffer_GetFifoLostEvents();
  return BAII_STATUS_OK;
}

BAII_StatusCode BAII_Platform_GetRtcTime(uint64_t *unix_time_us)
{
  (void)unix_time_us;
  return BAII_STATUS_NOT_SUPPORTED;
}

BAII_StatusCode BAII_Platform_SetRtcTime(uint64_t unix_time_us)
{
  (void)unix_time_us;
  return BAII_STATUS_NOT_SUPPORTED;
}

BAII_StatusCode BAII_Platform_GetCanConfig(uint8_t channel,
                                           BAII_CanConfig *config)
{
  if ((channel != BAII_CAN_CHANNEL_1) || (config == NULL))
  {
    return BAII_STATUS_INVALID_PARAM;
  }

  FillCanConfig(config);
  return BAII_STATUS_OK;
}

BAII_StatusCode BAII_Platform_SetCanConfig(const BAII_CanConfig *requested,
                                           BAII_CanConfig *applied)
{
  if ((requested == NULL) || (applied == NULL))
  {
    return BAII_STATUS_INVALID_PARAM;
  }

  /*
   * The first sniffer profile is intentionally locked to a safe, passive
   * 500 kbit/s Classic CAN setup.  Future profiles can add a timing solver
   * without allowing the probe to transmit onto the monitored bus.
   */
  if ((requested->channel != BAII_CAN_CHANNEL_1) ||
      (requested->mode != BAII_CAN_MODE_LISTEN_ONLY) ||
      (requested->frame_format != BAII_CAN_FORMAT_CLASSIC) ||
      (requested->nominal_bitrate != CAN1_BITRATE) ||
      ((requested->data_bitrate != 0U) &&
       (requested->data_bitrate != CAN1_BITRATE)) ||
      ((requested->nominal_sample_point_permille != 0U) &&
       (requested->nominal_sample_point_permille != CAN1_SAMPLE_POINT)))
  {
    return BAII_STATUS_INVALID_PARAM;
  }

  FillCanConfig(applied);
  return BAII_STATUS_OK;
}

BAII_StatusCode BAII_Platform_CaptureStart(void)
{
  CAN_Sniffer_Start();
  return BAII_STATUS_OK;
}

BAII_StatusCode BAII_Platform_CaptureStop(void)
{
  CAN_Sniffer_Stop();
  return BAII_STATUS_OK;
}

BAII_StatusCode BAII_Platform_CaptureClear(void)
{
  CAN_Sniffer_Clear();
  return BAII_STATUS_OK;
}

BAII_StatusCode BAII_Platform_GetCaptureStatus(BAII_CaptureStatus *status)
{
  if (status == NULL)
  {
    return BAII_STATUS_INVALID_PARAM;
  }

  memset(status, 0, sizeof(*status));
  status->enabled = CAN_Sniffer_IsRunning() ? 1U : 0U;
  status->buffered_frames = CAN_Sniffer_GetBufferedCount();
  status->dropped_frames = CAN_Sniffer_GetDroppedCount();
  status->fifo_lost_events = CAN_Sniffer_GetFifoLostEvents();
  status->received_frames = CAN_Sniffer_GetRxCount();
  return BAII_STATUS_OK;
}
