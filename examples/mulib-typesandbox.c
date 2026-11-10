/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typesandbox.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPESANDBOX
#include "mulib-typesandbox.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typesandbox_policy_t p;mulib_typesandbox_init(&p,100);mulib_typesandbox_charge(&p,12);printf("remaining %zu\\n",mulib_typesandbox_remaining(&p));return 0;}
