/* Standalone API contract test for mulib-mathfraction. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHFRACTION
#include "mulib-mathfraction.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathfraction_metrics_t m;assert(mulib_mathfraction_stack(40,10,20,8,4,1,&m));assert(m.width==40&&m.height==14.5);return 0;}
