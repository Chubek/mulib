/* Standalone API contract test for mulib-unicodebreak. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_UNICODEBREAK
#include "mulib-unicodebreak.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){assert(mulib_unicodebreak_possible("one two",7,4));assert(!mulib_unicodebreak_possible("one two",7,2));return 0;}
