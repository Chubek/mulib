/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typeincremental.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEINCREMENTAL
#include "mulib-typeincremental.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeincremental_policy_t p;mulib_typeincremental_init(&p,100);mulib_typeincremental_charge(&p,12);printf("remaining %zu\\n",mulib_typeincremental_remaining(&p));return 0;}
