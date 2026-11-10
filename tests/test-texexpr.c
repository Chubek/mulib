/* Standalone API contract test for mulib-texexpr. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXEXPR
#include "mulib-texexpr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double x=0;assert(mulib_texexpr_evaluate("(2+3)*4-6/2",&x));assert(x==17);assert(!mulib_texexpr_evaluate("4/0",&x));return 0;}
