#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

void Logger_Init(UART_HandleTypeDef *uart);
void Logger_Write(const char *message);
void Logger_Printf(const char *format, ...);
uint32_t Logger_GetDroppedCount(void);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
