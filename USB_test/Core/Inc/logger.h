#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

void Logger_Init(UART_HandleTypeDef *uart);
void Logger_SetItmEnabled(uint8_t enabled);
void Logger_Write(const char *message);
void Logger_Printf(const char *format, ...);
uint32_t Logger_GetDroppedCount(void);
uint32_t Logger_GetItmDroppedCount(void);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
