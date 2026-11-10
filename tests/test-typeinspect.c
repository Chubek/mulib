/* Standalone API contract test for mulib-typeinspect. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEINSPECT
#include "mulib-typeinspect.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeinspect_table_t t={0}; assert(mulib_typeinspect_set(&t,"example","first"));assert(mulib_typeinspect_set(&t,"example","second")); assert(!strcmp(mulib_typeinspect_get(&t,"example"),"second"));assert(mulib_typeinspect_erase(&t,"example")); assert(!mulib_typeinspect_get(&t,"example")); mulib_typeinspect_destroy(&t);return 0;}
