#define MULIB_IMPL_WEBLOCATION
#include "mulib-weblocation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
mulib_weblocation_t l={0};assert(mulib_weblocation_set(&l,"https://example.com"));assert(l.href);mulib_weblocation_free(&l);
return 0;
}
