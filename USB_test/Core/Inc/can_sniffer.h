#ifndef CAN_SNIFFER_H
#define CAN_SNIFFER_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_SNIFFER_BITRATE_250K 250000UL
#define CAN_SNIFFER_BITRATE_500K 500000UL

void CAN_Sniffer_Init(void);
void CAN_Sniffer_Process(void);
void CAN_Sniffer_Start(void);
void CAN_Sniffer_Stop(void);
void CAN_Sniffer_Clear(void);
bool CAN_Sniffer_IsRunning(void);
bool CAN_Sniffer_SetBitrate(uint32_t bitrate);
bool CAN_Sniffer_SetBitTiming(uint32_t prop_seg,
                              uint32_t phase_seg1,
                              uint32_t phase_seg2,
                              uint32_t sjw,
                              uint32_t brp);
bool CAN_Sniffer_StartListenOnly(void);
void CAN_Sniffer_Reset(void);
uint32_t CAN_Sniffer_GetBitrate(void);

uint32_t CAN_Sniffer_GetRxCount(void);
uint32_t CAN_Sniffer_GetErrorCount(void);
uint32_t CAN_Sniffer_GetBufferedCount(void);
uint32_t CAN_Sniffer_GetDroppedCount(void);
uint32_t CAN_Sniffer_GetFifoLostEvents(void);
uint32_t CAN_Sniffer_GetMaxFifoFill(void);

#endif
