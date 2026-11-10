/* Standalone API contract test for mulib-locale. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_LOCALE
#include "mulib-locale.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_locale_table_t t={0}; assert(mulib_locale_set(&t,"example","first"));assert(mulib_locale_set(&t,"example","second")); assert(!strcmp(mulib_locale_get(&t,"example"),"second"));assert(mulib_locale_erase(&t,"example")); assert(!mulib_locale_get(&t,"example")); mulib_locale_destroy(&t);return 0;}
