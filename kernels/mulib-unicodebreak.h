/** @file mulib-unicodebreak.h
 * @brief Line-break opportunity discovery after ASCII whitespace and common separators.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_UNICODEBREAK once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-unicodebreak.md for scope and limitations.
 */
#ifndef MULIB_UNICODEBREAK_H
#define MULIB_UNICODEBREAK_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_unicodebreak_possible(const char *utf8,size_t size,size_t offset);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_UNICODEBREAK_H */
#if defined(MULIB_IMPL_UNICODEBREAK) && !defined(MULIB_UNICODEBREAK_H_IMPLEMENTED)
#define MULIB_UNICODEBREAK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_unicodebreak_possible(const char *s,size_t n,size_t at){if(!s||at>n)return 0;if(at==n)return 1;if(!at)return 0;unsigned char c=(unsigned char)s[at-1];return c==' '||c=='\n'||c=='\t'||c=='-'||c=='/';}
#ifdef __cplusplus
}
#endif
#endif
