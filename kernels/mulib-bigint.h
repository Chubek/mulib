/* MuLIB browser staging addition: bigint
 * Implemented subset: Signed bounded 256-bit integer add/subtract, multiply, decimal parse and format.
 * Remaining: Unbounded limbs, general division, shifts, bitwise operations and runtime integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BIGINT_H
#define MULIB_BIGINT_H
#include "detail/mulib-browser-additions-common.h"

#define MULIB_BIGINT_LIMBS 8
typedef struct {uint32_t limb[MULIB_BIGINT_LIMBS];int sign;} mulib_bigint_t;
static inline int mulib_bigint_cmpmag(const mulib_bigint_t *a,const mulib_bigint_t *b){for(size_t i=MULIB_BIGINT_LIMBS;i--;){if(a->limb[i]!=b->limb[i])return a->limb[i]>b->limb[i]?1:-1;}return 0;}
static inline void mulib_bigint_normalize(mulib_bigint_t *a){for(size_t i=0;i<MULIB_BIGINT_LIMBS;i++)if(a->limb[i])return;a->sign=0;}
static inline int mulib_bigint_add(const mulib_bigint_t *a,const mulib_bigint_t *b,mulib_bigint_t *out){if(!a||!b||!out)return 0;mulib_bigint_t r={{0},0};if(!a->sign){*out=*b;return 1;}if(!b->sign){*out=*a;return 1;}if(a->sign==b->sign){uint64_t c=0;for(size_t i=0;i<MULIB_BIGINT_LIMBS;i++){uint64_t v=(uint64_t)a->limb[i]+b->limb[i]+c;r.limb[i]=(uint32_t)v;c=v>>32;}if(c)return 0;r.sign=a->sign;}else{const mulib_bigint_t *x=a,*y=b;if(mulib_bigint_cmpmag(a,b)<0){x=b;y=a;}uint64_t borrow=0;for(size_t i=0;i<MULIB_BIGINT_LIMBS;i++){uint64_t sub=(uint64_t)y->limb[i]+borrow;uint64_t v=x->limb[i];r.limb[i]=(uint32_t)(v-sub);borrow=v<sub;}r.sign=x->sign;}mulib_bigint_normalize(&r);*out=r;return 1;}
static inline int mulib_bigint_sub(const mulib_bigint_t *a,const mulib_bigint_t *b,mulib_bigint_t *out){if(!b)return 0;mulib_bigint_t n=*b;n.sign=-n.sign;return mulib_bigint_add(a,&n,out);}
static inline int mulib_bigint_mul_small(mulib_bigint_t *a,uint32_t v){if(!a)return 0;mulib_bigint_t r=*a;uint64_t c=0;for(size_t i=0;i<MULIB_BIGINT_LIMBS;i++){uint64_t x=(uint64_t)r.limb[i]*v+c;r.limb[i]=(uint32_t)x;c=x>>32;}if(c)return 0;mulib_bigint_normalize(&r);*a=r;return 1;}
static inline int mulib_bigint_parse(muba_span s,mulib_bigint_t *out){if(!out||!muba_span_ok(s)||!s.len)return 0;size_t i=0;int neg=s.data[0]=='-';if(neg||s.data[0]=='+')i++;if(i==s.len)return 0;mulib_bigint_t r={{0},0};for(;i<s.len;i++){if(!muba_digit(s.data[i])||!mulib_bigint_mul_small(&r,10))return 0;mulib_bigint_t d={{(uint32_t)(s.data[i]-'0')},1};mulib_bigint_normalize(&d);if(!mulib_bigint_add(&r,&d,&r))return 0;}if(neg)r.sign=-r.sign;*out=r;return 1;}
static inline int mulib_bigint_mul(const mulib_bigint_t *a,const mulib_bigint_t *b,mulib_bigint_t *out)
{
    if(!a||!b||!out)return 0;
    uint32_t wide[2*MULIB_BIGINT_LIMBS]={0};
    for(size_t i=0;i<MULIB_BIGINT_LIMBS;i++){
        uint64_t carry=0;
        for(size_t j=0;j<MULIB_BIGINT_LIMBS;j++){
            uint64_t x=(uint64_t)a->limb[i]*b->limb[j]+wide[i+j]+carry;
            wide[i+j]=(uint32_t)x;carry=x>>32;
        }
        wide[i+MULIB_BIGINT_LIMBS]=(uint32_t)carry;
    }
    for(size_t i=MULIB_BIGINT_LIMBS;i<2*MULIB_BIGINT_LIMBS;i++)if(wide[i])return 0;
    mulib_bigint_t r={{0},a->sign*b->sign};memcpy(r.limb,wide,sizeof r.limb);
    mulib_bigint_normalize(&r);*out=r;return 1;
}
static inline int mulib_bigint_format(const mulib_bigint_t *a,char *out,size_t cap)
{
    if(!a||!out)return 0;
    mulib_bigint_t q=*a;char rev[80];size_t n=0;
    do{uint64_t rem=0;for(size_t i=MULIB_BIGINT_LIMBS;i--;){uint64_t x=(rem<<32)|q.limb[i];q.limb[i]=(uint32_t)(x/10);rem=x%10;}rev[n++]=(char)('0'+rem);mulib_bigint_normalize(&q);}while(q.sign);
    if(a->sign<0)rev[n++]='-';
    if(n>=cap)return 0;
    for(size_t i=0;i<n;i++)out[i]=rev[n-1-i];
    out[n]=0;return 1;
}
#endif /* MULIB_BIGINT_H */
