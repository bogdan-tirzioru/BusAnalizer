/**
  ******************************************************************************
  * @file    stm322xg_eval_ioe.c
  * @author  MCD Application Team
  * @version V4.6.2
  * @date    22-July-2011
  * @brief   This file provides a set of functions needed to manage the STMPE811
  *          IO Expander devices mounted on STM322xG-EVAL evaluation board(MB786)
  *          RevA and RevB.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************  
  */ 

  /* File Info : ---------------------------------------------------------------
  
    Note:
    -----
    - This driver uses the DMA method for sending and receiving data on I2C bus
      which allow higher efficiency and reliability of the communication.  
  
    SUPPORTED FEATURES:
      - IO Read/write : Set/Reset and Read (Polling/Interrupt)
      - Joystick: config and Read (Polling/Interrupt)
      - Touch Screen Features: Single point mode (Polling/Interrupt)
      - TempSensor Feature: accuracy not determined (Polling).

    UNSUPPORTED FEATURES:
      - Row ADC Feature is not supported (not implemented on STM322xG_EVAL board)
  ----------------------------------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include "stm322xg_eval_ioe.h"

/** @defgroup STM324xG_EVAL_IOE_Private_Functions
  * @{
  */ 


/**
  * @brief  Initializes and Configures the two IO_Expanders Functionalities 
  *         (IOs, Touch Screen ..) and configures all STM324xG_EVAL necessary
  *         hardware (GPIOs, APB clocks ..).
  * @param  None
  * @retval IOE_OK if all initializations done correctly. Other value if error.
  */
uint8_t IOE_Config(void)
{
  	GPIO_InitTypeDef  GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOB, ENABLE);

	//JOYSTICK 
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

  return IOE_OK; 
}


/**
  * @brief  Returns the current Joystick status.
  * @param  None
  * @retval The code of the Joystick key pressed: 
  *   @arg  JOY_NONE
  *   @arg  JOY_SEL
  *   @arg  JOY_DOWN
  *   @arg  JOY_LEFT
  *   @arg  JOY_RIGHT
  *   @arg  JOY_UP
  */
JOYState_TypeDef
 IOE_JoyStickGetState(void)
{
  if(!(GPIOD->IDR & GPIO_Pin_15))
  {
    return (JOYState_TypeDef) JOY_SEL;
  }
  else if(!(GPIOD->IDR & GPIO_Pin_11))
  {
    return (JOYState_TypeDef) JOY_DOWN;
  }
  else if(!(GPIOB->IDR & GPIO_Pin_15))
  {
    return (JOYState_TypeDef) JOY_LEFT;
  }
  else if(!(GPIOD->IDR & GPIO_Pin_13))
  {
    return (JOYState_TypeDef) JOY_RIGHT;
  }
  else if(!(GPIOD->IDR & GPIO_Pin_9))
  {
    return (JOYState_TypeDef) JOY_UP;
  }
  else
  { 
    return (JOYState_TypeDef) JOY_NONE;
  }
}

/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */      
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
