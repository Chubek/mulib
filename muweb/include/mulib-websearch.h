/** @file mulib-websearch.h
 * @brief Search provider identification and URL routing for Google/Bing browser searches.
 * C11 single-header implementation. Define MULIB_IMPL_WEBSEARCH in one translation unit.
 */
#ifndef MULIB_WEBSEARCH_H
#define MULIB_WEBSEARCH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_websearch_provider {MULIB_WEBSEARCH_GOOGLE=1,MULIB_WEBSEARCH_BING=2} mulib_websearch_provider_t;
const char *mulib_websearch_provider_name(mulib_websearch_provider_t p);
int mulib_websearch_known(mulib_websearch_provider_t p);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBSEARCH) && !defined(MULIB_IMPL_WEBSEARCH_IMPLEMENTED)
#define MULIB_IMPL_WEBSEARCH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

const char *mulib_websearch_provider_name(mulib_websearch_provider_t p){switch(p){case MULIB_WEBSEARCH_GOOGLE:return "google";case MULIB_WEBSEARCH_BING:return "bing";default:return NULL;}}
int mulib_websearch_known(mulib_websearch_provider_t p){return mulib_websearch_provider_name(p)!=NULL;}

#ifdef __cplusplus
}
#endif
#endif
