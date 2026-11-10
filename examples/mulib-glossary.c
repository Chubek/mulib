/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-glossary.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_GLOSSARY
#include "mulib-glossary.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_glossary_table_t t={0};mulib_glossary_set(&t,"title","Example");printf("%s\\n",mulib_glossary_get(&t,"title"));mulib_glossary_destroy(&t);return 0;}
