#ifndef CAN_SNIFFER_H
#define CAN_SNIFFER_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_SNIFFER_FDCAN_CLOCK_HZ 80000000UL
#define CAN_SNIFFER_BITRATE_250K    250000UL
#define CAN_SNIFFER_BITRATE_500K    500000UL
#define CAN_SNIFFER_BITRATE_1M      1000000UL
#define CAN_SNIFFER_DATA_BITRATE_5M 5000000UL
#define CAN_SNIFFER_CHANNEL_COUNT   2U

void CAN_Sniffer_Init(void);
void CAN_Sniffer_Process(void);

/* Legacy channel-0 wrappers kept for the existing application code. */
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
bool CAN_Sniffer_SetDataBitTiming(uint32_t prop_seg,
                                  uint32_t phase_seg1,
                                  uint32_t phase_seg2,
                                  uint32_t sjw,
                                  uint32_t brp);
bool CAN_Sniffer_StartListenOnly(void);
bool CAN_Sniffer_StartListenOnlyMode(bool can_fd);
void CAN_Sniffer_Reset(void);

/* Per-channel API used by the gs_usb control plane. */
bool CAN_Sniffer_IsChannelRunning(uint8_t channel);
bool CAN_Sniffer_SetBitTimingChannel(uint8_t channel,
                                     uint32_t prop_seg,
                                     uint32_t phase_seg1,
                                     uint32_t phase_seg2,
                                     uint32_t sjw,
                                     uint32_t brp);
bool CAN_Sniffer_SetDataBitTimingChannel(uint8_t channel,
                                         uint32_t prop_seg,
                                         uint32_t phase_seg1,
                                         uint32_t phase_seg2,
                                         uint32_t sjw,
                                         uint32_t brp);
bool CAN_Sniffer_StartListenOnlyModeChannel(uint8_t channel, bool can_fd);
void CAN_Sniffer_ResetChannel(uint8_t channel);
void CAN_Sniffer_ResetAll(void);

uint32_t CAN_Sniffer_GetBitrate(void);
uint32_t CAN_Sniffer_GetDataBitrate(void);

/* Aggregate RX count is intentionally used by CPU-load accounting. */
uint32_t CAN_Sniffer_GetRxCount(void);
uint32_t CAN_Sniffer_GetChannelRxCount(uint8_t channel);
uint32_t CAN_Sniffer_GetChannelErrorCount(uint8_t channel);
uint32_t CAN_Sniffer_GetChannelFifoLostEvents(uint8_t channel);
uint32_t CAN_Sniffer_GetChannelMaxFifoFill(uint8_t channel);
uint32_t CAN_Sniffer_GetErrorCount(void);
uint32_t CAN_Sniffer_GetBufferedCount(void);
uint32_t CAN_Sniffer_GetDroppedCount(void);
uint32_t CAN_Sniffer_GetFifoLostEvents(void);
uint32_t CAN_Sniffer_GetMaxFifoFill(void);

#endif
