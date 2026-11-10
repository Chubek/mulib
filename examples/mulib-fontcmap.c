/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontcmap.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTCMAP
#include "mulib-fontcmap.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char t[28]={0,12,0,0,0,0,0,28,0,0,0,0,0,0,0,1,0,0,0,65,0,0,0,65,0,0,0,7};mulib_fontcmap_map_t m;assert(mulib_fontcmap_open_format12(&m,t,sizeof(t)));uint32_t g=0;assert(mulib_fontcmap_glyph(&m,65,&g)&&g==7);assert(mulib_fontcmap_glyph(&m,66,&g)&&g==0);return 0;}
