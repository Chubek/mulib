/* Standalone API contract test for mulib-typestatus. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPESTATUS
#include "mulib-typestatus.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){assert(!strcmp(mulib_typestatus_name(mulib_typestatus_UNSUPPORTED),"unsupported feature"));return 0;}
