#ifndef CAN_CAPTURE_BUFFER_H
#define CAN_CAPTURE_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "gs_usb_frame.h"

#define CAN_CAPTURE_CAPACITY 4096U

/* Store the final gs_usb wire record so the USB service path only has to
 * dequeue into an available endpoint buffer and select 20 or 76 bytes. */

void CAN_CaptureBuffer_Init(void);
bool CAN_CaptureBuffer_Push(const GS_USB_HostFrame *frame);
bool CAN_CaptureBuffer_Pop(GS_USB_HostFrame *frame);
void CAN_CaptureBuffer_Clear(void);
uint32_t CAN_CaptureBuffer_GetCount(void);
uint32_t CAN_CaptureBuffer_GetFree(void);
uint32_t CAN_CaptureBuffer_GetDropped(void);

#endif
