/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typesrc.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPESRC
#include "mulib-typesrc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typesrc_source_t s={0};assert(mulib_typesrc_init(&s,"one\ntwo",7));size_t l=0,c=0;assert(mulib_typesrc_locate(&s,5,&l,&c));assert(l==2&&c==2);mulib_typesrc_destroy(&s);return 0;}
