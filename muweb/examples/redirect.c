#define MULIB_IMPL_REDIRECT
#include "mulib-redirect.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
assert(mulib_redirect_status(302));assert(!mulib_redirect_post_to_get(307));
return 0;
}
