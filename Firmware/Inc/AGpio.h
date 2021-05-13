#ifndef AGPIO_H
#define AGPIO_H

#include "stm32h7xx_hal.h"
class AGpio{
	uint8 ui8Membru;
public:
	uint8 ui8Membru1;
	AGpio()
	{
		//aici o sa fac ceva pe viitor
	}
	AGpio(AGpio &iAGpio)
	{

	}
	AGpio(AGpio &&iAGpio)
	{

	}
	AGpio(uint8 unparametru,uint16 doiparam=2)
	{

	}
	AGpio():ui8Membru(0)
	{

	}
	void Setup(AGpio *p)
	{
		p->ui8Membru = 10;
	}
	void Setup(AGpio &p) /*referinta*/
	{
		uint8 myvar=11;
		uint8 *pvar=&myvar; /*adressa*/
		uint8 myvar2= &(*pvar);
		p.ui8Membru1 = 10;

	}
};
#endif
