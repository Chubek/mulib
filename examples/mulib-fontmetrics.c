/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontmetrics.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTMETRICS
#include "mulib-fontmetrics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char data[8]={0,10,0,2,0,20,0,3};uint16_t a=0,b=0;assert(mulib_fontmetrics_advance(data,8,2,1,&a,&b));assert(a==20&&b==3);return 0;}
