#define MULIB_IMPL_NETPOLICY
#include "mulib-netpolicy.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
assert(!mulib_netpolicy_public_url("http://127.0.0.1/a"));assert(mulib_netpolicy_public_url("https://example.com/"));
return 0;
}
