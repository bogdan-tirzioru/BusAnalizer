#ifndef CONSOLE_H
#define CONSOLE_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Drop-in non-blocking replacement for HAL_UART_Transmit().
 *
 * The message is copied into an internal static queue and USART transmission
 * continues in the background using the TX DMA configured by CubeMX.
 * The timeout argument is kept only to preserve the HAL call signature.
 */
HAL_StatusTypeDef Console_UART_Transmit(
        UART_HandleTypeDef *huart,
        const uint8_t *data,
        uint16_t size,
        uint32_t timeout);

uint32_t Console_GetDroppedMessageCount(void);
uint32_t Console_GetErrorCount(void);

/*
 * Application code can continue using HAL_UART_Transmit().  Any source file
 * that includes main.h receives this redirect after the STM32 HAL declarations
 * have already been parsed.
 */
#define HAL_UART_Transmit(huart, data, size, timeout) \
    Console_UART_Transmit((huart), (const uint8_t *)(data), (size), (timeout))

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_H */
