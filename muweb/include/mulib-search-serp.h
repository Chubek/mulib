/** @file mulib-search-serp.h
 * @brief Extract href attributes from simple quoted HTML anchor markup.
 * C11 single-header implementation. Define MULIB_IMPL_SEARCH_SERP in one translation unit.
 */
#ifndef MULIB_SEARCH_SERP_H
#define MULIB_SEARCH_SERP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Retrieve Nth quoted href attribute; does not decode entities or execute scripts. */
int mulib_search_serp_href(const char *html,size_t index,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEARCH_SERP) && !defined(MULIB_IMPL_SEARCH_SERP_IMPLEMENTED)
#define MULIB_IMPL_SEARCH_SERP_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_search_serp_href(const char *s,size_t ix,char *o,size_t cap){if(!s||!o||!cap)return 0;const char *p=s;while((p=strstr(p,"<a"))){const char *e=strchr(p,'>');if(!e)return 0;const char *q=strstr(p,"href=\"");if(q&&q<e){q+=6;const char *z=strchr(q,'\"');if(z&&z<e){if(!ix--){size_t n=(size_t)(z-q);if(n>=cap)return 0;memcpy(o,q,n);o[n]=0;return 1;}}}p=e+1;}return 0;}

#ifdef __cplusplus
}
#endif
#endif
