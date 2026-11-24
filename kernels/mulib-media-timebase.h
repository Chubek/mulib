/** @file mulib-media-timebase.h
 * Exact rational timebase normalization and checked rescaling.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: May reject very large intermediate products even if final quotient would fit.
 */
#ifndef MULIB_MEDIA_TIMEBASE_H
#define MULIB_MEDIA_TIMEBASE_H
#include "mulib-media-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline uint64_t mulib_media_timebase_gcd(uint64_t a,uint64_t b){while(b){uint64_t t=a%b;a=b;b=t;}return a;}
static inline bool mulib_media_timebase_normalize(mulib_media_rational *r){if(!r||r->num<=0||r->den<=0)return false;uint64_t g=mulib_media_timebase_gcd((uint64_t)r->num,(uint64_t)r->den);r->num/=(int64_t)g;r->den/=(int64_t)g;return true;}
/* Exact ratio reduction followed by checked integer arithmetic; rounds toward zero. */
static inline bool mulib_media_timebase_rescale(int64_t v,mulib_media_rational from,mulib_media_rational to,int64_t *out){
 if(!out||!mulib_media_timebase_normalize(&from)||!mulib_media_timebase_normalize(&to))return false;
 uint64_t a[3]={v<0?(uint64_t)(-(v+1))+1:(uint64_t)v,(uint64_t)from.num,(uint64_t)to.den}, b[2]={(uint64_t)from.den,(uint64_t)to.num};
 for(size_t i=0;i<3;i++)for(size_t j=0;j<2;j++){uint64_t g=mulib_media_timebase_gcd(a[i],b[j]);a[i]/=g;b[j]/=g;}
#if defined(__SIZEOF_INT128__)
 __extension__ typedef unsigned __int128 wide;wide n=1,d=(wide)b[0]*b[1];for(size_t i=0;i<3;i++){if(a[i]&&n>(~(wide)0)/a[i])return false;n*=a[i];}wide q=n/d;uint64_t max=(uint64_t)INT64_MAX+(v<0);if(q>max)return false;uint64_t z=(uint64_t)q;
#else
 uint64_t n=1,d;if(b[1]&&b[0]>UINT64_MAX/b[1])return false;d=b[0]*b[1];for(size_t i=0;i<3;i++){if(a[i]&&n>UINT64_MAX/a[i])return false;n*=a[i];}uint64_t z=n/d;if(z>(uint64_t)INT64_MAX+(v<0))return false;
#endif
 *out=v<0?(z==((uint64_t)INT64_MAX+1)?INT64_MIN:-(int64_t)z):(int64_t)z;return true;}
#ifdef __cplusplus
}
#endif
#endif
