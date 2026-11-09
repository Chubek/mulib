/** @file mulib-webstorage.h
 * @brief In-memory Web Storage style string key-value store.
 * C11 single-header implementation. Define MULIB_IMPL_WEBSTORAGE in one translation unit.
 */
#ifndef MULIB_WEBSTORAGE_H
#define MULIB_WEBSTORAGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_webstorage_entry {char *key,*value;} mulib_webstorage_entry_t;
typedef struct mulib_webstorage {mulib_webstorage_entry_t *entries;size_t len,cap;} mulib_webstorage_t;
int mulib_webstorage_set(mulib_webstorage_t *s,const char *key,const char *value);
const char *mulib_webstorage_get(const mulib_webstorage_t *s,const char *key);
void mulib_webstorage_free(mulib_webstorage_t *s);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBSTORAGE) && !defined(MULIB_IMPL_WEBSTORAGE_IMPLEMENTED)
#define MULIB_IMPL_WEBSTORAGE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

static char *mulib_webstorage_dup_(const char *s){size_t n=strlen(s)+1;char *p=(char*)malloc(n);if(p)memcpy(p,s,n);return p;}
const char *mulib_webstorage_get(const mulib_webstorage_t *s,const char *key){if(!s||!key)return NULL;for(size_t i=0;i<s->len;i++)if(!strcmp(s->entries[i].key,key))return s->entries[i].value;return NULL;}
int mulib_webstorage_set(mulib_webstorage_t *s,const char *key,const char *value){if(!s||!key||!value)return 0;char *v=mulib_webstorage_dup_(value);if(!v)return 0;for(size_t i=0;i<s->len;i++)if(!strcmp(s->entries[i].key,key)){free(s->entries[i].value);s->entries[i].value=v;return 1;}char *k=mulib_webstorage_dup_(key);if(!k){free(v);return 0;}if(s->len==s->cap){size_t c=s->cap?s->cap*2:8;if(c>SIZE_MAX/sizeof(*s->entries)){free(k);free(v);return 0;}void *p=realloc(s->entries,c*sizeof(*s->entries));if(!p){free(k);free(v);return 0;}s->entries=(mulib_webstorage_entry_t*)p;s->cap=c;}s->entries[s->len].key=k;s->entries[s->len++].value=v;return 1;}
void mulib_webstorage_free(mulib_webstorage_t *s){if(!s)return;for(size_t i=0;i<s->len;i++){free(s->entries[i].key);free(s->entries[i].value);}free(s->entries);memset(s,0,sizeof *s);}

#ifdef __cplusplus
}
#endif
#endif
