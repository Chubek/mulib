#define MULIB_IMPL_BROWSERWAIT
#include "mulib-browserwait.h"
#include <assert.h>
#include <string.h>
int main(void) {
mulib_browserwait_t w;assert(mulib_browserwait_init(&w,10,100,20));assert(mulib_browserwait_poll(&w,10)==1);assert(mulib_browserwait_poll(&w,15)==0);assert(mulib_browserwait_poll(&w,111)==-1);
return 0;
}
