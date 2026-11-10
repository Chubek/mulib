/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docnormalize.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCNORMALIZE
#include "mulib-docnormalize.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecore_buf_t b={0};mulib_docnormalize_write_cstr(&b,"A&B");mulib_typecore_ends(&b);printf("%s\\n",(char*)b.data);mulib_typecore_free(&b);return 0;}
