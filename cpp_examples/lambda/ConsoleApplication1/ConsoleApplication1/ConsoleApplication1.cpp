// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>

class Schimba
{
	int a;
	int b;
public:
	Schimba(int alc, int blc) :a{ alc }, b{ blc } { ; };
	void operator()(int &al, int &bl) { int aux = al; al = bl; bl = aux; };
};

int main()
{
	auto fct = [](int a, int b)->float {return (a + b) / 2.f; };
	std::cout << "fct="<<fct(3,2)<<std::endl;
	auto y = [](int &a, int &b) {int aux; aux = a; a = b; b = aux; };
	int n1 = 10;
	int n2{20};
	y(n1, n2);
	std::cout << "n1 =" << n1 << " n2=" << n2 << std::endl;
	/*aici implementez un lambda function*/
	auto z = [&n1,&n2]() {int aux; aux = n1; n1 = n2; n2 = aux; };
	std::cout << "n1 =" << n1 << " n2=" << n2 << std::endl;
	Schimba{ 3,4 }(n1,n2);
    return 0;
}

