/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typeparallel.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEPARALLEL
#include "mulib-typeparallel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeparallel_policy_t p;mulib_typeparallel_init(&p,100);mulib_typeparallel_charge(&p,12);printf("remaining %zu\\n",mulib_typeparallel_remaining(&p));return 0;}
