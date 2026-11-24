/** @file mulib-bibtex.h
 * @brief Simple BibTeX entry type and citation-key extraction.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_BIBTEX once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-bibtex.md for scope and limitations.
 */
#ifndef MULIB_BIBTEX_H
#define MULIB_BIBTEX_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_bibtex_entry(const char *source,size_t bytes,char *type,size_t type_cap,char *key,size_t key_cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BIBTEX_H */
#if defined(MULIB_IMPL_BIBTEX) && !defined(MULIB_BIBTEX_H_IMPLEMENTED)
#define MULIB_BIBTEX_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_bibtex_entry(const char *s,size_t n,char *type,size_t tc,char *key,size_t kc){if(!s||!type||!key||!tc||!kc)return 0;size_t i=0;while(i<n&&isspace((unsigned char)s[i]))i++;if(i==n||s[i++]!='@')return 0;size_t a=i;while(i<n&&isalpha((unsigned char)s[i]))i++;if(i==a||i-a>=tc)return 0;memcpy(type,s+a,i-a);type[i-a]=0;while(i<n&&isspace((unsigned char)s[i]))i++;if(i==n||(s[i]!='{'&&s[i]!='('))return 0;i++;while(i<n&&isspace((unsigned char)s[i]))i++;a=i;while(i<n&&s[i]!=','&&s[i]!='}'&&s[i]!=')'&&!isspace((unsigned char)s[i]))i++;if(i==a||i-a>=kc)return 0;memcpy(key,s+a,i-a);key[i-a]=0;return 1;}
#ifdef __cplusplus
}
#endif
#endif
