/* Standalone API contract test for mulib-textrace. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXTRACE
#include "mulib-textrace.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_textrace_table_t t={0}; assert(mulib_textrace_set(&t,"example","first"));assert(mulib_textrace_set(&t,"example","second")); assert(!strcmp(mulib_textrace_get(&t,"example"),"second"));assert(mulib_textrace_erase(&t,"example")); assert(!mulib_textrace_get(&t,"example")); mulib_textrace_destroy(&t);return 0;}
