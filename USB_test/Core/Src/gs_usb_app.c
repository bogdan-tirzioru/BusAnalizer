#include "gs_usb_app.h"

#include "can_capture_buffer.h"
#include "can_sniffer.h"
#include "logger.h"
#include "stm32h7xx_hal_fdcan.h"
#include "usb_device.h"
#include "usbd_bulk.h"

#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceHS;
extern FDCAN_HandleTypeDef hfdcan2;

__ALIGN_BEGIN static GS_USB_HostFrame tx_frame __ALIGN_END;
static uint32_t last_log_ms;
static uint32_t last_logged_frames;
static uint32_t last_logged_can2_frames;
static uint32_t can2_rx_frames;
static uint32_t can2_fifo_lost_events;
static uint32_t can2_max_fifo_fill;
static uint8_t can2_started;
static uint8_t can2_start_attempted;
static uint8_t was_configured;

static uint8_t CAN2_Validation_Start(void)
{
  can2_rx_frames = 0U;
  can2_fifo_lost_events = 0U;
  can2_max_fifo_fill = 0U;
  can2_started = 0U;

  /*
   * FDCAN2 is a validation-only passive receiver for now. CubeMX already
   * configured it for CAN FD+BRS, bus-monitoring mode and its own Message RAM
   * region. Accept every standard, extended and remote frame into FIFO0, just
   * like the production CAN1 sniffer.
   */
  if ((HAL_FDCAN_ConfigGlobalFilter(&hfdcan2,
                                    FDCAN_ACCEPT_IN_RX_FIFO0,
                                    FDCAN_ACCEPT_IN_RX_FIFO0,
                                    FDCAN_FILTER_REMOTE,
                                    FDCAN_FILTER_REMOTE) != HAL_OK) ||
      (HAL_FDCAN_ConfigTimestampCounter(
           &hfdcan2, FDCAN_TIMESTAMP_PRESC_8) != HAL_OK) ||
      (HAL_FDCAN_EnableTimestampCounter(
           &hfdcan2, FDCAN_TIMESTAMP_INTERNAL) != HAL_OK))
  {
    return 0U;
  }

  /* Do not carry a stale FIFO-lost indication into a new validation run. */
  hfdcan2.Instance->IR = FDCAN_IR_RF0L;

  if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK)
  {
    return 0U;
  }

  can2_started = 1U;
  return 1U;
}

static void CAN2_Validation_Stop(void)
{
  if (can2_started != 0U)
  {
    (void)HAL_FDCAN_Stop(&hfdcan2);
  }

  can2_started = 0U;
  can2_start_attempted = 0U;
  can2_rx_frames = 0U;
  can2_fifo_lost_events = 0U;
  can2_max_fifo_fill = 0U;
}

