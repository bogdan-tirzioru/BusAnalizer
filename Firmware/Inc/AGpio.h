
#ifndef AGPIO_H
#define AGPIO_H

#ifdef __cplusplus
extern "C" {
#endif

// Includes
#include "stm32h7xx_hal.h"

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
