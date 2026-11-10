/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-sfnt.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_SFNT
#include "mulib-sfnt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char f[28]={0,1,0,0,0,1};memcpy(f+12,"head",4);f[23]=28;mulib_sfnt_font_t font;assert(mulib_sfnt_open(&font,f,sizeof(f)));mulib_typecore_slice_t table;assert(mulib_sfnt_table(&font,"head",&table));assert(table.size==0);return 0;}
