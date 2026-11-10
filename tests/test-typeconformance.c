/* Standalone API contract test for mulib-typeconformance. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPECONFORMANCE
#include "mulib-typeconformance.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeconformance_table_t t={0}; assert(mulib_typeconformance_set(&t,"example","first"));assert(mulib_typeconformance_set(&t,"example","second")); assert(!strcmp(mulib_typeconformance_get(&t,"example"),"second"));assert(mulib_typeconformance_erase(&t,"example")); assert(!mulib_typeconformance_get(&t,"example")); mulib_typeconformance_destroy(&t);return 0;}
