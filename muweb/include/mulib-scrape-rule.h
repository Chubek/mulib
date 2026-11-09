/** @file mulib-scrape-rule.h
 * @brief Owned declarative selector/property pair for extraction rules.
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE_RULE in one translation unit.
 */
#ifndef MULIB_SCRAPE_RULE_H
#define MULIB_SCRAPE_RULE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_scrape_rule {char *selector,*attribute;int all;} mulib_scrape_rule_t;
int mulib_scrape_rule_init(mulib_scrape_rule_t *r,const char *selector,const char *attribute,int all);
void mulib_scrape_rule_free(mulib_scrape_rule_t *r);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_RULE) && !defined(MULIB_IMPL_SCRAPE_RULE_IMPLEMENTED)
#define MULIB_IMPL_SCRAPE_RULE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

static char *mulib_scrape_rule_dup_(const char *s){if(!s)return NULL;size_t n=strlen(s)+1;char *p=(char*)malloc(n);if(p)memcpy(p,s,n);return p;}
int mulib_scrape_rule_init(mulib_scrape_rule_t *r,const char *s,const char *a,int all){if(!r||!s||!*s||strlen(s)>8192||(a&&strlen(a)>1024))return 0;char *x=mulib_scrape_rule_dup_(s),*y=mulib_scrape_rule_dup_(a);if(!x||(a&&!y)){free(x);free(y);return 0;}r->selector=x;r->attribute=y;r->all=!!all;return 1;}
void mulib_scrape_rule_free(mulib_scrape_rule_t *r){if(r){free(r->selector);free(r->attribute);memset(r,0,sizeof *r);}}

#ifdef __cplusplus
}
#endif
#endif
