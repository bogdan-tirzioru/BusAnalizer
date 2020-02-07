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
#define NumberOfRxBuffers 64
#define MaxPayload 64
#define MaxSizeRxMessage 4096
#define MaxSizeTxMessage 2048
#define NumberOfRxToSendUsb 60

typedef struct {
	/*header of the CAN msg received*/
	FDCAN_RxHeaderTypeDef sRxHeaderTypeDef;
	/*actual payload*/
	uint8_t aui8PayLoad[MaxPayload];
	/*state of the message*/
	uint8_t ui8StateMsg;
}TMessageInfo;

extern TMessageInfo sListRxMessage[MaxSizeRxMessage];
extern TMessageInfo sListTxMessage[MaxSizeTxMessage];

extern uint16_t ui16IndexRxMsg;
extern uint16_t ui16IndexRxUsb;
extern uint16_t ui16IndexTxMsg;
extern uint16_t ui16IndexTxUsb;
extern uint16_t ui16RxToBeSend;
extern uint16_t ui16TxToBeSend;
extern void FindNextMsgRx(uint16_t *ui16Index,uint8_t ui8Conditie);
extern void FindNextMsgTx(uint16_t *ui16Index,uint8_t ui8Conditie);
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
