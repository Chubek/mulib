/** @file mulib-unicodegrapheme.h
 * @brief UTF-8 grapheme iterator with combining-mark continuation baseline.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_UNICODEGRAPHEME once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-unicodegrapheme.md for scope and limitations.
 */
#ifndef MULIB_UNICODEGRAPHEME_H
#define MULIB_UNICODEGRAPHEME_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_unicodegrapheme_next(const char *utf8,size_t bytes,size_t *offset,size_t *length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_UNICODEGRAPHEME_H */
#if defined(MULIB_IMPL_UNICODEGRAPHEME) && !defined(MULIB_UNICODEGRAPHEME_H_IMPLEMENTED)
#define MULIB_UNICODEGRAPHEME_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_unicodegrapheme_next(const char *s,size_t n,size_t *at,size_t *len){if(!s||!at||!len||*at>=n)return 0;size_t begin=*at,i=begin;uint32_t cp;if(!mulib_typecore_utf8((const unsigned char*)s,n,&i,&cp))return 0;while(i<n){size_t j=i;uint32_t u;if(!mulib_typecore_utf8((const unsigned char*)s,n,&j,&u))return 0;if((u>=0x300u&&u<=0x36fu)||(u>=0x1ab0u&&u<=0x1affu)||(u>=0xfe20u&&u<=0xfe2fu))i=j;else break;}*at=i;*len=i-begin;return 1;}
#ifdef __cplusplus
}
#endif
#endif
