/** @file mulib-scrape.h
 * @brief Simple query-free HTML text extraction orchestrator.
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE in one translation unit.
 */
#ifndef MULIB_SCRAPE_H
#define MULIB_SCRAPE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Extract readable text from HTML through a caller-owned buffer. */
size_t mulib_scrape_text(const char *html,char *out,size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE) && !defined(MULIB_IMPL_SCRAPE_IMPLEMENTED)
#define MULIB_IMPL_SCRAPE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_scrape_text(const char *s,char *o,size_t cap){if(!s||!o||!cap)return 0;size_t k=0;int in=0;for(;*s;s++){if(*s=='<'){in=1;continue;}if(*s=='>'){in=0;continue;}if(!in&&k+1<cap)o[k++]=*s;}o[k]=0;return k;}

#ifdef __cplusplus
}
#endif
#endif
