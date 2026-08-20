#include "gs_usb_app.h"

#include "can_capture_buffer.h"
#include "can_sniffer.h"
#include "logger.h"
#include "usb_device.h"
#include "usbd_bulk.h"

#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceHS;

__ALIGN_BEGIN static GS_USB_HostFrame tx_frame __ALIGN_END;
static uint32_t last_log_ms;
static uint32_t last_logged_frames;
static uint8_t was_configured;

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
  was_configured = 0U;
}

void GS_USB_App_Task(void)
{
  CAN_SnifferFrame frame;
  uint32_t now = HAL_GetTick();
  uint32_t frames;

  if (hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED)
  {
    if (was_configured != 0U)
    {
      was_configured = 0U;
      CAN_Sniffer_Reset();
      Logger_Write("gs_usb disconnected\r\n");
    }
    return;
  }

  if (was_configured == 0U)
  {
    was_configured = 1U;
    Logger_Write("gs_usb configured; waiting for SocketCAN link-up\r\n");
  }

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
    frames = CAN_Sniffer_GetRxCount();
    Logger_Printf("SocketCAN CAN1 %luk/%luk FD: %lu fps, buffered=%lu dropped=%lu fifo_lost=%lu\r\n",
                  (unsigned long)(CAN_Sniffer_GetBitrate() / 1000U),
                  (unsigned long)(CAN_Sniffer_GetDataBitrate() / 1000U),
                  (unsigned long)(frames - last_logged_frames),
                  (unsigned long)CAN_Sniffer_GetBufferedCount(),
                  (unsigned long)CAN_Sniffer_GetDroppedCount(),
                  (unsigned long)CAN_Sniffer_GetFifoLostEvents());
    last_logged_frames = frames;
    last_log_ms = now;
  }
}
