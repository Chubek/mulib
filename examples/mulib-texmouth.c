/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texmouth.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXMOUTH
#include "mulib-texmouth.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texmouth_scanner_t s;mulib_texmouth_token_t t;assert(mulib_texmouth_init(&s,"\\\\section{Hi}",12));assert(mulib_texmouth_next(&s,&t)==1);assert(t.kind==mulib_texmouth_CONTROL);return 0;}
