// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

constexpr float cFactor = 2.2046F; /*text*/ /*kg->pound*/
const float CFactor1 = 3.5; /*dataini*/
class Urs; /*declaratie*/
class Animal
{
    float greutate;
public:
    Animal() { greutate = 0; };
    Animal(float a) :greutate(a) { ; };
    friend float ConvertGreutate(Animal a);
    float GetGreutate(void) { return greutate; };
    void SetGreutate(float lgreutate) { greutate = lgreutate; };
    friend class Urs;
};

float ConvertGreutate(Animal a)
{
    return a.greutate * cFactor;
}

class Urs
{
    float total;
public:
    enum class Blana { alba, verde, neagra };
    Blana tipbl;
    Urs(Blana b, Animal a)
    {
        tipbl = b;
        total = a.greutate + 0.5;
    }
};
int main()
{
    Animal a1(10);
    Urs u1(Urs::Blana::alba, a1);
    Urs::Blana bl = Urs::Blana::alba;
    int conv = static_cast<int>(bl);
    //int conv1 = bl;
    std::cout << ConvertGreutate(a1)<<"\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
