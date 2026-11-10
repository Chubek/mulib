/* Standalone API contract test for mulib-mathoperator. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHOPERATOR
#include "mulib-mathoperator.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathoperator_table_t t={0}; assert(mulib_mathoperator_set(&t,"example","first"));assert(mulib_mathoperator_set(&t,"example","second")); assert(!strcmp(mulib_mathoperator_get(&t,"example"),"second"));assert(mulib_mathoperator_erase(&t,"example")); assert(!mulib_mathoperator_get(&t,"example")); mulib_mathoperator_destroy(&t);return 0;}
