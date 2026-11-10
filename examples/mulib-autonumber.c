/* Build: cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-autonumber.c -lm -o demo */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_AUTONUMBER
#include "mulib-autonumber.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_autonumber_counter_t c={0};unsigned n=0;assert(mulib_autonumber_next(&c,0,&n)&&n==1);assert(mulib_autonumber_next(&c,1,&n)&&n==1);assert(mulib_autonumber_next(&c,0,&n)&&n==2);return 0;}
