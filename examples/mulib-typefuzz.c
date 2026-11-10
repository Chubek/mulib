/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typefuzz.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEFUZZ
#include "mulib-typefuzz.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typefuzz_policy_t p;mulib_typefuzz_init(&p,100);mulib_typefuzz_charge(&p,12);printf("remaining %zu\\n",mulib_typefuzz_remaining(&p));return 0;}
