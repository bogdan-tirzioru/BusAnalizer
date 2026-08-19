#include "can_capture_buffer.h"

#include <string.h>

_Static_assert(sizeof(CAN_SnifferFrame) == 16U,
               "CAN_SnifferFrame must remain a compact 16-byte record");

static CAN_SnifferFrame capture_buffer[CAN_CAPTURE_CAPACITY];
static volatile uint32_t write_sequence;
static volatile uint32_t read_sequence;
static volatile uint32_t dropped_frames;

void CAN_CaptureBuffer_Init(void)
{
  CAN_CaptureBuffer_Clear();
  dropped_frames = 0U;
}

bool CAN_CaptureBuffer_Push(const CAN_SnifferFrame *frame)
{
  if ((frame == NULL) || ((write_sequence - read_sequence) >= CAN_CAPTURE_CAPACITY))
  {
    if (frame != NULL)
    {
      dropped_frames++;
    }
    return false;
  }

  capture_buffer[write_sequence & (CAN_CAPTURE_CAPACITY - 1U)] = *frame;
  __DMB();
  write_sequence++;
  return true;
}

bool CAN_CaptureBuffer_Pop(CAN_SnifferFrame *frame)
{
  if ((frame == NULL) || (read_sequence == write_sequence))
  {
    return false;
  }

  *frame = capture_buffer[read_sequence & (CAN_CAPTURE_CAPACITY - 1U)];
  __DMB();
  read_sequence++;
  return true;
}

void CAN_CaptureBuffer_Clear(void)
{
  write_sequence = 0U;
  read_sequence = 0U;
}

uint32_t CAN_CaptureBuffer_GetCount(void)
{
  return write_sequence - read_sequence;
}

uint32_t CAN_CaptureBuffer_GetFree(void)
{
  return CAN_CAPTURE_CAPACITY - CAN_CaptureBuffer_GetCount();
}

uint32_t CAN_CaptureBuffer_GetDropped(void)
{
  return dropped_frames;
}
