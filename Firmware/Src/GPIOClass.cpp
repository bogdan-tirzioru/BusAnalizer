#include "main.h"

#define GPIO_MODE             (0x00000003U)
#define ANALOG_MODE           (0x00000008U)
#define EXTI_MODE             (0x10000000U)
#define GPIO_MODE_IT          (0x00010000U)
#define GPIO_MODE_EVT         (0x00020000U)
#define RISING_EDGE           (0x00100000U)
#define FALLING_EDGE          (0x00200000U)
#define GPIO_OUTPUT_TYPE      (0x00000010U)

#if defined(DUAL_CORE)
#define EXTI_CPU1             (0x01000000U)
#define EXTI_CPU2             (0x02000000U)
#endif /*DUAL_CORE*/
#define GPIO_NUMBER           (16U)







void GPIOClass::GPIOInit(void)
{
	//GPIOPinInitStruct GPIO_InitStructure={0};

	  /* GPIO Ports Clock Enable */

	   this->GPIO_StartClock(GPIOC);
	   this->GPIO_StartClock(GPIOH);
	   this->GPIO_StartClock(GPIOA);
	   this->GPIO_StartClock(GPIOB);


	  /*Configure GPIO pin Output Level */
	  this->GPIO_ResetPin(GPIOA, FD1_STBM_Pin|FD2_STBM_Pin);

	  /*Configure GPIO pin Output Level */
	  this->GPIO_ResetPin(GPIOB, GPIO_PIN_0|GPIO_PIN_1);

	  /*Configure GPIO pins : FD1_STBM_Pin FD2_STBM_Pin */
	  this->GPIO_InitStructure[0].Pin = FD1_STBM_Pin|FD2_STBM_Pin;
	  this->GPIO_InitStructure[0].Mode = GPIO_MODE_OUTPUT_PP;
	  this->GPIO_InitStructure[0].Pull = GPIO_NOPULL;
	  this->GPIO_InitStructure[0].Speed = GPIO_SPEED_FREQ_LOW;

	  this->GPIO_RegisterInit(GPIOA, &GPIO_InitStructure[0]);

	  /*Configure GPIO pins : PB0 PB1 */
	  this->GPIO_InitStructure[1].Pin = GPIO_PIN_0|GPIO_PIN_1;
	  this->GPIO_InitStructure[1].Mode = GPIO_MODE_OUTPUT_PP;
	  this->GPIO_InitStructure[1].Pull = GPIO_NOPULL;
	  this->GPIO_InitStructure[1].Speed = GPIO_SPEED_FREQ_LOW;

	  this->GPIO_RegisterInit(GPIOB, &GPIO_InitStructure[1]);

}

void::GPIOClass::GPIO_StartClock(GPIO_TypeDef  *GPIOx)
{
		if((uint32_t)GPIOA == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOA_CLK_ENABLE();}
		else if((uint32_t)GPIOB == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOB_CLK_ENABLE();}
		else if((uint32_t)GPIOC == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOC_CLK_ENABLE();}
		else if((uint32_t)GPIOD == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOD_CLK_ENABLE();}
		else if((uint32_t)GPIOE == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOE_CLK_ENABLE();}
		else if((uint32_t)GPIOF == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOF_CLK_ENABLE();}
		else if((uint32_t)GPIOG == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOG_CLK_ENABLE();}
		else if((uint32_t)GPIOH == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOH_CLK_ENABLE();}
		else if((uint32_t)GPIOI == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOI_CLK_ENABLE();}
		else if((uint32_t)GPIOJ == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOJ_CLK_ENABLE();}
		else if((uint32_t)GPIOK == (uint32_t) GPIOx)
				{__HAL_RCC_GPIOK_CLK_ENABLE();}
		else
				{ /*do nothing*/              }
}


void GPIOClass::GPIOInit(GPIO_TypeDef  *GPIOx, GPIOPinInitStruct *GPIO_Init)
{

	this->GPIO_StartClock(GPIOx);
	this->GPIO_ResetPin(GPIOx,GPIO_Init->Pin);
	this->GPIO_RegisterInit(GPIOx, GPIO_Init);

}

