#include "stdafx.h"
#include "myClass.h"

int myClass::ii=9;
myClass::myClass()
{
	ii ++;
}


myClass::~myClass()
{
	ii--;
}
