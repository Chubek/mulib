/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-colorprofile.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_COLORPROFILE
#include "mulib-colorprofile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_colorprofile_table_t t={0};mulib_colorprofile_set(&t,"title","Example");printf("%s\\n",mulib_colorprofile_get(&t,"title"));mulib_colorprofile_destroy(&t);return 0;}
