#ifndef BAII_PROTOCOL_H
#define BAII_PROTOCOL_H

#include <stdint.h>

#define BAII_PROTOCOL_VERSION_MAJOR       0U
#define BAII_PROTOCOL_VERSION_MINOR       1U
#define BAII_PROTOCOL_HEADER_SIZE         20U
#define BAII_PROTOCOL_COMMAND_HEADER_SIZE 4U
#define BAII_PROTOCOL_MAX_CONTROL_PAYLOAD 128U
#define BAII_PROTOCOL_CAN_DATA_MAX        64U
#define BAII_PROTOCOL_CAN_RECORD_BASE     18U

#define BAII_MSG_FLAG_NONE                0U

#define BAII_CAP_RTC                      (1UL << 0)
#define BAII_CAP_CAN_CONFIG               (1UL << 1)
#define BAII_CAP_CAPTURE_STATUS           (1UL << 2)
#define BAII_CAP_HYPERRAM                 (1UL << 3)
#define BAII_CAP_CAN_FD_WIRE_FORMAT       (1UL << 4)
#define BAII_CAP_STREAMING                (1UL << 5)

#define BAII_CAN_FLAG_EXT                 (1U << 0)
#define BAII_CAN_FLAG_RTR                 (1U << 1)
#define BAII_CAN_FLAG_FD                  (1U << 2)
#define BAII_CAN_FLAG_BRS                 (1U << 3)
#define BAII_CAN_FLAG_ESI                 (1U << 4)
#define BAII_CAN_FLAG_TX                  (1U << 5)
#define BAII_CAN_FLAG_ERROR               (1U << 6)

#define BAII_CAN_CHANNEL_1                1U
#define BAII_CAN_CHANNEL_2                2U
#define BAII_CAN_MODE_NORMAL              0U
#define BAII_CAN_MODE_LISTEN_ONLY         1U
#define BAII_CAN_FORMAT_CLASSIC           0U
#define BAII_CAN_FORMAT_FD_NO_BRS         1U
#define BAII_CAN_FORMAT_FD_BRS            2U

typedef enum
{
  BAII_MSG_COMMAND  = 0x01,
  BAII_MSG_RESPONSE = 0x02,
  BAII_MSG_EVENT    = 0x03,
  BAII_MSG_CAN_DATA = 0x10
} BAII_MessageType;

typedef enum
{
  BAII_CMD_GET_INFO           = 0x0001,
  BAII_CMD_GET_STATUS         = 0x0002,
  BAII_CMD_GET_RTC_TIME       = 0x0010,
  BAII_CMD_SET_RTC_TIME       = 0x0011,
  BAII_CMD_GET_CAN_CONFIG     = 0x0020,
  BAII_CMD_SET_CAN_CONFIG     = 0x0021,
  BAII_CMD_CAPTURE_START      = 0x0030,
  BAII_CMD_CAPTURE_STOP       = 0x0031,
  BAII_CMD_CAPTURE_CLEAR      = 0x0032,
  BAII_CMD_GET_CAPTURE_STATUS = 0x0033
} BAII_CommandId;

typedef enum
{
  BAII_STATUS_OK              = 0x0000,
  BAII_STATUS_BAD_LENGTH      = 0x0001,
  BAII_STATUS_UNKNOWN_COMMAND = 0x0002,
  BAII_STATUS_INVALID_PARAM   = 0x0003,
  BAII_STATUS_BUSY            = 0x0004,
  BAII_STATUS_HAL_ERROR       = 0x0005,
  BAII_STATUS_NOT_SUPPORTED   = 0x0006,
  BAII_STATUS_INTERNAL_ERROR  = 0x0007
} BAII_StatusCode;

typedef enum
{
  BAII_PROTOCOL_OK = 0,
  BAII_PROTOCOL_BAD_ARGUMENT,
  BAII_PROTOCOL_BAD_MAGIC,
  BAII_PROTOCOL_UNSUPPORTED_VERSION,
  BAII_PROTOCOL_BAD_MESSAGE_TYPE,
  BAII_PROTOCOL_BAD_LENGTH,
  BAII_PROTOCOL_OUTPUT_TOO_SMALL
} BAII_ProtocolResult;

typedef struct
{
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t message_type;
  uint8_t flags;
  uint32_t transaction_id;
  uint32_t sequence;
  uint32_t payload_length;
} BAII_MessageHeader;

typedef struct
{
  uint8_t firmware_major;
  uint8_t firmware_minor;
  uint16_t firmware_patch;
  uint32_t capabilities;
  uint32_t fdcan_clock_hz;
  uint32_t hyperram_size_bytes;
  uint32_t device_id;
  uint8_t can_channel_count;
  uint8_t rtc_valid;
} BAII_DeviceInfo;

typedef struct
{
  uint32_t uptime_ms;
  uint32_t can_rx_frames;
  uint32_t sram_buffered_frames;
  uint32_t sram_dropped_frames;
  uint32_t fdcan_fifo_lost_events;
  uint32_t hyperram_stored_frames;
  uint32_t hyperram_write_errors;
  uint32_t hyperram_lost_frames;
  uint32_t hyperram_wrap_count;
} BAII_DeviceStatus;

typedef struct
{
  uint8_t channel;
  uint8_t mode;
  uint8_t frame_format;
  uint8_t reserved;
  uint32_t fdcan_clock_hz;
  uint32_t nominal_bitrate;
  uint32_t data_bitrate;
  uint16_t nominal_sample_point_permille;
  uint16_t data_sample_point_permille;
  uint16_t nominal_prescaler;
  uint16_t nominal_time_seg1;
  uint16_t nominal_time_seg2;
  uint16_t nominal_sjw;
} BAII_CanConfig;

typedef struct
{
  uint8_t enabled;
  uint32_t buffered_frames;
  uint32_t dropped_frames;
  uint32_t fifo_lost_events;
  uint32_t received_frames;
} BAII_CaptureStatus;

typedef struct
{
  uint64_t timestamp_us;
  uint32_t can_id;
  uint16_t flags;
  uint8_t channel;
  uint8_t dlc;
  uint8_t data_length;
  uint8_t data[BAII_PROTOCOL_CAN_DATA_MAX];
} BAII_CanRecord;

void BAII_Protocol_Init(void);
BAII_ProtocolResult BAII_Protocol_DecodeHeader(const uint8_t *message,
                                               uint32_t message_length,
                                               BAII_MessageHeader *header);
uint32_t BAII_Protocol_BuildMessage(uint8_t message_type, uint8_t flags,
                                    uint32_t transaction_id, uint32_t sequence,
                                    const uint8_t *payload, uint32_t payload_length,
                                    uint8_t *output, uint32_t output_capacity);
BAII_ProtocolResult BAII_Protocol_HandleMessage(const uint8_t *request,
                                                uint32_t request_length,
                                                uint8_t *response,
                                                uint32_t response_capacity,
                                                uint32_t *response_length);
uint32_t BAII_Protocol_EncodeCanRecord(const BAII_CanRecord *record,
                                       uint8_t *output,
                                       uint32_t output_capacity);

#endif
