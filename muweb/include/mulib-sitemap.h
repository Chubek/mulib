/** @file mulib-sitemap.h
 * @brief Extract next loc entry from XML sitemap bytes without fetching.
 * C11 single-header implementation. Define MULIB_IMPL_SITEMAP in one translation unit.
 */
#ifndef MULIB_SITEMAP_H
#define MULIB_SITEMAP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Parse next <loc>...</loc> starting at *offset; plain XML subset, no namespace or entity decoding. */
int mulib_sitemap_next(const char *xml,size_t len,size_t *offset,char *url,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SITEMAP) && !defined(MULIB_IMPL_SITEMAP_IMPLEMENTED)
#define MULIB_IMPL_SITEMAP_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_sitemap_next(const char *s,size_t n,size_t *pos,char *out,size_t cap){if(!s||!pos||!out||*pos>n)return 0;for(size_t i=*pos;i+5<=n;i++)if(!memcmp(s+i,"<loc>",5)){size_t a=i+5,j=a;while(j+6<=n&&memcmp(s+j,"</loc>",6))j++;if(j+6>n||j-a>=cap)return 0;memcpy(out,s+a,j-a);out[j-a]=0;*pos=j+6;return 1;}return 0;}

#ifdef __cplusplus
}
#endif
#endif
