/* Standalone API contract test for mulib-docstyle. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCSTYLE
#include "mulib-docstyle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docstyle_table_t t={0}; assert(mulib_docstyle_set(&t,"example","first"));assert(mulib_docstyle_set(&t,"example","second")); assert(!strcmp(mulib_docstyle_get(&t,"example"),"second"));assert(mulib_docstyle_erase(&t,"example")); assert(!mulib_docstyle_get(&t,"example")); mulib_docstyle_destroy(&t);return 0;}
