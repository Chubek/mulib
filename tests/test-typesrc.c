/* Standalone API contract test for mulib-typesrc. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPESRC
#include "mulib-typesrc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typesrc_source_t s={0};assert(mulib_typesrc_init(&s,"one\ntwo",7));size_t l=0,c=0;assert(mulib_typesrc_locate(&s,5,&l,&c));assert(l==2&&c==2);assert(mulib_typesrc_assign(&s,s.data+4,3)&&s.size==3&&!strcmp(s.data,"two"));assert(!mulib_typesrc_assign(&s,NULL,1)&&s.size==3);assert(!mulib_typesrc_assign(&s,"x",SIZE_MAX));mulib_typesrc_destroy(&s);s.size=1;l=99;c=99;assert(!mulib_typesrc_locate(&s,1,&l,&c)&&l==99&&c==99);s.size=0;return 0;}
