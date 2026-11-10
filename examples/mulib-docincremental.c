/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docincremental.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCINCREMENTAL
#include "mulib-docincremental.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docincremental_policy_t p;mulib_docincremental_init(&p,100);mulib_docincremental_charge(&p,12);printf("remaining %zu\\n",mulib_docincremental_remaining(&p));return 0;}
