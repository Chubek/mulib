/** @file mulib-browserlocator.h
 * @brief Typed, owned browser locator definitions for CSS, XPath, text, and role queries.
 * C11 single-header implementation. Define MULIB_IMPL_BROWSERLOCATOR in one translation unit.
 */
#ifndef MULIB_BROWSERLOCATOR_H
#define MULIB_BROWSERLOCATOR_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_browserlocator_kind {MULIB_BROWSERLOCATOR_CSS=1,MULIB_BROWSERLOCATOR_XPATH=2,MULIB_BROWSERLOCATOR_TEXT=3,MULIB_BROWSERLOCATOR_ROLE=4} mulib_browserlocator_kind_t;
typedef struct mulib_browserlocator {mulib_browserlocator_kind_t kind;char *selector;} mulib_browserlocator_t;
int mulib_browserlocator_init(mulib_browserlocator_t *l,mulib_browserlocator_kind_t kind,const char *s);
void mulib_browserlocator_free(mulib_browserlocator_t *l);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BROWSERLOCATOR) && !defined(MULIB_IMPL_BROWSERLOCATOR_IMPLEMENTED)
#define MULIB_IMPL_BROWSERLOCATOR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_browserlocator_init(mulib_browserlocator_t *l,mulib_browserlocator_kind_t k,const char *s){if(!l||!s||!*s||k<1||k>4||strlen(s)>8192)return 0;size_t n=strlen(s)+1;char *p=(char*)malloc(n);if(!p)return 0;memcpy(p,s,n);l->kind=k;l->selector=p;return 1;}
void mulib_browserlocator_free(mulib_browserlocator_t *l){if(l){free(l->selector);l->selector=NULL;l->kind=0;}}

#ifdef __cplusplus
}
#endif
#endif
