/** @file mulib-search-query.h
 * @brief Percent-encoded form query builder and Google/Bing URL construction.
 * C11 single-header implementation. Define MULIB_IMPL_SEARCH_QUERY in one translation unit.
 */
#ifndef MULIB_SEARCH_QUERY_H
#define MULIB_SEARCH_QUERY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Encode a search phrase as application/x-www-form-urlencoded URL for supported provider. */
int mulib_search_query_url(const char *provider,const char *query,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEARCH_QUERY) && !defined(MULIB_IMPL_SEARCH_QUERY_IMPLEMENTED)
#define MULIB_IMPL_SEARCH_QUERY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_search_query_url(const char *provider,const char *query,char *out,size_t cap){if(!provider||!query||!out)return 0;const char *base=!strcmp(provider,"google")?"https://www.google.com/search?q=":!strcmp(provider,"bing")?"https://www.bing.com/search?q=":NULL;if(!base)return 0;size_t k=strlen(base);if(k>=cap)return 0;memcpy(out,base,k);const char *hex="0123456789ABCDEF";for(const unsigned char *p=(const unsigned char*)query;*p;p++){unsigned c=*p;if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~'){if(k+1>=cap)return 0;out[k++]=(char)c;}else if(c==' '){if(k+1>=cap)return 0;out[k++]='+';}else{if(k+3>=cap)return 0;out[k++]='%';out[k++]=hex[c>>4];out[k++]=hex[c&15];}}out[k]=0;return 1;}

#ifdef __cplusplus
}
#endif
#endif
