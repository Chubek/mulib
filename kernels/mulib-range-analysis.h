/* MuLIB compiler tools: Checked interval propagation for integer constants and arithmetic.
 * ISO C11 single-header module. Define MULIB_IMPL_RANGE_ANALYSIS in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_RANGE_ANALYSIS_H
#define MULIB_RANGE_ANALYSIS_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {int64_t lo,hi;
int known;
} mulib_range_t;
mulib_range_t mulib_range_const(int64_t value);
mulib_range_t mulib_range_join(mulib_range_t a,mulib_range_t b);
mulib_range_t mulib_range_add(mulib_range_t a,mulib_range_t b);
mulib_range_t mulib_range_sub(mulib_range_t a,mulib_range_t b);
mulib_range_t mulib_range_mul(mulib_range_t a,mulib_range_t b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_RANGE_ANALYSIS_H */
#if defined(MULIB_IMPL_RANGE_ANALYSIS) && !defined(MULIB_RANGE_ANALYSIS_IMPLEMENTED)
#define MULIB_RANGE_ANALYSIS_IMPLEMENTED
#include "detail/mulib-compiler-integer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_range_t mulib_range_const(int64_t x){return (mulib_range_t){x,x,1};
}
mulib_range_t mulib_range_join(mulib_range_t a,mulib_range_t b){if(!a.known||!b.known||a.lo>a.hi||b.lo>b.hi)return (mulib_range_t){0,0,0};
return (mulib_range_t){a.lo<b.lo?a.lo:b.lo,a.hi>b.hi?a.hi:b.hi,1};
}
static int mulib_range_sum_(int64_t a,int64_t b,int64_t *out){if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))return 0;
*out=a+b;
return 1;
}
mulib_range_t mulib_range_add(mulib_range_t a,mulib_range_t b){mulib_range_t r={0,0,0};
if(a.known&&b.known&&a.lo<=a.hi&&b.lo<=b.hi&&mulib_range_sum_(a.lo,b.lo,&r.lo)&&mulib_range_sum_(a.hi,b.hi,&r.hi))r.known=1;
return r;
}
mulib_range_t mulib_range_sub(mulib_range_t a,mulib_range_t b){mulib_range_t r={0,0,0};
if(!a.known||!b.known||a.lo>a.hi||b.lo>b.hi)return r;
if(mulib_compiler_sub_i64_(a.lo,b.hi,&r.lo)&&mulib_compiler_sub_i64_(a.hi,b.lo,&r.hi))r.known=1;
return r;
}
mulib_range_t mulib_range_mul(mulib_range_t a,mulib_range_t b){
mulib_range_t r={0,0,0};
int64_t products[4];
if(!a.known||!b.known||a.lo>a.hi||b.lo>b.hi||
 !mulib_compiler_mul_i64_(a.lo,b.lo,&products[0])||
 !mulib_compiler_mul_i64_(a.lo,b.hi,&products[1])||
 !mulib_compiler_mul_i64_(a.hi,b.lo,&products[2])||
 !mulib_compiler_mul_i64_(a.hi,b.hi,&products[3]))return r;
r.lo=r.hi=products[0];r.known=1;
for(size_t i=1;i<4;i++){if(products[i]<r.lo)r.lo=products[i];if(products[i]>r.hi)r.hi=products[i];}
return r;
}
#ifdef __cplusplus
}
#endif
#endif
