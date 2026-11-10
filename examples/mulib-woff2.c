/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-woff2.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_WOFF2
#include "mulib-woff2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char b[]={0,1,2,3};mulib_woff2_view_t v;mulib_woff2_open(&v,b,sizeof(b));uint32_t x=0;mulib_woff2_read_u32(&v,0,&x);printf("word: %u\\n",x);return 0;}
