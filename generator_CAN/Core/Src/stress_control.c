#include "stm32h7xx_hal.h"
#include "console.h"

#include <stdio.h>
#include <stdint.h>

extern FDCAN_HandleTypeDef hfdcan1;
extern UART_HandleTypeDef huart1;

#define STRESS_CONTROL_MAX_FPS          4300U
#define STRESS_CONTROL_DEFAULT_PERCENT  100U
#define STRESS_CONTROL_CREDIT_SCALE     1000U
#define STRESS_CONTROL_MAX_CREDIT       (32U * STRESS_CONTROL_CREDIT_SCALE)

static uint32_t stress_percent = STRESS_CONTROL_DEFAULT_PERCENT;
static uint32_t stress_credit = 0U;
static uint32_t stress_last_tick = 0U;
static uint8_t stress_initialized = 0U;

static uint16_t command_value = 0U;
static uint8_t command_digits = 0U;

static uint32_t StressControl_TargetFps(void)
{
    if (stress_percent >= 100U)
    {
        return STRESS_CONTROL_MAX_FPS;
    }

    return (STRESS_CONTROL_MAX_FPS * stress_percent + 50U) / 100U;
}

static void StressControl_SendText(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    uint16_t length = 0U;

    while ((text[length] != '\0') && (length < 383U))
    {
        length++;
    }

    (void)Console_UART_Transmit(
        &huart1,
        (const uint8_t *)text,
        length,
        0U);
}

static void StressControl_PrintCurrent(void)
{
    char msg[160];
    int length;

    if (stress_percent == 0U)
    {
        length = snprintf(
            msg,
            sizeof(msg),
            "STRESS set=0%% mode=PAUSED\r\n");
    }
    else if (stress_percent >= 100U)
    {
        length = snprintf(
            msg,
            sizeof(msg),
            "STRESS set=100%% mode=MAX (~%lu frame/s)\r\n",
            (unsigned long)STRESS_CONTROL_MAX_FPS);
    }
    else
    {
        length = snprintf(
            msg,
            sizeof(msg),
            "STRESS set=%lu%% target~%lu frame/s\r\n",
            (unsigned long)stress_percent,
            (unsigned long)StressControl_TargetFps());
    }

    if (length > 0)
    {
        (void)Console_UART_Transmit(
            &huart1,
            (const uint8_t *)msg,
            (uint16_t)length,
            0U);
    }
}

static void StressControl_SetLevel(uint32_t percent)
{
    if (percent > 100U)
    {
        StressControl_SendText(
            "STRESS invalid: enter a value from 0 to 100\r\n");
        return;
    }

    stress_percent = percent;
    stress_credit = 0U;
    stress_last_tick = HAL_GetTick();

    StressControl_PrintCurrent();
}

static void StressControl_PollConsole(void)
{
    /*
     * USART RX is polled directly so changing the stress level never adds a
     * blocking HAL receive call or another interrupt path to the generator.
     */
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
    {
        uint8_t ch = (uint8_t)(huart1.Instance->RDR & 0xFFU);

        if ((ch >= (uint8_t)'0') && (ch <= (uint8_t)'9'))
        {
            if (command_digits < 3U)
            {
                command_value =
                    (uint16_t)(command_value * 10U +
                    (uint16_t)(ch - (uint8_t)'0'));
                command_digits++;
            }
            else
            {
                command_value = 101U;
                command_digits = 4U;
            }
        }
        else if ((ch == (uint8_t)'\r') || (ch == (uint8_t)'\n'))
        {
            if (command_digits != 0U)
            {
                StressControl_SetLevel((uint32_t)command_value);
                command_value = 0U;
                command_digits = 0U;
            }
        }
        else if ((ch == (uint8_t)'?') ||
                 (ch == (uint8_t)'h') ||
                 (ch == (uint8_t)'H'))
        {
            command_value = 0U;
            command_digits = 0U;
            StressControl_SendText(
                "STRESS command: type 0..100 then Enter; 0=pause, 100=max\r\n");
        }
        else if ((ch == 0x08U) || (ch == 0x7FU))
        {
            command_value = 0U;
            command_digits = 0U;
        }
        else
        {
            command_value = 0U;
            command_digits = 0U;
        }
    }

    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
    }
}

static void StressControl_Update(void)
{
    uint32_t now;
    uint32_t elapsed_ms;
    uint32_t target_fps;
    uint64_t new_credit;

    if (stress_initialized == 0U)
    {
        stress_initialized = 1U;
        stress_last_tick = HAL_GetTick();

        StressControl_SendText(
            "STRESS control: type 0..100 then Enter; default=100%%\r\n");
        StressControl_PrintCurrent();
    }

    StressControl_PollConsole();

    if ((stress_percent == 0U) || (stress_percent >= 100U))
    {
        stress_credit = 0U;
        stress_last_tick = HAL_GetTick();
        return;
    }

    now = HAL_GetTick();
    elapsed_ms = now - stress_last_tick;

    if (elapsed_ms == 0U)
    {
        return;
    }

    stress_last_tick = now;
    target_fps = StressControl_TargetFps();

    new_credit =
        (uint64_t)stress_credit +
        ((uint64_t)target_fps * (uint64_t)elapsed_ms);

    if (new_credit > STRESS_CONTROL_MAX_CREDIT)
    {
        stress_credit = STRESS_CONTROL_MAX_CREDIT;
    }
    else
    {
        stress_credit = (uint32_t)new_credit;
    }
}

uint32_t StressControl_GetLevelPercent(void)
{
    return stress_percent;
}

uint32_t StressControl_GetTargetFramesPerSecond(void)
{
    return StressControl_TargetFps();
}

uint32_t StressControl_FDCAN_GetTxFifoFreeLevel(
        FDCAN_HandleTypeDef *hfdcan)
{
    uint32_t actual_free;
    uint32_t allowed_frames;

    actual_free = HAL_FDCAN_GetTxFifoFreeLevel(hfdcan);

    if (hfdcan != &hfdcan1)
    {
        return actual_free;
    }

    StressControl_Update();

    if (stress_percent >= 100U)
    {
        return actual_free;
    }

    if (stress_percent == 0U)
    {
        return 0U;
    }

    allowed_frames = stress_credit / STRESS_CONTROL_CREDIT_SCALE;

    if (allowed_frames > actual_free)
    {
        allowed_frames = actual_free;
    }

    return allowed_frames;
}

HAL_StatusTypeDef StressControl_FDCAN_AddMessageToTxFifoQ(
        FDCAN_HandleTypeDef *hfdcan,
        const FDCAN_TxHeaderTypeDef *pTxHeader,
        const uint8_t *pTxData)
{
    HAL_StatusTypeDef status;

    if (hfdcan != &hfdcan1)
    {
        return HAL_FDCAN_AddMessageToTxFifoQ(
            hfdcan,
            pTxHeader,
            pTxData);
    }

    StressControl_Update();

    if (stress_percent == 0U)
    {
        return HAL_BUSY;
    }

    if ((stress_percent < 100U) &&
        (stress_credit < STRESS_CONTROL_CREDIT_SCALE))
    {
        return HAL_BUSY;
    }

    status = HAL_FDCAN_AddMessageToTxFifoQ(
        hfdcan,
        pTxHeader,
        pTxData);

    if ((status == HAL_OK) && (stress_percent < 100U))
    {
        stress_credit -= STRESS_CONTROL_CREDIT_SCALE;
    }

    return status;
}
