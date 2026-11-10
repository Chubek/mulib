/* Standalone API contract test for mulib-typeincremental. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEINCREMENTAL
#include "mulib-typeincremental.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeincremental_policy_t p;mulib_typeincremental_init(&p,10);assert(mulib_typeincremental_charge(&p,6));assert(!mulib_typeincremental_charge(&p,5));assert(mulib_typeincremental_remaining(&p)==4);return 0;}
