/* Standalone API contract test for mulib-pagetemplate. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGETEMPLATE
#include "mulib-pagetemplate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pagetemplate_table_t t={0}; assert(mulib_pagetemplate_set(&t,"example","first"));assert(mulib_pagetemplate_set(&t,"example","second")); assert(!strcmp(mulib_pagetemplate_get(&t,"example"),"second"));assert(mulib_pagetemplate_erase(&t,"example")); assert(!mulib_pagetemplate_get(&t,"example")); mulib_pagetemplate_destroy(&t);return 0;}
