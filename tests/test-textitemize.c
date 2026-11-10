/* Standalone API contract test for mulib-textitemize. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXTITEMIZE
#include "mulib-textitemize.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char b[]={0,1,2,3,4,5};mulib_textitemize_view_t v;assert(mulib_textitemize_open(&v,b,sizeof(b)));uint16_t x=0;assert(mulib_textitemize_read_u16(&v,0,&x));assert(x==1);mulib_typecore_slice_t s;assert(!mulib_textitemize_slice(&v,5,2,&s));return 0;}
