/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typeprofile.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TYPEPROFILE
#include "mulib-typeprofile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_typeprofile_policy_t p;mulib_typeprofile_init(&p,100);mulib_typeprofile_charge(&p,12);printf("remaining %zu\\n",mulib_typeprofile_remaining(&p));return 0;}
