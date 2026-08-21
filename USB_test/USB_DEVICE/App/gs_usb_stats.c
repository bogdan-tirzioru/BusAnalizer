#include "gs_usb_stats.h"

#include "stm32h7xx_hal.h"

#include <stddef.h>

static volatile GS_USB_PerfStats interval_stats;
static volatile uint32_t last_completion_cycle;
static volatile uint8_t completion_clock_valid;

void GS_USB_Stats_Init(void)
{
  uint32_t primask;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  primask = __get_PRIMASK();
  __disable_irq();
  interval_stats.transfers_started = 0U;
  interval_stats.transfers_completed = 0U;
  interval_stats.bytes_completed = 0U;
  interval_stats.endpoint_idle_events = 0U;
  interval_stats.max_completion_gap_cycles = 0U;
  interval_stats.fifo_preload_hits = 0U;
  interval_stats.fifo_preload_fallbacks = 0U;
  last_completion_cycle = 0U;
  completion_clock_valid = 0U;
  if (primask == 0U)
  {
    __enable_irq();
  }
}

void GS_USB_Stats_RecordTransferStarted(void)
{
  interval_stats.transfers_started++;
}

void GS_USB_Stats_RecordTransferCompleted(uint32_t length)
{
  uint32_t now;
  uint32_t gap;

  now = DWT->CYCCNT;
  if (completion_clock_valid != 0U)
  {
    gap = now - last_completion_cycle;
    if (gap > interval_stats.max_completion_gap_cycles)
    {
      interval_stats.max_completion_gap_cycles = gap;
    }
  }
  last_completion_cycle = now;
  completion_clock_valid = 1U;

  interval_stats.transfers_completed++;
  interval_stats.bytes_completed += length;
}

void GS_USB_Stats_RecordEndpointIdle(void)
{
  interval_stats.endpoint_idle_events++;
  completion_clock_valid = 0U;
}

void GS_USB_Stats_RecordFifoPreload(uint8_t preloaded)
{
  if (preloaded != 0U)
  {
    interval_stats.fifo_preload_hits++;
  }
  else
  {
    interval_stats.fifo_preload_fallbacks++;
  }
}

void GS_USB_Stats_SnapshotAndReset(GS_USB_PerfStats *stats)
{
  uint32_t primask;

  if (stats == NULL)
  {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  stats->transfers_started = interval_stats.transfers_started;
  stats->transfers_completed = interval_stats.transfers_completed;
  stats->bytes_completed = interval_stats.bytes_completed;
  stats->endpoint_idle_events = interval_stats.endpoint_idle_events;
  stats->max_completion_gap_cycles =
      interval_stats.max_completion_gap_cycles;
  stats->fifo_preload_hits = interval_stats.fifo_preload_hits;
  stats->fifo_preload_fallbacks = interval_stats.fifo_preload_fallbacks;

  interval_stats.transfers_started = 0U;
  interval_stats.transfers_completed = 0U;
  interval_stats.bytes_completed = 0U;
  interval_stats.endpoint_idle_events = 0U;
  interval_stats.max_completion_gap_cycles = 0U;
  interval_stats.fifo_preload_hits = 0U;
  interval_stats.fifo_preload_fallbacks = 0U;
  if (primask == 0U)
  {
    __enable_irq();
  }
}

uint32_t GS_USB_Stats_CyclesToMicroseconds(uint32_t cycles)
{
  if (SystemCoreClock == 0U)
  {
    return 0U;
  }

  return (uint32_t)((((uint64_t)cycles * 1000000ULL) +
                     ((uint64_t)SystemCoreClock / 2ULL)) /
                    (uint64_t)SystemCoreClock);
}
