#define MULIB_IMPL_CHARSET
#include "mulib-charset.h"
#include <assert.h>
#include <string.h>
int main(void) {
char b[32];assert(mulib_charset_extract("text/html; charset=\"UTF-8\"",b,sizeof b));assert(!strcmp(b,"utf-8"));
return 0;
}
