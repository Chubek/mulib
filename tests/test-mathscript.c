/* Standalone API contract test for mulib-mathscript. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHSCRIPT
#include "mulib-mathscript.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double y=0;assert(mulib_mathscript_offset(10,6,2,&y)&&y==12);return 0;}
