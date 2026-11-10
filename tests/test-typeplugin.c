/* Standalone API contract test for mulib-typeplugin. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEPLUGIN
#include "mulib-typeplugin.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeplugin_table_t t={0}; assert(mulib_typeplugin_set(&t,"example","first"));assert(mulib_typeplugin_set(&t,"example","second")); assert(!strcmp(mulib_typeplugin_get(&t,"example"),"second"));assert(mulib_typeplugin_erase(&t,"example")); assert(!mulib_typeplugin_get(&t,"example")); mulib_typeplugin_destroy(&t);return 0;}
