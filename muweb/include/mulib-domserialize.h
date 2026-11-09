/** @file mulib-domserialize.h
 * @brief HTML text escaping into caller-owned output buffer.
 * C11 single-header implementation. Define MULIB_IMPL_DOMSERIALIZE in one translation unit.
 */
#ifndef MULIB_DOMSERIALIZE_H
#define MULIB_DOMSERIALIZE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Escape &, <, >, double quote, single quote; return number of output bytes or 0 for failure. */
size_t mulib_domserialize_escape(const char *text,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DOMSERIALIZE) && !defined(MULIB_IMPL_DOMSERIALIZE_IMPLEMENTED)
#define MULIB_IMPL_DOMSERIALIZE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_domserialize_escape(const char *s,char *o,size_t cap){if(!s||!o||!cap)return 0;size_t k=0;for(;*s;s++){const char *rep=NULL;switch(*s){case '&':rep="&amp;";break;case '<':rep="&lt;";break;case '>':rep="&gt;";break;case '"':rep="&quot;";break;case '\'':rep="&#39;";break;default:break;}size_t n=rep?strlen(rep):1;if(n>=cap-k)return 0;if(rep)memcpy(o+k,rep,n);else o[k]=*s;k+=n;}o[k]=0;return k;}

#ifdef __cplusplus
}
#endif
#endif
