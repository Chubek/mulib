/** @file mulib-search-rank.h
 * @brief Deterministic lexical score for ranking title/description matches.
 * C11 single-header implementation. Define MULIB_IMPL_SEARCH_RANK in one translation unit.
 */
#ifndef MULIB_SEARCH_RANK_H
#define MULIB_SEARCH_RANK_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Case-insensitive token occurrence count of query in haystack; capped at SIZE_MAX. */
size_t mulib_search_rank_count(const char *text,const char *query);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SEARCH_RANK) && !defined(MULIB_IMPL_SEARCH_RANK_IMPLEMENTED)
#define MULIB_IMPL_SEARCH_RANK_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_search_rank_count(const char *s,const char *q){if(!s||!q||!*q)return 0;size_t n=0,ql=strlen(q);for(;*s;s++){size_t i=0;for(;i<ql&&s[i];i++)if(tolower((unsigned char)s[i])!=tolower((unsigned char)q[i]))break;if(i==ql&&n<SIZE_MAX)n++;}return n;}

#ifdef __cplusplus
}
#endif
#endif
