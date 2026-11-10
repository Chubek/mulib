/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-unicodegrapheme.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_UNICODEGRAPHEME
#include "mulib-unicodegrapheme.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){const char *s="a\xCC\x81" "b";size_t at=0,n=0;assert(mulib_unicodegrapheme_next(s,4,&at,&n));assert(at==3&&n==3);return 0;}
