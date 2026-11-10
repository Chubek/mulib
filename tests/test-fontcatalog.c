/* Standalone API contract test for mulib-fontcatalog. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTCATALOG
#include "mulib-fontcatalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_fontcatalog_table_t t={0}; assert(mulib_fontcatalog_set(&t,"example","first"));assert(mulib_fontcatalog_set(&t,"example","second")); assert(!strcmp(mulib_fontcatalog_get(&t,"example"),"second"));assert(mulib_fontcatalog_erase(&t,"example")); assert(!mulib_fontcatalog_get(&t,"example")); mulib_fontcatalog_destroy(&t);return 0;}
