
#ifndef AGPIO_H
#define AGPIO_H

// Includes
#include "stm32h7xx_hal.h"

// Classes
#ifdef __cplusplus
class AGpio
{
private:
	// No private members so far
public:
	// Constructor
	AGpio(void);

	// Write pin method
	void WritePin(void);

	// Destructor
	~AGpio(void) {;};;
};
#endif // __cplusplus

#endif // AGPIO_H
