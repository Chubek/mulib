/* Standalone API contract test for mulib-docincremental. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCINCREMENTAL
#include "mulib-docincremental.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docincremental_policy_t p;mulib_docincremental_init(&p,10);assert(mulib_docincremental_charge(&p,6));assert(!mulib_docincremental_charge(&p,5));assert(mulib_docincremental_remaining(&p)==4);return 0;}
