#include "logger.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOGGER_BUFFER_SIZE 256U
#define LOGGER_TIMEOUT_MS  100U

static UART_HandleTypeDef *logger_uart;

void Logger_Init(UART_HandleTypeDef *uart)
{
  logger_uart = uart;
}

void Logger_Write(const char *message)
{
  size_t length;

  if ((logger_uart == NULL) || (message == NULL))
  {
    return;
  }

  length = strlen(message);
  if (length > UINT16_MAX)
  {
    length = UINT16_MAX;
  }

  (void)HAL_UART_Transmit(logger_uart, (uint8_t *)message,
                          (uint16_t)length, LOGGER_TIMEOUT_MS);
}

void Logger_Printf(const char *format, ...)
{
  char buffer[LOGGER_BUFFER_SIZE];
  int length;
  va_list args;

  if ((logger_uart == NULL) || (format == NULL))
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

  (void)HAL_UART_Transmit(logger_uart, (uint8_t *)buffer,
                          (uint16_t)length, LOGGER_TIMEOUT_MS);
}
