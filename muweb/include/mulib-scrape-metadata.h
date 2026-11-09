/** @file mulib-scrape-metadata.h
 * @brief Extract bounded HTML meta content attributes by name.
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE_METADATA in one translation unit.
 */
#ifndef MULIB_SCRAPE_METADATA_H
#define MULIB_SCRAPE_METADATA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Extract <meta name="key" content="value"> in a conservative, quoted-attribute subset. */
int mulib_scrape_metadata_get(const char *html,const char *key,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_METADATA) && !defined(MULIB_IMPL_SCRAPE_METADATA_IMPLEMENTED)
#define MULIB_IMPL_SCRAPE_METADATA_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_scrape_metadata_get(const char *h,const char *key,char *out,size_t cap){if(!h||!key||!out||!cap)return 0;const char *p=h;while((p=strstr(p,"<meta"))){const char *end=strchr(p,'>');if(!end)return 0;const char *a=strstr(p,"name=\"");if(a&&a<end){a+=6;const char *b=strchr(a,'\"');if(b&&b<end&&(size_t)(b-a)==strlen(key)&&!memcmp(a,key,b-a)){const char *c=strstr(p,"content=\"");if(c&&c<end){c+=9;const char *d=strchr(c,'\"');if(d&&d<end&&(size_t)(d-c)<cap){memcpy(out,c,(size_t)(d-c));out[d-c]=0;return 1;}}}}p=end+1;}return 0;}

#ifdef __cplusplus
}
#endif
#endif
