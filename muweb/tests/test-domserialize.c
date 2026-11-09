#define MULIB_IMPL_DOMSERIALIZE
#include "mulib-domserialize.h"
#include <assert.h>
#include <string.h>
int main(void) {
char b[64];assert(mulib_domserialize_escape("<x&>",b,sizeof b));assert(!strcmp(b,"&lt;x&amp;&gt;"));
return 0;
}
