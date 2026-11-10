/* Standalone API contract test for mulib-mathnumber. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MATHNUMBER
#include "mulib-mathnumber.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_mathnumber_table_t t={0}; assert(mulib_mathnumber_set(&t,"example","first"));assert(mulib_mathnumber_set(&t,"example","second")); assert(!strcmp(mulib_mathnumber_get(&t,"example"),"second"));assert(mulib_mathnumber_erase(&t,"example")); assert(!mulib_mathnumber_get(&t,"example")); mulib_mathnumber_destroy(&t);return 0;}
