#include "can_capture_buffer.h"

#include <stddef.h>

_Static_assert(sizeof(CAN_SnifferFrame) == 72U,
               "CAN_SnifferFrame CAN FD ABI mismatch");

static CAN_SnifferFrame capture_buffer[CAN_CAPTURE_CAPACITY];
static volatile uint32_t write_sequence;
static volatile uint32_t read_sequence;
static volatile uint32_t dropped_frames;
static uint32_t high_watermark;

void CAN_CaptureBuffer_Init(void)
{
  CAN_CaptureBuffer_Clear();
  dropped_frames = 0U;
}

CAN_SnifferFrame *CAN_CaptureBuffer_BeginPush(void)
{
  if ((write_sequence - read_sequence) >= CAN_CAPTURE_CAPACITY)
  {
    return NULL;
  }

  return &capture_buffer[write_sequence & (CAN_CAPTURE_CAPACITY - 1U)];
}

void CAN_CaptureBuffer_CommitPush(void)
{
  uint32_t count;

  write_sequence++;
  count = write_sequence - read_sequence;
  if (count > high_watermark)
  {
    high_watermark = count;
  }
}

void CAN_CaptureBuffer_RecordDrop(void)
{
  dropped_frames++;
}

const CAN_SnifferFrame *CAN_CaptureBuffer_Peek(void)
{
  if (read_sequence == write_sequence)
  {
    return NULL;
  }

  return &capture_buffer[read_sequence & (CAN_CAPTURE_CAPACITY - 1U)];
}

void CAN_CaptureBuffer_Release(void)
{
  read_sequence++;
}

void CAN_CaptureBuffer_Clear(void)
{
  write_sequence = 0U;
  read_sequence = 0U;
  high_watermark = 0U;
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

uint32_t CAN_CaptureBuffer_GetAndResetHighWater(void)
{
  uint32_t high_water = high_watermark;

  high_watermark = CAN_CaptureBuffer_GetCount();
  return high_water;
}
