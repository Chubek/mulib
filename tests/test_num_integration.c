/* Focused executable regression for mulib-num-integration.h. */
#define MULIB_IMPL_NUM_INTEGRATION
#include "mulib-num-integration.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static double f0(double x,void*u){(void)u;return x*x;}
int main(void) {
double x=0;assert(mulib_num_integrate_trapezoid(f0,NULL,0,1,1000,&x)&&fabs(x-.3333333333)<1e-6);assert(mulib_num_integrate_simpson(f0,NULL,0,1,100,&x)&&fabs(x-.3333333333)<1e-9);assert(!mulib_num_integrate_simpson(f0,NULL,0,1,3,&x));
    puts("PASS mulib-num-integration");
    return 0;
}
