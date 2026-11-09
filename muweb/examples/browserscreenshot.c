#define MULIB_IMPL_BROWSERSCREENSHOT
#include "mulib-browserscreenshot.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
size_t n=0;assert(mulib_browserscreenshot_size(10,20,4,&n));assert(n==800);
return 0;
}
