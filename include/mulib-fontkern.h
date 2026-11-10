/** @file mulib-fontkern.h
 * @brief Simple legacy TrueType kern format 0 pair lookup.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_FONTKERN once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-fontkern.md for scope and limitations.
 */
#ifndef MULIB_FONTKERN_H
#define MULIB_FONTKERN_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fontkern_pair(const void *subtable,size_t length,uint16_t left,uint16_t right,int16_t *value);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FONTKERN_H */
#if defined(MULIB_IMPL_FONTKERN) && !defined(MULIB_FONTKERN_H_IMPLEMENTED)
#define MULIB_FONTKERN_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fontkern_pair(const void *p,size_t n,uint16_t left,uint16_t right,int16_t *out){if(!p||!out||n<14)return 0;mulib_typecore_slice_t s={(const unsigned char*)p,n};uint16_t version,length,coverage,count;if(!mulib_typecore_read16be(s,0,&version)||version||!mulib_typecore_read16be(s,2,&length)||length<14||length>n||!mulib_typecore_read16be(s,4,&coverage)||(coverage>>8)||!mulib_typecore_read16be(s,6,&count)||count>(length-14u)/6)return 0;s.size=length;uint32_t key=((uint32_t)left<<16)|right;size_t lo=0,hi=count;while(lo<hi){size_t mid=lo+(hi-lo)/2;uint32_t tag;size_t at=14+6*mid;if(!mulib_typecore_read32be(s,at,&tag))return 0;if(key<tag)hi=mid;else if(key>tag)lo=mid+1;else{uint16_t k;if(!mulib_typecore_read16be(s,at+4,&k))return 0;*out=(int16_t)(k<=INT16_MAX?(int32_t)k:(int32_t)k-65536);return 1;}}*out=0;return 1;}
#ifdef __cplusplus
}
#endif
#endif
