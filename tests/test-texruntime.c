#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXRUNTIME
#include "mulib-texruntime.h"
#include <assert.h>
int main(void){
    double result=-100;
    assert(mulib_texruntime_eval("fn square(x)=x*x; eval square(7)+1;",5000,64,&result));
    assert(result==50);
    assert(mulib_texruntime_eval("fn fib(n)=n<=1?n:fib(n-1)+fib(n-2); fib(10);",200000,64,&result));
    assert(result==55);
    assert(mulib_texruntime_eval("fn fact(n)=n==0?1:n*fact(n-1); fact(8)",30000,64,&result));
    assert(result==40320);
    assert(!mulib_texruntime_eval("fn loop(x)=loop(x);loop(1)",10000,10,&result));
    assert(!mulib_texruntime_eval("1/0",100,10,&result));
    assert(result==40320);
    return 0;
}
