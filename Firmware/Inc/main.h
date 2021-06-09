/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

#include "GPIO.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#ifdef __cplusplus
class BusAnalizer{
private:
	FDCAN_RxHeaderTypeDef RxHeader;
	uint8_t RxData[8];
	uint8_t FixedTxData[8];
	unsigned char sText[100];
	FDCAN_TxHeaderTypeDef FixedTxHeader;

	gpio gpio_var1;

	uint16_t ui16MessageTrigger =0;
	uint32_t ui32CounterTransmisionErrorCAN1=0;
	uint8_t ui8ErrorTransmisionCAN1 =0;
	bool ui16MessageTriggerFlag =false;
	uint32_t ui32TimerValue =0;
	uint32_t ui32USBerrors =0;
	bool ui8SetRequestToUsbCAN1 =false;
	uint32_t ui32DeltameasureTransmit=0;
	uint32_t ui32DeltameasureTransmitMax =0;
public:
  BusAnalizer(void);
  void Run(void);
  void SystemClock_Config(void);
  void MX_FDCAN1_Init(void);
  void MX_FDCAN2_Init(void);
  void MX_I2C1_Init(void);
  void MX_RTC_Init(void);
  void MX_SPI1_Init(void);
  void MX_TIM2_Init(void);
  void MX_USART1_UART_Init(void);
//  void MX_GPIO_Init(void);
  void Error_Handler(void);
  FDCAN_RxHeaderTypeDef *GetRxHeadearPointer(void){ return &(this->RxHeader);};
  uint8_t *GetRxDataPointer(void){return this->RxData;};
  void SetTimerValueRxFifo0(uint32_t lui32TimerValue){ui32TimerValue=lui32TimerValue;};
  void SetRequesttoUsbCAN1(bool ub){ui8SetRequestToUsbCAN1 =ub;}
  void IncrementMessageTrigger(void){ui16MessageTrigger++;};
  uint16_t Getui16MessageTrigger(void){return ui16MessageTrigger;};
  void SetMessageTriggerFlag(bool lbMessageflag){ui16MessageTriggerFlag = lbMessageflag;};
  ~BusAnalizer(void){;};;
};
#endif
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define FD1_STBM_Pin GPIO_PIN_3
#define FD1_STBM_GPIO_Port GPIOA
#define FD2_STBM_Pin GPIO_PIN_4
#define FD2_STBM_GPIO_Port GPIOA
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
