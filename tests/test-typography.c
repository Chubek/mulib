/* Standalone API contract test for mulib-typography. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPOGRAPHY
#include "mulib-typography.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typography_table_t t={0}; assert(mulib_typography_set(&t,"example","first"));assert(mulib_typography_set(&t,"example","second")); assert(!strcmp(mulib_typography_get(&t,"example"),"second"));assert(mulib_typography_erase(&t,"example")); assert(!mulib_typography_get(&t,"example")); mulib_typography_destroy(&t);return 0;}
