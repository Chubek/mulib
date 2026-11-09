/* Focused executable regression for mulib-num-rootfind.h. */
#define MULIB_IMPL_NUM_ROOTFIND
#include "mulib-num-rootfind.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static double f0(double x,void*u){(void)u;return x*x-2;}
static double f1(double x,void*u){(void)u;return 2*x;}
int main(void) {
double x=0;assert(mulib_num_bisect(f0,NULL,1,2,1e-11,200,&x)&&fabs(x-1.4142135623730951)<1e-9);assert(!mulib_num_bisect(f0,NULL,2,3,1e-11,50,&x));assert(mulib_num_newton(f0,f1,NULL,1,1e-12,30,&x));
    puts("PASS mulib-num-rootfind");
    return 0;
}
