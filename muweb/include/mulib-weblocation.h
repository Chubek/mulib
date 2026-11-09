/** @file mulib-weblocation.h
 * @brief Owned current URL state for navigation integration.
 * C11 single-header implementation. Define MULIB_IMPL_WEBLOCATION in one translation unit.
 */
#ifndef MULIB_WEBLOCATION_H
#define MULIB_WEBLOCATION_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_weblocation {char *href;} mulib_weblocation_t;
int mulib_weblocation_set(mulib_weblocation_t *l,const char *href);
void mulib_weblocation_free(mulib_weblocation_t *l);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBLOCATION) && !defined(MULIB_IMPL_WEBLOCATION_IMPLEMENTED)
#define MULIB_IMPL_WEBLOCATION_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_weblocation_set(mulib_weblocation_t *l,const char *h){if(!l||!h||strlen(h)>8192||!(strstr(h,"://")))return 0;size_t n=strlen(h)+1;char *p=(char*)malloc(n);if(!p)return 0;memcpy(p,h,n);free(l->href);l->href=p;return 1;}
void mulib_weblocation_free(mulib_weblocation_t *l){if(l){free(l->href);l->href=NULL;}}

#ifdef __cplusplus
}
#endif
#endif
