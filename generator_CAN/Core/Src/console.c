#include "console.h"

#include <string.h>

#define CONSOLE_TX_QUEUE_DEPTH  4U
#define CONSOLE_TX_SLOT_SIZE    384U

typedef struct
{
    uint16_t length;
    uint8_t data[CONSOLE_TX_SLOT_SIZE];
} ConsoleTxSlot;

/*
 * Static storage is important here: DMA must never reference a stack buffer
 * that can disappear while USART is still shifting bytes.
 *
 * The project linker places .bss in RAM_D1 (0x24000000), which is accessible
 * to DMA1 on this STM32H7 target.
 */
__attribute__((aligned(32)))
static ConsoleTxSlot tx_queue[CONSOLE_TX_QUEUE_DEPTH];

static volatile uint8_t tx_head = 0U;
static volatile uint8_t tx_tail = 0U;
static volatile uint8_t tx_active = 0U;

static volatile uint32_t tx_dropped_messages = 0U;
static volatile uint32_t tx_error_count = 0U;

static UART_HandleTypeDef *console_uart = NULL;

static void Console_TryStart(void)
{
    UART_HandleTypeDef *uart;
    uint8_t slot;
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();

    if ((tx_active != 0U) ||
        (tx_tail == tx_head) ||
        (console_uart == NULL))
    {
        if (primask == 0U)
        {
            __enable_irq();
        }
        return;
    }

    slot = tx_tail;
    uart = console_uart;
    tx_active = 1U;

    if (primask == 0U)
    {
        __enable_irq();
    }

    if (HAL_UART_Transmit_DMA(
            uart,
            tx_queue[slot].data,
            tx_queue[slot].length) != HAL_OK)
    {
        primask = __get_PRIMASK();
        __disable_irq();

        tx_active = 0U;
        tx_error_count++;

        if (primask == 0U)
        {
            __enable_irq();
        }
    }
}

HAL_StatusTypeDef Console_UART_Transmit(
        UART_HandleTypeDef *huart,
        const uint8_t *data,
        uint16_t size,
        uint32_t timeout)
{
    uint8_t head;
    uint8_t next;

    (void)timeout;

    if ((huart == NULL) || (data == NULL))
    {
        return HAL_ERROR;
    }

    if (size == 0U)
    {
        return HAL_OK;
    }

    if (size > CONSOLE_TX_SLOT_SIZE)
    {
        tx_dropped_messages++;
        return HAL_ERROR;
    }

    if (console_uart == NULL)
    {
        console_uart = huart;
    }
    else if (console_uart != huart)
    {
        return HAL_ERROR;
    }

    /*
     * Single producer (main loop), single consumer (DMA completion ISR).
     * The producer writes the free slot first, then publishes the new head.
     */
    head = tx_head;
    next = (uint8_t)((head + 1U) % CONSOLE_TX_QUEUE_DEPTH);

    if (next == tx_tail)
    {
        tx_dropped_messages++;
        return HAL_BUSY;
    }

    memcpy(tx_queue[head].data, data, size);
    tx_queue[head].length = size;

    /* Publish slot contents before advancing the producer index. */
    __DMB();
    tx_head = next;

    Console_TryStart();

    return HAL_OK;
}

uint32_t Console_GetDroppedMessageCount(void)
{
    return tx_dropped_messages;
}

uint32_t Console_GetErrorCount(void)
{
    return tx_error_count;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    uint32_t primask;

    if (huart != console_uart)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if ((tx_active != 0U) && (tx_tail != tx_head))
    {
        tx_tail = (uint8_t)((tx_tail + 1U) % CONSOLE_TX_QUEUE_DEPTH);
    }

    tx_active = 0U;

    if (primask == 0U)
    {
        __enable_irq();
    }

    Console_TryStart();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    uint32_t primask;

    if (huart != console_uart)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    tx_error_count++;

    /* Drop only the failed active message, then continue with the queue. */
    if ((tx_active != 0U) && (tx_tail != tx_head))
    {
        tx_tail = (uint8_t)((tx_tail + 1U) % CONSOLE_TX_QUEUE_DEPTH);
    }

    tx_active = 0U;

    if (primask == 0U)
    {
        __enable_irq();
    }

    Console_TryStart();
}
