/* Standalone API contract test for mulib-mathparse. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHPARSE
#include "mulib-mathparse.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){size_t i=0;assert(mulib_mathparse_validate("x^{2}+y_1",10,&i));assert(!mulib_mathparse_validate("x^{2",4,&i));return 0;}
