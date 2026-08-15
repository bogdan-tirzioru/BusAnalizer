#ifndef STRESS_CONTROL_H
#define STRESS_CONTROL_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Runtime CAN-bus stress control for the generator.
 *
 * 100% keeps the original behaviour: FDCAN1 is filled as fast as possible.
 * 0..99% paces the 8-byte standard frames against an approximately
 * 4300 frame/s full-load reference at 500 kbit/s.
 *
 * Runtime console command:
 *     <0..100><Enter>
 * Examples:
 *     25<Enter>
 *     50<Enter>
 *     100<Enter>
 *
 * '?' prints a short help line.
 */
uint32_t StressControl_GetLevelPercent(void);
uint32_t StressControl_GetTargetFramesPerSecond(void);

uint32_t StressControl_FDCAN_GetTxFifoFreeLevel(
        FDCAN_HandleTypeDef *hfdcan);

HAL_StatusTypeDef StressControl_FDCAN_AddMessageToTxFifoQ(
        FDCAN_HandleTypeDef *hfdcan,
        const FDCAN_TxHeaderTypeDef *pTxHeader,
        const uint8_t *pTxData);

/*
 * main.h includes this file only after STM32 HAL declarations have been
 * parsed. Application code therefore keeps its normal HAL calls while the
 * generator TX path is transparently paced by this module.
 */
#define HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) \
    StressControl_FDCAN_GetTxFifoFreeLevel((hfdcan))

#define HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, pTxHeader, pTxData) \
    StressControl_FDCAN_AddMessageToTxFifoQ( \
        (hfdcan), (pTxHeader), (pTxData))

#ifdef __cplusplus
}
#endif

#endif /* STRESS_CONTROL_H */
