/* Standalone API contract test for mulib-typefuzz. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEFUZZ
#include "mulib-typefuzz.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typefuzz_policy_t p;mulib_typefuzz_init(&p,10);assert(mulib_typefuzz_charge(&p,6));assert(!mulib_typefuzz_charge(&p,5));assert(mulib_typefuzz_remaining(&p)==4);return 0;}
