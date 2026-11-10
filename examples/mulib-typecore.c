#define MULIB_IMPL_TYPECORE
#include "mulib-typecore.h"
#include <stdio.h>
int main(void){mulib_typecore_buf_t b={0};if(!mulib_typecore_text(&b,"Hello MuLIB")||!mulib_typecore_ends(&b))return 1;puts((char*)b.data);mulib_typecore_free(&b);return 0;}
