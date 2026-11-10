/* Standalone API contract test for mulib-texcatcode. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXCATCODE
#include "mulib-texcatcode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texcatcode_table_t t;mulib_texcatcode_init(&t);assert(mulib_texcatcode_get(&t,'a')==mulib_texcatcode_LETTER);mulib_texcatcode_set(&t,'@',mulib_texcatcode_LETTER);assert(mulib_texcatcode_get(&t,'@')==mulib_texcatcode_LETTER);return 0;}
