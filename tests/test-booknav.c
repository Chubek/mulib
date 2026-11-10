/* Standalone API contract test for mulib-booknav. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_BOOKNAV
#include "mulib-booknav.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_booknav_table_t t={0}; assert(mulib_booknav_set(&t,"example","first"));assert(mulib_booknav_set(&t,"example","second")); assert(!strcmp(mulib_booknav_get(&t,"example"),"second"));assert(mulib_booknav_erase(&t,"example")); assert(!mulib_booknav_get(&t,"example")); mulib_booknav_destroy(&t);return 0;}
