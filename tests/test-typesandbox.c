/* Standalone API contract test for mulib-typesandbox. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPESANDBOX
#include "mulib-typesandbox.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typesandbox_policy_t p;mulib_typesandbox_init(&p,10);assert(mulib_typesandbox_charge(&p,6));assert(!mulib_typesandbox_charge(&p,5));assert(mulib_typesandbox_remaining(&p)==4);return 0;}
