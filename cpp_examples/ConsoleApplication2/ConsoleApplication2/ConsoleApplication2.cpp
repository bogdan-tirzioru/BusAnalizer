// ConsoleApplication2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "newfile.h"
#include <iostream>
static int i = 0;

void my_function(void)
{
	// do nothing
	i++;
}

int main()
{
	std::cout << i;
	my_function();
	std::cout << i;
    return 0;
}

