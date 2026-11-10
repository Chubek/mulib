/* Standalone API contract test for mulib-bibstyle. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_BIBSTYLE
#include "mulib-bibstyle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_bibstyle_table_t t={0}; assert(mulib_bibstyle_set(&t,"example","first"));assert(mulib_bibstyle_set(&t,"example","second")); assert(!strcmp(mulib_bibstyle_get(&t,"example"),"second"));assert(mulib_bibstyle_erase(&t,"example")); assert(!mulib_bibstyle_get(&t,"example")); mulib_bibstyle_destroy(&t);return 0;}
