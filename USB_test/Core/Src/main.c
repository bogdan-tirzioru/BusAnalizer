/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "can_sniffer.h"
#include "gs_usb_app.h"
#include "logger.h"
#include "stm32h7xx_hal_fdcan.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CPU_LOAD_WINDOW_MS      1000U
#define CPU_LOAD_REPORT_WINDOWS 5U
#define CPU_LOAD_POLL_DIVIDER   16384U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

FDCAN_HandleTypeDef hfdcan1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceHS;

static uint32_t cpu_idle_ref_loops;
static uint32_t cpu_idle_ref_cycles;
static uint32_t cpu_window_start_cycles;
static uint32_t cpu_window_start_ms;
static uint32_t cpu_window_loops;
static uint32_t cpu_task_poll_divider;
static uint32_t cpu_last_rx_frames;
static uint32_t cpu_load_sum_permille;
static uint32_t cpu_load_max_permille;
static uint32_t cpu_rx_sum;
static uint32_t cpu_load_samples;
static uint32_t cpu_wait_windows;
static uint8_t cpu_load_ready;
static uint8_t cpu_baseline_valid;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void CPU_Load_Init(void);
static void CPU_Load_Task(void);
static void CPU_Load_ResetWindow(uint32_t frames);
static void CPU_Load_ResetStats(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void CPU_Load_ResetWindow(uint32_t frames)
{
  cpu_window_loops = 0U;
  cpu_last_rx_frames = frames;
  cpu_window_start_ms = HAL_GetTick();
  cpu_window_start_cycles = DWT->CYCCNT;
}

static void CPU_Load_ResetStats(void)
{
  cpu_load_sum_permille = 0U;
  cpu_load_max_permille = 0U;
  cpu_rx_sum = 0U;
  cpu_load_samples = 0U;
}

static void CPU_Load_Init(void)
{
  /* Enable the Cortex-M7 DWT cycle counter. */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
  {
    cpu_load_ready = 0U;
    Logger_Write("CPU load measurement unavailable: DWT CYCCNT disabled\r\n");
    return;
  }

  /*
   * Do not calibrate here.  At boot gs_usb is not yet in the same state as
   * the normal SocketCAN runtime, and using that faster boot loop as the idle
   * reference creates a large false CPU-load offset.
   *
   * The baseline is locked later from a full one-second window only when:
   *   - USB is configured,
   *   - SocketCAN has started the CAN sniffer, and
   *   - no CAN frames were received in the window.
   */
  cpu_idle_ref_loops = 0U;
  cpu_idle_ref_cycles = 0U;
  cpu_task_poll_divider = 0U;
  cpu_wait_windows = 0U;
  cpu_baseline_valid = 0U;
  cpu_load_ready = 1U;
  CPU_Load_ResetStats();
  CPU_Load_ResetWindow(CAN_Sniffer_GetRxCount());

  Logger_Write("CPU load: waiting for configured CAN-idle runtime baseline\r\n");
}

static void CPU_Load_Task(void)
{
  uint32_t now_ms;
  uint32_t now_cycles;
  uint32_t elapsed_cycles;
  uint32_t frames;
  uint32_t rx_delta;
  uint32_t load_permille;
  uint32_t avg_permille;
  uint32_t loops_per_second;
  uint32_t idle_loops_per_second;
  uint64_t expected_idle_loops;
  uint8_t runtime_ready;
  uint8_t baseline_was_valid;

  if (cpu_load_ready == 0U)
  {
    return;
  }

  now_ms = HAL_GetTick();
  if ((uint32_t)(now_ms - cpu_window_start_ms) < CPU_LOAD_WINDOW_MS)
  {
    return;
  }

  now_cycles = DWT->CYCCNT;
  elapsed_cycles = now_cycles - cpu_window_start_cycles;
  frames = CAN_Sniffer_GetRxCount();
  rx_delta = frames - cpu_last_rx_frames;

  if (elapsed_cycles == 0U)
  {
    CPU_Load_ResetWindow(frames);
    return;
  }

  runtime_ready = ((hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED) &&
                   CAN_Sniffer_IsRunning()) ? 1U : 0U;

  /*
   * A USB disconnect or SocketCAN link-down changes the cost of the polling
   * loop.  Throw the reference away and reacquire it after link-up instead of
   * comparing two different runtime states.
   */
  if (runtime_ready == 0U)
  {
    cpu_baseline_valid = 0U;
    cpu_wait_windows = 0U;
    CPU_Load_ResetStats();
    CPU_Load_ResetWindow(frames);
    return;
  }

  /*
   * A configured, running, traffic-free window is the correct idle reference.
   * Once locked, only a faster traffic-free window may improve the reference.
   * This removes the old ~56%% false idle load caused by boot-time calibration.
   */
  if ((rx_delta == 0U) && (cpu_window_loops != 0U))
  {
    baseline_was_valid = cpu_baseline_valid;

    if ((cpu_baseline_valid == 0U) ||
        (((uint64_t)cpu_window_loops * cpu_idle_ref_cycles) >
         ((uint64_t)cpu_idle_ref_loops * elapsed_cycles)))
    {
      cpu_idle_ref_loops = cpu_window_loops;
      cpu_idle_ref_cycles = elapsed_cycles;
      cpu_baseline_valid = 1U;
    }

    if ((baseline_was_valid == 0U) && (cpu_baseline_valid != 0U))
    {
      idle_loops_per_second =
          (uint32_t)(((uint64_t)cpu_idle_ref_loops *
                      (uint64_t)HAL_RCC_GetSysClockFreq()) /
                     cpu_idle_ref_cycles);
      Logger_Printf("CPU load baseline locked: %lu loops/s (USB configured, CAN idle)\r\n",
                    (unsigned long)idle_loops_per_second);
      CPU_Load_ResetStats();
      cpu_wait_windows = 0U;
      /* Exclude the diagnostic UART transmission from the next window. */
      CPU_Load_ResetWindow(frames);
      return;
    }
  }

  if (cpu_baseline_valid == 0U)
  {
    cpu_wait_windows++;
    if (cpu_wait_windows >= CPU_LOAD_REPORT_WINDOWS)
    {
      Logger_Write("CPU load: no idle baseline yet; leave CAN traffic idle for 1 s\r\n");
      cpu_wait_windows = 0U;
    }
    CPU_Load_ResetWindow(frames);
    return;
  }

  expected_idle_loops = ((uint64_t)cpu_idle_ref_loops * elapsed_cycles) /
                        cpu_idle_ref_cycles;

  if ((expected_idle_loops == 0U) ||
      ((uint64_t)cpu_window_loops >= expected_idle_loops))
  {
    load_permille = 0U;
  }
  else
  {
    load_permille = 1000U -
                    (uint32_t)(((uint64_t)cpu_window_loops * 1000U) /
                               expected_idle_loops);
  }

  if (load_permille > 1000U)
  {
    load_permille = 1000U;
  }

  cpu_load_sum_permille += load_permille;
  if (load_permille > cpu_load_max_permille)
  {
    cpu_load_max_permille = load_permille;
  }
  cpu_rx_sum += rx_delta;
  cpu_load_samples++;

  loops_per_second =
      (uint32_t)(((uint64_t)cpu_window_loops *
                  (uint64_t)HAL_RCC_GetSysClockFreq()) /
                 elapsed_cycles);
  idle_loops_per_second =
      (uint32_t)(((uint64_t)cpu_idle_ref_loops *
                  (uint64_t)HAL_RCC_GetSysClockFreq()) /
                 cpu_idle_ref_cycles);

  if (cpu_load_samples >= CPU_LOAD_REPORT_WINDOWS)
  {
    avg_permille = cpu_load_sum_permille / cpu_load_samples;
    Logger_Printf("CPU load: avg=%lu.%lu%% max=%lu.%lu%% loops=%lu/s idle=%lu/s CAN=%lu fps\r\n",
                  (unsigned long)(avg_permille / 10U),
                  (unsigned long)(avg_permille % 10U),
                  (unsigned long)(cpu_load_max_permille / 10U),
                  (unsigned long)(cpu_load_max_permille % 10U),
                  (unsigned long)loops_per_second,
                  (unsigned long)idle_loops_per_second,
                  (unsigned long)(cpu_rx_sum / cpu_load_samples));

    CPU_Load_ResetStats();
    /* Exclude CPU-load diagnostic UART time from the next measurement. */
    CPU_Load_ResetWindow(frames);
    return;
  }

  CPU_Load_ResetWindow(frames);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  CAN_Sniffer_Init();
  GS_USB_App_Init();
  Logger_Write("CAN1 passive sniffer ready: USB gs_usb / SocketCAN\r\n");
  CPU_Load_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    CAN_Sniffer_Process();
    GS_USB_App_Task();

    if (cpu_load_ready != 0U)
    {
      cpu_window_loops++;
      cpu_task_poll_divider++;
      if (cpu_task_poll_divider >= CPU_LOAD_POLL_DIVIDER)
      {
        cpu_task_poll_divider = 0U;
        CPU_Load_Task();
      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 120;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  hfdcan1.Init.Mode = FDCAN_MODE_BUS_MONITORING;
  hfdcan1.Init.AutoRetransmission = DISABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 5;
  hfdcan1.Init.NominalSyncJumpWidth = 2;
  hfdcan1.Init.NominalTimeSeg1 = 13;
  hfdcan1.Init.NominalTimeSeg2 = 2;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 3;
  hfdcan1.Init.DataTimeSeg1 = 12;
  hfdcan1.Init.DataTimeSeg2 = 3;
  hfdcan1.Init.MessageRAMOffset = 0;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr = 64;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_64;
  hfdcan1.Init.RxFifo1ElmtsNbr = 0;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr = 0;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  Logger_Init(&huart1);
  Logger_Write("\r\nUSB_test starting\r\n");
  Logger_Printf("CPU clock: %lu Hz\r\n",
                (unsigned long)HAL_RCC_GetSysClockFreq());

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CAN1_STBY_GPIO_Port, CAN1_STBY_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : CAN1_STBY_Pin */
  GPIO_InitStruct.Pin = CAN1_STBY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CAN1_STBY_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
