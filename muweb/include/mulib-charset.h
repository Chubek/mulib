/** @file mulib-charset.h
 * @brief Bounded charset parameter extraction from Content-Type headers.
 * C11 single-header implementation. Define MULIB_IMPL_CHARSET in one translation unit.
 */
#ifndef MULIB_CHARSET_H
#define MULIB_CHARSET_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Find charset token, write lowercase encoding name. */
int mulib_charset_extract(const char *value,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CHARSET) && !defined(MULIB_IMPL_CHARSET_IMPLEMENTED)
#define MULIB_IMPL_CHARSET_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_charset_extract(const char *s,char *out,size_t cap){if(!s||!out||!cap)return 0;while(*s){if(*s++!=';')continue;while(*s==' '||*s=='\t')s++;const char *a=s;while(isalnum((unsigned char)*s)||*s=='-')s++;if((size_t)(s-a)!=7)continue;const char *k="charset";size_t i;for(i=0;i<7;i++)if(tolower((unsigned char)a[i])!=k[i])break;if(i!=7)continue;while(*s==' '||*s=='\t')s++;if(*s++!='=')continue;while(*s==' '||*s=='\t')s++;int quoted=*s=='\"';if(quoted)s++;size_t n=0;while(*s&&(quoted?*s!='\"':*s!=';'&&!isspace((unsigned char)*s))){if(n+1>=cap)return 0;out[n++]=(char)tolower((unsigned char)*s++);}if(!n)return 0;out[n]=0;return 1;}return 0;}

#ifdef __cplusplus
}
#endif
#endif
