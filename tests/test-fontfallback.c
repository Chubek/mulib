/* Standalone API contract test for mulib-fontfallback. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTFALLBACK
#include "mulib-fontfallback.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_fontfallback_table_t t={0}; assert(mulib_fontfallback_set(&t,"example","first"));assert(mulib_fontfallback_set(&t,"example","second")); assert(!strcmp(mulib_fontfallback_get(&t,"example"),"second"));assert(mulib_fontfallback_erase(&t,"example")); assert(!mulib_fontfallback_get(&t,"example")); mulib_fontfallback_destroy(&t);return 0;}
