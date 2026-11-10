/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-textitemize.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXTITEMIZE
#include "mulib-textitemize.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char b[]={0,1,2,3};mulib_textitemize_view_t v;mulib_textitemize_open(&v,b,sizeof(b));uint32_t x=0;mulib_textitemize_read_u32(&v,0,&x);printf("word: %u\\n",x);return 0;}