void GPIOClass::GPIO_RegisterInit(GPIO_TypeDef  *GPIOx, GPIOPinInitStruct *GPIO_Init)
{
	  uint32_t position = 0x00U;
	  uint32_t iocurrent;
	  uint32_t temp;
	  EXTI_Core_TypeDef *EXTI_CurrentCPU;

	#if defined(DUAL_CORE) && defined(CORE_CM4)
	  EXTI_CurrentCPU = EXTI_D2; /* EXTI for CM4 CPU */
	#else
	  EXTI_CurrentCPU = EXTI_D1; /* EXTI for CM7 CPU */
	#endif


	  /* Configure the port pins */
	  while (((GPIO_Init->Pin) >> position) != 0x00U)
	  {
	    /* Get current io position */
	    iocurrent = (GPIO_Init->Pin) & (1UL << position);

	    if (iocurrent != 0x00U)
	    {
	      /*--------------------- GPIO Mode Configuration ------------------------*/
	      /* In case of Output or Alternate function mode selection */
	      if ((GPIO_Init->Mode == GPIO_MODE_OUTPUT_PP) || (GPIO_Init->Mode == GPIO_MODE_AF_PP) ||
	          (GPIO_Init->Mode == GPIO_MODE_OUTPUT_OD) || (GPIO_Init->Mode == GPIO_MODE_AF_OD))
	      {
	        /* Check the Speed parameter */
	        assert_param(IS_GPIO_SPEED(GPIO_Init->Speed));
	        /* Configure the IO Speed */
	        temp = GPIOx->OSPEEDR;
	        temp &= ~(GPIO_OSPEEDR_OSPEED0 << (position * 2U));
	        temp |= (GPIO_Init->Speed << (position * 2U));
	        GPIOx->OSPEEDR = temp;

	        /* Configure the IO Output Type */
	        temp = GPIOx->OTYPER;
	        temp &= ~(GPIO_OTYPER_OT0 << position) ;
	        temp |= (((GPIO_Init->Mode & GPIO_OUTPUT_TYPE) >> 4U) << position);
	        GPIOx->OTYPER = temp;
	      }

	      /* Activate the Pull-up or Pull down resistor for the current IO */
	      temp = GPIOx->PUPDR;
	      temp &= ~(GPIO_PUPDR_PUPD0 << (position * 2U));
	      temp |= ((GPIO_Init->Pull) << (position * 2U));
	      GPIOx->PUPDR = temp;

	      /* In case of Alternate function mode selection */
	      if ((GPIO_Init->Mode == GPIO_MODE_AF_PP) || (GPIO_Init->Mode == GPIO_MODE_AF_OD))
	      {
	        /* Check the Alternate function parameters */
	        assert_param(IS_GPIO_AF_INSTANCE(GPIOx));
	        assert_param(IS_GPIO_AF(GPIO_Init->Alternate));

	        /* Configure Alternate function mapped with the current IO */
	        temp = GPIOx->AFR[position >> 3U];
	        temp &= ~(0xFU << ((position & 0x07U) * 4U));
	        temp |= ((GPIO_Init->Alternate) << ((position & 0x07U) * 4U));
	        GPIOx->AFR[position >> 3U] = temp;
	      }

	      /* Configure IO Direction mode (Input, Output, Alternate or Analog) */
	      temp = GPIOx->MODER;
	      temp &= ~(GPIO_MODER_MODE0 << (position * 2U));
	      temp |= ((GPIO_Init->Mode & GPIO_MODE) << (position * 2U));
	      GPIOx->MODER = temp;

	      /*--------------------- EXTI Mode Configuration ------------------------*/
	      /* Configure the External Interrupt or event for the current IO */
	      if ((GPIO_Init->Mode & EXTI_MODE) == EXTI_MODE)
	      {
	        /* Enable SYSCFG Clock */
	        __HAL_RCC_SYSCFG_CLK_ENABLE();

	        temp = SYSCFG->EXTICR[position >> 2U];
	        temp &= ~(0x0FUL << (4U * (position & 0x03U)));
	        temp |= (GPIO_GET_INDEX(GPIOx) << (4U * (position & 0x03U)));
	        SYSCFG->EXTICR[position >> 2U] = temp;

	        /* Clear EXTI line configuration */
	        temp = EXTI_CurrentCPU->IMR1;
	        temp &= ~(iocurrent);
	        if ((GPIO_Init->Mode & GPIO_MODE_IT) == GPIO_MODE_IT)
	        {
	          temp |= iocurrent;
	        }
	        EXTI_CurrentCPU->IMR1 = temp;

	        temp = EXTI_CurrentCPU->EMR1;
	        temp &= ~(iocurrent);
	        if ((GPIO_Init->Mode & GPIO_MODE_EVT) == GPIO_MODE_EVT)
	        {
	          temp |= iocurrent;
	        }
	        EXTI_CurrentCPU->EMR1 = temp;

	        /* Clear Rising Falling edge configuration */
	        temp = EXTI->RTSR1;
	        temp &= ~(iocurrent);
	        if ((GPIO_Init->Mode & RISING_EDGE) == RISING_EDGE)
	        {
	          temp |= iocurrent;
	        }
	        EXTI->RTSR1 = temp;

	        temp = EXTI->FTSR1;
	        temp &= ~(iocurrent);
	        if ((GPIO_Init->Mode & FALLING_EDGE) == FALLING_EDGE)
	        {
	          temp |= iocurrent;
	        }
	        EXTI->FTSR1 = temp;
	      }
	    }

	    position++;
	  }

}

