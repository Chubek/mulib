#define MULIB_IMPL_WEBSTORAGE
#include "mulib-webstorage.h"
#include <assert.h>
#include <string.h>
int main(void) {
mulib_webstorage_t s={0};assert(mulib_webstorage_set(&s,"x","y"));assert(!strcmp(mulib_webstorage_get(&s,"x"),"y"));mulib_webstorage_free(&s);
return 0;
}
