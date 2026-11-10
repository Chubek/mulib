/** @file mulib-mathparse.h
 * @brief Math-brace and superscript/subscript token validation.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_MATHPARSE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-mathparse.md for scope and limitations.
 */
#ifndef MULIB_MATHPARSE_H
#define MULIB_MATHPARSE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathparse_validate(const char *source,size_t bytes,size_t *error_offset);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MATHPARSE_H */
#if defined(MULIB_IMPL_MATHPARSE) && !defined(MULIB_MATHPARSE_H_IMPLEMENTED)
#define MULIB_MATHPARSE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_mathparse_validate(const char *s,size_t n,size_t *err){if(!s||!err)return 0;size_t depth=0;for(size_t i=0;i<n;i++){if(s[i]=='{'){if(++depth>256){*err=i;return 0;}}else if(s[i]=='}'){if(!depth){*err=i;return 0;}depth--;}else if(s[i]=='^'||s[i]=='_'){if(i+1>=n){*err=i;return 0;}}}if(depth){*err=n;return 0;}*err=n;return 1;}
#ifdef __cplusplus
}
#endif
#endif
