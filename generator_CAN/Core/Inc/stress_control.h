#ifndef STRESS_CONTROL_H
#define STRESS_CONTROL_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Runtime traffic and CAN-mode control for the generator.
 *
 * Stress command:
 *     <0..100><Enter>
 *     0 pauses traffic, 100 keeps the original maximum-throughput behaviour.
 *
 * CAN commands:
 *     can classic 500k
 *     can classic 1m
 *     can fd 500k 2m
 *     can fd 500k 5m
 *     can fd 1m 2m
 *     can fd 1m 5m
 *     can status
 *
 * '?' prints the command list.
 */
uint32_t StressControl_GetLevelPercent(void);
uint32_t StressControl_GetTargetFramesPerSecond(void);

HAL_StatusTypeDef StressControl_FDCAN_Start(
        FDCAN_HandleTypeDef *hfdcan);

uint32_t StressControl_FDCAN_GetTxFifoFreeLevel(
        FDCAN_HandleTypeDef *hfdcan);

HAL_StatusTypeDef StressControl_FDCAN_AddMessageToTxFifoQ(
        FDCAN_HandleTypeDef *hfdcan,
        const FDCAN_TxHeaderTypeDef *pTxHeader,
        const uint8_t *pTxData);

/*
 * main.h includes this file only after STM32 HAL declarations have been
 * parsed. Application code therefore keeps its normal HAL calls while the
 * generator TX path is transparently paced by this module. FDCAN1 start is
 * also wrapped so transmitter delay compensation is enabled before CAN FD+BRS
 * traffic begins. Classic CAN starts without TDC.
 */
#define HAL_FDCAN_Start(hfdcan) \
    StressControl_FDCAN_Start((hfdcan))

#define HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) \
    StressControl_FDCAN_GetTxFifoFreeLevel((hfdcan))

#define HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, pTxHeader, pTxData) \
    StressControl_FDCAN_AddMessageToTxFifoQ( \
        (hfdcan), (pTxHeader), (pTxData))

#ifdef __cplusplus
}
#endif

#endif /* STRESS_CONTROL_H */