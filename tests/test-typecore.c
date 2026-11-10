#define MULIB_IMPL_TYPECORE
#include "mulib-typecore.h"
#include <assert.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};assert(mulib_typecore_text(&b,"Hello"));assert(mulib_typecore_append(&b,b.data,5));assert(b.size==10);assert(mulib_typecore_ends(&b));assert(!strcmp((char*)b.data,"HelloHello"));mulib_typecore_free(&b);assert(mulib_typecore_crc32("123456789",9)==0xcbf43926u);return 0;}
