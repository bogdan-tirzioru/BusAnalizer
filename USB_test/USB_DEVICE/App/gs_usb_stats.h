#ifndef GS_USB_STATS_H
#define GS_USB_STATS_H

#include <stdint.h>

typedef struct
{
  uint32_t transfers_started;
  uint32_t transfers_completed;
  uint32_t bytes_completed;
  uint32_t endpoint_idle_events;
  uint32_t max_completion_gap_cycles;
  uint32_t fifo_preload_hits;
  uint32_t fifo_preload_fallbacks;
} GS_USB_PerfStats;

void GS_USB_Stats_Init(void);
void GS_USB_Stats_RecordTransferStarted(void);
void GS_USB_Stats_RecordTransferCompleted(uint32_t length);
void GS_USB_Stats_RecordEndpointIdle(void);
void GS_USB_Stats_RecordFifoPreload(uint8_t preloaded);
void GS_USB_Stats_SnapshotAndReset(GS_USB_PerfStats *stats);
uint32_t GS_USB_Stats_CyclesToMicroseconds(uint32_t cycles);

#endif