void GPIO_RegisterDeInit(GPIO_TypeDef  *GPIOx, uint32_t GPIO_Pin)
{
	uint32_t position = 0x00U;
	  uint32_t iocurrent;
	  uint32_t tmp;
	  EXTI_Core_TypeDef *EXTI_CurrentCPU;

	#if defined(DUAL_CORE) && defined(CORE_CM4)
	  EXTI_CurrentCPU = EXTI_D2; /* EXTI for CM4 CPU */
	#else
	  EXTI_CurrentCPU = EXTI_D1; /* EXTI for CM7 CPU */
	#endif

	  /* Configure the port pins */
	  while ((GPIO_Pin >> position) != 0x00U)
	  {
	    /* Get current io position */
	    iocurrent = GPIO_Pin & (1UL << position) ;

	    if (iocurrent != 0x00U)
	    {
	      /*------------------------- EXTI Mode Configuration --------------------*/
	      /* Clear the External Interrupt or Event for the current IO */
	      tmp = SYSCFG->EXTICR[position >> 2U];
	      tmp &= (0x0FUL << (4U * (position & 0x03U)));
	      if (tmp == (GPIO_GET_INDEX(GPIOx) << (4U * (position & 0x03U))))
	      {
	        /* Clear EXTI line configuration for Current CPU */
	        EXTI_CurrentCPU->IMR1 &= ~(iocurrent);
	        EXTI_CurrentCPU->EMR1 &= ~(iocurrent);

	        /* Clear Rising Falling edge configuration */
	        EXTI->RTSR1 &= ~(iocurrent);
	        EXTI->FTSR1 &= ~(iocurrent);

	        tmp = 0x0FUL << (4U * (position & 0x03U));
	        SYSCFG->EXTICR[position >> 2U] &= ~tmp;
	      }

	      /*------------------------- GPIO Mode Configuration --------------------*/
	      /* Configure IO in Analog Mode */
	      GPIOx->MODER |= (GPIO_MODER_MODE0 << (position * 2U));

	      /* Configure the default Alternate Function in current IO */
	      GPIOx->AFR[position >> 3U] &= ~(0xFU << ((position & 0x07U) * 4U)) ;

	      /* Deactivate the Pull-up and Pull-down resistor for the current IO */
	      GPIOx->PUPDR &= ~(GPIO_PUPDR_PUPD0 << (position * 2U));

	      /* Configure the default value IO Output Type */
	      GPIOx->OTYPER  &= ~(GPIO_OTYPER_OT0 << position) ;

	      /* Configure the default value for IO Speed */
	      GPIOx->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED0 << (position * 2U));
	    }

	    position++;
	  }
}


void GPIOClass::GPIO_SetPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
		GPIOx->ODR|=GPIO_Pin;
}


void GPIOClass::GPIO_ResetPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
		GPIOx->ODR&=(~GPIO_Pin);
}


bool GPIOClass::GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{

	  return ((GPIOx->IDR & GPIO_Pin) > 0 ? 1:0);
}


void GPIOClass::GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{

	if (this->GPIO_ReadPin(GPIOx,GPIO_Pin))
		GPIO_ResetPin(GPIOx, GPIO_Pin);
	else
		GPIO_SetPin(GPIOx, GPIO_Pin);

}

