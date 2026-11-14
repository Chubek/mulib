#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
int64_t on1x_external_record(int64_t x){printf("%lld\n",(long long)x);return x;}
int64_t on1x_external_add(int64_t a,int64_t b){return a+b;}
bool on1x_external_condition(void){static int n=0;return ++n <= 2;}
