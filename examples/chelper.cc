#include <iostream>
#include <cmath>
using namespace std;

extern "C" int64_t iadd(int64_t a, int64_t b);
extern "C" double ffadd(double a, double b);
extern "C" int complex_comp(int a, int b, int c, int d);
extern "C" int64_t addr(int64_t a);
extern "C" int64_t always10(int64_t r);
extern "C" int64_t myabs(uint64_t a);
extern "C" int64_t mygcd(uint64_t a, uint64_t b);
extern "C" int64_t breakandcall();

void testabs(){
    cout << "start test abs\n";
    cout << myabs(-898) <<  ' ' << myabs(898) << endl;
}

void testgcd(){
    cout << "start gcd\n";
    cout << mygcd(12, 60) << ' ' << mygcd(5, 50) << endl;
}

void testbreakandcall(){
    cout << "start breakandcall\n";
    cout << breakandcall() << endl;
}

int main(){
    std::cout << iadd(1, 1) << std::endl << ffadd(12.34, 4.12) << std::endl << complex_comp(4, 5, 7, 888) << std::endl;
    std::cout << addr(44) << std::endl << always10(1) << endl;
    testabs();
    testgcd();
    testbreakandcall();
    return 0;
}