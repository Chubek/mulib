/* Standalone API contract test for mulib-unicodebidi. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_UNICODEBIDI
#include "mulib-unicodebidi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned d=99;assert(mulib_unicodebidi_base("abc",3,&d)&&d==mulib_unicodebidi_LTR);return 0;}
