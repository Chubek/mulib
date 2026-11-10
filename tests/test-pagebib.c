/* Standalone API contract test for mulib-pagebib. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEBIB
#include "mulib-pagebib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagebib_table_t t={0}; assert(mulib_pagebib_set(&t,"example","first"));assert(mulib_pagebib_set(&t,"example","second")); assert(!strcmp(mulib_pagebib_get(&t,"example"),"second"));assert(mulib_pagebib_erase(&t,"example")); assert(!mulib_pagebib_get(&t,"example")); mulib_pagebib_destroy(&t);return 0;}
