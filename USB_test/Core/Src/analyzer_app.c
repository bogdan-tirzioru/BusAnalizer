#include "analyzer_app.h"

#include "baii_protocol.h"
#include "can_capture_buffer.h"
#include "can_sniffer.h"
#include "logger.h"
#include "main.h"
#include "usb_device.h"
#include "usbd_bulk.h"

#include <string.h>

#define ANALYZER_TX_SIZE       BULK_TEST_BLOCK_SIZE
#define ANALYZER_PAYLOAD_SIZE  (ANALYZER_TX_SIZE - BAII_PROTOCOL_HEADER_SIZE)
#define ANALYZER_MAX_RECORD    (BAII_PROTOCOL_CAN_RECORD_BASE + 8U)

extern USBD_HandleTypeDef hUsbDeviceHS;

__ALIGN_BEGIN static uint8_t tx_buffer[ANALYZER_TX_SIZE] __ALIGN_END;
static uint8_t command_buffer[BULK_COMMAND_MAX_SIZE];
static uint8_t response_buffer[BULK_COMMAND_MAX_SIZE];
static uint32_t response_length;
static uint32_t stream_sequence;
static uint32_t last_log_ms;
static uint32_t last_logged_frames;
static uint64_t timestamp_epoch;
static uint16_t last_timestamp;
static uint8_t timestamp_started;
static uint8_t response_pending;
static uint8_t was_configured;

static uint8_t DlcToLength(uint8_t dlc)
{
  static const uint8_t lengths[16] =
  {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
    8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U
  };
  return lengths[dlc & 0x0FU];
}

static uint64_t ExtendTimestamp(uint16_t raw)
{
  if (timestamp_started == 0U)
  {
    timestamp_started = 1U;
    last_timestamp = raw;
    return raw;
  }

  if ((raw < last_timestamp) &&
      ((uint16_t)(last_timestamp - raw) > 0x8000U))
  {
    timestamp_epoch += 0x10000ULL;
  }

  last_timestamp = raw;
  return timestamp_epoch + raw;
}

static uint16_t TranslateFlags(uint8_t flags)
{
  uint16_t wire_flags = 0U;

  if ((flags & CAN_FRAME_FLAG_EXTENDED) != 0U)
  {
    wire_flags |= BAII_CAN_FLAG_EXT;
  }
  if ((flags & CAN_FRAME_FLAG_RTR) != 0U)
  {
    wire_flags |= BAII_CAN_FLAG_RTR;
  }
  if ((flags & CAN_FRAME_FLAG_FD) != 0U)
  {
    wire_flags |= BAII_CAN_FLAG_FD;
  }
  if ((flags & CAN_FRAME_FLAG_BRS) != 0U)
  {
    wire_flags |= BAII_CAN_FLAG_BRS;
  }
  if ((flags & CAN_FRAME_FLAG_ESI) != 0U)
  {
    wire_flags |= BAII_CAN_FLAG_ESI;
  }

  return wire_flags;
}

static void HandleCommand(void)
{
  uint32_t command_length;

  if ((response_pending != 0U) ||
      (USBD_BULK_GetCommand(command_buffer, sizeof(command_buffer),
                            &command_length) == 0U))
  {
    return;
  }

  if (BAII_Protocol_HandleMessage(command_buffer, command_length,
                                  response_buffer, sizeof(response_buffer),
                                  &response_length) == BAII_PROTOCOL_OK)
  {
    response_pending = 1U;
  }
  else
  {
    Logger_Write("BAII rejected malformed command\r\n");
  }
}

static void SendCanFrames(void)
{
  CAN_SnifferFrame frame;
  BAII_CanRecord record;
  uint32_t payload_length = 0U;
  uint32_t encoded;
  uint32_t total;
  uint8_t data_length;

  while ((CAN_Sniffer_GetBufferedCount() != 0U) &&
         ((ANALYZER_PAYLOAD_SIZE - payload_length) >= ANALYZER_MAX_RECORD))
  {
    if (!CAN_CaptureBuffer_Pop(&frame))
    {
      break;
    }

    memset(&record, 0, sizeof(record));
    data_length = DlcToLength(frame.dlc);
    if (data_length > sizeof(frame.data))
    {
      data_length = sizeof(frame.data);
    }
    if ((frame.flags & CAN_FRAME_FLAG_RTR) != 0U)
    {
      data_length = 0U;
    }

    record.timestamp_us = ExtendTimestamp(frame.timestamp);
    record.can_id = frame.id;
    record.flags = TranslateFlags(frame.flags);
    record.channel = BAII_CAN_CHANNEL_1;
    record.dlc = frame.dlc;
    record.data_length = data_length;
    memcpy(record.data, frame.data, data_length);

    encoded = BAII_Protocol_EncodeCanRecord(
        &record, &tx_buffer[BAII_PROTOCOL_HEADER_SIZE + payload_length],
        ANALYZER_PAYLOAD_SIZE - payload_length);
    if (encoded == 0U)
    {
      continue;
    }
    payload_length += encoded;
  }

  if (payload_length == 0U)
  {
    return;
  }

  total = BAII_Protocol_BuildMessage(BAII_MSG_CAN_DATA, BAII_MSG_FLAG_NONE,
                                     0U, stream_sequence++,
                                     &tx_buffer[BAII_PROTOCOL_HEADER_SIZE],
                                     payload_length,
                                     tx_buffer, sizeof(tx_buffer));
  if (total != 0U)
  {
    /* Terminate the rare max-packet-aligned message with a ZLP so each\n       libusb read ends on one complete BAII frame. */\n    (void)USBD_BULK_TransmitControl(&hUsbDeviceHS, tx_buffer, total);
  }
}

void Analyzer_App_Init(void)
{
  BAII_Protocol_Init();
  response_length = 0U;
  stream_sequence = 0U;
  last_log_ms = HAL_GetTick();
  last_logged_frames = 0U;
  timestamp_epoch = 0U;
  last_timestamp = 0U;
  timestamp_started = 0U;
  response_pending = 0U;
  was_configured = 0U;
}

void Analyzer_App_Task(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t frames;

  if (hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED)
  {
    if (was_configured != 0U)
    {
      was_configured = 0U;
      response_pending = 0U;
      Logger_Write("USB analyzer disconnected\r\n");
    }
    return;
  }

  if (was_configured == 0U)
  {
    was_configured = 1U;
    response_pending = 0U;
    Logger_Write("USB analyzer configured\r\n");
  }

  HandleCommand();

  if (USBD_BULK_TxReady(&hUsbDeviceHS) != 0U)
  {
    if (response_pending != 0U)
    {
      if (USBD_BULK_TransmitControl(&hUsbDeviceHS, response_buffer,
                                    response_length) == USBD_OK)
      {
        response_pending = 0U;
      }
    }
    else
    {
      SendCanFrames();
    }
  }

  if ((uint32_t)(now - last_log_ms) >= 1000U)
  {
    frames = CAN_Sniffer_GetRxCount();
    Logger_Printf("CAN1 500k: %lu fps, buffered=%lu dropped=%lu fifo_lost=%lu\r\n",
                  (unsigned long)(frames - last_logged_frames),
                  (unsigned long)CAN_Sniffer_GetBufferedCount(),
                  (unsigned long)CAN_Sniffer_GetDroppedCount(),
                  (unsigned long)CAN_Sniffer_GetFifoLostEvents());
    last_logged_frames = frames;
    last_log_ms = now;
  }
}
