#include "logger.h"

#include "SEGGER_RTT.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOGGER_BUFFER_SIZE   320U
#define LOGGER_QUEUE_CAPACITY 4U

_Static_assert((LOGGER_QUEUE_CAPACITY & (LOGGER_QUEUE_CAPACITY - 1U)) == 0U,
               "logger queue capacity must be a power of two");

#if defined(__GNUC__)
#define LOGGER_DMA_ALIGNED __attribute__((aligned(32)))
#else
#define LOGGER_DMA_ALIGNED
#endif

static UART_HandleTypeDef *logger_uart;
static uint8_t logger_buffers[LOGGER_QUEUE_CAPACITY][LOGGER_BUFFER_SIZE]
    LOGGER_DMA_ALIGNED;
static uint16_t logger_lengths[LOGGER_QUEUE_CAPACITY];
static volatile uint32_t logger_write_sequence;
static volatile uint32_t logger_read_sequence;
static volatile uint32_t logger_dropped_messages;
static volatile uint32_t logger_rtt_dropped_messages;
static volatile uint8_t logger_tx_busy;

static void Logger_StartNext(void)
{
  HAL_StatusTypeDef status;
  uint32_t primask;
  uint32_t index;
  uint16_t length;

  if (logger_uart == NULL)
  {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  if ((logger_tx_busy != 0U) ||
      (logger_read_sequence == logger_write_sequence))
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return;
  }

  index = logger_read_sequence & (LOGGER_QUEUE_CAPACITY - 1U);
  length = logger_lengths[index];
  logger_tx_busy = 1U;

  if (primask == 0U)
  {
    __enable_irq();
  }

  status = HAL_UART_Transmit_DMA(logger_uart, logger_buffers[index], length);
  if (status != HAL_OK)
  {
    primask = __get_PRIMASK();
    __disable_irq();
    logger_tx_busy = 0U;
    logger_read_sequence++;
    logger_dropped_messages++;
    if (primask == 0U)
    {
      __enable_irq();
    }
    Logger_StartNext();
  }
}

static void Logger_Queue(const uint8_t *message, size_t length)
{
  uint32_t write_sequence;
  uint32_t index;
  unsigned rtt_written;

  if ((logger_uart == NULL) || (message == NULL) || (length == 0U))
  {
    return;
  }
  if (length > LOGGER_BUFFER_SIZE)
  {
    length = LOGGER_BUFFER_SIZE;
  }

  rtt_written = SEGGER_RTT_Write(0U, message, (unsigned)length);
  if (rtt_written != (unsigned)length)
  {
    logger_rtt_dropped_messages++;
  }

  write_sequence = logger_write_sequence;
  if ((write_sequence - logger_read_sequence) >= LOGGER_QUEUE_CAPACITY)
  {
    logger_dropped_messages++;
    return;
  }

  index = write_sequence & (LOGGER_QUEUE_CAPACITY - 1U);
  (void)memcpy(logger_buffers[index], message, length);
  logger_lengths[index] = (uint16_t)length;

  /* Publish the completely filled slot only after its contents and length are
   * visible to the DMA-completion interrupt. */
  __DMB();
  logger_write_sequence = write_sequence + 1U;
  Logger_StartNext();
}

static void Logger_CompleteActive(UART_HandleTypeDef *uart, uint8_t failed)
{
  uint32_t primask;

  if ((uart == NULL) || (uart != logger_uart))
  {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (logger_tx_busy != 0U)
  {
    logger_tx_busy = 0U;
    logger_read_sequence++;
    if (failed != 0U)
    {
      logger_dropped_messages++;
    }
  }
  if (primask == 0U)
  {
    __enable_irq();
  }

  Logger_StartNext();
}

void Logger_Init(UART_HandleTypeDef *uart)
{
  SEGGER_RTT_Init();
  (void)SEGGER_RTT_SetFlagsUpBuffer(0U, SEGGER_RTT_MODE_NO_BLOCK_SKIP);

  logger_uart = uart;
  logger_write_sequence = 0U;
  logger_read_sequence = 0U;
  logger_dropped_messages = 0U;
  logger_rtt_dropped_messages = 0U;
  logger_tx_busy = 0U;
}

void Logger_Write(const char *message)
{
  size_t length;

  if (message == NULL)
  {
    return;
  }

  length = strlen(message);
  Logger_Queue((const uint8_t *)message, length);
}

void Logger_Printf(const char *format, ...)
{
  char buffer[LOGGER_BUFFER_SIZE];
  int length;
  va_list args;

  if (format == NULL)
  {
    return;
  }

  va_start(args, format);
  length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (length <= 0)
  {
    return;
  }
  if ((size_t)length >= sizeof(buffer))
  {
    length = (int)sizeof(buffer) - 1;
  }

  Logger_Queue((const uint8_t *)buffer, (size_t)length);
}

uint32_t Logger_GetDroppedCount(void)
{
  return logger_dropped_messages;
}

uint32_t Logger_GetRttDroppedCount(void)
{
  return logger_rtt_dropped_messages;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
  Logger_CompleteActive(uart, 0U);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  Logger_CompleteActive(uart, 1U);
}
