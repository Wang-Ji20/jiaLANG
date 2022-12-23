#include <cmath>
#include <cstring>
#include <iostream>
using namespace std;

extern "C" int64_t iadd(int64_t a, int64_t b);
extern "C" int64_t ass(int64_t a, int64_t b);
extern "C" double ffadd(double a, double b);
extern "C" int complex_comp(int a, int b, int c, int d);
extern "C" int64_t addr(int64_t a);
extern "C" int64_t always10(int64_t r);
extern "C" int64_t myabs(uint64_t a);
extern "C" int64_t mygcd(uint64_t a, uint64_t b);
extern "C" int64_t breakandcall();
extern "C" int64_t arraysum(int64_t arr[], int64_t n);
extern "C" int64_t copyarr(int64_t src[5], int64_t dst[5]);
extern "C" int64_t kmp(char txt[100], int64_t txtlen, char pat[100],
                       int64_t patlen, int64_t ans[100]);
extern "C" void sort(int64_t nums[100], int64_t len);

void testabs() {
  cout << "start test abs\n";
  cout << myabs(-898) << ' ' << myabs(898) << endl;
}

void testgcd() {
  cout << "start gcd\n";
  cout << mygcd(12, 60) << ' ' << mygcd(5, 50) << endl;
}

void testbreakandcall() {
  cout << "start breakandcall\n";
  cout << breakandcall() << endl;
}

void testarraysum() {
  int64_t ar[5] = {1, 2, 3, 4, 5};
  cout << "start array sum\n";
  cout << arraysum(ar, 4);
}

void testass() {
  cout << "start testass\n";
  cout << ass(1, 2) << endl;
}

void testcopyarr() {
  cout << "start testcopyass\n";
  int64_t src[5] = {1, 2, 3, 4, 5};
  int64_t dst[5] = {0, 0, 0, 0, 0};
  copyarr(src, dst);
  for (int i = 0; i < 5; i++) {
    cout << dst[i] << ' ';
  }
  cout << endl;
}

void testkmp() {
  cout << "start test kmp.\n";
  char text[100] = "Dont Panic, Panic only results in more Panic.";
  char pattern[100] = "Panic";
  int64_t ans[100];
  kmp(text, strlen(text), pattern, strlen(pattern), ans);
  for (auto &&i : ans) {
    if (i == -1) {
      break;
    }
    cout << i << endl;
  }
  cout << endl;
}

void testsort() {
  int64_t arr[10] = {1, 3, 4, 1, 3, 22, 2, 4, 6, 8};
  cout << "start test sort.\n";
  for (auto &&i : arr) {
    cout << i << ' ';
  }
  cout << endl;
  sort(arr, 10);
  for (auto &&i : arr) {
    cout << i << ' ';
  }
  cout << endl;
}

void (*testSuite[8])() = {testabs, testgcd,     testbreakandcall, testarraysum,
                          testass, testcopyarr, testkmp,          testsort};

int main() {
  std::cout << iadd(1, 1) << std::endl
            << ffadd(12.34, 4.12) << std::endl
            << complex_comp(4, 5, 7, 888) << std::endl;
  std::cout << addr(44) << std::endl << always10(1) << endl;
  for (auto &&i : testSuite) {
    i();
  }

  return 0;
}