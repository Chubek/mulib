/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-unicodeprops.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_UNICODEPROPS
#include "mulib-unicodeprops.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){assert(mulib_unicodeprops_valid(0x1f600));assert(!mulib_unicodeprops_valid(0xd800));assert(mulib_unicodeprops_rtl(0x05d0));assert(mulib_unicodeprops_class('A')==mulib_unicodeprops_LETTER);return 0;}
