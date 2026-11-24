/** @file mulib-unicodeprops.h
 * @brief Unicode scalar validity and conservative text class classification.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_UNICODEPROPS once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-unicodeprops.md for scope and limitations.
 */
#ifndef MULIB_UNICODEPROPS_H
#define MULIB_UNICODEPROPS_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { mulib_unicodeprops_CONTROL=1,mulib_unicodeprops_SPACE=2,mulib_unicodeprops_MARK=3,mulib_unicodeprops_LETTER=4,mulib_unicodeprops_NUMBER=5,mulib_unicodeprops_OTHER=6 };
int mulib_unicodeprops_valid(uint32_t cp);
unsigned mulib_unicodeprops_class(uint32_t cp);
int mulib_unicodeprops_rtl(uint32_t cp);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_UNICODEPROPS_H */
#if defined(MULIB_IMPL_UNICODEPROPS) && !defined(MULIB_UNICODEPROPS_H_IMPLEMENTED)
#define MULIB_UNICODEPROPS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_unicodeprops_valid(uint32_t c){return c<=0x10ffffu&&(c<0xd800u||c>0xdfffu);}
unsigned mulib_unicodeprops_class(uint32_t c){if(!mulib_unicodeprops_valid(c))return mulib_unicodeprops_OTHER;if(c<=31u||c==127u)return mulib_unicodeprops_CONTROL;if(c==32u||c==0xa0u||c==0x3000u)return mulib_unicodeprops_SPACE;if((c>='0'&&c<='9')||(c>=0x0660&&c<=0x0669))return mulib_unicodeprops_NUMBER;if((c>=0x0300u&&c<=0x036fu)||(c>=0x1ab0u&&c<=0x1affu))return mulib_unicodeprops_MARK;if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>=0x0590u&&c<=0x9fffu))return mulib_unicodeprops_LETTER;return mulib_unicodeprops_OTHER;}
int mulib_unicodeprops_rtl(uint32_t c){return (c>=0x0590u&&c<=0x08ffu)||(c>=0xfb1du&&c<=0xfdffu)||(c>=0xfe70u&&c<=0xfeffu);}
#ifdef __cplusplus
}
#endif
#endif
