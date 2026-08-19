#ifndef CAN_SNIFFER_H
#define CAN_SNIFFER_H

#include <stdbool.h>
#include <stdint.h>

void CAN_Sniffer_Init(void);
void CAN_Sniffer_Process(void);
void CAN_Sniffer_Start(void);
void CAN_Sniffer_Stop(void);
void CAN_Sniffer_Clear(void);
bool CAN_Sniffer_IsRunning(void);

uint32_t CAN_Sniffer_GetRxCount(void);
uint32_t CAN_Sniffer_GetErrorCount(void);
uint32_t CAN_Sniffer_GetBufferedCount(void);
uint32_t CAN_Sniffer_GetDroppedCount(void);
uint32_t CAN_Sniffer_GetFifoLostEvents(void);
uint32_t CAN_Sniffer_GetMaxFifoFill(void);

#endif
