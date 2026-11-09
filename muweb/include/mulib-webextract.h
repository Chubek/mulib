/** @file mulib-webextract.h
 * @brief Locate JSON-LD script block within HTML as borrowed byte span.
 * C11 single-header implementation. Define MULIB_IMPL_WEBEXTRACT in one translation unit.
 */
#ifndef MULIB_WEBEXTRACT_H
#define MULIB_WEBEXTRACT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_webextract_span {const char *data;size_t length;} mulib_webextract_span_t;
int mulib_webextract_jsonld(const char *html,mulib_webextract_span_t *out);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBEXTRACT) && !defined(MULIB_IMPL_WEBEXTRACT_IMPLEMENTED)
#define MULIB_IMPL_WEBEXTRACT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_webextract_jsonld(const char *s,mulib_webextract_span_t *o){if(!s||!o)return 0;const char *p=s;while((p=strstr(p,"<script"))){const char *e=strchr(p,'>');if(!e)return 0;const char *type=strstr(p,"application/ld+json");if(type&&type<e){const char *z=strstr(e+1,"</script>");if(!z)return 0;o->data=e+1;o->length=(size_t)(z-e-1);return 1;}p=e+1;}return 0;}

#ifdef __cplusplus
}
#endif
#endif
