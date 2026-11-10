/* Standalone API contract test for mulib-csl. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_CSL
#include "mulib-csl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_csl_table_t t={0}; assert(mulib_csl_set(&t,"example","first"));assert(mulib_csl_set(&t,"example","second")); assert(!strcmp(mulib_csl_get(&t,"example"),"second"));assert(mulib_csl_erase(&t,"example")); assert(!mulib_csl_get(&t,"example")); mulib_csl_destroy(&t);return 0;}
