/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docvalidate.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCVALIDATE
#include "mulib-docvalidate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docvalidate_policy_t p;mulib_docvalidate_init(&p,100);mulib_docvalidate_charge(&p,12);printf("remaining %zu\\n",mulib_docvalidate_remaining(&p));return 0;}
