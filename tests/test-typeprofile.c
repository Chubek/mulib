/* Standalone API contract test for mulib-typeprofile. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEPROFILE
#include "mulib-typeprofile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeprofile_policy_t p;mulib_typeprofile_init(&p,10);assert(mulib_typeprofile_charge(&p,6));assert(!mulib_typeprofile_charge(&p,5));assert(mulib_typeprofile_remaining(&p)==4);return 0;}
