#include "stm32h7xx_hal.h"
#include "console.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern FDCAN_HandleTypeDef hfdcan1;
extern UART_HandleTypeDef huart1;

/* Implemented in main.c.  Keep this module independent of main.h so the
 * HAL_FDCAN_* wrapper macros from stress_control.h do not recurse here. */
extern HAL_StatusTypeDef Generator_CAN_ApplyConfig(
        uint8_t can_fd,
        uint32_t nominal_bitrate,
        uint32_t data_bitrate);
extern void Generator_CAN_PrintStatus(void);

#define STRESS_CONTROL_MAX_FPS          4300U
#define STRESS_CONTROL_DEFAULT_PERCENT  100U
#define STRESS_CONTROL_CREDIT_SCALE     1000U
#define STRESS_CONTROL_MAX_CREDIT       (32U * STRESS_CONTROL_CREDIT_SCALE)
#define STRESS_CONTROL_COMMAND_SIZE     48U

#define CAN_BITRATE_500K  500000UL
#define CAN_BITRATE_1M   1000000UL
#define CAN_DBITRATE_2M  2000000UL
#define CAN_DBITRATE_5M  5000000UL

static uint32_t stress_percent = STRESS_CONTROL_DEFAULT_PERCENT;
static uint32_t stress_credit = 0U;
static uint32_t stress_last_tick = 0U;
static uint8_t stress_initialized = 0U;

static char command_line[STRESS_CONTROL_COMMAND_SIZE];
static uint8_t command_length = 0U;

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
    uint16_t length = 0U;

    if (text == NULL)
    {
        return;
    }

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
            "STRESS set=100%% mode=MAX\r\n");
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

static void StressControl_PrintHelp(void)
{
    StressControl_SendText(
        "Commands:\r\n"
        "  0..100             stress level\r\n"
        "  can classic 500k\r\n"
        "  can classic 1m\r\n"
        "  can fd 500k 2m\r\n"
        "  can fd 500k 5m\r\n"
        "  can fd 1m 2m\r\n"
        "  can fd 1m 5m\r\n"
        "  can status\r\n");
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

static uint8_t StressControl_ParsePercent(const char *text, uint32_t *value)
{
    uint32_t parsed = 0U;
    uint32_t i = 0U;

    if ((text == NULL) || (value == NULL) || (text[0] == '\0'))
    {
        return 0U;
    }

    while (text[i] != '\0')
    {
        if ((text[i] < '0') || (text[i] > '9'))
        {
            return 0U;
        }

        parsed = (parsed * 10U) + (uint32_t)(text[i] - '0');
        if (parsed > 100U)
        {
            return 0U;
        }
        i++;
    }

    *value = parsed;
    return 1U;
}

static void StressControl_ApplyCanCommand(
        uint8_t can_fd,
        uint32_t nominal_bitrate,
        uint32_t data_bitrate)
{
    if (Generator_CAN_ApplyConfig(
            can_fd,
            nominal_bitrate,
            data_bitrate) != HAL_OK)
    {
        StressControl_SendText("CAN command failed\r\n");
    }
}

static void StressControl_HandleCommand(const char *command)
{
    uint32_t percent;

    if ((command == NULL) || (command[0] == '\0'))
    {
        return;
    }

    if (StressControl_ParsePercent(command, &percent) != 0U)
    {
        StressControl_SetLevel(percent);
        return;
    }

    if (strcmp(command, "can classic 500k") == 0)
    {
        StressControl_ApplyCanCommand(0U, CAN_BITRATE_500K, 0U);
        return;
    }

    if (strcmp(command, "can classic 1m") == 0)
    {
        StressControl_ApplyCanCommand(0U, CAN_BITRATE_1M, 0U);
        return;
    }

    if (strcmp(command, "can fd 500k 2m") == 0)
    {
        StressControl_ApplyCanCommand(
            1U, CAN_BITRATE_500K, CAN_DBITRATE_2M);
        return;
    }

    if (strcmp(command, "can fd 500k 5m") == 0)
    {
        StressControl_ApplyCanCommand(
            1U, CAN_BITRATE_500K, CAN_DBITRATE_5M);
        return;
    }

    if (strcmp(command, "can fd 1m 2m") == 0)
    {
        StressControl_ApplyCanCommand(
            1U, CAN_BITRATE_1M, CAN_DBITRATE_2M);
        return;
    }

    if (strcmp(command, "can fd 1m 5m") == 0)
    {
        StressControl_ApplyCanCommand(
            1U, CAN_BITRATE_1M, CAN_DBITRATE_5M);
        return;
    }

    if (strcmp(command, "can status") == 0)
    {
        Generator_CAN_PrintStatus();
        StressControl_PrintCurrent();
        return;
    }

    StressControl_SendText("Unknown command. Type ? for help.\r\n");
}

static void StressControl_PollConsole(void)
{
    /*
     * USART RX is polled directly so runtime commands do not add a blocking
     * HAL receive call or another interrupt path to the traffic generator.
     */
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
    {
        uint8_t ch = (uint8_t)(huart1.Instance->RDR & 0xFFU);

        if ((ch == (uint8_t)'\r') || (ch == (uint8_t)'\n'))
        {
            if (command_length != 0U)
            {
                while ((command_length != 0U) &&
                       (command_line[command_length - 1U] == ' '))
                {
                    command_length--;
                }

                command_line[command_length] = '\0';
                StressControl_HandleCommand(command_line);
                command_length = 0U;
            }
        }
        else if ((ch == (uint8_t)'?') && (command_length == 0U))
        {
            StressControl_PrintHelp();
        }
        else if ((ch == 0x08U) || (ch == 0x7FU))
        {
            if (command_length != 0U)
            {
                command_length--;
            }
        }
        else if ((ch == (uint8_t)' ') || (ch == (uint8_t)'\t'))
        {
            if ((command_length != 0U) &&
                (command_line[command_length - 1U] != ' ') &&
                (command_length < (STRESS_CONTROL_COMMAND_SIZE - 1U)))
            {
                command_line[command_length++] = ' ';
            }
        }
        else if ((ch >= 0x20U) && (ch <= 0x7EU))
        {
            if ((ch >= (uint8_t)'A') && (ch <= (uint8_t)'Z'))
            {
                ch = (uint8_t)(ch + ((uint8_t)'a' - (uint8_t)'A'));
            }

            if (command_length < (STRESS_CONTROL_COMMAND_SIZE - 1U))
            {
                command_line[command_length++] = (char)ch;
            }
            else
            {
                command_length = 0U;
                StressControl_SendText("Command too long\r\n");
            }
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
            "Runtime console ready: type ? for commands\r\n");
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

HAL_StatusTypeDef StressControl_FDCAN_Start(
        FDCAN_HandleTypeDef *hfdcan)
{
    /* TDC is required only for the FDCAN1 CAN FD+BRS transmitter. */
    if ((hfdcan == &hfdcan1) &&
        (hfdcan->Init.FrameFormat == FDCAN_FRAME_FD_BRS))
    {
        uint32_t tdc_offset =
            hfdcan->Init.DataPrescaler * hfdcan->Init.DataTimeSeg1;

        if (HAL_FDCAN_ConfigTxDelayCompensation(
                hfdcan,
                tdc_offset,
                0U) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (HAL_FDCAN_EnableTxDelayCompensation(hfdcan) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    return HAL_FDCAN_Start(hfdcan);
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
