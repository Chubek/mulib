/* Standalone API contract test for mulib-docsemantic. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCSEMANTIC
#include "mulib-docsemantic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docsemantic_table_t t={0}; assert(mulib_docsemantic_set(&t,"example","first"));assert(mulib_docsemantic_set(&t,"example","second")); assert(!strcmp(mulib_docsemantic_get(&t,"example"),"second"));assert(mulib_docsemantic_erase(&t,"example")); assert(!mulib_docsemantic_get(&t,"example")); mulib_docsemantic_destroy(&t);return 0;}
