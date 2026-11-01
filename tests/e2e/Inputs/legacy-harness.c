/* C port of the 2022 harness (examples/chelper.cc). The 2022 compiler made
 * int 64 bits wide; Jia's int is now C's int, so the harness uses int. */
#include <stdio.h>
#include <string.h>

int iadd(int a, int b);
int ass(int a, int b);
float ffadd(float a, float b);
int complex_comp(int a, int b, int c, int d);
int addr(int a);
int always10(int v);
int myabs(int a);
int mygcd(int a, int b);
int breakandcall(void);
int arraysum(int arr[], int n);
int copyarr(int src[], int dst[]);
int kmp(char txt[], int txtlen, char pat[], int patlen, int ans[]);
void sort(int nums[], int len);

int main(void) {
  printf("iadd %d\n", iadd(1, 1));
  printf("ffadd %.2f\n", ffadd(12.34f, 4.12f));
  printf("complex_comp %d\n", complex_comp(4, 5, 7, 888));
  printf("addr %d\n", addr(44));
  printf("always10 %d\n", always10(1));
  printf("abs %d %d\n", myabs(-898), myabs(898));
  printf("gcd %d %d\n", mygcd(12, 60), mygcd(5, 50));
  printf("breakandcall %d\n", breakandcall());

  int ar[5] = {1, 2, 3, 4, 5};
  printf("arraysum %d\n", arraysum(ar, 4));
  printf("ass %d\n", ass(1, 2));

  int src[5] = {1, 2, 3, 4, 5}, dst[5] = {0};
  copyarr(src, dst);
  printf("copyarr %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4]);

  char text[100] = "Dont Panic, Panic only results in more Panic.";
  char pattern[100] = "Panic";
  int ans[100];
  kmp(text, (int)strlen(text), pattern, (int)strlen(pattern), ans);
  printf("kmp");
  for (int i = 0; i < 100 && ans[i] != -1; i++)
    printf(" %d", ans[i]);
  printf("\n");

  int nums[10] = {1, 3, 4, 1, 3, 22, 2, 4, 6, 8};
  sort(nums, 10);
  printf("sort");
  for (int i = 0; i < 10; i++)
    printf(" %d", nums[i]);
  printf("\n");
  return 0;
}
