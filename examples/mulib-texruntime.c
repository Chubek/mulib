#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXRUNTIME
#include "mulib-texruntime.h"
#include <stdio.h>
int main(void){
  const char *program="fn fib(n)=n<=1?n:fib(n-1)+fib(n-2); eval fib(12)";
  double answer=0;
  if(!mulib_texruntime_eval(program,200000,64,&answer))return 1;
  printf("fibonacci(12)=%.0f\n",answer);
  return 0;
}
