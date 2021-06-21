#pragma once
class myClass
{
	int i = 5;
	static int ii;
public:
	myClass();
	static void my_function(int l_i = 3)
	{
		ii = l_i; 
		//i++;
		//ii = ii + i;
	};
	void my_second_function(int l_i = 4)
	{
		ii = l_i;
	}
	~myClass();
};

