/* Standalone API contract test for mulib-texdim. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXDIM
#include "mulib-texdim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texdim_sp_t x=0,y=0;assert(mulib_texdim_from_pt(12.5,&x));assert(mulib_texdim_from_pt(1,&y));assert(mulib_texdim_add(x,y,&x));assert(mulib_texdim_to_pt(x)==13.5);return 0;}
