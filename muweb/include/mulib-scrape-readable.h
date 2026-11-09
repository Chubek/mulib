/** @file mulib-scrape-readable.h
 * @brief Remove markup and collapse HTML whitespace into readable text (basic subset).
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE_READABLE in one translation unit.
 */
#ifndef MULIB_SCRAPE_READABLE_H
#define MULIB_SCRAPE_READABLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Extract plain text, drop tags and scripts/styles, collapse whitespace; entities are not decoded. */
size_t mulib_scrape_readable(const char *html,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_READABLE) && !defined(MULIB_IMPL_SCRAPE_READABLE_IMPLEMENTED)
#define MULIB_IMPL_SCRAPE_READABLE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_scrape_readable(const char *s,char *o,size_t cap){if(!s||!o||!cap)return 0;size_t k=0;int tag=0,sp=0,skip=0;for(size_t i=0;s[i];i++){if(s[i]=='<'){if(!strncmp(s+i,"<script",7)||!strncmp(s+i,"<style",6))skip=1;else if(!strncmp(s+i,"</script",8)||!strncmp(s+i,"</style",7))skip=0;tag=1;continue;}if(tag){if(s[i]=='>')tag=0;continue;}if(skip)continue;unsigned char c=(unsigned char)s[i];if(isspace(c)){if(k&&!sp&&k+1<cap){o[k++]=' ';sp=1;}}else if(k+1<cap){o[k++]=(char)c;sp=0;}}if(k&&o[k-1]==' ')k--;o[k]=0;return k;}

#ifdef __cplusplus
}
#endif
#endif
