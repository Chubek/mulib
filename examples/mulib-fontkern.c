/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontkern.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTKERN
#include "mulib-fontkern.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char data[20]={0,0,0,20,0,1,0,1,0,0,0,0,0,0,0,1,0,2,0xff,0xfd};int16_t v=0;assert(mulib_fontkern_pair(data,sizeof(data),1,2,&v)&&v==-3);return 0;}
