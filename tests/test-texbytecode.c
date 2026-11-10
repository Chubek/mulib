/* Standalone API contract test for mulib-texbytecode. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXBYTECODE
#include "mulib-texbytecode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texbytecode_insn_t p[]={{mulib_texbytecode_PUSH,2},{mulib_texbytecode_PUSH,3},{mulib_texbytecode_MUL,0},{mulib_texbytecode_HALT,0}};double x=0;assert(mulib_texbytecode_run(p,4,20,&x));assert(x==6);return 0;}
