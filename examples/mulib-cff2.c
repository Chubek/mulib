/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-cff2.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_CFF2
#include "mulib-cff2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char b[]={0,1,2,3};mulib_cff2_view_t v;mulib_cff2_open(&v,b,sizeof(b));uint32_t x=0;mulib_cff2_read_u32(&v,0,&x);printf("word: %u\\n",x);return 0;}
