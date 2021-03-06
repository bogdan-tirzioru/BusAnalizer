// ConsoleApplication2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "newfile.h"
#include <iostream>
#include "myClass.h"
static int i = 0;
/*1) Variable outside the body of any function -> The scope of the variable is limited to the file in which it is declared*/
/*2) Variable declaration inside a function -> The variable is permanent. It is initialized once and only one copy is created even if the function is called recursively*/
/*3) Function declaration -> The scope of the function is limited to the file in which it is declared*/
/*4) Member variable -> One copy of the variable is created per class (not one per variable)*/
/*5) Member function -> Function can only access static members of the class.*/
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
	myClass myc1;
	myClass myc2;
    return 0;
}

