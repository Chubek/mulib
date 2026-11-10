/** @file mulib-texvalue.h
 * @brief Owned number/string value container with safe copy and destruction.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXVALUE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texvalue.md for scope and limitations.
 */
#ifndef MULIB_TEXVALUE_H
#define MULIB_TEXVALUE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { mulib_texvalue_NIL=0,mulib_texvalue_NUMBER=1,mulib_texvalue_STRING=2 };
typedef struct mulib_texvalue_value { unsigned tag;double number;char *string; } mulib_texvalue_value_t;
int mulib_texvalue_string(mulib_texvalue_value_t *out,const char *str);
int mulib_texvalue_number(mulib_texvalue_value_t *out,double number);
int mulib_texvalue_copy(mulib_texvalue_value_t *out,const mulib_texvalue_value_t *src);
void mulib_texvalue_destroy(mulib_texvalue_value_t *value);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXVALUE_H */
#if defined(MULIB_IMPL_TEXVALUE) && !defined(MULIB_TEXVALUE_H_IMPLEMENTED)
#define MULIB_TEXVALUE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texvalue_string(mulib_texvalue_value_t *o,const char *s){if(!o||!s)return 0;size_t n=strlen(s);char *p=(char*)malloc(n+1);if(!p)return 0;memcpy(p,s,n+1);*o=(mulib_texvalue_value_t){mulib_texvalue_STRING,0,p};return 1;}
int mulib_texvalue_number(mulib_texvalue_value_t *o,double n){if(!o||!isfinite(n))return 0;*o=(mulib_texvalue_value_t){mulib_texvalue_NUMBER,n,NULL};return 1;}
int mulib_texvalue_copy(mulib_texvalue_value_t *o,const mulib_texvalue_value_t *v){if(!o||!v||o==v)return 0;if(v->tag==mulib_texvalue_STRING)return mulib_texvalue_string(o,v->string);*o=*v;return 1;}
void mulib_texvalue_destroy(mulib_texvalue_value_t *v){if(v){if(v->tag==mulib_texvalue_STRING)free(v->string);memset(v,0,sizeof(*v));}}
#ifdef __cplusplus
}
#endif
#endif
