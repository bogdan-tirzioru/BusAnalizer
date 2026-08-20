#ifndef CAN_CAPTURE_BUFFER_H
#define CAN_CAPTURE_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_CAPTURE_CAPACITY 4096U

typedef struct
{
  uint32_t id;
  uint16_t timestamp;
  uint8_t dlc;
  uint8_t flags;
  uint8_t data[64];
} CAN_SnifferFrame;

enum
{
  CAN_FRAME_FLAG_EXTENDED = (1U << 0),
  CAN_FRAME_FLAG_RTR      = (1U << 1),
  CAN_FRAME_FLAG_FD       = (1U << 2),
  CAN_FRAME_FLAG_BRS      = (1U << 3),
  CAN_FRAME_FLAG_ESI      = (1U << 4)
};

void CAN_CaptureBuffer_Init(void);
bool CAN_CaptureBuffer_Push(const CAN_SnifferFrame *frame);
bool CAN_CaptureBuffer_Pop(CAN_SnifferFrame *frame);
void CAN_CaptureBuffer_Clear(void);
uint32_t CAN_CaptureBuffer_GetCount(void);
uint32_t CAN_CaptureBuffer_GetFree(void);
uint32_t CAN_CaptureBuffer_GetDropped(void);

#endif
