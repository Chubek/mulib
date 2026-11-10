/* Standalone API contract test for mulib-ttf. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TTF
#include "mulib-ttf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char b[]={0,1,2,3,4,5};mulib_ttf_view_t v;assert(mulib_ttf_open(&v,b,sizeof(b)));uint16_t x=0;assert(mulib_ttf_read_u16(&v,0,&x));assert(x==1);mulib_typecore_slice_t s;assert(!mulib_ttf_slice(&v,5,2,&s));return 0;}
