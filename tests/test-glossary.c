/* Standalone API contract test for mulib-glossary. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_GLOSSARY
#include "mulib-glossary.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_glossary_table_t t={0}; assert(mulib_glossary_set(&t,"example","first"));assert(mulib_glossary_set(&t,"example","second")); assert(!strcmp(mulib_glossary_get(&t,"example"),"second"));assert(mulib_glossary_erase(&t,"example")); assert(!mulib_glossary_get(&t,"example")); mulib_glossary_destroy(&t);return 0;}
