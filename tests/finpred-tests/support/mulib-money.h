/* TEST-ONLY FALLBACK: ABI-shaped subset of MuLIB Money for offline bootstrap.
 * DO NOT install/ship this as an application header. For canonical MuLIB tests
 * set MULIB_ROOT and compile with its include/ before tests/support/ .
 * Uses long double in scale, unlike upstream's checked integer algorithm.
 */
#ifndef FINPRED_TEST_MONEY_H
#define FINPRED_TEST_MONEY_H
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <string.h>
typedef struct mulib_money {int64_t minor;char currency[4];uint8_t decimals;} mulib_money_t;
static inline int mulib_money_valid(mulib_money_t m){
    return m.decimals<=6 && m.currency[3]==0 &&
      m.currency[0]>='A'&&m.currency[0]<='Z'&&
      m.currency[1]>='A'&&m.currency[1]<='Z'&&
      m.currency[2]>='A'&&m.currency[2]<='Z';
}
static inline int mulib_money_make(mulib_money_t *out,int64_t v,const char *c,uint8_t d){
    mulib_money_t m={0};
    if(!out||!c||strlen(c)!=3||d>6)return 0;
    m.minor=v;memcpy(m.currency,c,3);m.decimals=d;
    if(!mulib_money_valid(m))return 0;
    *out=m;return 1;
}
static inline int mulib_money_same_(mulib_money_t a,mulib_money_t b){
    return mulib_money_valid(a)&&mulib_money_valid(b)&&
           a.decimals==b.decimals&&!memcmp(a.currency,b.currency,4);
}
static inline int mulib_money_add(mulib_money_t a,mulib_money_t b,mulib_money_t *out){
    if(!out||!mulib_money_same_(a,b)||
      (b.minor>0 && a.minor>INT64_MAX-b.minor)||
      (b.minor<0 && a.minor<INT64_MIN-b.minor))return 0;
    a.minor+=b.minor;*out=a;return 1;
}
static inline int mulib_money_sub(mulib_money_t a,mulib_money_t b,mulib_money_t *out){
    if(!out||!mulib_money_same_(a,b)||
      (b.minor<0 && a.minor>INT64_MAX+b.minor)||
      (b.minor>0 && a.minor<INT64_MIN+b.minor))return 0;
    a.minor-=b.minor;*out=a;return 1;
}
static inline int mulib_money_scale(mulib_money_t a,int64_t n,int64_t d,mulib_money_t *out){
    long double z;
    if(!out||!mulib_money_valid(a)||n<0||d<=0)return 0;
    z=roundl((long double)a.minor*(long double)n/(long double)d);
    if(!isfinite(z)||z>INT64_MAX||z<INT64_MIN)return 0;
    a.minor=(int64_t)z;*out=a;return 1;
}
#endif
