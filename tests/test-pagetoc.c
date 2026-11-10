/* Standalone API contract test for mulib-pagetoc. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGETOC
#include "mulib-pagetoc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagetoc_table_t t={0}; assert(mulib_pagetoc_set(&t,"example","first"));assert(mulib_pagetoc_set(&t,"example","second")); assert(!strcmp(mulib_pagetoc_get(&t,"example"),"second"));assert(mulib_pagetoc_erase(&t,"example")); assert(!mulib_pagetoc_get(&t,"example")); mulib_pagetoc_destroy(&t);return 0;}
