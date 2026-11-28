/* MuLIB browser staging addition: punycode
 * Implemented subset: RFC 3492 encoding of bounded Unicode scalar labels.
 * Remaining: Decoder, full IDNA validation, normalization and conformance tests.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_PUNYCODE_H
#define MULIB_PUNYCODE_H
#include "detail/mulib-browser-additions-common.h"

static inline unsigned char mulib_punycode_digit(uint64_t d){return (unsigned char)(d<26?'a'+d:'0'+d-26);}
static inline uint64_t mulib_punycode_adapt(uint64_t d,uint64_t points,int first){d=first?d/700:d/2;d+=d/points;uint64_t k=0;while(d>455){d/=35;k+=36;}return k+(36*d)/(d+38);}
static inline int mulib_punycode_encode(const uint32_t *s,size_t n,muba_buf *out){if(!out||(!s&&n)||n>1024)return 0;muba_buf b={0};uint64_t cp=128,delta=0,bias=72;size_t basic=0;for(size_t i=0;i<n;i++){if(s[i]>0x10ffff||(s[i]>=0xd800&&s[i]<=0xdfff))goto fail;if(s[i]<128){if(!muba_byte(&b,(unsigned char)s[i]))goto fail;basic++;}}size_t h=basic;if(basic&&!muba_byte(&b,'-'))goto fail;while(h<n){uint64_t m=UINT64_MAX;for(size_t i=0;i<n;i++)if(s[i]>=cp&&s[i]<m)m=s[i];delta+=(m-cp)*(h+1);cp=m;for(size_t i=0;i<n;i++){if(s[i]<cp)delta++;if(s[i]==cp){uint64_t q=delta;for(uint64_t k=36;;k+=36){uint64_t t=k<=bias?1:k>=bias+26?26:k-bias;if(q<t)break;if(!muba_byte(&b,mulib_punycode_digit(t+(q-t)%(36-t))))goto fail;q=(q-t)/(36-t);}if(!muba_byte(&b,mulib_punycode_digit(q)))goto fail;bias=mulib_punycode_adapt(delta,h+1,h==basic);delta=0;h++;}}delta++;cp++;}return muba_commit(out,&b);fail:muba_free(&b);return 0;}
#endif /* MULIB_PUNYCODE_H */
