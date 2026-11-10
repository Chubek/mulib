/** @file mulib-unicodebidi.h
 * @brief Conservative base-direction detection from Hebrew/Arabic and ASCII letters.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_UNICODEBIDI once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-unicodebidi.md for scope and limitations.
 */
#ifndef MULIB_UNICODEBIDI_H
#define MULIB_UNICODEBIDI_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { mulib_unicodebidi_LTR=0,mulib_unicodebidi_RTL=1 };
int mulib_unicodebidi_base(const char *utf8,size_t length,unsigned *direction);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_UNICODEBIDI_H */
#if defined(MULIB_IMPL_UNICODEBIDI) && !defined(MULIB_UNICODEBIDI_H_IMPLEMENTED)
#define MULIB_UNICODEBIDI_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_unicodebidi_base(const char *s,size_t n,unsigned *d){if(!s||!d)return 0;size_t at=0;while(at<n){uint32_t cp;if(!mulib_typecore_utf8((const unsigned char*)s,n,&at,&cp))return 0;if((cp>=0x0590&&cp<=0x08ff)||(cp>=0xfb1d&&cp<=0xfeff)){*d=mulib_unicodebidi_RTL;return 1;}if((cp>='A'&&cp<='Z')||(cp>='a'&&cp<='z')){*d=mulib_unicodebidi_LTR;return 1;}}*d=mulib_unicodebidi_LTR;return 1;}
#ifdef __cplusplus
}
#endif
#endif
