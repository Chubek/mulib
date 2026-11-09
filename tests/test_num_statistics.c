/* Focused executable regression for mulib-num-statistics.h. */
#define MULIB_IMPL_NUM_STATISTICS
#include "mulib-num-statistics.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_num_stat_t s={0};double v;for(int i=1;i<=5;i++)assert(mulib_num_stat_push(&s,i));assert(s.mean==3&&s.min==1&&s.max==5);assert(mulib_num_stat_variance(&s,&v)&&fabs(v-2.5)<1e-12);assert(!mulib_num_stat_push(&s,NAN));
    puts("PASS mulib-num-statistics");
    return 0;
}
