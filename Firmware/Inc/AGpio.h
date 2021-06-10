
#ifndef AGPIO_H
#define AGPIO_H

#ifdef __cplusplus
extern "C" {
#endif

// Includes
#include "stm32h7xx_hal.h"

// Private defines
#define GPIO_MODE             (0x00000003U)
#define ANALOG_MODE           (0x00000008U)
#define EXTI_MODE             (0x10000000U)
#define GPIO_MODE_IT          (0x00010000U)
#define GPIO_MODE_EVT         (0x00020000U)
#define RISING_EDGE           (0x00100000U)
#define FALLING_EDGE          (0x00200000U)
#define GPIO_OUTPUT_TYPE      (0x00000010U)

// Type definitions
typedef struct
{
  uint32_t Pin;
  uint32_t Mode;
  uint32_t Pull;
  uint32_t Speed;
  uint32_t Alternate;
} AGPIO_InitTypeDef;

// Classes
#ifdef __cplusplus
class AGpio
{
private:
	// No private members so far
public:
	// Constructor
	AGpio(void) {;};

	// Init method
	void Init(GPIO_TypeDef *GPIOx, AGPIO_InitTypeDef *GPIO_Init);

	// Write pin method
	void WritePin(void);

	// Destructor
	~AGpio(void) {;};
};

void AGpio::Init(GPIO_TypeDef *GPIOx, AGPIO_InitTypeDef *GPIO_Init)
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

	/* Check the parameters */
	assert_param(IS_GPIO_ALL_INSTANCE(GPIOx));
	assert_param(IS_GPIO_PIN(GPIO_Init->Pin));
	assert_param(IS_GPIO_MODE(GPIO_Init->Mode));
	assert_param(IS_GPIO_PULL(GPIO_Init->Pull));

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

void AGpio::WritePin()
{
	// add content here
}
#endif // __cplusplus

#ifdef __cplusplus
}
#endif

#endif // AGPIO_H
