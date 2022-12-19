#include <iostream>

extern "C" int64_t iadd(int64_t a, int64_t b);
extern "C" double fadd(double a, double b);
extern "C" int complex_comp(int a, int b, int c, int d);
extern "C" int64_t addr(int64_t a);
extern "C" int64_t always10(int64_t r);

int main(){
    std::cout << iadd(1, 1) << std::endl << fadd(12.34, 4.12) << std::endl << complex_comp(4, 5, 7, 888) << std::endl;
    std::cout << addr(44) << std::endl << always10(1);
    return 0;
}