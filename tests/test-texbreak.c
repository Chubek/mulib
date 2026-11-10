/* Standalone API contract test for mulib-texbreak. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXBREAK
#include "mulib-texbreak.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double w[]={4,4,4,4};int p[]={0,0,0,0};mulib_texbreak_result_t r={0};assert(mulib_texbreak_solve(w,p,4,8,&r));assert(r.count==2&&r.breaks[0]==2&&r.breaks[1]==4);mulib_texbreak_destroy(&r);return 0;}
