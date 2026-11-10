/* Standalone API contract test for mulib-microtype. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MICROTYPE
#include "mulib-microtype.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){double out=0;assert(mulib_microtype_compute(4,2,&out));assert(isfinite(out));assert(mulib_microtype_clamp(15,0,10,&out));assert(out==10);return 0;}
