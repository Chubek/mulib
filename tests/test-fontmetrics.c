/* Standalone API contract test for mulib-fontmetrics. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTMETRICS
#include "mulib-fontmetrics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char data[10]={0,10,0,2,0,20,0xff,0xfd,0xff,0xfe};uint16_t a=0,b=0;assert(mulib_fontmetrics_advance(data,8,2,1,&a,&b));assert(a==20&&b==65533);int16_t signed_b=0;assert(mulib_fontmetrics_signed(data,10,2,2,&a,&signed_b)&&a==20&&signed_b==-2);a=123;b=456;assert(!mulib_fontmetrics_advance(data,8,2,2,&a,&b)&&a==123&&b==456);assert(!mulib_fontmetrics_signed(data,8,0,0,&a,&signed_b));assert(!mulib_fontmetrics_signed(NULL,10,2,1,&a,&signed_b));return 0;}