static void CAN2_Validation_Process(void)
{
  uint32_t fifo_status;
  uint32_t fill;
  uint32_t get_index;

  if (can2_started == 0U)
  {
    return;
  }

  fifo_status = hfdcan2.Instance->RXF0S;
  fill = fifo_status & FDCAN_RXF0S_F0FL;
  if (fill > can2_max_fifo_fill)
  {
    can2_max_fifo_fill = fill;
  }

  if ((fifo_status & FDCAN_RXF0S_RF0L) != 0U)
  {
    can2_fifo_lost_events++;
    hfdcan2.Instance->IR = FDCAN_IR_RF0L;
  }

  /*
   * Validation does not copy CAN2 payloads into the capture buffer yet. Drain
   * and acknowledge FIFO0 as quickly as possible and compare its frame rate to
   * CAN1, which is connected to the same physical CAN network.
   */
  while ((hfdcan2.Instance->RXF0S & FDCAN_RXF0S_F0FL) != 0U)
  {
    fifo_status = hfdcan2.Instance->RXF0S;
    get_index = (fifo_status & FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;
    can2_rx_frames++;
    hfdcan2.Instance->RXF0A = get_index;
  }
}

uint32_t GS_USB_App_GetCAN2RxCount(void)
{
  return can2_rx_frames;
}

static uint32_t GS_USB_EncodeCanId(const CAN_SnifferFrame *frame)
{
  uint32_t can_id;

  if ((frame->flags & CAN_FRAME_FLAG_EXTENDED) != 0U)
  {
    can_id = (frame->id & GS_CAN_EFF_MASK) | GS_CAN_EFF_FLAG;
  }
  else
  {
    can_id = frame->id & GS_CAN_SFF_MASK;
  }

  if ((frame->flags & CAN_FRAME_FLAG_RTR) != 0U)
  {
    can_id |= GS_CAN_RTR_FLAG;
  }
  return can_id;
}

void GS_USB_App_Init(void)
{
  (void)memset(&tx_frame, 0, sizeof(tx_frame));
  last_log_ms = HAL_GetTick();
  last_logged_frames = 0U;
  last_logged_can2_frames = 0U;
  can2_rx_frames = 0U;
  can2_fifo_lost_events = 0U;
  can2_max_fifo_fill = 0U;
  can2_started = 0U;
  can2_start_attempted = 0U;
  was_configured = 0U;
}

void GS_USB_App_Task(void)
{
  CAN_SnifferFrame frame;
  uint32_t now = HAL_GetTick();
  uint32_t frames;
  uint32_t can2_frames;

  if (hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED)
  {
    if (was_configured != 0U)
    {
      was_configured = 0U;
      CAN_Sniffer_Reset();
      CAN2_Validation_Stop();
      Logger_Write("gs_usb disconnected\r\n");
    }
    return;
  }

  if (was_configured == 0U)
  {
    was_configured = 1U;

    /*
     * Perform the one-time blocking USB status print before FDCAN2 is started.
     * This prevents boot/status UART traffic from filling CAN2 FIFO0 and
     * contaminating the validation lost/max statistics.
     */
    Logger_Write("gs_usb configured; waiting for SocketCAN link-up\r\n");

    if (can2_start_attempted == 0U)
    {
      can2_start_attempted = 1U;
      if (CAN2_Validation_Start() == 0U)
      {
        Logger_Write("CAN2 validation receiver START FAILED\r\n");
      }
    }

    now = HAL_GetTick();
    last_log_ms = now;
    can2_frames = can2_rx_frames;
    last_logged_frames = CAN_Sniffer_GetRxCount() - can2_frames;
    last_logged_can2_frames = can2_frames;
  }

  CAN2_Validation_Process();

  if ((USBD_GS_USB_TxReady(&hUsbDeviceHS) != 0U) &&
      CAN_CaptureBuffer_Pop(&frame))
  {
    uint32_t usb_length = GS_USB_CLASSIC_HOST_FRAME_SIZE;

    (void)memset(&tx_frame, 0, sizeof(tx_frame));
    tx_frame.echo_id = GS_HOST_FRAME_ECHO_ID_RX;
    tx_frame.can_id = GS_USB_EncodeCanId(&frame);
    tx_frame.can_dlc = frame.dlc & 0x0FU;
    tx_frame.channel = 0U;

    if ((frame.flags & CAN_FRAME_FLAG_FD) != 0U)
    {
      tx_frame.flags |= GS_CAN_FLAG_FD;
      usb_length = GS_USB_FD_HOST_FRAME_SIZE;

      if ((frame.flags & CAN_FRAME_FLAG_BRS) != 0U)
      {
        tx_frame.flags |= GS_CAN_FLAG_BRS;
      }
      if ((frame.flags & CAN_FRAME_FLAG_ESI) != 0U)
      {
        tx_frame.flags |= GS_CAN_FLAG_ESI;
      }
    }

    if ((frame.flags & CAN_FRAME_FLAG_RTR) == 0U)
    {
      (void)memcpy(tx_frame.data, frame.data,
                   ((frame.flags & CAN_FRAME_FLAG_FD) != 0U) ? 64U : 8U);
    }

    (void)USBD_GS_USB_Transmit(&hUsbDeviceHS,
                               (uint8_t *)&tx_frame,
                               usb_length);
  }

  if ((uint32_t)(now - last_log_ms) >= 1000U)
  {
    can2_frames = can2_rx_frames;
    frames = CAN_Sniffer_GetRxCount() - can2_frames;

    Logger_Printf("CAN1 %lu fps buf=%lu drop=%lu lost=%lu | CAN2 %lu fps lost=%lu max=%lu\r\n",
                  (unsigned long)(frames - last_logged_frames),
                  (unsigned long)CAN_Sniffer_GetBufferedCount(),
                  (unsigned long)CAN_Sniffer_GetDroppedCount(),
                  (unsigned long)CAN_Sniffer_GetFifoLostEvents(),
                  (unsigned long)(can2_frames - last_logged_can2_frames),
                  (unsigned long)can2_fifo_lost_events,
                  (unsigned long)can2_max_fifo_fill);

    last_logged_frames = frames;
    last_logged_can2_frames = can2_frames;
    last_log_ms = now;
  }
}
