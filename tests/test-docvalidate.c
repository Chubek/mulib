/* Standalone API contract test for mulib-docvalidate. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCVALIDATE
#include "mulib-docvalidate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docvalidate_policy_t p;mulib_docvalidate_init(&p,10);assert(mulib_docvalidate_charge(&p,6));assert(!mulib_docvalidate_charge(&p,5));assert(mulib_docvalidate_remaining(&p)==4);return 0;}
