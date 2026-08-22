#include "logger.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOGGER_BUFFER_SIZE        320U
#define LOGGER_QUEUE_CAPACITY     4U
#define LOGGER_ITM_BUFFER_SIZE    1024U
#define LOGGER_ITM_DRAIN_BUDGET   16U

_Static_assert((LOGGER_QUEUE_CAPACITY & (LOGGER_QUEUE_CAPACITY - 1U)) == 0U,
               "logger queue capacity must be a power of two");
_Static_assert((LOGGER_ITM_BUFFER_SIZE & (LOGGER_ITM_BUFFER_SIZE - 1U)) == 0U,
               "logger ITM buffer size must be a power of two");

#if defined(__GNUC__)
#define LOGGER_DMA_ALIGNED __attribute__((aligned(32)))
#else
#define LOGGER_DMA_ALIGNED
#endif

static UART_HandleTypeDef *logger_uart;
static uint8_t logger_buffers[LOGGER_QUEUE_CAPACITY][LOGGER_BUFFER_SIZE]
    LOGGER_DMA_ALIGNED;
static uint16_t logger_lengths[LOGGER_QUEUE_CAPACITY];
static uint8_t logger_itm_buffer[LOGGER_ITM_BUFFER_SIZE];
static volatile uint32_t logger_write_sequence;
static volatile uint32_t logger_read_sequence;
static volatile uint32_t logger_dropped_messages;
static volatile uint32_t logger_itm_dropped_characters;
static volatile uint32_t logger_itm_write_sequence;
static volatile uint32_t logger_itm_read_sequence;
static volatile uint8_t logger_tx_busy;
static volatile uint8_t logger_itm_enabled;

static uint8_t Logger_ItmTraceActive(void)
{
  return ((logger_itm_enabled != 0U) &&
          ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U) &&
          ((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0U) &&
          ((ITM->TER & 1UL) != 0U)) ? 1U : 0U;
}

static void Logger_WriteItm(const uint8_t *message, size_t length)
{
  uint32_t write_sequence;
  uint32_t index;
  size_t first_length;

  if (Logger_ItmTraceActive() == 0U)
  {
    return;
  }

  write_sequence = logger_itm_write_sequence;
  if (length >
      (size_t)(LOGGER_ITM_BUFFER_SIZE -
               (write_sequence - logger_itm_read_sequence)))
  {
    /* Drop complete messages so a full buffer cannot create broken lines. */
    logger_itm_dropped_characters += (uint32_t)length;
    return;
  }

  index = write_sequence & (LOGGER_ITM_BUFFER_SIZE - 1U);
  first_length = LOGGER_ITM_BUFFER_SIZE - index;
  if (first_length > length)
  {
    first_length = length;
  }

  (void)memcpy(&logger_itm_buffer[index], message, first_length);
  if (length > first_length)
  {
    (void)memcpy(logger_itm_buffer, &message[first_length],
                 length - first_length);
  }

  __DMB();
  logger_itm_write_sequence = write_sequence + (uint32_t)length;
}

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

  if ((logger_uart == NULL) || (message == NULL) || (length == 0U))
  {
    return;
  }
  if (length > LOGGER_BUFFER_SIZE)
  {
    length = LOGGER_BUFFER_SIZE;
  }

  Logger_WriteItm(message, length);

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
  logger_uart = uart;
  logger_write_sequence = 0U;
  logger_read_sequence = 0U;
  logger_dropped_messages = 0U;
  logger_itm_dropped_characters = 0U;
  logger_itm_write_sequence = 0U;
  logger_itm_read_sequence = 0U;
  logger_tx_busy = 0U;
  logger_itm_enabled = 0U;
}

void Logger_SetItmEnabled(uint8_t enabled)
{
  logger_itm_enabled = (enabled != 0U) ? 1U : 0U;
  if (logger_itm_enabled == 0U)
  {
    logger_itm_read_sequence = logger_itm_write_sequence;
  }
}

void Logger_Process(void)
{
  uint32_t budget = LOGGER_ITM_DRAIN_BUDGET;

  if (Logger_ItmTraceActive() == 0U)
  {
    return;
  }

  while ((budget != 0U) &&
         (logger_itm_read_sequence != logger_itm_write_sequence))
  {
    if (ITM->PORT[0U].u32 == 0U)
    {
      return;
    }

    (void)ITM_SendChar(
        (uint32_t)logger_itm_buffer[logger_itm_read_sequence &
                                    (LOGGER_ITM_BUFFER_SIZE - 1U)]);
    logger_itm_read_sequence++;
    budget--;
  }
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

uint32_t Logger_GetItmDroppedCount(void)
{
  return logger_itm_dropped_characters;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
  Logger_CompleteActive(uart, 0U);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  Logger_CompleteActive(uart, 1U);
}
