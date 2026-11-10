/* Standalone API contract test for mulib-typeparallel. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEPARALLEL
#include "mulib-typeparallel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeparallel_policy_t p;mulib_typeparallel_init(&p,10);assert(mulib_typeparallel_charge(&p,6));assert(!mulib_typeparallel_charge(&p,5));assert(mulib_typeparallel_remaining(&p)==4);return 0;}
