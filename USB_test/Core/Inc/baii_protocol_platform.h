#ifndef BAII_PROTOCOL_PLATFORM_H
#define BAII_PROTOCOL_PLATFORM_H

#include "baii_protocol.h"

BAII_StatusCode BAII_Platform_GetInfo(BAII_DeviceInfo *info);
BAII_StatusCode BAII_Platform_GetStatus(BAII_DeviceStatus *status);
BAII_StatusCode BAII_Platform_GetRtcTime(uint64_t *unix_time_us);
BAII_StatusCode BAII_Platform_SetRtcTime(uint64_t unix_time_us);
BAII_StatusCode BAII_Platform_GetCanConfig(uint8_t channel,
                                           BAII_CanConfig *config);
BAII_StatusCode BAII_Platform_SetCanConfig(const BAII_CanConfig *requested,
                                           BAII_CanConfig *applied);
BAII_StatusCode BAII_Platform_CaptureStart(void);
BAII_StatusCode BAII_Platform_CaptureStop(void);
BAII_StatusCode BAII_Platform_CaptureClear(void);
BAII_StatusCode BAII_Platform_GetCaptureStatus(BAII_CaptureStatus *status);

#endif
