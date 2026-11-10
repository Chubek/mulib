/* Standalone API contract test for mulib-fontkern. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTKERN
#include "mulib-fontkern.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char data[20]={0,0,0,20,0,1,0,1,0,0,0,0,0,0,0,1,0,2,0xff,0xfd};int16_t v=0;assert(mulib_fontkern_pair(data,sizeof(data),1,2,&v)&&v==-3);assert(mulib_fontkern_pair(data,sizeof(data),2,1,&v)&&v==0);data[3]=0;v=42;assert(!mulib_fontkern_pair(data,sizeof(data),1,2,&v)&&v==42);data[3]=20;data[4]=1;assert(!mulib_fontkern_pair(data,sizeof(data),1,2,&v)&&v==42);assert(!mulib_fontkern_pair(NULL,20,1,2,&v));return 0;}
