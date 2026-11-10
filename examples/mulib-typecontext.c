/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typecontext.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPECONTEXT
#include "mulib-typecontext.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typecontext_context_t c;mulib_typecontext_init(&c,8);assert(mulib_typecontext_append(&c,"hello",5));assert(!mulib_typecontext_append(&c,"world",5));mulib_typecontext_reset(&c);mulib_typecontext_destroy(&c);return 0;}
