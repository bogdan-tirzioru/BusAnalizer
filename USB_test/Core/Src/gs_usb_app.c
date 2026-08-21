#include "gs_usb_app.h"

#include "can_capture_buffer.h"
#include "can_sniffer.h"
#include "logger.h"
#include "usb_device.h"
#include "usbd_bulk.h"

#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceHS;

__ALIGN_BEGIN static GS_USB_HostFrame tx_frames[2] __ALIGN_END;
static uint8_t tx_fill_index;
static uint32_t last_log_ms;
static uint32_t last_logged_frames[CAN_SNIFFER_CHANNEL_COUNT];
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
  (void)memset(tx_frames, 0, sizeof(tx_frames));
  (void)memset(last_logged_frames, 0, sizeof(last_logged_frames));
  tx_fill_index = 0U;
  last_log_ms = HAL_GetTick();
  was_configured = 0U;
}

void GS_USB_App_Task(void)
{
  const CAN_SnifferFrame *frame;
  uint32_t now = HAL_GetTick();
  uint32_t can1_frames;
  uint32_t can2_frames;

  if (hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED)
  {
    if (was_configured != 0U)
    {
      was_configured = 0U;
      CAN_Sniffer_ResetAll();
      Logger_Write("gs_usb disconnected\r\n");
    }
    return;
  }

  if (was_configured == 0U)
  {
    was_configured = 1U;
    last_log_ms = now;
    last_logged_frames[0] = CAN_Sniffer_GetChannelRxCount(0U);
    last_logged_frames[1] = CAN_Sniffer_GetChannelRxCount(1U);
    Logger_Write("gs_usb configured; CAN channels 0/1 available\r\n");
  }

  while ((USBD_GS_USB_TxSlotsAvailable(&hUsbDeviceHS) != 0U) &&
         ((frame = CAN_CaptureBuffer_Peek()) != NULL))
  {
    GS_USB_HostFrame *tx_frame = &tx_frames[tx_fill_index];
    uint32_t usb_length = GS_USB_CLASSIC_HOST_FRAME_SIZE;

    tx_frame->echo_id = GS_HOST_FRAME_ECHO_ID_RX;
    tx_frame->can_id = GS_USB_EncodeCanId(frame);
    tx_frame->can_dlc = frame->dlc & 0x0FU;
    tx_frame->channel =
        ((frame->flags & CAN_FRAME_FLAG_CHANNEL_1) != 0U) ? 1U : 0U;
    tx_frame->flags = 0U;
    tx_frame->reserved = 0U;

    if ((frame->flags & CAN_FRAME_FLAG_FD) != 0U)
    {
      tx_frame->flags |= GS_CAN_FLAG_FD;
      usb_length = GS_USB_FD_HOST_FRAME_SIZE;

      if ((frame->flags & CAN_FRAME_FLAG_BRS) != 0U)
      {
        tx_frame->flags |= GS_CAN_FLAG_BRS;
      }
      if ((frame->flags & CAN_FRAME_FLAG_ESI) != 0U)
      {
        tx_frame->flags |= GS_CAN_FLAG_ESI;
      }
    }

    if ((frame->flags & CAN_FRAME_FLAG_RTR) == 0U)
    {
      (void)memcpy(tx_frame->data, frame->data,
                   ((frame->flags & CAN_FRAME_FLAG_FD) != 0U) ? 64U : 8U);
    }
    else
    {
      (void)memset(tx_frame->data, 0, 8U);
    }

    if (USBD_GS_USB_Transmit(&hUsbDeviceHS,
                             (uint8_t *)tx_frame,
                             usb_length) != USBD_OK)
    {
      break;
    }
    CAN_CaptureBuffer_Release();
    tx_fill_index ^= 1U;
  }

  if ((uint32_t)(now - last_log_ms) >= 1000U)
  {
    can1_frames = CAN_Sniffer_GetChannelRxCount(0U);
    can2_frames = CAN_Sniffer_GetChannelRxCount(1U);

    Logger_Printf("CAN1 %lu fps buf=%lu drop=%lu lost=%lu max=%lu | CAN2 %lu fps lost=%lu max=%lu\r\n",
                  (unsigned long)(can1_frames - last_logged_frames[0]),
                  (unsigned long)CAN_Sniffer_GetBufferedCount(),
                  (unsigned long)CAN_Sniffer_GetDroppedCount(),
                  (unsigned long)CAN_Sniffer_GetChannelFifoLostEvents(0U),
                  (unsigned long)CAN_Sniffer_GetChannelMaxFifoFill(0U),
                  (unsigned long)(can2_frames - last_logged_frames[1]),
                  (unsigned long)CAN_Sniffer_GetChannelFifoLostEvents(1U),
                  (unsigned long)CAN_Sniffer_GetChannelMaxFifoFill(1U));

    last_logged_frames[0] = can1_frames;
    last_logged_frames[1] = can2_frames;
    last_log_ms = now;
  }
}
